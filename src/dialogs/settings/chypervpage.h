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
#include <string>
#include <vector>

#include <libssha/utils/logger.h>

#include "stdatl.h"
#include "dialogs/cdpilistview.h"
#include "resources/resource.h"
#include "system/uac-helper.h"

using nglab::libssha::LogEnabler;
using nglab::libssha::Logger;
namespace nglab
{
    namespace saru
    {

        class CHyperVPage : public CPropertyPageImpl<CHyperVPage>, virtual public LogEnabler
        {
        public:
            enum
            {
                IDD = IDD_SETTINGS_HYPERV,
                WM_HELPER_EXITED = WM_APP + 0x100
            };

            CDpiListView m_list;
            // compact item representation for each list row
            struct Item
            {
                std::string name;
                std::string guid;
                bool enabled{false};
                bool loaded_state{false};
            };

            std::vector<Item> m_items;

            BEGIN_MSG_MAP(CHyperVPage)
            MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
            COMMAND_ID_HANDLER(IDC_HV_ADD, OnAdd)
            COMMAND_ID_HANDLER(IDC_HV_DELETE, OnDelete)
            COMMAND_ID_HANDLER(IDC_HV_RESCAN, OnRescan)
            NOTIFY_HANDLER(IDC_HYPERV_LIST, NM_DBLCLK, OnListClick)
            NOTIFY_HANDLER(IDC_HYPERV_LIST, LVN_KEYDOWN, OnListKeyDown)
            NOTIFY_HANDLER(IDC_HYPERV_LIST, LVN_ITEMCHANGED, OnListItemChanged)
            MESSAGE_HANDLER(WM_HELPER_EXITED, OnHelperExited)
            CHAIN_MSG_MAP(CPropertyPageImpl<CHyperVPage>)
            END_MSG_MAP()

            CHyperVPage();

            static void initDefault();

            LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL &);

            LRESULT OnHelperExited(UINT, WPARAM wParam, LPARAM lParam, BOOL &);

            int OnApply();

            LRESULT OnAdd(WORD, WORD, HWND, BOOL &);

            LRESULT OnDelete(WORD, WORD, HWND, BOOL &);

            LRESULT OnRescan(WORD, WORD, HWND, BOOL &);

            LRESULT OnListClick(int idCtrl, LPNMHDR pnmh, BOOL &);
            LRESULT OnListKeyDown(int idCtrl, LPNMHDR pnmh, BOOL &);

            LRESULT OnListItemChanged(int idCtrl, LPNMHDR pnmh, BOOL &);

        private:
            void reloadList();
            void updateButtonsState();
            CWindow m_deleteButton;
        };
    } // namespace saru
} // namespace nglab
