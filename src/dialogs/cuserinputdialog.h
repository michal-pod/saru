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
#include <shlobj.h>

#include "stdatl.h"
#include "config.h"
#include "resources/resource.h"
#include "servers/client-info.h"
#include "cdpiresourceicons.h"

namespace nglab
{
    namespace saru
    {
        using namespace ATL;
        using nglab::libssha::LogEnabler;

        enum WindowsSessionType : uint8_t;

        template <typename T>
        class CUserInputDialog : public CDialogImpl<T>, public CDpiResourceIcons<T>, virtual public LogEnabler
        {
        public:
            enum
            {
                IDT_TIMEOUT_CHECK = 1

            };

            CUserInputDialog(const ClientInfo &clientInfo)
                : LogEnabler("CUserInputDialog"), m_clientInfo(clientInfo)
            {
            }

            BEGIN_MSG_MAP(CUserInputDialog)
            MESSAGE_HANDLER(WM_DPICHANGED, OnAppIconDpiChanged)
            MESSAGE_HANDLER(WM_DPICHANGED_AFTERPARENT, OnAppIconDpiChanged)
            MESSAGE_HANDLER(WM_REFRESH_APP_ICON, OnRefreshAppIcon)
            MESSAGE_HANDLER(WM_NCDESTROY, OnAppIconWindowDestroyed)
            CHAIN_MSG_MAP(CDpiResourceIcons<T>)
            END_MSG_MAP()

            void initTimeout()
            {
                m_cancelButton.Attach(this->GetDlgItem(T::CancelButtonId));
                this->initResourceIcons({
                    {T::PrimaryButtonId, IDI_ACCEPT, 16},
                    {T::CancelButtonId, IDI_CANCEL, 16},
                });

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
                CStatic wndIcon(this->GetDlgItem(IDC_APP_ICON));
                if (!wndIcon.IsWindow())
                    return;

                const int size = MulDiv(32, GetDpiForWindow(this->m_hWnd), USER_DEFAULT_SCREEN_DPI);
                if (size != m_appIconSize)
                {
                    CIcon replacement;
                    if (m_clientInfo.SystemType == ClientInfoSystemType::Windows)
                    {
                        HICON extracted = nullptr;
                        const HRESULT result = SHDefExtractIconW(
                            m_clientInfo.ClientPath.c_str(), 0, 0, &extracted, nullptr, MAKELONG(size, 0));
                        replacement.Attach(extracted);
                        if (result != S_OK || replacement.IsNull())
                            log.warning("Failed to load icon for requesting app '{}', using default icon",
                                        m_clientInfo.ClientPath.string());

                        if (replacement.IsNull())
                        {
                            // Use Explorer's default EXE icon, extracted at the target size.
                            SHSTOCKICONINFO info{};
                            info.cbSize = sizeof(info);
                            if (SUCCEEDED(SHGetStockIconInfo(SIID_APPLICATION, SHGSI_ICONLOCATION, &info)))
                            {
                                HICON defaultIcon = nullptr;
                                SHDefExtractIconW(info.szPath, info.iIcon, 0,
                                                  &defaultIcon, nullptr, MAKELONG(size, 0));
                                replacement.Attach(defaultIcon);
                            }
                        }
                    }

                    if (replacement.IsNull())
                    {
                        const int resourceId = m_clientInfo.SystemType == ClientInfoSystemType::Linux
                            ? IDI_LINUX : IDI_ICON1;
                        replacement.Attach(static_cast<HICON>(LoadImage(
                            _Module.GetResourceInstance(), MAKEINTRESOURCE(resourceId),
                            IMAGE_ICON, size, size, LR_DEFAULTCOLOR)));
                    }
                    if (replacement.IsNull())
                        return;

                    wndIcon.SetIcon(replacement);
                    m_appIcon.Attach(replacement.Detach());
                    m_appIconSize = size;
                }

                // PMv2 may restore the zero dimensions from the dialog template.
                wndIcon.ModifyStyle(SS_TYPEMASK | SS_CENTERIMAGE | SS_REALSIZECONTROL,
                                    SS_ICON | SS_REALSIZEIMAGE);
                wndIcon.SetIcon(m_appIcon);
                wndIcon.SetWindowPos(nullptr, 0, 0, size, size,
                                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
                wndIcon.Invalidate();
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

        private:
            enum { WM_REFRESH_APP_ICON = WM_APP + 0x104 };

            LRESULT OnAppIconDpiChanged(UINT, WPARAM, LPARAM, BOOL &handled)
            {
                // Keep the native dialog scaling, then update the image.
                this->PostMessage(WM_REFRESH_APP_ICON);
                handled = FALSE;
                return 0;
            }

            LRESULT OnRefreshAppIcon(UINT, WPARAM, LPARAM, BOOL &)
            {
                loadAppIcon();
                return 0;
            }

            LRESULT OnAppIconWindowDestroyed(UINT, WPARAM, LPARAM, BOOL &handled)
            {
                if (!m_appIcon.IsNull())
                    m_appIcon.DestroyIcon();
                m_appIconSize = 0;
                handled = FALSE;
                return 0;
            }

            CIcon m_appIcon;
            int m_appIconSize = 0;
        };
    }
}
