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
#include "cnotificationpage.h"

#include "config.h"
#include "dialogs/cskipexecutableslist.h"

namespace nglab::saru
{

CNotificationPage::CNotificationPage()
    : CPropertyPageImpl<CNotificationPage>(_T("Notifications")), LogEnabler("CNotificationPage")
{
}

void CNotificationPage::initDefault()
{
    CRegKey key;
    if (key.Create(HKEY_CURRENT_USER, _T(SARU_KEY_ROOT)) != ERROR_SUCCESS)
    {
        return;
    }

    const auto setDefault = [&key](LPCTSTR name, DWORD defaultValue)
    {
        DWORD value;
        if (key.QueryDWORDValue(name, value) == ERROR_FILE_NOT_FOUND)
        {
            key.SetDWORDValue(name, defaultValue);
        }
    };

    setDefault(_T("NotifyKeyUsage"), 1);
    setDefault(_T("AlwaysConfirm"), 0);
    setDefault(_T("KeyOperations"), 0);
    setDefault(_T("NotifyKeyDeclined"), 1);
    setDefault(_T("ShowKeySelection"), 0);
    setDefault(_T("AutoDecline"), 0);
    setDefault(_T("ForceNotificationOnTop"), 1);
    setDefault(_T("ShowOriginatingProcess"), 1);
}

LRESULT CNotificationPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL &)
{
    m_EditSkipExecutablesButton.Attach(GetDlgItem(IDC_EDIT_SKIP_EXECUTABLES));

    CRegKey key;
    if (key.Open(HKEY_CURRENT_USER, _T(SARU_KEY_ROOT), KEY_READ) == ERROR_SUCCESS)
    {
        DWORD value = 0;
        if (key.QueryDWORDValue("NotifyKeyUsage", value) == ERROR_SUCCESS)
        {
            m_NotifyKeyUsage = (value != 0);
            CheckDlgButton(IDC_NOTIFY_KEY_USAGE, m_NotifyKeyUsage ? BST_CHECKED : BST_UNCHECKED);
        }
        else
            CheckDlgButton(IDC_NOTIFY_KEY_USAGE, BST_CHECKED);

        if (key.QueryDWORDValue("AlwaysConfirm", value) == ERROR_SUCCESS)
        {
            m_AlwaysConfirm = (value != 0);
            CheckDlgButton(IDC_ALWAYS_CONFIRM, m_AlwaysConfirm ? BST_CHECKED : BST_UNCHECKED);
        }
        else
            CheckDlgButton(IDC_ALWAYS_CONFIRM, BST_UNCHECKED);

        if (key.QueryDWORDValue("KeyOperations", value) == ERROR_SUCCESS)
        {
            m_KeyOperations = (value != 0);
            CheckDlgButton(IDC_NOTIFY_KEY_OPERATIONS, m_KeyOperations ? BST_CHECKED : BST_UNCHECKED);
        }
        else
            CheckDlgButton(IDC_NOTIFY_KEY_OPERATIONS, BST_UNCHECKED);

        if (key.QueryDWORDValue("NotifyKeyDeclined", value) == ERROR_SUCCESS)
        {
            m_NotifyKeyDeclined = (value != 0);
            CheckDlgButton(IDC_NOTIFY_KEY_DECLINE, m_NotifyKeyDeclined ? BST_CHECKED : BST_UNCHECKED);
        }
        else
            CheckDlgButton(IDC_NOTIFY_KEY_DECLINE, BST_CHECKED);

        if (key.QueryDWORDValue("ShowKeySelection", value) == ERROR_SUCCESS)
        {
            m_ShowKeySelection = (value != 0);
            CheckDlgButton(IDC_SHOW_KEY_SELECTION, m_ShowKeySelection ? BST_CHECKED : BST_UNCHECKED);
        }
        else
            CheckDlgButton(IDC_SHOW_KEY_SELECTION, BST_UNCHECKED);

        if (key.QueryDWORDValue("AutoDecline", value) == ERROR_SUCCESS)
        {
            m_AutoDecline = (value != 0);
            CheckDlgButton(IDC_AUTO_DECLINE, m_AutoDecline ? BST_CHECKED : BST_UNCHECKED);
        }
        else
            CheckDlgButton(IDC_AUTO_DECLINE, BST_UNCHECKED);

        if (key.QueryDWORDValue("ForceNotificationOnTop", value) == ERROR_SUCCESS)
        {
            m_ForceOnTop = (value != 0);
            CheckDlgButton(IDC_FORCE_ON_TOP, m_ForceOnTop ? BST_CHECKED : BST_UNCHECKED);
        }
        else
            CheckDlgButton(IDC_FORCE_ON_TOP, BST_CHECKED);

        if (key.QueryDWORDValue("ShowOriginatingProcess", value) == ERROR_SUCCESS)
        {
            m_ShowOriginatingProcess = (value != 0);
            CheckDlgButton(IDC_SHOW_ORIGINATING_PROCESS, m_ShowOriginatingProcess ? BST_CHECKED : BST_UNCHECKED);
        }
        else
            CheckDlgButton(IDC_SHOW_ORIGINATING_PROCESS, BST_CHECKED);

        key.Close();
    }
    else
    {
        CheckDlgButton(IDC_NOTIFY_KEY_USAGE, BST_CHECKED);
        CheckDlgButton(IDC_ALWAYS_CONFIRM, BST_UNCHECKED);
        CheckDlgButton(IDC_NOTIFY_KEY_OPERATIONS, BST_UNCHECKED);
        CheckDlgButton(IDC_NOTIFY_KEY_DECLINE, BST_CHECKED);
        CheckDlgButton(IDC_SHOW_KEY_SELECTION, BST_UNCHECKED);
        CheckDlgButton(IDC_AUTO_DECLINE, BST_UNCHECKED);
        CheckDlgButton(IDC_FORCE_ON_TOP, BST_CHECKED);
        CheckDlgButton(IDC_SHOW_ORIGINATING_PROCESS, BST_CHECKED);
    }
    m_EditSkipExecutablesButton.EnableWindow(m_ShowOriginatingProcess);
    return TRUE;
}

