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
#pragma once
#include <libssha/utils/logger.h>

#include "stdatl.h"
#include "config.h"
using nglab::libssha::LogEnabler;
using nglab::libssha::Logger;

class CIntegrationPage : public CPropertyPageImpl<CIntegrationPage>, public LogEnabler
{
public:
    enum { IDD = IDD_SETTINGS_INTEGRATION };

    BEGIN_MSG_MAP(CIntegrationPage)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    CHAIN_MSG_MAP(CPropertyPageImpl<CIntegrationPage>)
    COMMAND_ID_HANDLER(IDC_AUTO_START, OnSomethingChanged)
    COMMAND_ID_HANDLER(IDC_PAGEANT_MODE, OnSomethingChanged)
    COMMAND_ID_HANDLER(IDC_NAMED_PIPE, OnSomethingChanged)
    COMMAND_ID_HANDLER(IDC_HYPERV_INTEGRATION, OnSomethingChanged)
    COMMAND_ID_HANDLER(IDC_ENABLE_DEBUG_CONSOLE, OnSomethingChanged)
    COMMAND_ID_HANDLER(IDC_DEBUG_LEVEL, OnSomethingChanged)
    END_MSG_MAP()

    CIntegrationPage() : CPropertyPageImpl<CIntegrationPage>(_T("Integration")), LogEnabler("CIntegrationPage") {}

    LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL &)
    {
        CRegKey key;
        CheckDlgButton(IDC_AUTO_START, BST_UNCHECKED);
        if (key.Open(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), KEY_READ) == ERROR_SUCCESS)
        {
            ULONG cbChars;
            CheckDlgButton(IDC_AUTO_START,
                           (key.QueryStringValue("SKYM", nullptr, &cbChars) == ERROR_SUCCESS) ? BST_CHECKED : BST_UNCHECKED);
            key.Close();
        }

        m_debugLevelCombo.Attach(GetDlgItem(IDC_DEBUG_LEVEL));
        m_debugLevelCombo.AddString(_T("Error"));
        m_debugLevelCombo.AddString(_T("Warning"));
        m_debugLevelCombo.AddString(_T("Info"));
        m_debugLevelCombo.AddString(_T("Trace"));
        m_debugLevelCombo.AddString(_T("Debug"));
        m_debugLevelCombo.AddString(_T("Verbose Debug"));
        m_debugLevelCombo.SetCurSel(static_cast<int>(Logger::instance().getLevel()));

        if (key.Open(HKEY_CURRENT_USER, _T(SKYM_KEY_ROOT), KEY_READ) == ERROR_SUCCESS)
        {
            DWORD val = 0;
            if (key.QueryDWORDValue("PageantMode", val) == ERROR_SUCCESS)
            {
                m_PageantMode = (val != 0);
                CheckDlgButton(IDC_PAGEANT_MODE, m_PageantMode ? BST_CHECKED : BST_UNCHECKED);
            }
            else
                CheckDlgButton(IDC_PAGEANT_MODE, BST_CHECKED);

            if (key.QueryDWORDValue("NamedPipe", val) == ERROR_SUCCESS)
            {
                m_NamedPipe = (val != 0);
                CheckDlgButton(IDC_NAMED_PIPE, m_NamedPipe ? BST_CHECKED : BST_UNCHECKED);
            }
            else
                CheckDlgButton(IDC_NAMED_PIPE, BST_CHECKED);

            if (key.QueryDWORDValue("HyperVIntegration", val) == ERROR_SUCCESS)
            {
                m_HyperVIntegration = (val != 0);
                CheckDlgButton(IDC_HYPERV_INTEGRATION, m_HyperVIntegration ? BST_CHECKED : BST_UNCHECKED);
            }
            else
                CheckDlgButton(IDC_HYPERV_INTEGRATION, BST_UNCHECKED);
            
            if (key.QueryDWORDValue("EnableDebugConsole", val) == ERROR_SUCCESS)
            {
                bool enableDebugConsole = (val != 0);
                CheckDlgButton(IDC_ENABLE_DEBUG_CONSOLE, enableDebugConsole ? BST_CHECKED : BST_UNCHECKED);
            }
            else
                CheckDlgButton(IDC_ENABLE_DEBUG_CONSOLE, BST_UNCHECKED);

            key.Close();
        }
        else
        {
            // Default settings
            CheckDlgButton(IDC_PAGEANT_MODE, BST_CHECKED);
            CheckDlgButton(IDC_NAMED_PIPE, BST_CHECKED);            
            CheckDlgButton(IDC_HYPERV_INTEGRATION, BST_UNCHECKED);
            CheckDlgButton(IDC_ENABLE_DEBUG_CONSOLE, BST_UNCHECKED);
        }
        return TRUE;
    }

    int OnApply()
    {
        m_AutoStart = (IsDlgButtonChecked(IDC_AUTO_START) == BST_CHECKED);
        m_PageantMode = (IsDlgButtonChecked(IDC_PAGEANT_MODE) == BST_CHECKED);
        m_NamedPipe = (IsDlgButtonChecked(IDC_NAMED_PIPE) == BST_CHECKED);
        m_HyperVIntegration = (IsDlgButtonChecked(IDC_HYPERV_INTEGRATION) == BST_CHECKED);
        m_EnableDebugConsole = (IsDlgButtonChecked(IDC_ENABLE_DEBUG_CONSOLE) == BST_CHECKED);

        Logger::Level selectedLevel = static_cast<Logger::Level>(m_debugLevelCombo.GetCurSel());

        if(selectedLevel != Logger::instance().getLevel()) {
            auto& llog = Logger::instance();
            llog.setLevel(selectedLevel);
            llog.info("Log level changed to {}", Logger::getLevelName(selectedLevel));
        }

        CRegKey key;
        if (key.Open(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", KEY_WRITE) == ERROR_SUCCESS)
        {
            if (m_AutoStart)
            {
                TCHAR exePath[MAX_PATH];
                GetModuleFileName(NULL, exePath, MAX_PATH);
                key.SetStringValue("SKYM", exePath);
            }
            else
            {
                key.DeleteValue("SKYM");
            }
            key.Close();
        }
        else
        {
            log.error("Failed to open registry key for auto start settings");
            return PSNRET_INVALID;
        }

        if (key.Create(HKEY_CURRENT_USER, _T(SKYM_KEY_ROOT)) == ERROR_SUCCESS)
        {
            key.SetDWORDValue(_T("PageantMode"), m_PageantMode);
            key.SetDWORDValue(_T("NamedPipe"), m_NamedPipe);
            key.SetDWORDValue(_T("HyperVIntegration"), m_HyperVIntegration);
            key.SetDWORDValue(_T("EnableDebugConsole"), m_EnableDebugConsole);
            key.SetDWORDValue(_T("DebugLevel"), static_cast<DWORD>(selectedLevel));
            key.Close();
        }
        else
        {
            log.error("Failed to open/create registry key for integration settings");
            return PSNRET_INVALID;
        }

        SetModified(FALSE);
        return PSNRET_NOERROR;
    }

    LRESULT OnSomethingChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL & /*bHandled*/)
    {
        bool changed = false;
        changed |= (m_AutoStart != (IsDlgButtonChecked(IDC_AUTO_START) == BST_CHECKED));
        changed |= (m_PageantMode != (IsDlgButtonChecked(IDC_PAGEANT_MODE) == BST_CHECKED));
        changed |= (m_NamedPipe != (IsDlgButtonChecked(IDC_NAMED_PIPE) == BST_CHECKED));
        changed |= (m_HyperVIntegration != (IsDlgButtonChecked(IDC_HYPERV_INTEGRATION) == BST_CHECKED));
        changed |= (m_EnableDebugConsole != (IsDlgButtonChecked(IDC_ENABLE_DEBUG_CONSOLE) == BST_CHECKED));
        changed |= (static_cast<Logger::Level>(m_debugLevelCombo.GetCurSel()) != Logger::instance().getLevel());
        SetModified(changed);
        return 0;
    }

private:
    CComboBox m_debugLevelCombo;
    bool m_AutoStart = false;
    bool m_PageantMode = true;
    bool m_NamedPipe = true;
    bool m_HyperVIntegration = false;
    bool m_EnableDebugConsole = false;
};

