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
#include "chypervpage.h"

#include "system/directories.h"
#include "system/uac-helper.h"
#include "utils/string-converter.h"
#include "dialogs/addvm.h"
#include "config.h"

#include <shlobj.h>
#include <thread>


using namespace nglab::skym;

CHyperVPage::CHyperVPage() : CPropertyPageImpl<CHyperVPage>(_T("Hyper-V")), LogEnabler("CHyperVPage")
{
}

LRESULT CHyperVPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL &)
{
    m_list.Attach(GetDlgItem(IDC_HYPERV_LIST));
    m_list.InsertColumn(0, _T("Virtual Machine"), LVCFMT_LEFT, 260);
    m_list.InsertColumn(1, _T("Allowed"), LVCFMT_CENTER, 80);
    m_list.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT);

    bool canElevate = UACHelper::canElevate();
    bool isElevated = UACHelper::isElevated();

    m_deleteButton = GetDlgItem(IDC_HV_DELETE);
    m_deleteButton.EnableWindow(FALSE);


    CWindow scanButton = GetDlgItem(IDC_HV_RESCAN);
    CWindow addButton = GetDlgItem(IDC_HV_ADD);
    addButton.ShowWindow(SW_HIDE);

    log.vdebug("Hyper-V page initialized. isElevated={}, canElevate={}", isElevated, canElevate);

    if (!canElevate)
    {
        scanButton.EnableWindow(FALSE);
        addButton.ShowWindow(SW_SHOW);
    }

    if (canElevate && !isElevated)
    {
        SHSTOCKICONINFO sii = {};
        sii.cbSize = sizeof(SHSTOCKICONINFO);
        if (SUCCEEDED(SHGetStockIconInfo(SIID_SHIELD, SHGSI_ICON | SHGSI_SMALLICON, &sii)))
        {
            HICON hShieldIcon = sii.hIcon;
            scanButton.SendMessage(BM_SETIMAGE, IMAGE_ICON, (LPARAM)hShieldIcon);
        }
    }

    reloadList();

    return TRUE;
}

LRESULT CHyperVPage::OnHelperExited(UINT, WPARAM wParam, LPARAM lParam, BOOL &)
{
    log.vdebug("UAC helper process exited, reloading Hyper-V VM list with exit code {}", static_cast<DWORD>(wParam));
    reloadList();
    return 0;
}

int CHyperVPage::OnApply()
{
    // Persist current m_items into HKCU\Software\SKYM\HyperVVMs\<guid>\Enabled and Description
    CRegKey rootKey;
    LONG rc = rootKey.Create(HKEY_CURRENT_USER, SKYM_KEY_ROOT "\\HyperVVMs");
    if (rc != ERROR_SUCCESS)
    {
        log.error("Failed to create/open HyperVVMs registry key: {}", rc);
        return PSNRET_INVALID;
    }

    for (size_t i = 0; i < m_items.size(); ++i)
    {
        const auto &it = m_items[i];
        std::string subpath = std::string(SKYM_KEY_ROOT "\\HyperVVMs\\") + it.guid;
        CRegKey vmKey;
        LONG r2 = vmKey.Create(HKEY_CURRENT_USER, subpath.c_str());
        if (r2 != ERROR_SUCCESS)
        {
            log.error("Failed to create/open VM registry key {}: {}", subpath, r2);
            return PSNRET_INVALID;
        }
        // Save description and enabled flag
        vmKey.SetStringValue("Description", it.name.c_str());
        DWORD enabled = it.enabled ? 1 : 0;
        vmKey.SetDWORDValue("Enabled", enabled);
        vmKey.Close();
    }

    reloadList();

    return PSNRET_NOERROR;
}

LRESULT CHyperVPage::OnAdd(WORD, WORD, HWND, BOOL &)
{
    // Implementation for adding a VM
    CAddVMDialog dlg(m_hWnd);
    if (dlg.DoModal() != IDOK)
    {
        return 0;
    }
    reloadList();
    
    return 0;
}