int CNotificationPage::OnApply()
{
    m_NotifyKeyUsage = (IsDlgButtonChecked(IDC_NOTIFY_KEY_USAGE) == BST_CHECKED);
    m_AlwaysConfirm = (IsDlgButtonChecked(IDC_ALWAYS_CONFIRM) == BST_CHECKED);
    m_KeyOperations = (IsDlgButtonChecked(IDC_NOTIFY_KEY_OPERATIONS) == BST_CHECKED);
    m_NotifyKeyDeclined = (IsDlgButtonChecked(IDC_NOTIFY_KEY_DECLINE) == BST_CHECKED);
    m_ShowKeySelection = (IsDlgButtonChecked(IDC_SHOW_KEY_SELECTION) == BST_CHECKED);
    m_AutoDecline = (IsDlgButtonChecked(IDC_AUTO_DECLINE) == BST_CHECKED);
    m_ForceOnTop = (IsDlgButtonChecked(IDC_FORCE_ON_TOP) == BST_CHECKED);
    m_ShowOriginatingProcess = (IsDlgButtonChecked(IDC_SHOW_ORIGINATING_PROCESS) == BST_CHECKED);

    CRegKey key;
    if (key.Create(HKEY_CURRENT_USER, _T(SARU_KEY_ROOT)) == ERROR_SUCCESS)
    {
        key.SetDWORDValue(_T("NotifyKeyUsage"), m_NotifyKeyUsage);
        key.SetDWORDValue(_T("AlwaysConfirm"), m_AlwaysConfirm);
        key.SetDWORDValue(_T("KeyOperations"), m_KeyOperations);
        key.SetDWORDValue(_T("NotifyKeyDeclined"), m_NotifyKeyDeclined);
        key.SetDWORDValue(_T("ShowKeySelection"), m_ShowKeySelection);
        key.SetDWORDValue(_T("AutoDecline"), m_AutoDecline ? 30 : 0);
        key.SetDWORDValue(_T("ForceNotificationOnTop"), m_ForceOnTop);
        key.SetDWORDValue(_T("ShowOriginatingProcess"), m_ShowOriginatingProcess);
        key.Close();
    }
    else
    {
        log.error("Failed to open/create registry key for notification settings");
        return PSNRET_INVALID;
    }

    SetModified(FALSE);
    return PSNRET_NOERROR;
}

LRESULT CNotificationPage::OnSomethingChanged(WORD, WORD controlId, HWND, BOOL &)
{
    bool changed = false;
    changed |= (m_NotifyKeyUsage != (IsDlgButtonChecked(IDC_NOTIFY_KEY_USAGE) == BST_CHECKED));
    changed |= (m_AlwaysConfirm != (IsDlgButtonChecked(IDC_ALWAYS_CONFIRM) == BST_CHECKED));
    changed |= (m_KeyOperations != (IsDlgButtonChecked(IDC_NOTIFY_KEY_OPERATIONS) == BST_CHECKED));
    changed |= (m_NotifyKeyDeclined != (IsDlgButtonChecked(IDC_NOTIFY_KEY_DECLINE) == BST_CHECKED));
    changed |= (m_ShowKeySelection != (IsDlgButtonChecked(IDC_SHOW_KEY_SELECTION) == BST_CHECKED));
    changed |= (m_AutoDecline != (IsDlgButtonChecked(IDC_AUTO_DECLINE) == BST_CHECKED));
    changed |= (m_ForceOnTop != (IsDlgButtonChecked(IDC_FORCE_ON_TOP) == BST_CHECKED));
    changed |= (m_ShowOriginatingProcess != (IsDlgButtonChecked(IDC_SHOW_ORIGINATING_PROCESS) == BST_CHECKED));
    if (controlId == IDC_SHOW_ORIGINATING_PROCESS)
    {
        m_EditSkipExecutablesButton.EnableWindow(
            IsDlgButtonChecked(IDC_SHOW_ORIGINATING_PROCESS) == BST_CHECKED);
    }
    SetModified(changed);
    return 0;
}

LRESULT CNotificationPage::OnEditSkipExecutables(WORD, WORD, HWND, BOOL &)
{
    CSkipExecutablesList dialog;
    dialog.DoModal(m_hWnd);
    return 0;
}

}
