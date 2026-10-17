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
#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include <winsock2.h>
#include <windows.h>

#include <libssha/utils/secure_vector.h>
#include <libssha/agent/session.h>

#include "windows-session.h"
#include "async-server.h"

namespace nglab
{
    namespace libssha
    {
        class KeyBase;
    }

    namespace saru
    {
        using nglab::libssha::secure_vector;
        using nglab::libssha::Session;
        class HyperVSession : virtual public Session,
                              public WindowsSession<HyperVSession, WindowsSessionType::HyperV>,
                              public AsyncServer,
                              virtual public LogEnabler
        {
        public:
            HyperVSession();
            HyperVSession(SOCKET hvSocket, std::string vmName);

            ~HyperVSession();

            bool confirmRequest(const KeyBase &key) override
            {
                return WindowsSession<HyperVSession, WindowsSessionType::HyperV>::confirmRequest(key);
            }

            bool requiresConfirmation(KeyBasePtr key) const override
            {
                return WindowsSession<HyperVSession, WindowsSessionType::HyperV>::requiresConfirmation(key);
            }

            std::string client() const override
            {
                return WindowsSession<HyperVSession, WindowsSessionType::HyperV>::client();
            }

            bool processExtensionMessage(const ExtensionMessage &msg) override
            {
                return WindowsSession<HyperVSession, WindowsSessionType::HyperV>::processExtensionMessage(msg);
            }

            void processRequestIdentities(const nglab::libssha::Message &msg) override
            {
                WindowsSession<HyperVSession, WindowsSessionType::HyperV>::processRequestIdentities(msg);
            }

            bool send(secure_vector<uint8_t> &data) override;

            virtual bool onConnected();
            virtual bool onRead();
            virtual bool onWritten();

            virtual HANDLE hEventConnect() const;
            virtual HANDLE hEventRead() const;
            virtual HANDLE hEventWrite() const;

            bool handleEvents(SOCKET &newSocket);

            bool isListener() const { return m_isServerSocket; }

            ULONG clientPid() const;

            char type() const override;

            void cancelPendingOperations() override
            {
                stopProcessing();
                WindowsSession<HyperVSession, WindowsSessionType::HyperV>::cancelPendingOperations();
            }

            bool hasPendingOperations() const override
            {
                return Session::hasPendingOperations();
            }

        private:
            bool acceptClient(SOCKET &newSocket);
            bool readData();
            bool flushWrites();

            SOCKET m_hvSocket;
            WSAEVENT m_hEventConnect{nullptr};
            HANDLE m_hEventRead{nullptr};
            HANDLE m_hEventWrite{nullptr};
            bool m_isServerSocket{true};
            std::vector<uint8_t> m_readBuffer;
            // The socket is non-blocking, replies that do not fit are sent on FD_WRITE
            std::mutex m_writeMutex;
            std::deque<secure_vector<uint8_t>> m_writeQueue;
            size_t m_writeOffset{0};
        };

    }
}
