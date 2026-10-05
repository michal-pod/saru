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
#include <set>

#include <libssha/utils/logger.h>
#include <libssha/key/key-manager-observer.h>

#include "stdatl.h"
#include "cdpilistview.h"
#include "cdpiresourceicons.h"
#include "resources/resource.h"
#include "debug-console.h"
#include "factories/icon-factory.h"
#include "factories/bitmap-factory.h"
namespace nglab
{
    namespace saru
    {
        using nglab::libssha::KeyManagerObserver;
        using nglab::libssha::LogEnabler;
        using nglab::libssha::Logger;

        class CKeyList : public CDialogImpl<CKeyList>, public CDpiResourceIcons<CKeyList>,
                         public CMessageFilter,
                         public KeyManagerObserver,
                         public LogEnabler
        {
        public:
            enum
            {
                IDD = IDD_KEY_LIST,
                WM_TRAYNOTIFY = WM_USER + 100,
                WM_CROSS_UI_MESSAGE,
                IDM_SHOW,
                IDM_EXIT,
                IDM_SHOW_DEBUG_CONSOLE,
                IDM_SHOW_SETTINGS,
                IDM_ABOUT
            };


            BEGIN_MSG_MAP(CKeyList)
                MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
                MESSAGE_HANDLER(WM_CLOSE, OnDialogClose)
                MESSAGE_HANDLER(WM_DESTROY, OnDestroyDialog)
                MESSAGE_HANDLER(WM_TIMER, OnTimer)
                MESSAGE_HANDLER(WM_CROSS_UI_MESSAGE, OnCrossUIMessage)
                COMMAND_ID_HANDLER(IDCLOSE, OnClose)
                COMMAND_ID_HANDLER(IDCANCEL, OnClose)
                COMMAND_ID_HANDLER(IDM_EXIT, OnExit)
                COMMAND_ID_HANDLER(IDM_SHOW, OnShow)
                COMMAND_ID_HANDLER(IDC_DEST_CONS_LOAD, OnLoadConstraint)
                COMMAND_ID_HANDLER(IDC_DEST_CONS_CLEAR, OnClearConstraint)
                COMMAND_ID_HANDLER(IDC_COPY_PUB_ID, OnCopyPubId)
                COMMAND_ID_HANDLER(IDC_REQUIRE_CONFIRMATION, OnConfirmChanged)
                MESSAGE_HANDLER_EX(WM_TRAYNOTIFY, OnTrayNotification)
                NOTIFY_HANDLER(IDC_KEY_LIST, LVN_ITEMCHANGED, OnKeySelectionChanged)
                CHAIN_MSG_MAP(CDpiResourceIcons<CKeyList>)
            END_MSG_MAP()

            static CKeyList& instance()
            {
                static CKeyList instance;
                return instance;
            }

            void DisplayTrayNotification(const std::string &title, const std::string &message, DWORD infoType = NIIF_INFO, UINT timeout = 5000);
            void DisplayMessageBox(const std::string &title, const std::string &message);

            virtual BOOL PreTranslateMessage(MSG *pMsg);

            LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
            LRESULT OnDestroyDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
            LRESULT OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
            LRESULT OnKeyDown(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
            LRESULT OnTrayNotification(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam);
            LRESULT OnCrossUIMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
            void HandleTrayCommand(UINT cmd);
            LRESULT OnClose(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
            LRESULT OnDialogClose(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
            LRESULT OnShow(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
            LRESULT OnShowDebugConsole(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
            LRESULT OnExit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
            LRESULT OnKeySelectionChanged(int idCtrl, LPNMHDR pnmh, BOOL &bHandled);
            LRESULT OnCopyPubId(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
            LRESULT OnConfirmChanged(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);

            LRESULT OnLoadConstraint(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
            LRESULT OnClearConstraint(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);

            LRESULT OnShowSettings(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
            LRESULT OnAbout(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);

            void onKeyAdded(const nglab::libssha::KeyBasePtr key) override;
            void onKeyRemoved(const nglab::libssha::KeyBasePtr key) override;
            void onKeysCleared() override;
            void onKeyUsed(const nglab::libssha::KeyBasePtr key, const nglab::libssha::Session *session) override;
            void onKeyDeclined(const nglab::libssha::KeyBasePtr key, const nglab::libssha::Session *session) override;
            void onLocked() override;
            void onUnlocked() override;

        private:
            void addKeyToList(const nglab::libssha::KeyBasePtr key);
            void removeKeyFromList(const std::string &fingerprint);
            void updateKeyDetails(bool timerTriggered = false);
            bool shouldNotifyKeyOperations();
            void showOrHideWindow();
            void keyAdded(const nglab::libssha::KeyBasePtr key);
            void keyRemoved(const nglab::libssha::KeyBasePtr key);
            void keysCleared();
            nglab::libssha::KeyBasePtr getSelectedKey();
            CKeyList();
            CDpiListView m_keyList;
            CDpiListView m_keyConstraints;
            CButton m_confirmCheck;
            CButton m_constrainsLoad;
            CButton m_constrainsClear;
            CButton m_copyPubId;
            CStatic m_keyTimeoutText;
            CMenu m_trayMenu;

            std::set<std::string> m_userLoadedConstraints;
        };
    }
}
