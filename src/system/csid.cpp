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
#include "csid.h"

#include <windows.h>
#include <Aclapi.h>
#include <stdexcept>

namespace nglab
{
    namespace saru
    {
        using nglab::libssha::Logger;

        PSID CSID::m_user_sid = nullptr;

        CSID CSID::getUser()
        {
            if (m_user_sid)
            {
                return CSID(m_user_sid);
            }

            HANDLE hProcess = OpenProcess(MAXIMUM_ALLOWED, FALSE, GetCurrentProcessId());
            if (!hProcess)
            {
                throw std::runtime_error("OpenProcess failed");
            }

            HANDLE hToken;
            if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken))
            {
                CloseHandle(hProcess);
                throw std::runtime_error("OpenProcessToken failed");
            }

            DWORD dwBufferSize = 0;
            if (!GetTokenInformation(hToken, TokenUser, NULL, 0, &dwBufferSize) && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            {
                CloseHandle(hToken);
                CloseHandle(hProcess);
                throw std::runtime_error("GetTokenInformation failed to get buffer size");
            }

            TOKEN_USER *pTokenUser = (TOKEN_USER *)LocalAlloc(LPTR, dwBufferSize);
            if (!pTokenUser)
            {
                CloseHandle(hToken);
                CloseHandle(hProcess);
                throw std::runtime_error("LocalAlloc failed");
            }

            if (!GetTokenInformation(hToken, TokenUser, pTokenUser, dwBufferSize, &dwBufferSize))
            {
                LocalFree(pTokenUser);
                CloseHandle(hToken);
                CloseHandle(hProcess);
                throw std::runtime_error("GetTokenInformation failed");
            }

            CSID ret(pTokenUser->User.Sid);
            m_user_sid = ret.m_sid;

            LocalFree(pTokenUser);
            CloseHandle(hToken);
            CloseHandle(hProcess);

            return ret;
        }

        CSID CSID::getDefault()
        {
            HANDLE hProcess = OpenProcess(MAXIMUM_ALLOWED, FALSE, GetCurrentProcessId());
            if (!hProcess)
            {
                throw std::runtime_error("OpenProcess failed");
            }

            try
            {
                CSID sid = getSidOfHandle(hProcess);
                CloseHandle(hProcess);
                return sid;
            }
            catch (...)
            {
                CloseHandle(hProcess);
                throw;
            }
        }

        CSID CSID::getSidOfHandle(HANDLE hObject)
        {
            auto& log = Logger::instance();
            PSECURITY_DESCRIPTOR pSD = nullptr;
            PSID pSid = nullptr;
            if (GetSecurityInfo(hObject, SE_KERNEL_OBJECT, OWNER_SECURITY_INFORMATION, &pSid, nullptr, nullptr, nullptr, &pSD) != ERROR_SUCCESS)
            {
                log.error("GetSecurityInfo failed with code {}", GetLastError());
                throw std::runtime_error("GetSecurityInfo failed");
            }

            CSID ret(pSid);

            if (pSD)
            {
                LocalFree(pSD);
            }

            return ret;
        }

        CSID CSID::getSidOfProcess(DWORD processId)
        {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_DUP_HANDLE, FALSE, processId);
            if (!hProcess)
            {
                throw std::runtime_error("OpenProcess failed");
            }

            HANDLE hPrimaryToken;
            if (!OpenProcessToken(hProcess, TOKEN_QUERY | TOKEN_DUPLICATE, &hPrimaryToken))
            {
                CloseHandle(hProcess);
                throw std::runtime_error("OpenProcessToken failed");
            }

            HANDLE hToken;
            if (!DuplicateToken(hPrimaryToken, SecurityImpersonation, &hToken))
            {
                CloseHandle(hPrimaryToken);
                CloseHandle(hProcess);
                throw std::runtime_error("DuplicateTokenEx failed");
            }

            DWORD dwBufferSize = 0;
            if (!GetTokenInformation(hToken, TokenUser, NULL, 0, &dwBufferSize) && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            {
                CloseHandle(hToken);
                CloseHandle(hPrimaryToken);
                CloseHandle(hProcess);
                throw std::runtime_error("GetTokenInformation failed to get buffer size");
            }

            TOKEN_USER *pTokenUser = (TOKEN_USER *)LocalAlloc(LPTR, dwBufferSize);
            if (!pTokenUser)
            {
                CloseHandle(hToken);
                CloseHandle(hPrimaryToken);
                CloseHandle(hProcess);
                throw std::runtime_error("LocalAlloc failed");
            }

            if (!GetTokenInformation(hToken, TokenUser, pTokenUser, dwBufferSize, &dwBufferSize))
            {
                LocalFree(pTokenUser);
                CloseHandle(hToken);
                CloseHandle(hPrimaryToken);
                CloseHandle(hProcess);
                throw std::runtime_error("GetTokenInformation failed");
            }

            CSID ret = getSidOfHandle(pTokenUser->User.Sid);

            LocalFree(pTokenUser);
            CloseHandle(hToken);
            CloseHandle(hPrimaryToken);
            CloseHandle(hProcess);
            return ret;
        }


        CSID &CSID::operator=(CSID &&other) noexcept
        {
            fprintf(stderr, "CSID move assignment\n");
            if (this != &other)
            {
                if (m_sid)
                {
                    LocalFree(m_sid);
                }
                m_sid = other.m_sid;
                other.m_sid = nullptr;
            }
            return *this;
        }

        CSID::CSID(CSID &&other) noexcept : LogEnabler("CSID"), m_sid(other.m_sid)
        { 
            other.m_sid = nullptr;
        }

        bool CSID::operator==(const CSID &other) const
        {
            return EqualSid(this->m_sid, other.m_sid) != 0;
        }

        CSID::CSID(PSID sid) : LogEnabler("CSID"), m_sid(sid)
        {
            if (m_sid)
            {
                DWORD sidLength = GetLengthSid(m_sid);
                PSID sidCopy = (PSID)LocalAlloc(LPTR, sidLength);
                if (!sidCopy)
                {
                    throw std::runtime_error("LocalAlloc failed in CSID copy constructor");
                }
                if (!CopySid(sidLength, sidCopy, m_sid))
                {
                    LocalFree(sidCopy);
                    throw std::runtime_error("CopySid failed in CSID copy constructor");
                }
                m_sid = sidCopy;
            }
        }
    } // namespace saru
} // namespace nglab

