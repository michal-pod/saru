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
        using nglab::libssha::KeyBase;
        using nglab::libssha::secure_vector;
        using nglab::libssha::Session;

        class NamedPipeSession : virtual public Session,
                                 public WindowsSession<NamedPipeSession, WindowsSessionType::Pipe>,
                                 public AsyncServer,
                                 virtual public LogEnabler
        {
        public:
            NamedPipeSession(std::string sPipeName);
            ~NamedPipeSession();

            bool send(secure_vector<uint8_t> &data) override;

            bool confirmRequest(const KeyBase &key) override
            {
                return WindowsSession<NamedPipeSession, WindowsSessionType::Pipe>::confirmRequest(key);
            }

            bool requiresConfirmation(const KeyBasePtr key) const override
            {
                return WindowsSession<NamedPipeSession, WindowsSessionType::Pipe>::requiresConfirmation(key);
            }

            std::string client() const override
            {
                return WindowsSession<NamedPipeSession, WindowsSessionType::Pipe>::client();
            }

            bool processExtensionMessage(const ExtensionMessage &msg) override
            {
                return WindowsSession<NamedPipeSession, WindowsSessionType::Pipe>::processExtensionMessage(msg);
            }

            void processRequestIdentities(const nglab::libssha::Message &msg) override
            {
                WindowsSession<NamedPipeSession, WindowsSessionType::Pipe>::processRequestIdentities(msg);
            }
            bool getClientInfo();
            void initRead();
            virtual void onRead(DWORD dwBytesRead);
            virtual HANDLE hEventConnect() const;
            virtual HANDLE hEventRead() const;
            virtual HANDLE hEventWrite() const;
            virtual OVERLAPPED *ovConnect();
            virtual OVERLAPPED *ovRead();
            virtual OVERLAPPED *ovWrite();
            const std::string &clientPath() const;
            ULONG clientPid() const;
            HANDLE pipeHandle() const;
            virtual bool onConnected();
            virtual bool onRead();
            virtual bool onWritten();

            virtual char type() const override;

        private:
            HANDLE m_hPipe;
            HANDLE m_hEventConnect;
            HANDLE m_hEventRead;
            HANDLE m_hEventWrite;
            OVERLAPPED m_ovConnect;
            OVERLAPPED m_ovRead;
            OVERLAPPED m_ovWrite;
            std::string m_sClientPath;
            PSID m_sClientSid;
            std::vector<uint8_t> m_readBuffer;
        };
    }
}
