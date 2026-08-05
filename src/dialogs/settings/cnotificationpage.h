/*
 SKYM - SSH KeY Manager
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
#pragma once
#include <libssha/utils/logger.h>

#include "stdatl.h"
#include "config.h"

using nglab::libssha::LogEnabler;
using nglab::libssha::Logger;

class CNotificationPage : public CPropertyPageImpl<CNotificationPage>, public LogEnabler
{
public:
    enum { IDD = IDD_SETTINGS_NOTIFICATION };

    BEGIN_MSG_MAP(CNotificationPage)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    CHAIN_MSG_MAP(CPropertyPageImpl<CNotificationPage>)
    COMMAND_ID_HANDLER(IDC_NOTIFY_KEY_USAGE, OnSomethingChanged)
    COMMAND_ID_HANDLER(IDC_ALWAYS_CONFIRM, OnSomethingChanged)
    COMMAND_ID_HANDLER(IDC_NOTIFY_KEY_OPERATIONS, OnSomethingChanged)
    COMMAND_ID_HANDLER(IDC_NOTIFY_KEY_DECLINE, OnSomethingChanged)
    COMMAND_ID_HANDLER(IDC_SHOW_KEY_SELECTION, OnSomethingChanged)
    COMMAND_ID_HANDLER(IDC_AUTO_DECLINE, OnSomethingChanged)
    COMMAND_ID_HANDLER(IDC_FORCE_ON_TOP, OnSomethingChanged)
    END_MSG_MAP()

    CNotificationPage() : CPropertyPageImpl<CNotificationPage>(_T("Notifications")), LogEnabler("CNotificationPage") {}

    LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL &)
    {
        CRegKey key;
        if (key.Open(HKEY_CURRENT_USER, _T(SKYM_KEY_ROOT), KEY_READ) == ERROR_SUCCESS)
        {
            DWORD val = 0;
            if (key.QueryDWORDValue("NotifyKeyUsage", val) == ERROR_SUCCESS)
            {
                m_NotifyKeyUsage = (val != 0);
                CheckDlgButton(IDC_NOTIFY_KEY_USAGE, m_NotifyKeyUsage ? BST_CHECKED : BST_UNCHECKED);
            }
            else
                CheckDlgButton(IDC_NOTIFY_KEY_USAGE, BST_CHECKED);

            if (key.QueryDWORDValue("AlwaysConfirm", val) == ERROR_SUCCESS)
            {
                m_AlwaysConfirm = (val != 0);
                CheckDlgButton(IDC_ALWAYS_CONFIRM, m_AlwaysConfirm ? BST_CHECKED : BST_UNCHECKED);
            }
            else
                CheckDlgButton(IDC_ALWAYS_CONFIRM, BST_UNCHECKED);

            if (key.QueryDWORDValue("KeyOperations", val) == ERROR_SUCCESS)
            {
                m_KeyOperations = (val != 0);
                CheckDlgButton(IDC_NOTIFY_KEY_OPERATIONS, m_KeyOperations ? BST_CHECKED : BST_UNCHECKED);
            }
            else
                CheckDlgButton(IDC_NOTIFY_KEY_OPERATIONS, BST_UNCHECKED);

            if (key.QueryDWORDValue("NotifyKeyDeclined", val) == ERROR_SUCCESS)
            {
                m_NotifyKeyDeclined = (val != 0);
                CheckDlgButton(IDC_NOTIFY_KEY_DECLINE, m_NotifyKeyDeclined ? BST_CHECKED : BST_UNCHECKED);
            }
            else
                CheckDlgButton(IDC_NOTIFY_KEY_DECLINE, BST_CHECKED);

            if (key.QueryDWORDValue("ShowKeySelection", val) == ERROR_SUCCESS)
            {
                m_ShowKeySelection = (val != 0);
                CheckDlgButton(IDC_SHOW_KEY_SELECTION, m_ShowKeySelection ? BST_CHECKED : BST_UNCHECKED);
            }
            else
                CheckDlgButton(IDC_SHOW_KEY_SELECTION, BST_UNCHECKED);
            
            if (key.QueryDWORDValue("AutoDecline", val) == ERROR_SUCCESS)
            {
                m_AutoDecline = (val != 0);
                CheckDlgButton(IDC_AUTO_DECLINE, m_AutoDecline ? BST_CHECKED : BST_UNCHECKED);
            }
            else
                CheckDlgButton(IDC_AUTO_DECLINE, BST_UNCHECKED);

            if (key.QueryDWORDValue("ForceNotificationOnTop", val) == ERROR_SUCCESS)
            {
                m_ForceOnTop = (val != 0);
                CheckDlgButton(IDC_FORCE_ON_TOP, m_ForceOnTop ? BST_CHECKED : BST_UNCHECKED);
            }
            else
                CheckDlgButton(IDC_FORCE_ON_TOP, BST_CHECKED);

            key.Close();
        }
        else
        {
            // Defaults
            CheckDlgButton(IDC_NOTIFY_KEY_USAGE, BST_CHECKED);
            CheckDlgButton(IDC_ALWAYS_CONFIRM, BST_UNCHECKED);
            CheckDlgButton(IDC_NOTIFY_KEY_OPERATIONS, BST_UNCHECKED);
            CheckDlgButton(IDC_NOTIFY_KEY_DECLINE, BST_CHECKED);
            CheckDlgButton(IDC_SHOW_KEY_SELECTION, BST_UNCHECKED);
            CheckDlgButton(IDC_AUTO_DECLINE, BST_UNCHECKED);
            CheckDlgButton(IDC_FORCE_ON_TOP, BST_CHECKED);
        }
        return TRUE;
    }

    int OnApply()
    {
        m_NotifyKeyUsage = (IsDlgButtonChecked(IDC_NOTIFY_KEY_USAGE) == BST_CHECKED);
        m_AlwaysConfirm = (IsDlgButtonChecked(IDC_ALWAYS_CONFIRM) == BST_CHECKED);
        m_KeyOperations = (IsDlgButtonChecked(IDC_NOTIFY_KEY_OPERATIONS) == BST_CHECKED);
        m_NotifyKeyDeclined = (IsDlgButtonChecked(IDC_NOTIFY_KEY_DECLINE) == BST_CHECKED);
        m_ShowKeySelection = (IsDlgButtonChecked(IDC_SHOW_KEY_SELECTION) == BST_CHECKED);
        m_AutoDecline = (IsDlgButtonChecked(IDC_AUTO_DECLINE) == BST_CHECKED);
        m_ForceOnTop = (IsDlgButtonChecked(IDC_FORCE_ON_TOP) == BST_CHECKED);

        CRegKey key;
        if (key.Create(HKEY_CURRENT_USER, _T(SKYM_KEY_ROOT)) == ERROR_SUCCESS)
        {
            key.SetDWORDValue(_T("NotifyKeyUsage"), m_NotifyKeyUsage);
            key.SetDWORDValue(_T("AlwaysConfirm"), m_AlwaysConfirm);
            key.SetDWORDValue(_T("KeyOperations"), m_KeyOperations);
            key.SetDWORDValue(_T("NotifyKeyDeclined"), m_NotifyKeyDeclined);
            key.SetDWORDValue(_T("ShowKeySelection"), m_ShowKeySelection);
            key.SetDWORDValue(_T("AutoDecline"), m_AutoDecline ? 30 : 0);
            key.SetDWORDValue(_T("ForceNotificationOnTop"), m_ForceOnTop);
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

    LRESULT OnSomethingChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL & /*bHandled*/)
    {
        bool changed = false;
        changed |= (m_NotifyKeyUsage != (IsDlgButtonChecked(IDC_NOTIFY_KEY_USAGE) == BST_CHECKED));
        changed |= (m_AlwaysConfirm != (IsDlgButtonChecked(IDC_ALWAYS_CONFIRM) == BST_CHECKED));
        changed |= (m_KeyOperations != (IsDlgButtonChecked(IDC_NOTIFY_KEY_OPERATIONS) == BST_CHECKED));
        changed |= (m_NotifyKeyDeclined != (IsDlgButtonChecked(IDC_NOTIFY_KEY_DECLINE) == BST_CHECKED));
        changed |= (m_ShowKeySelection != (IsDlgButtonChecked(IDC_SHOW_KEY_SELECTION) == BST_CHECKED));
        changed |= (m_AutoDecline != (IsDlgButtonChecked(IDC_AUTO_DECLINE) == BST_CHECKED));
        changed |= (m_ForceOnTop != (IsDlgButtonChecked(IDC_FORCE_ON_TOP) == BST_CHECKED));
        SetModified(changed);
        return 0;
    }

private:
    bool m_NotifyKeyUsage = true;
    bool m_AlwaysConfirm = false;
    bool m_KeyOperations = false;
    bool m_NotifyKeyDeclined = false;
    bool m_ShowKeySelection = false;
    bool m_AutoDecline = false;
    bool m_ForceOnTop = true;
};

