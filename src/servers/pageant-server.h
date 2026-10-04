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
#include <string>
#include <unordered_map>
#include <chrono>

#include <libssha/agent/session.h>
#include <libssha/utils/logger.h>

#include "stdatl.h"
#include "windows-session.h"
#include "dialogs/ckeylist.h"

namespace nglab
{
    namespace libssha
    {
        class KeyBase;
    }

    namespace saru
    {
        using nglab::libssha::KeyBase;
        using nglab::libssha::LogEnabler;
        using nglab::libssha::secure_vector;
        using nglab::libssha::Session;

        enum PageantSessionTimers
        {
            CleanupTimer = 1,
            SendResponseTimer = 2
        };

        class PageantSession : virtual public Session,
                               public WindowsSession<PageantSession, WindowsSessionType::Pageant>,
                               virtual public LogEnabler
        {
        public:
            PageantSession(DWORD processId) : LogEnabler("PageantSession"),
                                              WindowsSession<PageantSession, WindowsSessionType::Pageant>()
            {
                clientInfo.ClientPid = processId;

                m_async_operation = false;
                if (findBinary())
                {
                    log.debug(
                        "PageantSession created for PID={}, ConnectingPath={}, ClientPath={}",
                        clientInfo.ClientPid,
                        clientInfo.ConnectingApplicationPath.string(),
                        clientInfo.ClientPath.string());
                }
                else
                {
                    log.warning("Failed to find binary path for PID={}", clientInfo.ClientPid);
                }
            }

            virtual bool confirmRequest(const KeyBase &key)
            {
                return WindowsSession<PageantSession, WindowsSessionType::Pageant>::confirmRequest(key);
            }

            bool requiresConfirmation(KeyBasePtr key) const override
            {
                return WindowsSession<PageantSession, WindowsSessionType::Pageant>::requiresConfirmation(key);
            }

            std::string client() const override
            {
                return WindowsSession<PageantSession, WindowsSessionType::Pageant>::client();
            }

            bool processExtensionMessage(const ExtensionMessage &msg) override
            {
                // Disable extension messages for Pageant
                return false;
            }

            void processRequestIdentities(const nglab::libssha::Message &msg) override
            {
                WindowsSession<PageantSession, WindowsSessionType::Pageant>::processRequestIdentities(msg);
            }

            virtual bool send(secure_vector<uint8_t> &data)
            {
                m_response = data;
                return true;
            }

            void touch()
            {
                m_lastAccessTime = std::chrono::steady_clock::now();
            }

            bool isExpired() const
            {
                auto now = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastAccessTime);
                return duration.count() > 10; // expire after 10 seconds of inactivity
            }

            const secure_vector<uint8_t> &getResponse() const
            {
                return m_response;
            }

        private:
            secure_vector<uint8_t> m_response;
            std::chrono::steady_clock::time_point m_lastAccessTime;
        };

        class CPageantServer : public CWindowImpl<CPageantServer>, public LogEnabler
        {
        public:
            DECLARE_WND_CLASS(_T("Pageant"))

            BEGIN_MSG_MAP(CPageantServer)
            MESSAGE_HANDLER(WM_COPYDATA, OnCopyData)
            MESSAGE_HANDLER(WM_TIMER, OnTimer)
            END_MSG_MAP()

            CPageantServer() : LogEnabler("PageantServer")
            {
            }

            BOOL Start();

            void Stop();

            bool isRunning() const
            {
                return m_thread.joinable();
            }

        private:
            LRESULT OnCopyData(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);

            LRESULT OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled);

            std::unordered_map<DWORD, PageantSession> m_sessions;
            static constexpr DWORD PAGEANT_COPYDATA_ID = 0x804e50ba;
            std::thread m_thread;
        };

    }
}
