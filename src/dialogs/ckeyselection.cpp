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
#include "ckeyselection.h"

#include <algorithm>
#include <string>
#include <utility>

#include "servers/windows-session.h"

using namespace nglab::saru;

CKeySelectionDlg::CKeySelectionDlg(KeyListProvider keyListProvider, const ClientInfo &clientInfo, WindowsSessionType sessionType)
    : CUserInputDialog<CKeySelectionDlg>(clientInfo),
      LogEnabler("CKeySelectionDlg"),
      m_keyListProvider(std::move(keyListProvider))
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

    refreshKeys();

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

LRESULT CKeySelectionDlg::OnRefreshKeys(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
{
    refreshKeys();
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
    PostMessage(WM_REFRESH_KEYS);
}

void CKeySelectionDlg::onKeyRemoved(KeyBasePtr key)
{
    PostMessage(WM_REFRESH_KEYS);
}

void CKeySelectionDlg::onKeysCleared()
{
    PostMessage(WM_REFRESH_KEYS);
}

void CKeySelectionDlg::refreshKeys()
{
    std::string selectedFingerprint;
    const int selectedIndex = m_keyList.GetSelectedIndex();
    if (selectedIndex >= 0 && static_cast<size_t>(selectedIndex) < m_keys.size())
    {
        selectedFingerprint = m_keys[static_cast<size_t>(selectedIndex)].fingerprint;
    }

    const auto currentKeys = m_keyListProvider();

    for (int i = static_cast<int>(m_keys.size()) - 1; i >= 0; --i)
    {
        const auto &oldKey = m_keys[static_cast<size_t>(i)];
        const auto currentKey = std::find_if(currentKeys.begin(), currentKeys.end(),
                                             [&oldKey](const auto &key)
                                             {
                                                 return key.fingerprint == oldKey.fingerprint;
                                             });
        if (currentKey == currentKeys.end())
        {
            m_keyList.DeleteItem(i);
            m_keys.erase(m_keys.begin() + i);
        }
    }

    for (const auto &key : currentKeys)
    {
        const auto existingKey = std::find_if(m_keys.begin(), m_keys.end(),
                                              [&key](const auto &existing)
                                              {
                                                  return existing.fingerprint == key.fingerprint;
                                              });
        if (existingKey == m_keys.end())
        {
            const int index = m_keyList.GetItemCount();
            m_keyList.InsertItem(index, key.type.c_str());
            m_keyList.SetItemText(index, 1, key.comment.c_str());
            m_keyList.SetItemText(index, 2, key.fingerprint.c_str());
            m_keys.push_back(key);
        }
        else
        {
            const int index = static_cast<int>(std::distance(m_keys.begin(), existingKey));
            m_keyList.SetItemText(index, 0, key.type.c_str());
            m_keyList.SetItemText(index, 1, key.comment.c_str());
            m_keys[static_cast<size_t>(index)] = key;
        }
    }

    int newSelectedIndex = 0;
    if (!selectedFingerprint.empty())
    {
        const auto selectedKey = std::find_if(m_keys.begin(), m_keys.end(),
                                              [&selectedFingerprint](const auto &key)
                                              {
                                                  return key.fingerprint == selectedFingerprint;
                                              });
        if (selectedKey != m_keys.end())
        {
            newSelectedIndex = static_cast<int>(std::distance(m_keys.begin(), selectedKey));
        }
    }

    if (!m_keys.empty())
    {
        m_keyList.SetSelectionMark(newSelectedIndex);
        m_keyList.SetItemState(newSelectedIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        m_keyList.EnsureVisible(newSelectedIndex, FALSE);
        m_keyList.SetFocus();
    }
}
