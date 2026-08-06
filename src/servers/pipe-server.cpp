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
#include "pipe-server.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <windows.h>

#include <libssha/key/key-manager.h>

#include "dialogs/ckeylist.h"
#include "hyperv-session.h"
#include "pipe-agent-session.h"
#include "system/hv.h"

namespace nglab
{
    namespace saru
    {
        using nglab::libssha::KeyManager;
        using std::chrono::steady_clock;

        PipeServer::PipeServer(const std::string &pipeName)
            : LogEnabler("PipeServer"), m_pipeName(pipeName)
        {
            m_stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        }

        void PipeServer::disconnectClient(size_t index)
        {
            if (index < m_Clients.size())
            {
                log.debug("Disconnecting client PID={}", m_Clients[index]->clientPid());
                m_Clients.erase(m_Clients.begin() + index);
            }
        }

        void PipeServer::start()
        {
            log.debug("Starting PipeServer on pipe: {}", m_pipeName);
            m_ServerThread = std::thread([this]() {
                this->run();
            });
        }

        void PipeServer::stop()
        {
            log.debug("Stopping PipeServer");
            SetEvent(m_stopEvent);
            if (m_ServerThread.joinable())
            {
                m_ServerThread.join();
            }
            CloseHandle(m_stopEvent);
        }

