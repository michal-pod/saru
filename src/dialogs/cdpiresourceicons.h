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

#include <initializer_list>
#include <vector>

#include "stdatl.h"

namespace nglab::saru
{
    // Owns control icons; the dialog manager still owns dialog layout and fonts.
    template <typename T>
    class CDpiResourceIcons
    {
    public:
        struct ResourceIcon
        {
            int controlId;
            int resourceId;
            int size; // Logical pixels at 96 DPI.
        };

        BEGIN_MSG_MAP(CDpiResourceIcons)
        MESSAGE_HANDLER(WM_DPICHANGED, OnIconDpiChanged)
        MESSAGE_HANDLER(WM_DPICHANGED_AFTERPARENT, OnIconDpiChanged)
        MESSAGE_HANDLER(WM_REFRESH_RESOURCE_ICONS, OnRefreshResourceIcons)
        MESSAGE_HANDLER(WM_NCDESTROY, OnIconWindowDestroyed)
        END_MSG_MAP()

        CDpiResourceIcons() = default;
        CDpiResourceIcons(const CDpiResourceIcons &) = delete;
        CDpiResourceIcons &operator=(const CDpiResourceIcons &) = delete;

        ~CDpiResourceIcons()
        {
            clearResourceIcons();
        }

        void initResourceIcons(std::initializer_list<ResourceIcon> icons)
        {
            for (const auto &icon : icons)
                m_icons.push_back({icon});
            refreshResourceIcons();
        }

    private:
        enum { WM_REFRESH_RESOURCE_ICONS = WM_APP + 0x103 };

        struct LoadedIcon
        {
            ResourceIcon resource;
            HICON handle = nullptr;
            int pixelSize = 0;
        };

        LRESULT OnIconDpiChanged(UINT, WPARAM, LPARAM, BOOL &handled)
        {
            // Let PMv2 finish scaling the dialog before replacing its images.
            static_cast<T *>(this)->PostMessage(WM_REFRESH_RESOURCE_ICONS);
            handled = FALSE;
            return 0;
        }

        LRESULT OnRefreshResourceIcons(UINT, WPARAM, LPARAM, BOOL &)
        {
            refreshResourceIcons();
            return 0;
        }

        LRESULT OnIconWindowDestroyed(UINT, WPARAM, LPARAM, BOOL &handled)
        {
            clearResourceIcons();
            handled = FALSE;
            return 0;
        }

        void refreshResourceIcons()
        {
            auto *window = static_cast<T *>(this);
            const UINT dpi = GetDpiForWindow(window->m_hWnd);
            for (auto &icon : m_icons)
            {
                CWindow control = window->GetDlgItem(icon.resource.controlId);
                const int size = MulDiv(icon.resource.size, dpi, USER_DEFAULT_SCREEN_DPI);
                if (!control.IsWindow() || size <= 0)
                    continue;

                TCHAR className[32]{};
                GetClassName(control, className, _countof(className));
                const bool button = lstrcmpi(className, _T("Button")) == 0;
                const bool image = lstrcmpi(className, _T("Static")) == 0;
                if (!button && !image)
                    continue;

                HICON replacement = icon.handle;
                if (icon.pixelSize != size)
                    replacement = static_cast<HICON>(LoadImage(
                        _Module.GetResourceInstance(), MAKEINTRESOURCE(icon.resource.resourceId),
                        IMAGE_ICON, size, size, LR_DEFAULTCOLOR));
                if (!replacement)
                    continue;

                if (image)
                {
                    // Plain SS_ICON otherwise uses the system's large icon size.
                    control.ModifyStyle(SS_TYPEMASK | SS_CENTERIMAGE | SS_REALSIZECONTROL,
                                        SS_ICON | SS_REALSIZEIMAGE);
                    control.SendMessage(STM_SETIMAGE, IMAGE_ICON, reinterpret_cast<LPARAM>(replacement));
                    control.SetWindowPos(nullptr, 0, 0, size, size,
                                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
                }
                else
                    control.SendMessage(BM_SETIMAGE, IMAGE_ICON, reinterpret_cast<LPARAM>(replacement));

                if (icon.handle && icon.handle != replacement)
                    DestroyIcon(icon.handle);
                icon.handle = replacement;
                icon.pixelSize = size;
                control.Invalidate();
            }
        }

        void clearResourceIcons()
        {
            for (const auto &icon : m_icons)
                if (icon.handle)
                    DestroyIcon(icon.handle);
            m_icons.clear();
        }

        std::vector<LoadedIcon> m_icons;
    };
}
