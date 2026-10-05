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
#pragma once

#include <filesystem>
#include <vector>

#include <libssha/utils/logger.h>

#include "resources/resource.h"
#include "stdatl.h"
#include "cdpilistview.h"
#include "cdpiresourceicons.h"

namespace nglab::saru
{
    class CSkipExecutablesList : public CDialogImpl<CSkipExecutablesList>, public CDpiResourceIcons<CSkipExecutablesList>, public nglab::libssha::LogEnabler
    {
    public:
        enum
        {
            IDD = IDD_SKIP_EXECUTABLES_LIST
        };

        BEGIN_MSG_MAP(CSkipExecutablesList)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        NOTIFY_HANDLER(IDC_SKIP_EXECUTABLES_LIST, LVN_ITEMCHANGED, OnSelectionChanged)
        COMMAND_ID_HANDLER(IDC_SKIP_EXECUTABLES_ADD, OnAdd)
        COMMAND_ID_HANDLER(IDC_SKIP_EXECUTABLES_REMOVE, OnRemove)
        COMMAND_ID_HANDLER(IDOK, OnSave)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
        CHAIN_MSG_MAP(CDpiResourceIcons<CSkipExecutablesList>)
        END_MSG_MAP()

        CSkipExecutablesList();

        static void initDefault();

        LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
        LRESULT OnSelectionChanged(int idCtrl, LPNMHDR pnmh, BOOL &bHandled);
        LRESULT OnAdd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
        LRESULT OnRemove(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
        LRESULT OnSave(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
        LRESULT OnCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);

    private:
        static std::vector<std::filesystem::path> buildDefaullOriginatingProcessSkipList();

        void loadExecutables();
        bool saveExecutables();
        void refreshList();
        void updateRemoveButton();
        bool contains(const std::filesystem::path &path) const;

        CDpiListView m_list;
        CButton m_addButton;
        CButton m_removeButton;
        CButton m_cancelButton;
        CButton m_saveButton;
        HIMAGELIST m_systemImageList{nullptr};
        std::vector<std::filesystem::path> m_executables;
    };
}