        void PipeServer::run()
        {
            bool createNamedPipe = true;
            if(WaitNamedPipe(m_pipeName.c_str(), 0) || GetLastError() == ERROR_SEM_TIMEOUT){
                log.error("Another instance of the agent server is already running.");
                CKeyList::instance().DisplayTrayNotification("Error", "Another instance of the agent server is already running.", NIIF_ERROR);
                createNamedPipe = false;
            }

            CRegKey key;
            bool createHyperV = false;
            if(key.Open(HKEY_CURRENT_USER, SARU_KEY_ROOT, KEY_READ) == ERROR_SUCCESS){
                DWORD val;
                if(key.QueryDWORDValue("NamedPipe", val) == ERROR_SUCCESS){
                    createNamedPipe = (val != 0);
                }
                if(key.QueryDWORDValue("HyperVIntegration", val) == ERROR_SUCCESS){
                    createHyperV = (val != 0);
                }
                if(!HVDetector::isHyperVRunning()){
                    createHyperV = false;
                    log.info("Hyper-V is not running, disabling Hyper-V agent support.");
                }
                if(!HVDetector::isIntegrationServiceInstalled()){
                    createHyperV = false;
                    log.info("Hyper-V Integration Services are not installed, disabling Hyper-V agent support.");
                }
            }
            // Create the first client to listen for connections
            if(createNamedPipe){
                m_Clients.push_back(std::make_unique<NamedPipeSession>(m_pipeName));
            }
            else {
                log.trace("Named Pipe agent support is disabled by settings.");
            }

            try {
                if(createHyperV)
                    m_Clients.push_back(std::make_unique<HyperVSession>());
                else
                    log.trace("Hyper-V agent support is disabled by settings.");
            }
            catch (const std::exception &e)
            {
                std::string errMsg = std::format("Failed to create Hyper-V session: {}. Hyper-V agent support will be disabled.", e.what());
                CKeyList::instance().DisplayTrayNotification("Error", errMsg, NIIF_ERROR);
                log.error("Failed to create Hyper-V session: {}", e.what());
            }

            auto last_maintenance_check = std::chrono::steady_clock::now();
            auto last_info_update = std::chrono::steady_clock::now() - std::chrono::seconds(60);

            while (true)
            {
                std::vector<HANDLE> vecHandles;
                vecHandles.clear();
                for (const auto &client : m_Clients)
                {
                    vecHandles.push_back(client->hEventConnect());
                    vecHandles.push_back(client->hEventRead());
                    vecHandles.push_back(client->hEventWrite());
                }

                vecHandles.push_back(m_stopEvent);
                if (vecHandles.size() > MAXIMUM_WAIT_OBJECTS)
                {
                    log.error("Handle limit exceeded: {} handles", vecHandles.size());
                    break;
                }

                DWORD dwResult = WaitForMultipleObjects(
                    static_cast<DWORD>(vecHandles.size()),
                    vecHandles.data(),
                    FALSE,
                    1000); // 1 second timeout

                if(steady_clock::now() - last_maintenance_check > std::chrono::seconds(1)){
                    KeyManager::instance().cleanupExpiredKeys();

                    last_maintenance_check = std::chrono::steady_clock::now();
                }

                if (steady_clock::now() - last_info_update > std::chrono::seconds(30)){
                    //log.info("Updating debug console title with session info");
                    auto& debug_console = DebugConsole::instance();
                    std::unordered_map<char,size_t> session_counts;
                    for(auto & client : m_Clients){
                        session_counts[client->type()]++;
                    }
                    std::string info_str = "SARU Agent Sessions: ";
                    for(auto& [type,count] : session_counts){
                        info_str += std::format("{}={}, ", type, count);
                    }
                    info_str += std::format("total={}", m_Clients.size());
                    info_str += std::format(" | Keys loaded: {}", KeyManager::instance().keyCount());
                    debug_console.setTitle(info_str);
                    last_info_update = std::chrono::steady_clock::now();
                }

                if (dwResult == WAIT_TIMEOUT)
                {
                    // Timeout occurred, continue the loop
                    continue;
                }

                if(dwResult == WAIT_OBJECT_0 + vecHandles.size() -1 )
                {
                    log.info("Stop event signaled, exiting PipeServer run loop");
                    m_Clients.clear();
                    break;
                }

                if (dwResult >= WAIT_OBJECT_0 && dwResult < WAIT_OBJECT_0 + vecHandles.size())
                {
                    size_t index = dwResult - WAIT_OBJECT_0;
                    size_t clientIndex = index / 3;
                    size_t eventType = index % 3;

                    if (clientIndex >= m_Clients.size())
                    {
                        log.error("Invalid client index: {}", clientIndex);
                        continue;
                    }

                    AsyncServer &client = *m_Clients[clientIndex];

                    if (eventType == 0)
                    {
                        log.debug("Connect event for client index {}", clientIndex);
                        if (typeid(*m_Clients[clientIndex]) == typeid(HyperVSession))
                        {
                            log.vdebug("New Hyper-V client connected, PID={}", client.clientPid());
                            HyperVSession &hvClient = static_cast<HyperVSession &>(client);
                            // WSA Event need special handling because we can have only one event
                            // per socket

                            SOCKET newSocket = INVALID_SOCKET;

                            if (hvClient.handleEvents(newSocket))
                            {
                                if (newSocket != INVALID_SOCKET)
                                {
                                    if (!hasClientCapacity())
                                    {
                                        log.warning("Client limit ({}) reached, rejecting Hyper-V connection", MaxClients);
                                        closesocket(newSocket);
                                        continue;
                                    }

                                    // Create a new HyperVSession for the new socket
                                    log.debug("Hyper-V client accepted new connection, PID={}", hvClient.clientPid());

                                    m_Clients.push_back(std::make_unique<HyperVSession>(newSocket, hvClient.VMName()));
                                    continue;
                                }
                            }
                            else
                            {
                                disconnectClient(clientIndex);
                            }
                        }
                        else if (typeid(*m_Clients[clientIndex]) == typeid(NamedPipeSession))
                        {
                            log.debug("New Named Pipe client connected, PID={}", client.clientPid());
                            if (client.onConnected())
                            {
                                if (!hasClientCapacity())
                                {
                                    log.warning("Client limit ({}) reached, rejecting named pipe connection", MaxClients);
                                    disconnectClient(clientIndex);
                                    m_Clients.push_back(std::make_unique<NamedPipeSession>(m_pipeName));
                                    continue;
                                }

                                // Create a new client for the next connection
                                m_Clients.push_back(std::make_unique<NamedPipeSession>(m_pipeName));
                                continue;
                            }
                        }
                        else
                        {
                            throw std::logic_error("Unknown client type");
                        }
                    }
                    else if (eventType == 1)
                    {
                        if (!client.onRead())
                        {
                            log.debug("Client PID={} disconnected, removing from list", client.clientPid());
                            disconnectClient(clientIndex);
                        }
                    }
                    else if (eventType == 2)
                    {
                        if (!client.onWritten())
                        {
                            log.debug("Client PID={} write failed, removing from list", client.clientPid());
                            disconnectClient(clientIndex);
                        }
                    }
                }
                else if (dwResult == WAIT_FAILED)
                {
                    DWORD error = GetLastError();
                    log.error("WaitForMultipleObjects failed with error: {}", error);
                    if (error == ERROR_INVALID_HANDLE)
                    {
                        log.error("Invalid handle detected. Rebuilding handle list...");
                        m_Clients.clear();
                        m_Clients.push_back(std::make_unique<NamedPipeSession>(m_pipeName));
                        continue; // Restart the loop to rebuild vecHandles
                    }
                    break; // Exit the loop for other critical errors
                }
            }
        }
    } // namespace saru
} // namespace nglab
