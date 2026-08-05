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
#include "pipe-agent-session.h"

#include <format>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>

#include <libssha/key/key.h>

#include "dialogs/ckeyconfirm.h"
#include "system/csid.h"

// ...existing code for logging and secure_vector...
namespace nglab
{
    namespace skym
    {

        NamedPipeSession::NamedPipeSession(std::string sPipeName) : LogEnabler("NamedPipeSession")
        {
            // Build a security descriptor that allows only the current user to access the pipe.
            SECURITY_ATTRIBUTES sa{};
            SECURITY_DESCRIPTOR sd{};
            PSID pSidCopy = nullptr;
            PACL pAcl = nullptr;

            // Use CSID helper to obtain the current user's SID
            PSID pUserSid = nullptr;
 
            try
            {
            CSID                userSid = CSID::getUser();
            pUserSid = userSid.psid();
            }
            catch (const std::exception &e)
            {
                log.error("CSID::getUser failed: {}", e.what());
                throw;
            }

            if (!pUserSid)
            {
                log.error("CSID returned null SID");
                throw std::runtime_error("CSID returned null SID");
            }

            DWORD sidLen = GetLengthSid(pUserSid);
            pSidCopy = reinterpret_cast<PSID>(LocalAlloc(LPTR, sidLen));
            if (!pSidCopy || !CopySid(sidLen, pSidCopy, pUserSid))
            {
                DWORD err = GetLastError();
                log.error("CopySid failed: {}", err);
                if (pSidCopy) LocalFree(pSidCopy);
                throw std::runtime_error(std::format("CopySid failed: {}", err));
            }

            // Create an ACL with a single ACE granting the user GENERIC_READ | GENERIC_WRITE
            DWORD aclSize = sizeof(ACL) + (sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD)) + sidLen;
            pAcl = reinterpret_cast<PACL>(LocalAlloc(LPTR, aclSize));
            if (!pAcl || !InitializeAcl(pAcl, aclSize, ACL_REVISION) ||
                !AddAccessAllowedAce(pAcl, ACL_REVISION, GENERIC_READ | GENERIC_WRITE, pSidCopy))
            {
                DWORD err = GetLastError();
                log.error("Failed to create ACL: {}", err);
                if (pAcl) LocalFree(pAcl);
                LocalFree(pSidCopy);
                throw std::runtime_error(std::format("Failed to create ACL: {}", err));
            }

