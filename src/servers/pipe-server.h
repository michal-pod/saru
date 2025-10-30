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
#include <memory>
#include <string>
#include <vector>
#include <thread>

#include <libssha/utils/logger.h>

#include "stdatl.h"
#include "async-server.h"
#include "pipe-agent-session.h"

namespace nglab
{
    namespace skym
    {
        using nglab::libssha::LogEnabler;
        class PipeServer : public LogEnabler
        {
        public:
            PipeServer(const std::string &pipeName);
            void disconnectClient(size_t index);
            void run();
            void start();
            void stop();

        private:
            std::string m_pipeName;
            std::vector<std::unique_ptr<AsyncServer>> m_Clients;
            std::thread m_ServerThread;
            HANDLE m_stopEvent;
        };
    } // namespace skym
} // namespace nglab
