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
#include <windows.h>
#include <string>

#include <libssha/utils/logger.h>
namespace nglab
{
    namespace skym
    {
        using nglab::libssha::LogEnabler;
        class DebugConsole : public LogEnabler
        {
        public:        
            ~DebugConsole();

            static DebugConsole& instance()
            {
                static DebugConsole instance;
                return instance;
            }

            void show()
            {
                if (m_hwndConsole)
                {
                    ::ShowWindow(m_hwndConsole, SW_SHOW);
                }
            }

            void hide()
            {
                if (m_hwndConsole)
                {
                    ::ShowWindow(m_hwndConsole, SW_HIDE);
                }
            }

            bool isVisible() const
            {
                if (m_hwndConsole)
                {
                    return ::IsWindowVisible(m_hwndConsole) != FALSE;
                }
                return false;
            }

            void toggleVisibility()
            {
                if (isVisible())
                {
                    hide();
                }
                else
                {
                    show();
                }
            }

            void setTitle(const std::string &title)
            {
                if (m_hwndConsole)
                {
                    std::string newTitle = title + " | hide from tray icon";
                    ::SetWindowText(m_hwndConsole, newTitle.c_str());
                }
            }

            bool isEnabled() const
            {
                return m_enabled;
            }

        private:
            DebugConsole();
            HWND m_hwndConsole{ nullptr };
            bool m_enabled{ false };

        };
    }
}
