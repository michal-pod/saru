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

#include <libssha/utils/logger.h>

#include "stdatl.h"
#include "resources/resource.h"

namespace nglab::saru
{

using nglab::libssha::LogEnabler;

class CNotificationPage : public CPropertyPageImpl<CNotificationPage>, public LogEnabler
{
public:
    enum { IDD = IDD_SETTINGS_NOTIFICATION };

    BEGIN_MSG_MAP(CNotificationPage)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    CHAIN_MSG_MAP(CPropertyPageImpl<CNotificationPage>)
    COMMAND_ID_HANDLER(IDC_NOTIFY_KEY_USAGE, OnSomethingChanged)
    COMMAND_ID_HANDLER(IDC_ALWAYS_CONFIRM, OnSomethingChanged)
    COMMAND_ID_HANDLER(IDC_NOTIFY_KEY_OPERATIONS, OnSomethingChanged)
    COMMAND_ID_HANDLER(IDC_NOTIFY_KEY_DECLINE, OnSomethingChanged)
    COMMAND_ID_HANDLER(IDC_SHOW_KEY_SELECTION, OnSomethingChanged)
    COMMAND_ID_HANDLER(IDC_AUTO_DECLINE, OnSomethingChanged)
    COMMAND_ID_HANDLER(IDC_FORCE_ON_TOP, OnSomethingChanged)
    COMMAND_ID_HANDLER(IDC_SHOW_ORIGINATING_PROCESS, OnSomethingChanged)
    COMMAND_ID_HANDLER(IDC_EDIT_SKIP_EXECUTABLES, OnEditSkipExecutables)
    END_MSG_MAP()

    CNotificationPage();

    static void initDefault();

    LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL &);
    int OnApply();
    LRESULT OnSomethingChanged(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);
    LRESULT OnEditSkipExecutables(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL &bHandled);

private:
    CButton m_EditSkipExecutablesButton;
    bool m_NotifyKeyUsage = true;
    bool m_AlwaysConfirm = false;
    bool m_KeyOperations = false;
    bool m_NotifyKeyDeclined = false;
    bool m_ShowKeySelection = false;
    bool m_AutoDecline = false;
    bool m_ForceOnTop = true;
    bool m_ShowOriginatingProcess = true;
};

}

