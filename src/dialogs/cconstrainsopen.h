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
#include <shobjidl.h>
#include <fstream>

#include <libssha/utils/logger.h>
#include <libssha/utils/secure_vector.h>
#include <libssha/extensions/openssh-restrict-destination.h>

#include "stdatl.h"
#include "utils/string-converter.h"
namespace nglab
{
    namespace skym
    {
        using nglab::libssha::Deserializer;
        using nglab::libssha::LogEnabler;
        using nglab::libssha::Logger;
        using nglab::libssha::OpenSSHHopDescriptor;
        using nglab::libssha::OpenSSHSDestinationConstraint;
        using nglab::libssha::OpenSSHSRestrictDestination;
        using nglab::libssha::secure_vector;

        class CConstrainsOpenDialog : public LogEnabler
        {

        public:
            enum
            {                
                IDC_LOAD_PERMANENT = 1000,
            };
            CConstrainsOpenDialog(HWND hWndParent) : LogEnabler("CConstrainsOpenDialog"), m_hWndParent(hWndParent)
            {
                HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_fileDialog));
                if (!SUCCEEDED(hr))

                {
                    throw std::runtime_error("Failed to create File Open Dialog instance");
                }

                // Set options to allow multiple file selection
                DWORD dwOptions;
                if (!SUCCEEDED(m_fileDialog->GetOptions(&dwOptions)))
                {
                    throw std::runtime_error("Failed to get file dialog options");
                }
                m_fileDialog->SetOptions(dwOptions | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);

                // Set file types (e.g., .json files)
                COMDLG_FILTERSPEC rgSpec[] =
                    {
                        {L"SSH constrains", L"*.cdc"},
                        {L"All Files", L"*.*"}};
                m_fileDialog->SetFileTypes(ARRAYSIZE(rgSpec), rgSpec);
                m_fileDialog->SetDefaultExtension(L"cdc");

                if (!SUCCEEDED(m_fileDialog->QueryInterface(IID_PPV_ARGS(&m_fileDialogCustomize))))
                {
                    throw std::runtime_error("Failed to get IFileDialogCustomize interface");
                }

                m_fileDialogCustomize->AddCheckButton(IDC_LOAD_PERMANENT, L"Load as persistent constrains", TRUE);                
            }

            bool DoModal()
            {
                HRESULT hr = m_fileDialog->Show(m_hWndParent);
                if (SUCCEEDED(hr))
                {

                    m_fileDialogCustomize->GetCheckButtonState(IDC_LOAD_PERMANENT, &m_persistent);

                    log.debug("User selected to load file {} constrains as {}", getFilePath(), m_persistent ? "persistent" : "temporary");

                    return true;
                }
                else if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
                {
                    return false; // User cancelled the dialog
                }
                else
                {
                    throw std::runtime_error("Failed to show file open dialog");
                }
            }

            const std::vector<OpenSSHSDestinationConstraint> &constraints() const { return m_constraints; }
            BOOL isPersistent() const { return m_persistent; }

            std::string getFilePath()
            {
                CComPtr<IShellItem> pItem = nullptr;
                PWSTR pszFilePath = nullptr;
                HRESULT hr = m_fileDialog->GetResult(&pItem);
                if (SUCCEEDED(hr))
                {
                    hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
                    if (SUCCEEDED(hr))
                    {
                        std::string filePath = StringConverter::w2u8(pszFilePath);
                        CoTaskMemFree(pszFilePath);
                        return filePath;
                    }
                }
                return "";
            }

            bool getPersistent() const
            {
                return m_persistent;
            }

        private:
            CComPtr<IFileOpenDialog> m_fileDialog;
            CComPtr<IFileDialogCustomize> m_fileDialogCustomize;
            std::vector<OpenSSHSDestinationConstraint> m_constraints;
            BOOL m_persistent;
            HWND m_hWndParent;
        };
    }
}
