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
#include <shellscalingapi.h>

#include "stdatl.h"
#include "config.h"
#include "resources/resource.h"
#include "servers/client-info.h"
#include "factories/icon-factory.h"

namespace nglab
{
    namespace saru
    {
        using namespace ATL;
        using nglab::libssha::LogEnabler;

        enum WindowsSessionType : uint8_t;

        template <typename T>
        class CUserInputDialog : public CDialogImpl<T>, virtual public LogEnabler
        {
        public:
            enum
            {
                IDT_TIMEOUT_CHECK = 1

            };

            CUserInputDialog(const ClientInfo &clientInfo) : m_clientInfo(clientInfo)
            {
            }

            void initTimeout()
            {
                CButton btnAccept;
                btnAccept.Attach(this->GetDlgItem(T::PrimaryButtonId));
                btnAccept.SetIcon(IconFactory::get(IDI_ACCEPT));

                m_cancelButton.Attach(this->GetDlgItem(T::CancelButtonId));
                m_cancelButton.SetIcon(IconFactory::get(IDI_CANCEL));

                CRegKey key;
                if (key.Open(HKEY_CURRENT_USER, _T(SARU_KEY_ROOT)) == ERROR_SUCCESS)
                {
                    DWORD timeout = 0;
                    if (key.QueryDWORDValue(_T("AutoDecline"), timeout) == ERROR_SUCCESS && timeout > 0)
                    {
                        m_timer = static_cast<int>(timeout);
                        log.vdebug("CKeyConfirm dialog will timeout in {} seconds", m_timer);
                        updateDenyButton();
                        this->SetTimer(IDT_TIMEOUT_CHECK, 1000);
                    }
                }
            }

            void forceIntoForeground()
            {

                DWORD dwThis = GetCurrentThreadId();
                HWND hForeground = GetForegroundWindow();
                DWORD dwForegroud = hForeground ? GetWindowThreadProcessId(hForeground, NULL) : 0;
                bool attached = false;
                if (dwThis != dwForegroud)
                {
                    attached = true;
                    AttachThreadInput(dwThis, dwForegroud, TRUE);
                }

                HMONITOR hMonitor = MonitorFromWindow(hForeground, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi = {0};
                mi.cbSize = sizeof(MONITORINFO);
                if (!GetMonitorInfo(hMonitor, &mi))
                {
                    hMonitor = MonitorFromWindow(this->m_hWnd, MONITOR_DEFAULTTOPRIMARY);
                    GetMonitorInfo(hMonitor, &mi);
                }

                RECT rcWorkspace = mi.rcWork;
                RECT rcDlg;

                this->GetWindowRect(&rcDlg);
                int dialogWidth = rcDlg.right - rcDlg.left;
                int dialogHeight = rcDlg.bottom - rcDlg.top;
                auto sourceDpi = GetDpiForWindow(this->m_hWnd);
                UINT targetXDpi, targetYDpi;

                if (GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &targetXDpi, &targetYDpi) == ERROR_SUCCESS &&
                    sourceDpi != targetXDpi)
                {
                    dialogWidth = MulDiv(dialogWidth, targetXDpi, sourceDpi);
                    dialogHeight = MulDiv(dialogHeight, targetYDpi, sourceDpi);
                }
                int x = rcWorkspace.left + (rcWorkspace.right - rcWorkspace.left - dialogWidth) / 2;
                ;
                int y = rcWorkspace.top + (rcWorkspace.bottom - rcWorkspace.top - dialogHeight) / 2;

                this->SetWindowPos(HWND_TOP, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

                ::SetForegroundWindow(this->m_hWnd);
                this->SetActiveWindow();

                if (attached)
                {
                    AttachThreadInput(dwThis, dwForegroud, FALSE);
                }

                CRegKey key;
                bool forceOnTop = true;
                if (key.Open(HKEY_CURRENT_USER, _T(SARU_KEY_ROOT)) == ERROR_SUCCESS)
                {
                    DWORD val = 0;
                    if (key.QueryDWORDValue("ForceNotificationOnTop", val) == ERROR_SUCCESS)
                    {
                        forceOnTop = (val != 0);
                    }
                }
                if (forceOnTop)
                {
                    this->SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0,
                                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                }
            }

            void loadAppIcon()
            {
                CStatic wndIcon;
                wndIcon.Attach(this->GetDlgItem(IDC_APP_ICON));
                if (m_clientInfo.SystemType == ClientInfoSystemType::Windows)
                {
                    SHFILEINFO sfi = {0};
                    if (SHGetFileInfo(m_clientInfo.ClientPath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON))
                    {
                        log.debug("Loaded icon for requesting app '{}'", m_clientInfo.ClientPath);
                        wndIcon.SetIcon(sfi.hIcon);
                    }
                    else
                    {
                        log.warning("Failed to load icon for requesting app '{}', using default icon",
                                    m_clientInfo.ClientPath);
                        HICON hDefault = LoadIcon(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDI_ICON1));
                        wndIcon.SetIcon(hDefault);
                    }
                }
                else if (m_clientInfo.SystemType == ClientInfoSystemType::Linux)
                {
                    HICON hLinuxIcon = IconFactory::getLarge(IDI_LINUX);
                    wndIcon.SetIcon(hLinuxIcon);
                }
            }

            LRESULT OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
            {
                if (wParam == IDT_TIMEOUT_CHECK)
                {
                    updateDenyButton();
                    if (m_timer-- <= 0)
                    {
                        log.vdebug("CKeyConfirm dialog timed out");
                        this->KillTimer(IDT_TIMEOUT_CHECK);

                        this->PostMessage(WM_COMMAND, MAKEWPARAM(IDCANCEL, 0), 0);
                    }
                }
                return 0;
            }

        protected:
            void updateDenyButton()
            {
                if (m_timer > 0)
                {
                    auto cancelText = std::format("Deny in ({} s)", m_timer);
                    m_cancelButton.SetWindowText(cancelText.c_str());
                }
                else
                {
                    m_cancelButton.SetWindowText("Deny");
                }
            }

            ClientInfo m_clientInfo;
            CButton m_cancelButton;
            int m_timer = 0;
        };
    }
}
