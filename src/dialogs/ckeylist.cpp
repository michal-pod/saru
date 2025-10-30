/*
 SKYM - SSH KeY Manager
 Copyright (C) 2025 Michał Podsiadlik

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include "dialogs/ckeylist.h"

#include <cstring>
#include <string>

#include <libssha/agent/session.h>

#include "external/base64.hpp"

#include "stdatl.h"
#include "../factories/bitmap-factory.h"
#include "cabout.h"
#include "csettings.h"
#include "servers/windows-session.h"
#include "cconstrainsopen.h"
#include "system/directories.h"
#include "key-constrains-loader.h"
#include "dialogs/cdialogexception.h"
#include "config.h"

using namespace nglab::skym;
namespace
{
    constexpr GUID SKYM_TRAY_ICON_GUID = {0xc763880f, 0x8741, 0x4d11, {0xa6, 0x83, 0xc7, 0x5, 0x3c, 0xf7, 0x18, 0x5e}};
    enum
    {
        TIMER_ID_REFRESH_LIFETIME = 1,
        TIMER_ID_RESTORE_AUTH_LINE
    };

    struct CrossUIMessage
    {
        enum MessageType
        {
            TrayNotify,
            TaskBox
        } type;
        std::string title;
        std::string message;
        DWORD infoType; // Used for TrayNotify messages
        UINT timeout;
    };

}
CKeyList::CKeyList()
    : LogEnabler("CKeyList")
{
}

void CKeyList::DisplayTrayNotification(const std::string &title, const std::string &message, DWORD infoType, UINT timeout)
{

    CrossUIMessage *msg = new CrossUIMessage();
    msg->type = CrossUIMessage::TrayNotify;
    msg->title = title;
    msg->message = message;
    msg->infoType = infoType;
    msg->timeout = timeout;

    PostMessage(WM_CROSS_UI_MESSAGE, 0, reinterpret_cast<LPARAM>(msg));
}

void CKeyList::DisplayMessageBox(const std::string &title, const std::string &message)
{
    CrossUIMessage *msg = new CrossUIMessage();
    msg->type = CrossUIMessage::TaskBox;
    msg->title = title;
    msg->message = message;

    PostMessage(WM_CROSS_UI_MESSAGE, 0, reinterpret_cast<LPARAM>(msg));
}

BOOL CKeyList::PreTranslateMessage(MSG *pMsg)
{
    return IsDialogMessage(pMsg);
}

LRESULT CKeyList::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
{
    SetIcon(IconFactory::getLarge(IDI_ICON1));
    m_keyList.Attach(GetDlgItem(IDC_KEY_LIST));
    m_keyList.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_keyList.ModifyStyle(0, LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS);

    m_keyList.InsertColumn(0, "Key Type", LVCFMT_LEFT, 80);
    m_keyList.InsertColumn(1, "Comment", LVCFMT_LEFT, 160);
    m_keyList.InsertColumn(2, "Fingerprint", LVCFMT_LEFT, 360);


    m_keyConstraints.Attach(GetDlgItem(IDC_KEY_CONSTRAINS));
    m_keyConstraints.ModifyStyle(0, LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS);
    m_keyConstraints.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_keyConstraints.InsertColumn(0, "From", LVCFMT_LEFT, 150);
    m_keyConstraints.InsertColumn(1, "To", LVCFMT_LEFT, 150);
    m_keyConstraints.EnableWindow(FALSE);

    // Adjust column widths based on current control size
    BOOL bHandledDummy;
    OnDpiChanged(0, 0, 0, bHandledDummy);

    m_confirmCheck.Attach(GetDlgItem(IDC_REQUIRE_CONFIRMATION));
    m_confirmCheck.EnableWindow(FALSE);

    m_keyTimeoutText.Attach(GetDlgItem(IDC_KEY_TIMEOUT));
    m_keyTimeoutText.SetWindowText("N/A");

    m_constrainsLoad.Attach(GetDlgItem(IDC_DEST_CONS_LOAD));
    m_constrainsLoad.EnableWindow(FALSE);
    m_constrainsLoad.SetIcon(IconFactory::get(IDI_APPLICATION_PUT));

    m_constrainsClear.Attach(GetDlgItem(IDC_DEST_CONS_CLEAR));
    m_constrainsClear.EnableWindow(FALSE);
    m_constrainsClear.SetIcon(IconFactory::get(IDI_BIN));

    m_copyPubId.Attach(GetDlgItem(IDC_COPY_PUB_ID));
    m_copyPubId.EnableWindow(FALSE);
    m_copyPubId.SetIcon(IconFactory::get(IDI_PAGE_COPY));

    m_keyTimeoutIcon.Attach(GetDlgItem(IDC_KEY_TIMEOUT_ICON));

    CButton btnClose;
    btnClose.Attach(GetDlgItem(IDCANCEL));
    btnClose.SetIcon(IconFactory::get(IDI_CROSS));

    // Tray icon is initialized in the header's inline OnInitDialog; avoid duplicate registration here.
    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = m_hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYNOTIFY;
    nid.hIcon = LoadIcon(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDI_ICON1));
    nid.guidItem = SKYM_TRAY_ICON_GUID;
    nid.uVersion = NOTIFYICON_VERSION_4;
    // Copy tooltip text with strncpy and explicit null-termination
    strncpy(nid.szTip, _T("SSH Key Manager"), sizeof(nid.szTip) - 1);
    nid.szTip[sizeof(nid.szTip) - 1] = '\0';

    Shell_NotifyIcon(NIM_ADD, &nid);

    m_trayMenu.CreatePopupMenu();
    m_trayMenu.AppendMenu(MF_STRING, IDM_SHOW, "Show");
    m_trayMenu.SetMenuItemBitmaps(IDM_SHOW, MF_BYCOMMAND,
                                  BitmapFactory::get(IDI_KEY),
                                  nullptr);
    if (DebugConsole::instance().isEnabled())
    {
        m_trayMenu.AppendMenu(MF_STRING, IDM_SHOW_DEBUG_CONSOLE, "Show debug console");
        m_trayMenu.SetMenuItemBitmaps(IDM_SHOW_DEBUG_CONSOLE, MF_BYCOMMAND,
                                      BitmapFactory::get(IDI_BUG),
                                      nullptr);
    }
    m_trayMenu.AppendMenu(MF_SEPARATOR);
    m_trayMenu.AppendMenu(MF_STRING, IDM_SHOW_SETTINGS, "Settings");
    m_trayMenu.SetMenuItemBitmaps(IDM_SHOW_SETTINGS, MF_BYCOMMAND,
                                  BitmapFactory::get(IDI_WRENCH),
                                  nullptr);
    m_trayMenu.AppendMenu(MF_STRING, IDM_ABOUT, "About");
    m_trayMenu.SetMenuItemBitmaps(IDM_ABOUT, MF_BYCOMMAND,
                                  BitmapFactory::get(IDI_INFO),
                                  nullptr);
    m_trayMenu.AppendMenu(MF_SEPARATOR);
    m_trayMenu.AppendMenu(MF_STRING, IDM_EXIT, "Exit");
    m_trayMenu.SetMenuItemBitmaps(IDM_EXIT, MF_BYCOMMAND,
                                  BitmapFactory::get(IDI_DOOR_OUT),
                                  nullptr);

    return TRUE;
}

LRESULT CKeyList::OnDestroyDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
{
    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = m_hWnd;
    nid.uID = 1;
    nid.guidItem = SKYM_TRAY_ICON_GUID;

    Shell_NotifyIcon(NIM_DELETE, &nid);

    return 0;
}

LRESULT CKeyList::OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
{
    if (wParam == TIMER_ID_REFRESH_LIFETIME)
    {
        updateKeyDetails(true);
    }
    else if (wParam == TIMER_ID_RESTORE_AUTH_LINE)
    {
        updateKeyDetails();

        KillTimer(TIMER_ID_RESTORE_AUTH_LINE);
    }
    return 0;
}

LRESULT CKeyList::OnTrayNotification(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam)
{
    if (lParam == WM_LBUTTONUP)
    {
        showOrHideWindow();
    }
    else if (lParam == WM_MBUTTONUP)
    {
        DebugConsole::instance().toggleVisibility();
    }
    else if (lParam == WM_RBUTTONUP)
    {
        POINT pt;
        GetCursorPos(&pt);
        SetForegroundWindow(m_hWnd);

        UINT cmd = m_trayMenu.TrackPopupMenu(
            TPM_RIGHTBUTTON | TPM_RETURNCMD,
            pt.x, pt.y,
            m_hWnd);

        if (cmd != 0)
        {
            // Call the handler directly instead of posting WM_COMMAND. Posting a WM_COMMAND
            // can result in the same command being processed twice if the menu system also
            // generates a command message. Direct invocation avoids duplicate handling.
            HandleTrayCommand(cmd);
        }
    }

    return 0;
}

LRESULT CKeyList::OnCrossUIMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
{
    CrossUIMessage *msg = reinterpret_cast<CrossUIMessage *>(lParam);
    if (msg == nullptr)
    {
        log.error("Received null CrossUIMessage");
        return 0;
    }

    switch (msg->type)
    {
    case CrossUIMessage::TrayNotify:
    {
        NOTIFYICONDATA nid = {};
        nid.cbSize = sizeof(NOTIFYICONDATA);
        nid.hWnd = m_hWnd;
        nid.uID = 1;
        nid.uFlags = NIF_INFO;
        // Copy with strncpy and ensure null-termination for portability (avoid _s functions)
        strncpy(nid.szInfoTitle, msg->title.c_str(), sizeof(nid.szInfoTitle) - 1);
        nid.szInfoTitle[sizeof(nid.szInfoTitle) - 1] = '\0';
        strncpy(nid.szInfo, msg->message.c_str(), sizeof(nid.szInfo) - 1);
        nid.szInfo[sizeof(nid.szInfo) - 1] = '\0';
        nid.dwInfoFlags = msg->infoType;
        nid.uTimeout = msg->timeout;

        Shell_NotifyIcon(NIM_MODIFY, &nid);
    }
    break;
    case CrossUIMessage::TaskBox:
    {
        log.info("Displaying task dialog: {} - {}", msg->title, msg->message);
        CTaskDialog taskDialog;
        std::wstring title = StringConverter::u8w2(msg->title);
        std::wstring message = StringConverter::u8w2(msg->message);
        taskDialog.SetWindowTitle(title.c_str());
        taskDialog.SetContentText(message.c_str());
        TASKDIALOG_BUTTON buttons[] =
            {
                {IDOK, L"OK"}};
        taskDialog.SetButtons(buttons, ARRAYSIZE(buttons));
        taskDialog.SetMainIcon(TD_INFORMATION_ICON);
        taskDialog.DoModal(m_hWnd);
    }
    default:
        log.warning("Unhandled CrossUIMessage type: {}", static_cast<int>(msg->type));
        break;
    }

    delete msg;
    return 0;
}

// This is only leftover from trying to make this app DPI-aware; it wasn't worth the effort.
LRESULT CKeyList::OnDpiChanged(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
{
    RECT rcList;

    m_keyList.GetClientRect(&rcList);
    int width = rcList.right - rcList.left;
    m_keyList.SetColumnWidth(0, width * 15 / 100);
    m_keyList.SetColumnWidth(1, width * 20 / 100);
    m_keyList.SetColumnWidth(2, width * 65 / 100);

    m_keyConstraints.GetClientRect(&rcList);
    width = rcList.right - rcList.left;
    m_keyConstraints.SetColumnWidth(0, width / 2);
    m_keyConstraints.SetColumnWidth(1, width / 2);

    bHandled = TRUE;

    return 0;
}

LRESULT CKeyList::OnClose(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    showOrHideWindow();
    return 0;
}

LRESULT CKeyList::OnDialogClose(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
{
    showOrHideWindow();
    return 0;
}

LRESULT CKeyList::OnShow(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    showOrHideWindow();
    return 0;
}
LRESULT CKeyList::OnShowDebugConsole(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    auto &console = nglab::skym::DebugConsole::instance();
    // std::string msg = "Toggling debug console visibility " + std::to_string(wNotifyCode) + " " + std::to_string(wID) + " " + std::to_string(reinterpret_cast<uintptr_t>(hWndCtl));
    log.debug("Toggling debug console visibility wNotifyCode={}, wID={}, hWndCtl={}", wNotifyCode, wID, reinterpret_cast<uintptr_t>(hWndCtl));
    if (console.isVisible())
    {
        console.hide();
        m_trayMenu.ModifyMenu(IDM_SHOW_DEBUG_CONSOLE, MF_BYCOMMAND | MF_STRING, IDM_SHOW_DEBUG_CONSOLE, "Show debug console");
    }
    else
    {
        m_trayMenu.ModifyMenu(IDM_SHOW_DEBUG_CONSOLE, MF_BYCOMMAND | MF_STRING, IDM_SHOW_DEBUG_CONSOLE, "Hide debug console");
        console.show();
    }
    return 0;
}

void CKeyList::HandleTrayCommand(UINT cmd)
{
    BOOL bHandled = FALSE;
    switch (cmd)
    {
    case IDM_SHOW:
        showOrHideWindow();
        break;
    case IDM_SHOW_DEBUG_CONSOLE:
        // Forward to the existing handler with a local bHandled.
        OnShowDebugConsole(0, IDM_SHOW_DEBUG_CONSOLE, nullptr, bHandled);
        break;
    case IDM_EXIT:
        PostQuitMessage(0);
        break;
    case IDM_SHOW_SETTINGS:
        OnShowSettings(0, IDM_SHOW_SETTINGS, nullptr, bHandled);
        break;
    case IDM_ABOUT:
        OnAbout(0, IDM_ABOUT, nullptr, bHandled);
        break;
    default:
        log.warning("Unhandled tray command: {}", cmd);
        break;
    }
}

LRESULT CKeyList::OnExit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    PostQuitMessage(0);
    return 0;
}

void CKeyList::onKeyAdded(nglab::libssha::KeyBasePtr key)
{
    log.trace("Key added: {}", key->fingerprint());
    addKeyToList(key);
    if (!key->hasDestConstraints())
    {
        try
        {
            if (KeyConstrainsLoader::loadDestinationConstrains(key))
            {
                m_userLoadedConstraints.insert(key->fingerprint());
            }
        }
        catch (const nglab::skym::DialogException &e)
        {
            e.show(m_hWnd);
        }
        catch (const std::exception &e)
        {
            log.error("Failed to load constraints for key {}: {}", key->fingerprint(), e.what());
        }
    }
    if (shouldNotifyKeyOperations())
    {
        DisplayTrayNotification("Key Added", std::format("Key {} has been added to agent.", key->comment()), NIIF_INFO);
    }
}

void CKeyList::onKeyPreRemove(nglab::libssha::KeyBasePtr key)
{
    log.trace("Key about to be removed: {}", key->fingerprint());
    if (shouldNotifyKeyOperations())
    {
        DisplayTrayNotification("Key Removed", std::format("Key {} is being removed from agent.", key->comment()), NIIF_INFO);
    }
}

void CKeyList::onKeyRemoved(const std::string &fingerprint)
{
    log.trace("Key removed: {}", fingerprint);
    removeKeyFromList(fingerprint);
}

void CKeyList::onKeysCleared()
{
    log.trace("All keys cleared");
    m_keyList.DeleteAllItems();
    if (shouldNotifyKeyOperations())
    {
        DisplayTrayNotification("All Keys Removed", "All keys have been removed from the key manager.", NIIF_INFO);
    }
}

void CKeyList::onKeyUsed(nglab::libssha::KeyBasePtr key, const nglab::libssha::Session *session)
{
    log.info("Key used: {} in session", key->fingerprint());
    CRegKey reg_key;
    if (reg_key.Open(HKEY_CURRENT_USER, SKYM_KEY_ROOT, KEY_READ) == ERROR_SUCCESS)
    {
        DWORD notifyKeyUsage = 0;
        if (reg_key.QueryDWORDValue("NotifyKeyUsage", notifyKeyUsage) == ERROR_SUCCESS && notifyKeyUsage == 1)
        {
            DisplayTrayNotification("Key Used", std::format("Key {} was used by {}", key->comment(), session->client()), NIIF_INFO);
            return;
        }
    }
}

void CKeyList::onKeyDeclined(nglab::libssha::KeyBasePtr key, const nglab::libssha::Session *session)
{
    log.info("Key usage declined: {} in session", key->fingerprint());
    CRegKey reg_key;
    if (reg_key.Open(HKEY_CURRENT_USER, SKYM_KEY_ROOT, KEY_READ) == ERROR_SUCCESS)
    {
        DWORD notifyKeyDeclined = 0;
        if (reg_key.QueryDWORDValue("NotifyKeyDeclined", notifyKeyDeclined) == ERROR_SUCCESS && notifyKeyDeclined == 1)
        {
            DisplayTrayNotification("Key Usage Declined", std::format("Key {} usage from {} was declined", key->comment(), session->client()), NIIF_WARNING);
            return;
        }
    }
}

void CKeyList::onLocked()
{
    log.info("Key manager locked");
    DisplayTrayNotification("Key Manager Locked", "The key manager has been locked.", NIIF_WARNING);
}

void CKeyList::onUnlocked()
{
    log.info("Key manager unlocked");
    DisplayTrayNotification("Key Manager Unlocked", "The key manager has been unlocked.", NIIF_INFO);
}

void CKeyList::addKeyToList(nglab::libssha::KeyBasePtr key)
{
    for (int i = 0; i < m_keyList.GetItemCount(); ++i)
    {
        CHAR szText[256];
        m_keyList.GetItemText(i, 2, szText, sizeof(szText));
        if (key->fingerprint() == szText)
        {
            // Key already exists in the list
            m_keyList.SetItemText(i, 1, key->comment().c_str());
            if (m_keyList.GetSelectedIndex() == i)
            {
                log.debug("Updating details for selected key: {}", key->fingerprint());
                updateKeyDetails();
            }

            return;
        }
    }

    int index = m_keyList.GetItemCount();
    m_keyList.InsertItem(index, key->type().c_str());
    m_keyList.SetItemText(index, 1, key->comment().c_str());
    m_keyList.SetItemText(index, 2, key->fingerprint().c_str());

    log.trace("Added key to list: {}", key->fingerprint());
}

void CKeyList::removeKeyFromList(const std::string &fingerprint)
{
    for (int i = 0; i < m_keyList.GetItemCount(); ++i)
    {
        CHAR szText[256];
        m_keyList.GetItemText(i, 2, szText, sizeof(szText));
        if (fingerprint == szText)
        {
            m_keyList.DeleteItem(i);
            break;
        }
    }
}

void CKeyList::updateKeyDetails(bool timerTriggered)
{
    int selectedIndex = m_keyList.GetSelectedIndex();
    if (selectedIndex >= 0)
    {
        CHAR szText[256];
        m_keyList.GetItemText(selectedIndex, 2, szText, sizeof(szText));

        auto key = nglab::libssha::KeyManager::instance().getKeyByFingerprint(szText);
        if (!key)
        {
            log.warning("Key not found for fingerprint: {}", szText);
            return;
        }

        if (!timerTriggered)
        {
            std::string pub_key_string = key->pubKey().authKeyLine(key->comment());
            SetDlgItemText(IDC_COPY_PUB_ID_TEXT, pub_key_string.c_str());

            m_confirmCheck.EnableWindow(!key->confirmRequired());
            bool needConfirmation = KeyConstrainsLoader::needConfirmation(key) || key->confirmRequired();
            m_confirmCheck.SetCheck(needConfirmation ? BST_CHECKED : BST_UNCHECKED);

            if (key->hasDestConstraints())
            {
                log.debug("Key has destination constraints, updating UI");
                m_keyConstraints.DeleteAllItems();
                m_keyConstraints.EnableWindow(TRUE);
                const auto &constraints = key->destConstraints();
                for (const auto &cons : constraints)
                {
                    int itemIndex = m_keyConstraints.GetItemCount();

                    m_keyConstraints.InsertItem(itemIndex, cons.fromHop().toString().c_str());
                    m_keyConstraints.SetItemText(itemIndex, 1, cons.toHop().toString().c_str());
                }

                if (m_userLoadedConstraints.find(key->fingerprint()) != m_userLoadedConstraints.end())
                {
                    m_constrainsLoad.EnableWindow(TRUE);
                    m_constrainsLoad.SetWindowText("Replace");
                    m_constrainsClear.EnableWindow(TRUE);
                }
                else
                {
                    m_constrainsLoad.EnableWindow(FALSE);
                    m_constrainsClear.EnableWindow(FALSE);
                }
            }
            else
            {
                log.debug("Key doesn't have destination constraints, updating UI");
                m_keyConstraints.DeleteAllItems();
                m_keyConstraints.EnableWindow(FALSE);
                m_constrainsLoad.SetWindowText("Load");
                m_constrainsLoad.EnableWindow(TRUE);
                m_constrainsClear.EnableWindow(FALSE);
            }

            m_copyPubId.EnableWindow(TRUE);
        }
        if (key->expireInSeconds() < 0)
        {
            m_keyTimeoutText.SetWindowText("This key doesn't have a lifetime constraint");
            KillTimer(TIMER_ID_REFRESH_LIFETIME);
        }
        else
        {
            m_keyTimeoutText.SetWindowText(std::format("This key will expire in {} seconds", key->expireInSeconds()).c_str());
            SetTimer(TIMER_ID_REFRESH_LIFETIME, 1000);
        }
    }
}

LRESULT CKeyList::OnKeySelectionChanged(int idCtrl, LPNMHDR pnmh, BOOL &bHandled)
{
    NMLISTVIEW *pnmv = reinterpret_cast<NMLISTVIEW *>(pnmh);
    if (pnmv->uChanged & LVIF_STATE)
    {
        if ((pnmv->uNewState & LVIS_SELECTED) == 0)
        {
            log.debug("No key selected");
            SetDlgItemText(IDC_COPY_PUB_ID_TEXT, "");
            m_confirmCheck.SetCheck(BST_UNCHECKED);
            m_keyTimeoutText.SetWindowText("N/A");
            m_keyConstraints.DeleteAllItems();
            m_keyConstraints.EnableWindow(FALSE);
            m_constrainsLoad.EnableWindow(FALSE);
            m_confirmCheck.EnableWindow(FALSE);
            m_copyPubId.EnableWindow(FALSE);

            KillTimer(TIMER_ID_REFRESH_LIFETIME);
            return 0;
        }
        updateKeyDetails();
    }

    return 0;
}

LRESULT CKeyList::OnCopyPubId(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    CWindow wnd = GetDlgItem(IDC_COPY_PUB_ID_TEXT);

    int len = wnd.GetWindowTextLength();
    if (len <= 0)
    {
        log.debug("No text in IDC_COPY_PUB_ID_TEXT to copy");
        return 0;
    }

    std::vector<char> buf(static_cast<size_t>(len));
    wnd.GetWindowText(buf.data(), static_cast<int>(buf.size()));
    buf.push_back('\n');
    buf.push_back('\0');

    if (!OpenClipboard())
    {
        log.warning("Failed to open clipboard");
        return 0;
    }

    if (!EmptyClipboard())
    {
        log.warning("Failed to empty clipboard");
        CloseClipboard();
        return 0;
    }

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, buf.size());
    if (!hMem)
    {
        log.warning("GlobalAlloc failed");
        CloseClipboard();
        return 0;
    }

    void *mem = GlobalLock(hMem);
    if (!mem)
    {
        log.warning("GlobalLock failed");
        GlobalFree(hMem);
        CloseClipboard();
        return 0;
    }

    memcpy(mem, buf.data(), buf.size());
    GlobalUnlock(hMem);

    if (SetClipboardData(CF_TEXT, hMem) == nullptr)
    {
        log.warning("SetClipboardData failed");
        GlobalFree(hMem);
        CloseClipboard();
        return 0;
    }

    // Ownership of hMem has been transferred to the system on success.
    CloseClipboard();
    wnd.SetWindowText("Copied to clipboard");
    m_copyPubId.EnableWindow(FALSE);
    SetTimer(TIMER_ID_RESTORE_AUTH_LINE, 5000); // Restore every 5 seconds

    log.vdebug("Copied public id text to clipboard");
    return 0;
}

LRESULT CKeyList::OnConfirmChanged(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    auto key = getSelectedKey();
    bool needConfirmation = (m_confirmCheck.GetCheck() == BST_CHECKED);
    log.vdebug("Confirmation requirement changed for key {}: {}", key->fingerprint(), needConfirmation);
    KeyConstrainsLoader::setNeedConfirmation(key, needConfirmation);
    return 0;
}

LRESULT CKeyList::OnLoadConstraint(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    // Load constraint remains; implementation intentionally left minimal.
    log.debug("Load constraint command invoked");
    CConstrainsOpenDialog dlg(m_hWnd);
    if (dlg.DoModal())
    {
        KeyBasePtr selectedKey = getSelectedKey();
        try
        {
            KeyConstrainsLoader::loadDestinationConstrains(selectedKey, dlg.getFilePath(), dlg.getPersistent());
            log.debug("Loaded {} constraints from dialog", dlg.constraints().size());
            m_userLoadedConstraints.insert(selectedKey->fingerprint());
            updateKeyDetails();
        }
        catch (const nglab::skym::DialogException &e)
        {
            e.show(m_hWnd);
        }
        catch (const std::exception &e)
        {
            log.error("Failed to load constraints for key {}: {}", selectedKey->fingerprint(), e.what());
        }
    }

    return 0;
}

LRESULT CKeyList::OnClearConstraint(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    // Clear constraint remains; implementation intentionally left minimal.
    auto key = getSelectedKey();
    log.debug("Clear constraint command invoked");
    CTaskDialog dlg(m_hWnd);
    dlg.SetWindowTitle(L"Confirm Deletion");
    auto content = std::format(L"Are you sure you want to remove all destination constraints for key {}?", StringConverter::u8w2(key->fingerprint()));
    dlg.SetContentText(content.c_str());
    TASKDIALOG_BUTTON btn[] = {
        {IDYES, L"Yes"},
        {IDNO, L"No"}};
    dlg.SetButtons(btn, 2);
    dlg.SetDefaultButton(IDNO);
    dlg.SetMainIcon(TD_WARNING_ICON);
    dlg.SetExpandedInformationText(L"Removing destination constraints will allow the key to be used from any source to any destination without restrictions.\nYou can reapply constraints later if needed.");
    dlg.SetCollapsedControlText(L"More Information");
    dlg.SetExpandedControlText(L"Less Information");
    dlg.SetVerificationText(L"Remove persistent for this constraints");
    dlg.ModifyFlags(0, TDF_EXPAND_FOOTER_AREA | TDF_VERIFICATION_FLAG_CHECKED);
    int ret = 0;
    BOOL deletePersistent = FALSE;
    dlg.DoModal(m_hWnd, &ret, nullptr, &deletePersistent);
    if (ret != IDYES)
    {
        return 0;
    }

    KeyConstrainsLoader::unloadDestinationConstrains(key, deletePersistent != FALSE);

    updateKeyDetails();

    return 0;
}

LRESULT CKeyList::OnShowSettings(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    // Handle showing settings
    log.vdebug("Show settings command invoked");
    try
    {
        CSettingsSheet settingsSheet(m_hWnd);
        settingsSheet.DoModal();
    }
    catch (const std::exception &)
    {
    }
    return 0;
}

LRESULT CKeyList::OnAbout(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{

    log.vdebug("About command invoked");
    try
    {
        CAboutDlg aboutDlg;
        aboutDlg.DoModal();
    }
    catch (const std::exception &)
    {
    }

    return 0;
}

bool CKeyList::shouldNotifyKeyOperations()
{
    CRegKey reg_key;
    if (reg_key.Open(HKEY_CURRENT_USER, SKYM_KEY_ROOT, KEY_READ) == ERROR_SUCCESS)
    {
        DWORD notifyKeyOperations = 0;
        if (reg_key.QueryDWORDValue("KeyOperations", notifyKeyOperations) == ERROR_SUCCESS && notifyKeyOperations == 1)
        {
            return true;
        }
    }
    return false;
}

nglab::libssha::KeyBasePtr CKeyList::getSelectedKey()
{
    int selectedIndex = m_keyList.GetSelectedIndex();
    if (selectedIndex >= 0)
    {
        CHAR szText[256];
        m_keyList.GetItemText(selectedIndex, 2, szText, sizeof(szText));

        auto key = nglab::libssha::KeyManager::instance().getKeyByFingerprint(szText);
        if (!key)
        {
            log.warning("Key not found for fingerprint: {}", szText);
            throw std::runtime_error("Selected key not found");
        }
        return key;
    }
    else
    {
        return nullptr;
    }
}

void CKeyList::showOrHideWindow()
{
    if (IsWindowVisible())
    {
        m_trayMenu.ModifyMenu(IDM_SHOW, MF_BYCOMMAND | MF_STRING, IDM_SHOW, "Show");
        ShowWindow(SW_HIDE);
    }
    else
    {

        m_trayMenu.ModifyMenu(IDM_SHOW, MF_BYCOMMAND | MF_STRING, IDM_SHOW, "Hide");
        ShowWindow(SW_SHOW);
        SetForegroundWindow(m_hWnd);
    }
}

LRESULT CKeyList::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(&ps);

    HICON hIcon = IconFactory::get(IDI_CLOCK);
    if (hIcon)
    {
        RECT rc;
        m_keyTimeoutIcon.GetWindowRect(&rc);
        ScreenToClient(&rc);
        DrawIconEx(
            hdc,
            rc.left, rc.top,
            hIcon,
            16, 16,
            0,
            NULL,
            DI_NORMAL);
    }

    EndPaint(&ps);
    return 0;
}
