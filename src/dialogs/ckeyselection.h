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
#include <functional>
#include <string>
#include <vector>

#include <libssha/key/key-manager-observer.h>
#include <libssha/key/key-manager.h>
#include <libssha/utils/logger.h>

#include "stdatl.h"
#include "cdpilistview.h"
#include "resources/resource.h"
#include "dialogs/cuserinputdialog.h"
#include "servers/client-info.h"

namespace nglab
{
    namespace libssha
    {
        class KeyBase;
    }
    namespace saru
    {
        using nglab::libssha::KeyManagerObserver;
        using nglab::libssha::PubKeyItemList;
        using nglab::libssha::KeyBasePtr;
        using nglab::libssha::PubKeyItem;
        class CKeySelectionDlg : public CUserInputDialog<CKeySelectionDlg>,
                                 public nglab::libssha::KeyManagerObserver,
                                 virtual public LogEnabler
        {
        public:
            using KeyListProvider = std::function<PubKeyItemList()>;

            enum
            {
                IDD = IDD_KEY_SELECTION,
                PrimaryButtonId = IDOK,
                CancelButtonId = IDCANCEL,
                WM_REFRESH_KEYS = WM_APP + 1
            };

            BEGIN_MSG_MAP(CKeySelectionDlg)
            MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
            MESSAGE_HANDLER(WM_TIMER, OnTimer)
            MESSAGE_HANDLER(WM_REFRESH_KEYS, OnRefreshKeys)
            NOTIFY_HANDLER(IDC_KEY_LIST, NM_DBLCLK, OnListDblClick)
            COMMAND_ID_HANDLER(IDOK, OnOk)
            COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
            CHAIN_MSG_MAP(CDpiResourceIcons<CKeySelectionDlg>)
            END_MSG_MAP()

            CKeySelectionDlg(KeyListProvider keyListProvider, const ClientInfo &clientInfo, WindowsSessionType sessionType);
            ~CKeySelectionDlg();

            virtual BOOL PreTranslateMessage(MSG *pMsg);

            LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
            LRESULT OnListDblClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL &bHandled);
            LRESULT OnOk(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
            LRESULT OnCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
            LRESULT OnRefreshKeys(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);

            virtual void onKeyAdded(KeyBasePtr key) override;
            virtual void onKeyRemoved(KeyBasePtr key) override;
            virtual void onKeysCleared() override;
            virtual void onKeyUsed(KeyBasePtr key, const libssha::Session *session) override {};
            virtual void onKeyDeclined(KeyBasePtr key, const libssha::Session *session) override {};
            virtual void onLocked() override {};
            virtual void onUnlocked() override {};

            const PubKeyItem &selectedItem() const { return m_keys.at(static_cast<size_t>(m_selectedIndex)); }

        private:
            void refreshKeys();

            CDpiListView m_keyList;
            ATL::CString m_label;
            KeyListProvider m_keyListProvider;
            PubKeyItemList m_keys;
            int m_selectedIndex = -1;
        };
    }
}
