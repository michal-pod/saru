/*
 SKYM - SSH KeY Manager
 Copyright (C) 2025 Michał Podsiadlik

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
#include <vector>
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

    namespace skym
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

            ULONG clientPid() const;

            char type() const override;

        private:
            SOCKET m_hvSocket;
            WSAEVENT m_hEventConnect;
            HANDLE m_hEventRead;
            HANDLE m_hEventWrite;
            bool m_isServerSocket{true};
        };

    }
}