LRESULT CHyperVPage::OnDelete(WORD, WORD, HWND, BOOL &)
{
    // Implementation for deleting a VM
    if (m_list.GetSelectedCount() == 0)
    {
        log.warning("Delete button clicked but no VM is selected");
        return 0;
    }

    int index = m_list.GetNextItem(-1, LVNI_SELECTED);
    if (index != -1 && index < static_cast<int>(m_items.size()))
    {
        CTaskDialog dlg(m_hWnd);
        dlg.SetWindowTitle(L"Confirm Deletion");
        std::wstring message = std::format(L"Are you sure you want to remove access to {}", StringConverter::u8w2(m_items[index].name));
        dlg.SetContentText(message.c_str());
        TASKDIALOG_BUTTON btn[] = {
            {IDYES, L"Yes"},
            {IDNO, L"No"}
        };
        dlg.SetButtons(btn, 2);
        dlg.SetDefaultButton(IDNO);
        dlg.SetMainIcon(TD_WARNING_ICON);
        dlg.SetExpandedInformationText(L"Deleting a Virtual Machine will remove access to this agent.\nIf this VM is still present in Hyper-V Manager, next rescan will add it back but without access to this agent.");
        dlg.SetCollapsedControlText(L"More Information");
        dlg.SetExpandedControlText(L"Less Information");
        dlg.ModifyFlags(0, TDF_EXPAND_FOOTER_AREA);
        int ret = 0;
        dlg.DoModal(m_hWnd, &ret);
        if (ret != IDYES)
        {
            log.vdebug("User cancelled deletion of VM {}", m_items[index].guid);
            return 0;
        }
        CRegKey rootKey;
        LONG rc = rootKey.Open(HKEY_CURRENT_USER, SKYM_KEY_ROOT "\\HyperVVMs", KEY_WRITE);
        if (rc == ERROR_SUCCESS)
        {
            std::string subpath = std::string(SKYM_KEY_ROOT "\\HyperVVMs\\") + m_items[index].guid;
            CRegKey vmKey;
            LONG r2 = vmKey.Open(HKEY_CURRENT_USER, subpath.c_str(), KEY_WRITE);
            if (r2 == ERROR_SUCCESS)
            {
                vmKey.RecurseDeleteKey("");
                vmKey.Close();
                log.info("Deleted registry key for VM {}", m_items[index].guid);
            }
            else
            {
                log.error("Failed to open VM registry key {} for deletion: {}", subpath, r2);
            }
            rootKey.Close();
        }
        else
        {
            log.error("Failed to open HyperVVMs registry key for deletion: {}", rc);
        }
        reloadList();
    }
 
    return 0;
}

LRESULT CHyperVPage::OnRescan(WORD, WORD, HWND, BOOL &)
{
    log.vdebug("Rescan Hyper-V VMs button clicked");
    HWND hwnd = m_hWnd;
    std::thread([hwnd]()
                {
        auto log = Logger::instance();
        SHELLEXECUTEINFO sei = {};
        sei.cbSize = sizeof(SHELLEXECUTEINFO);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.hwnd = NULL;
        sei.lpVerb = _T("runas");
        std::string helperPath = Directories::getExecutableDirectoryA() + "\\skym-hvh.exe";
        sei.lpFile = helperPath.c_str();

        BOOL result = ShellExecuteEx(&sei);
        if (!result)
        {
            DWORD error = GetLastError();
            log.error("Failed to launch UAC helper for Hyper-V rescan. Error code: {}", error);
            if (::IsWindow(hwnd))
            {
                ::PostMessage(hwnd, WM_HELPER_EXITED, error, 0);
            }

            return;
        }

        HANDLE hProcess = sei.hProcess;
        DWORD exitCode = 0xFFFFFFFF;

        if (hProcess)
        {
            DWORD waitResult = WaitForSingleObject(hProcess, INFINITE);

            if (waitResult == WAIT_OBJECT_0)
            {
                auto success = GetExitCodeProcess(hProcess, &exitCode);
                if (success != 0) {
                    exitCode = GetLastError();
                }

                log.vdebug("UAC helper process exited with error code: {} success {}", exitCode, success);

            }
            else            
            {
                log.error("Waiting for UAC helper process failed with result: {}", waitResult);
                exitCode = GetLastError();                
            }
            CloseHandle(hProcess);
        }
        else {            
            exitCode = GetLastError();
            log.error("Failed to obtain handle for UAC helper process");
        }
        if(::IsWindow(hwnd))
        {
            ::PostMessage(hwnd, WM_HELPER_EXITED, (WPARAM)exitCode, 0);
        } })
        .detach();

    return 0;
}

