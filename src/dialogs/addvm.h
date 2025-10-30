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
#include <regex>

#include <libssha/utils/logger.h>

#include "stdatl.h"
#include "resources/resource.h"

namespace nglab
{
    namespace skym
    {
        using nglab::libssha::Logger;
        using nglab::libssha::LogEnabler;
        class CAddVMDialog : public CDialogImpl<CAddVMDialog>, public LogEnabler
        {
        public:
            enum
            {
                IDD = IDD_ADD_VM
            };

            BEGIN_MSG_MAP(CAddVMDialog)
            MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
            COMMAND_HANDLER(IDC_VM_NAME, EN_CHANGE, OnSomethingChanged)
            COMMAND_HANDLER(IDC_VM_GUID, EN_CHANGE, OnSomethingChanged)
            COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
            COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
            END_MSG_MAP()

            CAddVMDialog(HWND hParent = nullptr) : LogEnabler("CAddVMDialog")
            {

            }

            LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL &);

            LRESULT OnCloseCmd(WORD, WORD wID, HWND, BOOL &);

            LRESULT OnSomethingChanged(WORD, WORD, HWND, BOOL &);
            private:
            CEdit m_vmNameEdit;
            CEdit m_vmGuidEdit;
            CWindow m_validationStatus;
            CButton m_addButton;
            std::string m_guid;
        };
    }
}

