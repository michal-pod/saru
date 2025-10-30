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
#include "ckeyselection.h"

#include <string>

#include "servers/windows-session.h"

using namespace nglab::skym;

CKeySelectionDlg::CKeySelectionDlg(const PubKeyItemList &keys, const ClientInfo &clientInfo, WindowsSessionType sessionType)
    : m_keys(keys),
      CUserInputDialog<CKeySelectionDlg>(clientInfo),
      LogEnabler("CKeySelectionDlg")
{
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

    std::string appExecutable = m_clientInfo.ClientPath.substr(m_clientInfo.ClientPath.find_last_of("\\/") + 1);
    if (sessionType == WindowsSessionType::HyperV)
    {
        m_label.Format("The application '%s' from virtual machine '%s' is requesting list of keys.",
                       appExecutable.c_str(), m_clientInfo.VMName.c_str());
    }
    else
    {
        m_label.Format("The application '%s' is requesting list of keys %s.",
                       appExecutable.c_str(), sessionTypeStr.c_str());
    }
}

CKeySelectionDlg::~CKeySelectionDlg()
{
}

BOOL CKeySelectionDlg::PreTranslateMessage(MSG *pMsg)
{
    return ::IsDialogMessage(m_hWnd, pMsg);
}

LRESULT CKeySelectionDlg::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
{
    forceIntoForeground();

    loadAppIcon();

    initTimeout();

    CStatic wndLabel;
    wndLabel.Attach(this->GetDlgItem(IDC_HEADER_LINE));
    wndLabel.SetWindowText(m_label);

    m_keyList.Attach(GetDlgItem(IDC_KEY_LIST));
    m_keyList.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    m_keyList.ModifyStyle(0, LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS);

    m_keyList.InsertColumn(0, "Key Type", LVCFMT_LEFT, 80);
    m_keyList.InsertColumn(1, "Comment", LVCFMT_LEFT, 160);
    m_keyList.InsertColumn(2, "Fingerprint", LVCFMT_LEFT, 360);

    for (auto &key : m_keys)
    {
        int index = m_keyList.InsertItem(static_cast<int>(&key - &m_keys[0]), key.type.c_str());
        m_keyList.SetItemText(index, 1, key.comment.c_str());
        m_keyList.SetItemText(index, 2, key.fingerprint.c_str());
    }

    if (!m_keys.empty())
    {
        int sel = 0;
        // Set the selection mark first so keyboard navigation starts at this item
        m_keyList.SetSelectionMark(sel);
        m_keyList.SetItemState(sel, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        m_keyList.EnsureVisible(sel, FALSE);
        m_keyList.SetFocus();
    }

    // Returning FALSE because we explicitly set focus to a control
    return FALSE;
}

LRESULT CKeySelectionDlg::OnDpiChanged(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
{
    RECT rcList;
    m_keyList.GetClientRect(&rcList);
    int width = rcList.right - rcList.left;
    m_keyList.SetColumnWidth(0, width * 15 / 100);
    m_keyList.SetColumnWidth(1, width * 20 / 100);
    m_keyList.SetColumnWidth(2, width * 65 / 100);

    bHandled = TRUE;

    return 0;
}

LRESULT CKeySelectionDlg::OnOk(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    int selectedIndex = m_keyList.GetSelectedIndex();
    if (selectedIndex >= 0 && static_cast<size_t>(selectedIndex) < m_keys.size())
    {
        m_selectedIndex = selectedIndex;
        EndDialog(IDOK);
    }
    else
    {
        MessageBox("Please select a key before proceeding.", "No Key Selected", MB_OK | MB_ICONWARNING);
    }
    return 0;
}

LRESULT CKeySelectionDlg::OnCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled)
{
    EndDialog(IDCANCEL);
    return 0;
}

LRESULT CKeySelectionDlg::OnListDblClick(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL &bHandled)
{
    // On double-click accept the currently selected key
    return OnOk(0, IDOK, NULL, bHandled);
}

void CKeySelectionDlg::onKeyAdded(KeyBasePtr key)
{
    int index = m_keyList.GetItemCount();
    m_keyList.InsertItem(index, key->type().c_str());
    m_keyList.SetItemText(index, 1, key->comment().c_str());
    m_keyList.SetItemText(index, 2, key->fingerprint().c_str());

    log.debug("Added key to list: {}", key->fingerprint());
}

void CKeySelectionDlg::onKeyRemoved(const std::string &fingerprint)
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