            // Initialize security descriptor and set the DACL
            if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION) ||
                !SetSecurityDescriptorDacl(&sd, TRUE, pAcl, FALSE))
            {
                DWORD err = GetLastError();
                log.error("Failed to initialize security descriptor: {}", err);
                LocalFree(pAcl);
                LocalFree(pSidCopy);
                throw std::runtime_error(std::format("Failed to initialize security descriptor: {}", err));
            }

            sa.nLength = sizeof(sa);
            sa.bInheritHandle = FALSE;
            sa.lpSecurityDescriptor = &sd;

            // After CreateNamedPipeA returns we can free our temporary allocations.
            // (OS makes its own copy of the security descriptor at creation time.)
            m_hPipe = CreateNamedPipeA(
                sPipeName.c_str(),
                PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_REJECT_REMOTE_CLIENTS | PIPE_WAIT,
                PIPE_UNLIMITED_INSTANCES,
                512 * 1024,
                512 * 1024,
                0,
                &sa);

            LocalFree(pAcl);
            LocalFree(pSidCopy);

            if (m_hPipe == INVALID_HANDLE_VALUE)
            {
                log.error("CreateNamedPipe failed");
                throw std::runtime_error("CreateNamedPipe failed");
            }

            m_hEventConnect = CreateEventA(nullptr, TRUE, FALSE, nullptr);
            m_hEventRead = CreateEventA(nullptr, TRUE, FALSE, nullptr);
            m_hEventWrite = CreateEventA(nullptr, TRUE, FALSE, nullptr);

            if (!m_hEventConnect || !m_hEventRead || !m_hEventWrite)
            {
                log.error("Failed to create named pipe events: {}", GetLastError());
                if (m_hEventConnect) CloseHandle(m_hEventConnect);
                if (m_hEventRead) CloseHandle(m_hEventRead);
                if (m_hEventWrite) CloseHandle(m_hEventWrite);
                CloseHandle(m_hPipe);
                m_hPipe = INVALID_HANDLE_VALUE;
                throw std::runtime_error("Failed to create named pipe events");
            }

            ZeroMemory(&m_ovConnect, sizeof(OVERLAPPED));
            ZeroMemory(&m_ovRead, sizeof(OVERLAPPED));
            ZeroMemory(&m_ovWrite, sizeof(OVERLAPPED));
            m_ovConnect.hEvent = m_hEventConnect;

            BOOL bConnected = ConnectNamedPipe(m_hPipe, &m_ovConnect);
            DWORD dwError = GetLastError();
            if (!bConnected && dwError == ERROR_IO_PENDING)
            {
                log.debug("Waiting for client to connect...");
            }
            else if (!bConnected && dwError == ERROR_PIPE_CONNECTED)
            {
                SetEvent(m_hEventConnect);
            }
            else if (!bConnected)
            {
                log.error("ConnectNamedPipe failed: {}", dwError);
                CloseHandle(m_hEventConnect);
                CloseHandle(m_hEventRead);
                CloseHandle(m_hEventWrite);
                CloseHandle(m_hPipe);
                m_hEventConnect = nullptr;
                m_hEventRead = nullptr;
                m_hEventWrite = nullptr;
                m_hPipe = INVALID_HANDLE_VALUE;
                throw std::runtime_error(std::format("ConnectNamedPipe failed: {}", dwError));
            }
        }

        NamedPipeSession::~NamedPipeSession()
        {
            if (m_hPipe && m_hPipe != INVALID_HANDLE_VALUE)
            {
                FlushFileBuffers(m_hPipe);
                DisconnectNamedPipe(m_hPipe);
                CloseHandle(m_hPipe);
            }

            if (m_hEventConnect)
                CloseHandle(m_hEventConnect);
            if (m_hEventRead)
                CloseHandle(m_hEventRead);
            if (m_hEventWrite)
                CloseHandle(m_hEventWrite);

            log.debug("NamedPipeSession destroyed");
        }

        bool NamedPipeSession::send(secure_vector<uint8_t> &data)
        {
            std::lock_guard<std::mutex> lock(m_writeMutex);
            if (m_writePending)
            {
                log.error("Cannot send a response while another write is pending");
                return false;
            }

            m_writeBuffer = data;
            memset(data.data(), 0x42, data.size());

            DWORD bytesWritten = 0;
            ResetEvent(m_hEventWrite);
            ZeroMemory(&m_ovWrite, sizeof(OVERLAPPED));
            m_ovWrite.hEvent = m_hEventWrite;
            BOOL bResult = WriteFile(m_hPipe, m_writeBuffer.data(), static_cast<DWORD>(m_writeBuffer.size()), &bytesWritten, &m_ovWrite);
            if (bResult)
            {
                if (bytesWritten != m_writeBuffer.size())
                {
                    log.error("Incomplete synchronous write: {} of {} bytes", bytesWritten, m_writeBuffer.size());
                    memset(m_writeBuffer.data(), 0x42, m_writeBuffer.size());
                    m_writeBuffer.clear();
                    return false;
                }

                memset(m_writeBuffer.data(), 0x42, m_writeBuffer.size());
                m_writeBuffer.clear();
                ResetEvent(m_hEventWrite);
                return true;
            }

            if (GetLastError() != ERROR_IO_PENDING)
            {
                log.error("Failed to write to pipe");
                memset(m_writeBuffer.data(), 0x42, m_writeBuffer.size());
                m_writeBuffer.clear();
                return false;
            }

            m_writePending = true;
            return true;
        }

        bool NamedPipeSession::getClientInfo()
        {
            ULONG ulPid = 0;
            if (!GetNamedPipeClientProcessId(m_hPipe, &ulPid))
            {
                log.error("GetNamedPipeClientProcessId failed");
                return false;
            }
            clientInfo.ClientPid = ulPid;

            return findBinary();
        }

        void NamedPipeSession::initRead()
        {
            m_readBuffer.resize(256 * 1024);
            ResetEvent(m_hEventRead);
            ZeroMemory(&m_ovRead, sizeof(OVERLAPPED));
            m_ovRead.hEvent = m_hEventRead;
            DWORD dwBytesRead = 0;
            if (!ReadFile(m_hPipe, m_readBuffer.data(), static_cast<DWORD>(m_readBuffer.size()), &dwBytesRead, &m_ovRead))
            {
                if (GetLastError() != ERROR_IO_PENDING)
                {
                    log.error("ReadFile failed");
                }
            }
        }

        void NamedPipeSession::onRead(DWORD dwBytesRead)
        {
            process(m_readBuffer.data(), dwBytesRead);
            memset(m_readBuffer.data(), 0x42, dwBytesRead);
            initRead();
        }

        HANDLE NamedPipeSession::hEventConnect() const { return m_hEventConnect; }
        HANDLE NamedPipeSession::hEventRead() const { return m_hEventRead; }
        HANDLE NamedPipeSession::hEventWrite() const { return m_hEventWrite; }

        OVERLAPPED *NamedPipeSession::ovConnect() { return &m_ovConnect; }
        OVERLAPPED *NamedPipeSession::ovRead() { return &m_ovRead; }
        OVERLAPPED *NamedPipeSession::ovWrite() { return &m_ovWrite; }

        const std::string &NamedPipeSession::clientPath() const { return clientInfo.ClientPath; }
        ULONG NamedPipeSession::clientPid() const { return clientInfo.ClientPid; }
        HANDLE NamedPipeSession::pipeHandle() const { return m_hPipe; }

        bool NamedPipeSession::onConnected()
        {
            // Handle connect event
            DWORD dwDummy;
            if (GetOverlappedResult(m_hPipe, &m_ovConnect, &dwDummy, FALSE))
            {
                log.debug("Client connected: PID={}, Path={}", clientInfo.ClientPid, clientInfo.ClientPath);
                ResetEvent(m_hEventConnect);
                getClientInfo();
                initRead();
                
                return true;
            }
            else
            {
                log.error("GetOverlappedResult failed for client PID={}", clientInfo.ClientPid);
                return false;
            }
        }

        bool NamedPipeSession::onRead()
        {
            log.vdebug("Read event for client PID={}", clientInfo.ClientPid);
            DWORD dwBytesRead = 0;
            if (GetOverlappedResult(m_hPipe, &m_ovRead, &dwBytesRead, FALSE))
            {
                log.vdebug("Read {} bytes from client PID={}", dwBytesRead, clientInfo.ClientPid);
                if (dwBytesRead > 0)
                {
                    onRead(dwBytesRead);
                    return true;
                }
                else
                {
                    log.debug("Client PID={} disconnected, dwBytesRead==0", clientInfo.ClientPid);
                    return false;
                }
            }
            else
            {
                DWORD dwError = GetLastError();
                if (dwError == ERROR_BROKEN_PIPE || dwError == ERROR_PIPE_NOT_CONNECTED)
                {
                    log.debug("Client PID={} disconnected (pipe broken)", clientInfo.ClientPid);
                    return false;
                }
                else if (dwError != ERROR_IO_PENDING)
                {
                    log.error("GetOverlappedResult failed for client PID={} with error: {}", clientInfo.ClientPid, dwError);
                    return false;
                }
            }

            return true;
        }

        bool NamedPipeSession::onWritten()
        {
            log.vdebug("Write event for client PID={}", clientInfo.ClientPid);

            std::lock_guard<std::mutex> lock(m_writeMutex);
            if (!m_writePending)
            {
                return true;
            }

            DWORD dwBytesWritten = 0;
            if (GetOverlappedResult(m_hPipe, &m_ovWrite, &dwBytesWritten, FALSE))
            {
                ResetEvent(m_hEventWrite);
                log.vdebug("Wrote {} bytes to client PID={}", dwBytesWritten, clientInfo.ClientPid);
                if (dwBytesWritten != m_writeBuffer.size())
                {
                    log.error("Incomplete write: {} of {} bytes", dwBytesWritten, m_writeBuffer.size());
                    return false;
                }

                memset(m_writeBuffer.data(), 0x42, m_writeBuffer.size());
                m_writeBuffer.clear();
                m_writePending = false;
                return true;
            }
            else
            {
                log.error("GetOverlappedResult failed for client PID={}", clientInfo.ClientPid);
                return false;
            }
        }

        char NamedPipeSession::type() const {
            return clientInfo.ClientPid ? 'P' : 'p';
        }

    }
}
