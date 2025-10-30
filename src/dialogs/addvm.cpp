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
#include "addvm.h"

#include <algorithm>

#include "config.h"

using namespace nglab::skym;

LRESULT CAddVMDialog::OnInitDialog(UINT, WPARAM, LPARAM, BOOL &)
{
    // Set icon
    HICON hIcon = AtlLoadIconImage(IDI_ICON1, LR_DEFAULTCOLOR, 32, 32);
    SetIcon(hIcon, TRUE);

    m_vmNameEdit.Attach(GetDlgItem(IDC_VM_NAME));
    m_vmGuidEdit.Attach(GetDlgItem(IDC_VM_GUID));
    m_validationStatus.Attach(GetDlgItem(IDC_VALIDATION_STATUS));
    m_addButton.Attach(GetDlgItem(IDOK));

    m_addButton.EnableWindow(FALSE);

    return TRUE;
}

LRESULT CAddVMDialog::OnCloseCmd(WORD, WORD wID, HWND, BOOL &)
{
    if (wID == IDOK)
    {
        log.info("Adding VM with GUID: {}", m_guid);
        CRegKey key;
        std::string regPath = SKYM_KEY_ROOT "\\HyperVVMs\\" + m_guid;
        if (key.Create(HKEY_CURRENT_USER, regPath.c_str()) == ERROR_SUCCESS)
        {
            CString vmName;
            m_vmNameEdit.GetWindowText(vmName);
            key.SetDWORDValue("Enabled", 0);
            key.SetStringValue("Description", vmName.GetString());
            key.Close();
        }
        else
        {
            log.error("Failed to create registry key for VM with GUID: {}", m_guid);
        }
    }
    EndDialog(wID);
    return 0;
}

LRESULT CAddVMDialog::OnSomethingChanged(WORD, WORD, HWND, BOOL &)
{
    CString vmName;
    m_vmNameEdit.GetWindowText(vmName);
    CString vmGuid;
    m_vmGuidEdit.GetWindowText(vmGuid);

    bool isValid = true;

    std::string validationMessage;

    std::string vmNameStr(vmName.GetString());
    std::string vmGuidStr(vmGuid.GetString());

    // Check vmname only
    if (vmNameStr.empty())
    {
        validationMessage = "VM Name cannot be empty.";
        isValid = false;
    }

    if (isValid && vmNameStr.front() == '-')
    {
        validationMessage = "VM Name cannot start with hyphen.";
        isValid = false;
    }

    if (isValid && vmNameStr.back() == '-')
    {
        validationMessage = "VM Name cannot end with hyphen.";
        isValid = false;
    }

    for (auto ch : vmNameStr)
    {
        if (!(isalnum(ch) || ch == '-' || ch == ' ') && isValid)
        {
            validationMessage = "VM Name can only contain alphanumeric characters, spaces, or hyphens.";
            isValid = false;
            break;
        }
    }

    // remove spaces and braces
    vmGuidStr.erase(std::remove_if(vmGuidStr.begin(), vmGuidStr.end(), [](char c){ return c == ' ' || c == '{' || c == '}'; }), vmGuidStr.end());

    if (isValid && vmGuidStr.empty())
    {
        validationMessage = "VM GUID cannot be empty.";
        isValid = false;
    }

    if (isValid && vmGuidStr.length() != 36)
    {
        validationMessage = "VM GUID must be 36 characters long.";
        isValid = false;
    }

    for (int i = 0; i < (int)vmGuidStr.length() && isValid; ++i)
    {
        char ch = vmGuidStr[i];
        if (ch == '-' && (i == 8 || i == 13 || i == 18 || i == 23))
            continue;

        if (!isxdigit(static_cast<unsigned char>(ch)))
        {
            validationMessage = "VM GUID is not in a valid format.";
            isValid = false;
            break;
        }
    }

    if (isValid)
    {
        m_validationStatus.SetWindowText("");
        m_guid = vmGuidStr;
        std::transform(m_guid.begin(), m_guid.end(), m_guid.begin(), [](unsigned char c) { return std::toupper(c); });
    }
    else
    {
        m_validationStatus.SetWindowText(validationMessage.c_str());
    }

    log.info("VM Name: {}, VM GUID: {}, Valid: {}, Message: {}", vmNameStr, vmGuidStr, isValid, validationMessage);

    m_addButton.EnableWindow(isValid ? TRUE : FALSE);

    return 0;
}

