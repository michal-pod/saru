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
#include <libssha/key/key.h>
#include <libssha/key/key-manager-observer.h>

#include "stdatl.h"
#include "resources/resource.h"
#include "servers/client-info.h"
#include "dialogs/cuserinputdialog.h"

namespace nglab
{
    namespace libssha
    {
        class KeyBase;
    }
    namespace skym
    {
        using namespace ATL;
        using nglab::libssha::KeyBase;
        using nglab::libssha::KeyBasePtr;
        using nglab::libssha::Logger;
        using nglab::libssha::LogEnabler;

        enum WindowsSessionType : uint8_t;
        struct ClientInfo;

    class CKeyConfirm : public CUserInputDialog<CKeyConfirm>,
                public CWinDataExchange<CKeyConfirm>,                
                public nglab::libssha::KeyManagerObserver,
                virtual public LogEnabler
        {
        public:
            enum
            {
                IDD = IDD_KEY_CONFIRM,
                PrimaryButtonId = IDYES,
                CancelButtonId = IDNO
            };

            BEGIN_MSG_MAP(CKeyConfirm)
            MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
            MESSAGE_HANDLER(WM_PAINT, OnPaint)
            MESSAGE_HANDLER(WM_TIMER, OnTimer)
            COMMAND_ID_HANDLER(IDYES, OnAllow)
            COMMAND_ID_HANDLER(IDNO, OnDeny)
            COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
            COMMAND_ID_HANDLER(IDCLOSE, OnCancel)
            COMMAND_ID_HANDLER(IDC_REMEMBER_CHECK, OnRememberCheck)

            END_MSG_MAP()

            BEGIN_DDX_MAP(CKeyConfirm)
            DDX_TEXT(IDC_KEY_INFO, m_keyInfo)
            DDX_TEXT(IDC_FINGERPRINT, m_keyFingerprint)
            DDX_TEXT(IDC_HEADER_LINE, m_label)
            DDX_CHECK(IDC_REMEMBER_CHECK, m_rememberKey)
            //            DDX_COMBOBOX(IDC_TIME_COMBO, m_rememberTime)

            END_DDX_MAP()

            CKeyConfirm(const KeyBase &key, ClientInfo& clientInfo, WindowsSessionType sessionType);
            ~CKeyConfirm();

            virtual BOOL PreTranslateMessage(MSG *pMsg);

            LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
            LRESULT OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);
            LRESULT OnRememberCheck(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
            LRESULT OnAllow(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
            LRESULT OnDeny(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
            LRESULT OnCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);

            const int getRememberTime() const;
            const int getRememberKey() const;

            virtual void onKeyAdded(KeyBasePtr key) override {};
            virtual void onKeyRemoved(KeyBasePtr key) override;
            virtual void onKeysCleared() override {};
            virtual void onKeyUsed(KeyBasePtr key, const libssha::Session* session) override {};
            virtual void onKeyDeclined(KeyBasePtr key, const libssha::Session* session) override {};
            virtual void onLocked() override {};
            virtual void onUnlocked() override {};


        private:
            ATL::CString m_keyInfo;
            ATL::CString m_keyFingerprint;
            ATL::CString m_label;
            CComboBox m_timesList;        

            void readRememberSettings();

            const int REMEMBER_TIME_OPTIONS[3] = {60, 300, 3600}; // in seconds
            const char *REMEMBER_TIME_STRINGS[3] = {"1 minute", "5 minutes", "1 hour"};
            int m_rememberKey = 0;
            int m_rememberTime = 0;
            int m_extraLine1Icon = 0;
            int m_extraLine2Icon = 0;
            int m_timer = 0;
        };
    } // namespace skym
} // namespace nglab

