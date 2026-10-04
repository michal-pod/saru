/*
 SARU - SSH Agent Replacment Utility
 Copyright (C) 2025-2026 Michał Podsiadlik <michal@nglab.net>

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
#include "ckeyconfirm.h"

#include <format>
#include <thread>

#include <shlobj.h>
#include <shellapi.h>

#include "servers/windows-session.h"
#include "dialogs/ckeylist.h"
#include "factories/icon-factory.h"

using namespace nglab::saru;

CKeyConfirm::CKeyConfirm(const KeyBase &key, ClientInfo &clientInfo, WindowsSessionType sessionType)
    : CUserInputDialog<CKeyConfirm>(clientInfo),
      LogEnabler("CKeyConfirm")
{
    m_keyInfo.Format(
        "%s with comment: %s",
        key.type().c_str(),
        key.comment().empty() ? "(none)" : key.comment().c_str());

    m_keyFingerprint = key.fingerprint().c_str();

    std::string sessionTypeStr;
    if (sessionType == WindowsSessionType::Pageant)
    {
        sessionTypeStr = "via Pageant";
    }
    else if (sessionType == WindowsSessionType::HyperV)
    {
        sessionTypeStr = "from virtual machine";
    }
    else
    {
        sessionTypeStr = "via named pipe";
    }

    std::string forwardedStr = "";
    if (clientInfo.IsForwarded)
    {
        forwardedStr = "(forwarded connection) ";
    }

    const std::string appExecutable = m_clientInfo.ClientPath.filename().string();
    const std::string appConnectingExecutable = m_clientInfo.ConnectingApplicationPath.filename().string();
    if (sessionType == WindowsSessionType::HyperV)
    {
        m_label.Format("The application '%s' %sfrom virtual machine '%s' is requesting to use the following SSH key.%s",
                       appExecutable.c_str(), forwardedStr.c_str(), m_clientInfo.VMName.c_str());
    }
    else if(m_clientInfo.ClientPath != m_clientInfo.ConnectingApplicationPath){
        m_label.Format("The application '%s' (using '%s') %ss requesting to use the following SSH key %s.",
                       appExecutable.c_str(), appConnectingExecutable.c_str(), forwardedStr.c_str(), sessionTypeStr.c_str());
    }
    else
    {
        m_label.Format("The application '%s' %ss requesting to use the following SSH key %s.",
                       appExecutable.c_str(), forwardedStr.c_str(), sessionTypeStr.c_str());
    }
}

CKeyConfirm::~CKeyConfirm()
{
}

BOOL CKeyConfirm::PreTranslateMessage(MSG *pMsg)
{
    return ::IsDialogMessage(m_hWnd, pMsg);
}

LRESULT CKeyConfirm::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
{
    auto llog = Logger::instance();
    llog.debug("CKeyConfirm created for app '{}' (PID: {})", m_clientInfo.ClientPath.string(), m_clientInfo.ClientPid);

    forceIntoForeground();

    DoDataExchange(FALSE);

    for (auto &control : {IDC_EXTRA_LINE1_ICON, IDC_EXTRA_LINE1_LABEL, IDC_EXTRA_LINE1_VALUE,
                          IDC_EXTRA_LINE2_LABEL, IDC_EXTRA_LINE2_ICON, IDC_EXTRA_LINE2_VALUE})
    {
        CWindow wnd = GetDlgItem(control);
        wnd.ShowWindow(SW_HIDE);
    }

    m_timesList.Attach(GetDlgItem(IDC_TIME_COMBO));
    m_timesList.EnableWindow(FALSE);

    for (int i = 0; i < 3; ++i)
    {
        m_timesList.AddString(REMEMBER_TIME_STRINGS[i]);
    }

    m_timesList.SetCurSel(0);

    loadAppIcon();

    if (m_clientInfo.SystemType == ClientInfoSystemType::Linux)
    {
        m_extraLine1Icon = IDI_USER;
        CWindow wndExtra1Icon;
        wndExtra1Icon.Attach(GetDlgItem(IDC_EXTRA_LINE1_ICON));
        wndExtra1Icon.ShowWindow(SW_SHOW);
        CWindow wndExtra1Label;
        wndExtra1Label.Attach(GetDlgItem(IDC_EXTRA_LINE1_LABEL));
        wndExtra1Label.ShowWindow(SW_SHOW);
        wndExtra1Label.SetWindowTextA("User info:");

        CWindow wndExtra1Value;
        wndExtra1Value.Attach(GetDlgItem(IDC_EXTRA_LINE1_VALUE));
        wndExtra1Value.ShowWindow(SW_SHOW);
        std::string userInfo = m_clientInfo.User.empty() ? "(unknown user)" : m_clientInfo.User;
        if (!m_clientInfo.Distro.empty())
        {
            userInfo += " on " + m_clientInfo.Distro;
        }
        wndExtra1Value.SetWindowText(userInfo.c_str());
    }
    else
    {
    }

    initTimeout();

    CRegKey key;
    if (key.Open(HKEY_CURRENT_USER, _T(SARU_KEY_ROOT)) == ERROR_SUCCESS)
    {
        DWORD timeout = 0;
        if (key.QueryDWORDValue(_T("AutoDecline"), timeout) == ERROR_SUCCESS && timeout > 0)
        {
            m_timer = static_cast<int>(timeout);
            log.debug("CKeyConfirm dialog will timeout in {} seconds", m_timer);
            updateDenyButton();
            SetTimer(IDT_TIMEOUT_CHECK, 1000);
        }
    }

    SendMessage(DM_SETDEFID, IDNO, 0);

    return TRUE;
}

LRESULT CKeyConfirm::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(&ps);

    // Draw the key info icon
    HICON hIcon = IconFactory::get(IDI_PAGE_WHITE_TEXT);
    if (hIcon)
    {
        CWindow wnd = GetDlgItem(IDC_ICON_KEY);
        RECT rc;
        wnd.GetWindowRect(&rc);
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

    // Draw the fingerprint icon
    hIcon = IconFactory::get(IDI_KEY);
    if (hIcon)
    {
        CWindow wnd = GetDlgItem(IDC_ICON_FINGERPRINT);
        RECT rc;
        wnd.GetWindowRect(&rc);
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

    // Draw the hostname icon
    if (m_extraLine1Icon)
    {
        hIcon = IconFactory::get(IDI_SERVER);
        if (hIcon)
        {
            CWindow wnd = GetDlgItem(IDC_EXTRA_LINE1_ICON);
            if (wnd.IsWindowVisible())
            {
                RECT rc;
                wnd.GetWindowRect(&rc);
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
        }
    }

    // Draw the username icon
    if (m_extraLine2Icon)
    {
        hIcon = IconFactory::get(IDI_USER);
        if (hIcon)
        {
            CWindow wnd = GetDlgItem(IDC_EXTRA_LINE2_ICON);
            if (wnd.IsWindowVisible())
            {
                RECT rc;
                wnd.GetWindowRect(&rc);
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
        }
    }

    EndPaint(&ps);
    return 0;
}

LRESULT CKeyConfirm::OnRememberCheck(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    DoDataExchange(TRUE);
    CWindow wnd = GetDlgItem(IDC_TIME_COMBO);
    wnd.EnableWindow(m_rememberKey ? TRUE : FALSE);

    return 0;
}

LRESULT CKeyConfirm::OnAllow(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    readRememberSettings();

    EndDialog(IDYES);
    return 0;
}

LRESULT CKeyConfirm::OnDeny(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    readRememberSettings();
    EndDialog(IDNO);
    return 0;
}

LRESULT CKeyConfirm::OnCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    m_rememberKey = 0;
    EndDialog(IDNO);
    return 0;
}

void CKeyConfirm::readRememberSettings()
{
    // Handle confirm event
    m_rememberKey = IsDlgButtonChecked(IDC_REMEMBER_CHECK) == BST_CHECKED ? 1 : 0;
    if (m_rememberKey)
    {
        int sel = m_timesList.GetCurSel();
        if (sel != CB_ERR)
        {
            m_rememberTime = REMEMBER_TIME_OPTIONS[sel];
        }
        else
        {
            m_rememberTime = 0;
        }
    }
    else
    {
        m_rememberTime = 0;
    }
}

const int CKeyConfirm::getRememberTime() const
{
    return m_rememberTime;
}

const int CKeyConfirm::getRememberKey() const
{
    return m_rememberKey;
}

void CKeyConfirm::onKeyRemoved(KeyBasePtr key)
{
    // If the key being removed is the one this dialog is for, close the dialog
    if (key->fingerprint() == std::string(m_keyFingerprint))
    {
        std::string message = std::format(
            "Key {} with fingerprint {} that was used in this confirmation was removed.",
            key->comment(), key->fingerprint());
        log.trace("CKeyConfirm dialog closing because key {} is being removed",
                 key->fingerprint());
        CKeyList::instance().DisplayTrayNotification(
            "Key removed", message, NIIF_WARNING);

        PostMessage(WM_COMMAND, IDCANCEL);
    }
}