void CHyperVPage::reloadList()
{
    CRegKey key;
    m_list.DeleteAllItems();
    m_items.clear();

    if (key.Open(HKEY_CURRENT_USER, SKYM_KEY_ROOT "\\HyperVVMs", KEY_READ) == ERROR_SUCCESS)
    {
        // Read VM information from the registry and populate the list control
        int index = 0;
        CHAR vmGuid[256];
        while (true)
        {
            DWORD vmGuidSize = sizeof(vmGuid); // reset size each call
            LONG enumRc = key.EnumKey(index, vmGuid, &vmGuidSize);
            if (enumRc != ERROR_SUCCESS)
                break;
            ++index;

            // For each VM, read its properties and add them to the list control
            log.debug("Found VM GUID: {}", vmGuid);
            CRegKey vmKey;
            std::string vmPath = SKYM_KEY_ROOT "\\HyperVVMs\\" + std::string(vmGuid);
            if (vmKey.Open(HKEY_CURRENT_USER, vmPath.c_str(), KEY_READ) == ERROR_SUCCESS)
            {
                // Read VM properties from the registry
                CHAR vmName[256];
                DWORD vmNameSize = sizeof(vmName);
                DWORD enabled = 0;
                if (vmKey.QueryStringValue("Description", vmName, &vmNameSize) == ERROR_SUCCESS &&
                    vmKey.QueryDWORDValue("Enabled", enabled) == ERROR_SUCCESS)
                {
                    log.vdebug("adding VM: {} as {}", vmName, enabled ? "enabled" : "disabled");
                    int itemIndex = m_list.InsertItem(m_list.GetItemCount(), vmName);
                    // set textual Yes/No in the 'Allowed' column for clarity
                    m_list.SetItemText(itemIndex, 1, enabled ? _T("Yes") : _T("No"));
                    m_list.SetItemText(itemIndex, 2, vmGuid);
                    // create and append item record
                    CHyperVPage::Item it;
                    it.name = std::string(vmName);
                    it.guid = std::string(vmGuid);
                    it.enabled = (enabled != 0);
                    it.loaded_state = it.enabled;
                    m_items.push_back(std::move(it));
                    vmKey.Close();
                }
            }
        }
        key.Close();
    }

    // After reloading, no modifications pending
    SetModified(FALSE);
}

LRESULT CHyperVPage::OnListClick(int idCtrl, LPNMHDR pnmh, BOOL &)
{
    LPNMITEMACTIVATE nm = reinterpret_cast<LPNMITEMACTIVATE>(pnmh);
    if (!nm)
        return 0;
    int row = nm->iItem;

    if (row != -1 && row < static_cast<int>(m_items.size()))
    {
        // Toggle the enabled state
        m_items[row].enabled = !m_items[row].enabled;
        m_list.SetItemText(row, 1, m_items[row].enabled ? _T("Yes") : _T("No"));
        log.vdebug("Toggled VM {} via click to {}", m_items[row].guid, m_items[row].enabled ? "enabled" : "disabled");
        updateButtonsState();
    }

    return 0;
}

LRESULT CHyperVPage::OnListKeyDown(int idCtrl, LPNMHDR pnmh, BOOL &)
{
    LPNMLVKEYDOWN kv = reinterpret_cast<LPNMLVKEYDOWN>(pnmh);
    if (!kv)
        return 0;

    if (kv->wVKey == VK_SPACE)
    {
        int idx = m_list.GetNextItem(-1, LVNI_FOCUSED);
        if (idx != -1 && idx < static_cast<int>(m_items.size()))
        {
            m_items[idx].enabled = !m_items[idx].enabled;
            updateButtonsState();
            m_list.SetItemText(idx, 1, m_items[idx].enabled ? _T("Yes") : _T("No"));
            log.vdebug("Toggled VM {} via space to {}", m_items[idx].guid, m_items[idx].enabled ? "enabled" : "disabled");
        }
    }

    return 0;
}

LRESULT CHyperVPage::OnListItemChanged(int idCtrl, LPNMHDR pnmh, BOOL &)
{
    updateButtonsState();
    return 0;
}

void CHyperVPage::updateButtonsState()
{
    bool modified = false;
    for (auto &item : m_items)
    {
        if (item.enabled != item.loaded_state)
        {
            modified = true;
            break;
        }
    }

    // Enable or disable the Delete button based on selection
    if (m_list.GetSelectedCount() > 0)
    {
        m_deleteButton.EnableWindow(TRUE);
    }
    else
    {
        m_deleteButton.EnableWindow(FALSE);
    }

    SetModified(modified);
}
