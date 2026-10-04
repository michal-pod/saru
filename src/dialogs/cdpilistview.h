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

#include <algorithm>
#include <initializer_list>
#include <vector>

#include "stdatl.h"

namespace nglab::saru
{
    // The dialog manager owns control geometry and fonts. This subclass only
    // lays out columns after the list itself has processed a resize/DPI change.
    class CDpiListView : public CWindowImpl<CDpiListView, CListViewCtrl>
    {
    public:
        BEGIN_MSG_MAP(CDpiListView)
        MESSAGE_HANDLER(WM_SIZE, OnLayoutChanged)
        MESSAGE_HANDLER(WM_DPICHANGED_AFTERPARENT, OnLayoutChanged)
        MESSAGE_HANDLER(WM_REFRESH_COLUMNS, OnRefreshColumns)
        END_MSG_MAP()

        // Positive widths are logical pixels at 96 DPI; zero-width columns
        // share the remaining client width equally.
        void setColumnLayout(std::initializer_list<int> widths)
        {
            m_widths = widths;
            updateColumns();
        }

    private:
        enum { WM_REFRESH_COLUMNS = WM_APP + 0x102 };

        LRESULT OnLayoutChanged(UINT message, WPARAM wParam, LPARAM lParam, BOOL &)
        {
            const LRESULT result = DefWindowProc(message, wParam, lParam);
            updateColumns();
            if (message == WM_DPICHANGED_AFTERPARENT)
                PostMessage(WM_REFRESH_COLUMNS);
            return result;
        }

        LRESULT OnRefreshColumns(UINT, WPARAM, LPARAM, BOOL &)
        {
            // The common control finishes its DPI bookkeeping after the
            // parent notification. Recalculate the scroll range afterwards.
            updateColumns(true);
            return 0;
        }

        void updateColumns(bool refreshScrollRange = false)
        {
            if (m_widths.empty() || m_updating)
                return;

            // Changing columns can change scroll bars and send WM_SIZE again.
            m_updating = true;
            const UINT dpi = GetDpiForWindow(m_hWnd);
            int fixedWidth = 0;
            int flexibleCount = 0;
            for (const int width : m_widths)
            {
                if (width > 0)
                    fixedWidth += MulDiv(width, dpi, USER_DEFAULT_SCREEN_DPI);
                else
                    ++flexibleCount;
            }

            RECT client{};
            GetClientRect(&client);
            int remaining = std::max(0L, client.right - client.left - fixedWidth);
            const int minimumWidth = MulDiv(48, dpi, USER_DEFAULT_SCREEN_DPI);
            for (size_t index = 0; index < m_widths.size(); ++index)
            {
                int width = m_widths[index];
                if (width > 0)
                {
                    width = MulDiv(width, dpi, USER_DEFAULT_SCREEN_DPI);
                }
                else
                {
                    const int share = remaining / flexibleCount;
                    width = std::max(minimumWidth, share);
                    remaining -= share;
                    --flexibleCount;
                }
                if (refreshScrollRange && index + 1 == m_widths.size())
                {
                    // Setting the same width does not invalidate the control's
                    // cached total. Shrink temporarily to force recalculation
                    // without introducing an overflowing column.
                    SetColumnWidth(static_cast<int>(index), width - 1);
                }
                if (refreshScrollRange || GetColumnWidth(static_cast<int>(index)) != width)
                    SetColumnWidth(static_cast<int>(index), width);
            }
            m_updating = false;
        }

        std::vector<int> m_widths;
        bool m_updating = false;
    };
}
