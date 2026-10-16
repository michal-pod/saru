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
#include <atomic>
#include <windows.h>

namespace nglab
{
    namespace saru
    {
        // Owned by a session, used by CUserInputDialog shown from the session's
        // worker thread. The dialog object lives on that thread's stack, so other
        // threads only ever touch its HWND, never the object itself.
        class UserInputCancellation
        {
        public:
            // Thread safe. Closes the dialog if it is shown, otherwise makes sure
            // no dialog will be shown anymore.
            void cancel()
            {
                m_cancelled = true;
                closeDialog(m_hWnd.load());
            }

            bool isCancelled() const { return m_cancelled; }

            void attach(HWND hWnd)
            {
                m_hWnd = hWnd;
                // cancel() may have run before the window existed
                if (m_cancelled)
                {
                    closeDialog(hWnd);
                }
            }

            void detach() { m_hWnd = nullptr; }

        private:
            static void closeDialog(HWND hWnd)
            {
                if (hWnd)
                {
                    ::PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(IDCANCEL, 0), 0);
                }
            }

            std::atomic<HWND> m_hWnd{nullptr};
            std::atomic_bool m_cancelled{false};
        };
    }
}
