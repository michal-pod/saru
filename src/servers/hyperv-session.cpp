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
#include "hyperv-session.h"

#include <atlbase.h>
#include <stdexcept>
#include <hvsocket.h>

#include "config.h"
#include "utils/string-converter.h"
namespace nglab
{
    namespace saru
    {
        HyperVSession::HyperVSession()
            : LogEnabler("HyperVSession"), m_hvSocket(INVALID_SOCKET), m_isServerSocket(true)
        {
            log.debug("HyperVSession server created");
            // Initialization code for Hyper-V session can be added here

// Setup as listener
#if 1
            m_hvSocket = WSASocketW(AF_HYPERV, SOCK_STREAM, HV_PROTOCOL_RAW, NULL, 0, WSA_FLAG_OVERLAPPED);
            if (m_hvSocket == INVALID_SOCKET)
            {
                log.error("Failed to create Hyper-V socket, error: {}", WSAGetLastError());
                throw std::runtime_error("Failed to create Hyper-V socket");
            }
            SOCKADDR_HV serverAddr;
            memset(&serverAddr, 0, sizeof(serverAddr));
            serverAddr.Family = AF_HYPERV;
            serverAddr.VmId = HV_GUID_WILDCARD;
            serverAddr.ServiceId = {0x00002E70, 0xFACB, 0x11E6, {0xBD, 0x58, 0x64, 0x00, 0x6A, 0x79, 0x86, 0xD3}};
            if (bind(m_hvSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
            {
                log.error("Failed to bind Hyper-V socket, error: {}", WSAGetLastError());
                // The destructor does not run when the constructor throws
                closesocket(m_hvSocket);
                throw std::runtime_error("Failed to bind Hyper-V socket");
            }
#else
            m_hvSocket = socket(AF_INET, SOCK_STREAM, 0);
            if (m_hvSocket == INVALID_SOCKET)
            {
                log.error("Failed to create IPv4 socket, error: {}", WSAGetLastError());
                throw std::runtime_error("Failed to create IPv4 socket");
            }
            sockaddr_in serverAddr{};
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
            serverAddr.sin_port = htons(12345); // Example port
            if (bind(m_hvSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
            {
                log.error("Failed to bind IPv4 socket, error: {}", WSAGetLastError());
                throw std::runtime_error("Failed to bind IPv4 socket");
            }
#endif

            if (listen(m_hvSocket, 5) == SOCKET_ERROR)
            {
                log.error("Failed to listen on Hyper-V socket, error: {}", WSAGetLastError());
                closesocket(m_hvSocket);
                throw std::runtime_error("Failed to listen on Hyper-V socket");
            }

            m_hEventConnect = WSACreateEvent();
            m_hEventRead = CreateEvent(NULL, FALSE, FALSE, NULL);
            m_hEventWrite = CreateEvent(NULL, FALSE, FALSE, NULL);

            WSAEventSelect(m_hvSocket, m_hEventConnect, FD_ACCEPT);

            log.debug("HyperVSession listener initialized, socket {}", m_hvSocket);
        }

        HyperVSession::HyperVSession(SOCKET hvSocket, std::string vmName)
            : LogEnabler("HyperVSession"), m_hvSocket(hvSocket), m_isServerSocket(false)
        {
            m_hEventConnect = WSACreateEvent();
            m_hEventRead = CreateEvent(NULL, FALSE, FALSE, NULL);
            m_hEventWrite = CreateEvent(NULL, FALSE, FALSE, NULL);

            clientInfo.VMName = vmName;

            WSAEventSelect(m_hvSocket, m_hEventConnect, FD_READ | FD_WRITE | FD_CLOSE);

            log.debug("HyperVSession client initialized, socket {}", m_hvSocket);
        }

        HyperVSession::~HyperVSession()
        {
            // A worker thread may still use this object and the socket, see Session::joinWorkers
            cancelPendingOperations();
            joinWorkers();

            if (m_hvSocket != INVALID_SOCKET)
            {
                closesocket(m_hvSocket);
            }
            if (m_hEventConnect)
                WSACloseEvent(m_hEventConnect);
            if (m_hEventRead)
                CloseHandle(m_hEventRead);
            if (m_hEventWrite)
                CloseHandle(m_hEventWrite);

            log.debug("HyperVSession destroyed");
        }

        bool HyperVSession::send(secure_vector<uint8_t> &data)
        {
            if (m_isServerSocket)
            {
                log.error("Attempt to send data on server socket, socket {}", m_hvSocket);
                throw std::logic_error("Attempt to send data on server socket");
            }

            std::lock_guard<std::mutex> lock(m_writeMutex);
            m_writeQueue.push_back(data);
            memset(data.data(), 0x42, data.size());
            return flushWrites();
        }

        // Caller must hold m_writeMutex
        bool HyperVSession::flushWrites()
        {
            while (!m_writeQueue.empty())
            {
                auto &data = m_writeQueue.front();
                const int bytesSent = ::send(m_hvSocket, reinterpret_cast<const char *>(data.data() + m_writeOffset),
                                             static_cast<int>(data.size() - m_writeOffset), 0);
                if (bytesSent == SOCKET_ERROR)
                {
                    const int error = WSAGetLastError();
                    if (error == WSAEWOULDBLOCK)
                    {
                        // FD_WRITE is signaled once the socket accepts data again
                        return true;
                    }

                    log.error("Failed to send data to Hyper-V client, error: {}", error);
                    m_writeQueue.clear();
                    m_writeOffset = 0;
                    return false;
                }

                m_writeOffset += static_cast<size_t>(bytesSent);
                if (m_writeOffset == data.size())
                {
                    memset(data.data(), 0x42, data.size());
                    m_writeQueue.pop_front();
                    m_writeOffset = 0;
                }
            }
            return true;
        }

        HANDLE HyperVSession::hEventConnect() const { return m_hEventConnect; }
        HANDLE HyperVSession::hEventRead() const { return m_hEventRead; }
        HANDLE HyperVSession::hEventWrite() const { return m_hEventWrite; }

        ULONG HyperVSession::clientPid() const
        {
            return clientInfo.ClientPid;
        }

        bool HyperVSession::onConnected()
        {
            return true;
        }

        bool HyperVSession::onRead()
        {
            return true;
        }

        bool HyperVSession::onWritten()
        {
            return true;
        }

        bool HyperVSession::handleEvents(SOCKET &newSocket)
        {
            WSANETWORKEVENTS ne{};
            if (WSAEnumNetworkEvents(m_hvSocket, m_hEventConnect, &ne) == SOCKET_ERROR)
            {
                log.error("WSAEnumNetworkEvents failed for Hyper-V socket, error: {}", WSAGetLastError());
                return false;
            }

            if (ne.lNetworkEvents & FD_ACCEPT)
            {
                return acceptClient(newSocket);
            }

            // All recorded events are reset at once, so each of them must be handled here
            bool keepConnection = true;
            if (ne.lNetworkEvents & FD_READ)
            {
                keepConnection = readData();
            }
            if (keepConnection && (ne.lNetworkEvents & FD_WRITE))
            {
                log.vdebug("FD_WRITE event for Hyper-V client");
                std::lock_guard<std::mutex> lock(m_writeMutex);
                keepConnection = flushWrites();
            }
            if (ne.lNetworkEvents & FD_CLOSE)
            {
                log.debug("FD_CLOSE event for Hyper-V client");
                keepConnection = false;
            }
            return keepConnection;
        }

        bool HyperVSession::acceptClient(SOCKET &newSocket)
        {
            SOCKADDR_HV clientAddr;
            int addrLen = sizeof(clientAddr);
            SOCKET clientSocket = accept(m_hvSocket, reinterpret_cast<SOCKADDR *>(&clientAddr), &addrLen);
            // WSAResetEvent(m_hEventConnect);
            if (clientSocket == INVALID_SOCKET)
            {
                const int error = WSAGetLastError();
                // The client gave up before it was accepted, the listener is fine
                if (error == WSAEWOULDBLOCK || error == WSAECONNRESET)
                {
                    log.warning("Hyper-V connection was not accepted, error: {}", error);
                    return true;
                }
                log.error("Failed to accept Hyper-V connection, error: {}", error);
                return false;
            }

            auto guidStr = StringConverter::fromGUIDA(clientAddr.VmId);

            CRegKey vmKey;
            std::string subpath = std::string(SARU_KEY_ROOT "\\HyperVVMs\\") + guidStr;
            if (vmKey.Open(HKEY_CURRENT_USER, subpath.c_str(), KEY_READ) == ERROR_SUCCESS)
            {
                DWORD enabled = 0;
                if (vmKey.QueryDWORDValue("Enabled", enabled) != ERROR_SUCCESS || enabled == 0)
                {
                    log.warning("Connection from Hyper-V VM {} denied: not enabled in settings", guidStr);
                    closesocket(clientSocket);
                    return true;
                }

                CHAR vmName[256];
                DWORD vmNameSize = sizeof(vmName);
                if(vmKey.QueryStringValue("Description", vmName, &vmNameSize) == ERROR_SUCCESS){
                    log.debug("Setting client info name to {}", vmName);
                    clientInfo.VMName = vmName;
                }
                else
                {
                    clientInfo.VMName = guidStr;
                }
            }
            else
            {
                log.warning("Connection from Hyper-V VM {} denied: no settings found", guidStr);
                closesocket(clientSocket);
                return true;
            }

            log.debug("Accepted new Hyper-V connection from VM {}", guidStr);

            newSocket = clientSocket;
            return true;
        }

        bool HyperVSession::readData()
        {
            log.vdebug("FD_READ event for Hyper-V client");
            // Not on the stack of the server thread
            m_readBuffer.resize(256 * 1024);
            const int bytesRead = recv(m_hvSocket, reinterpret_cast<char *>(m_readBuffer.data()), static_cast<int>(m_readBuffer.size()), 0);
            if (bytesRead > 0)
            {
                log.vdebug("Received {} bytes from Hyper-V client", bytesRead);
                const bool processed = process(m_readBuffer.data(), static_cast<size_t>(bytesRead));
                memset(m_readBuffer.data(), 0x42, static_cast<size_t>(bytesRead));
                if (!processed)
                {
                    log.error("Closing Hyper-V connection after a protocol error");
                    return false;
                }
                return true;
            }
            if (bytesRead == 0)
            {
                log.debug("Hyper-V client disconnected");
                return false;
            }

            const int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK)
            {
                return true;
            }
            log.error("Failed to read from Hyper-V client, error: {}", error);
            return false;
        }

        char HyperVSession::type() const
        {
            return m_isServerSocket ? 'H' : 'h';
        }

    }
}
