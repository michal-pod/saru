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
#include <string>
#include <string_view>
#include <unordered_map>

#include <charconv>
#include <cstring>
#include <system_error>
#include "pageant-server.h"
#include "stdatl.h"
#include "system/csid.h"

using namespace nglab::saru;

BOOL CPageantServer::Start()
{
    if (::FindWindowW(L"Pageant", L"Pageant"))
    {
        CKeyList::instance().DisplayTrayNotification("Error", "Another instance of Pageant is already running.\nPageant support will be disabled.", NIIF_ERROR);
        log.error("Another instance of Pageant is already running");
        return FALSE;
    }

    if (m_thread.joinable())
    {
        log.warning("Pageant server is already running");
        return FALSE;
    }

    m_thread = std::thread([this]()
    {
        CMessageLoop theLoop;
        _Module.AddMessageLoop(&theLoop);
        this->Create(NULL, CWindow::rcDefault, _T("Pageant"), WS_OVERLAPPEDWINDOW, 0);
        this->SetTimer(CleanupTimer, 1000);
        theLoop.Run();
        this->DestroyWindow();
        _Module.RemoveMessageLoop();
    });

    return TRUE;
}

void CPageantServer::Stop()
{
    PostMessage(WM_QUIT);
    if (m_thread.joinable())
    {
        m_thread.join();
    }
}

LRESULT CPageantServer::OnCopyData(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
{
    constexpr std::string_view mappingPrefix = "PageantRequest";
    constexpr size_t mappingNameLength = mappingPrefix.size() + sizeof(DWORD) * 2;
    constexpr SIZE_T mappingSize = 8192;

    HANDLE hMapping = nullptr;
    LPVOID pMapView = nullptr;

    try
    {
        const auto *copyData = reinterpret_cast<const COPYDATASTRUCT *>(lParam);
        if (!copyData || !copyData->lpData || copyData->dwData != PAGEANT_COPYDATA_ID)
        {
            return FALSE;
        }

        if (copyData->cbData != mappingNameLength + 1)
        {
            log.warning("Rejected COPYDATA with an invalid Pageant mapping name");
            return FALSE;
        }

        const auto *mappingData = static_cast<const char *>(copyData->lpData);
        if (mappingData[mappingNameLength] != '\0')
        {
            log.warning("Rejected COPYDATA with an invalid Pageant mapping name");
            return FALSE;
        }

        std::string mappingName(mappingData, mappingNameLength);
        if (!std::string_view(mappingName).starts_with(mappingPrefix))
        {
            log.warning("Rejected COPYDATA with an invalid Pageant mapping name");
            return FALSE;
        }

        DWORD mappingThreadId = 0;
        const auto [end, error] = std::from_chars(
            mappingName.data() + mappingPrefix.size(), mappingName.data() + mappingName.size(), mappingThreadId, 16);
        if (error != std::errc{} || end != mappingName.data() + mappingName.size() || mappingThreadId == 0)
        {
            log.warning("Rejected COPYDATA with an invalid Pageant mapping name");
            return FALSE;
        }

        DWORD processId = 0;
        HANDLE hThread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, mappingThreadId);
        if (hThread)
        {
            processId = GetProcessIdOfThread(hThread);
            CloseHandle(hThread);
        }

        if (processId == 0)
        {
            log.warning("Could not determine Pageant client process ID");
        }

        log.debug("Received COPYDATA from process ID {}", processId);

        hMapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, mappingName.c_str());
        if (!hMapping)
        {
            log.error("OpenFileMapping failed");
            return FALSE;
        }

        CSID expected = CSID::getUser();
        CSID expectedLegacy = CSID::getDefault();
        CSID actual = CSID::getSidOfHandle(hMapping);

        if (!(expected == actual || expectedLegacy == actual))
        {
            log.error("Client SID does not match expected SID");
            CloseHandle(hMapping);
            return FALSE;
        }

        pMapView = MapViewOfFile(hMapping, FILE_MAP_WRITE, 0, 0, mappingSize);
        if (!pMapView)
        {
            log.error("MapViewOfFile failed");
            CloseHandle(hMapping);
            return FALSE;
        }

        auto session = m_sessions.find(processId);
        if (session == m_sessions.end())
        {
            session = m_sessions.emplace(processId, processId).first;
        }
        PageantSession &pageantSession = session->second;
        pageantSession.process(static_cast<uint8_t *>(pMapView), mappingSize);
        pageantSession.touch();

        const auto &response = pageantSession.getResponse();
        if (response.size() > mappingSize)
        {
            log.error("Response size exceeds Pageant shared memory size");
            UnmapViewOfFile(pMapView);
            CloseHandle(hMapping);
            return FALSE;
        }
        memcpy(pMapView, response.data(), response.size());

        UnmapViewOfFile(pMapView);
        CloseHandle(hMapping);
        return TRUE;
    }
    catch (const std::exception &e)
    {
        if (pMapView)
        {
            UnmapViewOfFile(pMapView);
        }
        if (hMapping)
        {
            CloseHandle(hMapping);
        }
        log.error("Failed to process Pageant COPYDATA: {}", e.what());
        return FALSE;
    }
    catch (...)
    {
        if (pMapView)
        {
            UnmapViewOfFile(pMapView);
        }
        if (hMapping)
        {
            CloseHandle(hMapping);
        }
        log.error("Failed to process Pageant COPYDATA with an unknown exception");
        return FALSE;
    }
}

LRESULT CPageantServer::OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
{
    if (wParam == CleanupTimer)
    {
        for (auto it = m_sessions.begin(); it != m_sessions.end();)
        {
            if (it->second.isExpired())
            {
                log.debug("Removing expired session for process ID {}", it->first);
                it = m_sessions.erase(it);
            }
            else
            {
                ++it;
            }
        }
        SetTimer(CleanupTimer, 1000);
    }

    return 0;
}
