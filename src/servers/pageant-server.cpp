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
#include <string>
#include <unordered_map>

#include <system_error>
#include "pageant-server.h"
#include "stdatl.h"
#include "system/csid.h"

using namespace nglab::skym;

LRESULT CPageantServer::OnCopyData(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
{
    PCOPYDATASTRUCT pCDS = reinterpret_cast<PCOPYDATASTRUCT>(lParam);

    // Validate COPYDATASTRUCT
    if (!pCDS || !pCDS->lpData || pCDS->cbData == 0)
    {
        return FALSE;
    }

    // Check the identifier
    if (pCDS->dwData != PAGEANT_COPYDATA_ID)
    {
        return FALSE;
    }

    DWORD threadId=0;
    // Extract threadId from mapping name using std::string
    std::string mappingName(static_cast<const char *>(pCDS->lpData));
    if (mappingName.rfind("PageantRequest", 0) == 0) // Check if it starts with "PageantRequest"
    {
        std::string threadIdStr = mappingName.substr(14); // Extract the thread ID part
        threadId = std::stoul(threadIdStr, nullptr, 16);
    }
    // Try to find process ID
    DWORD processId = 0;// FindProcessId(static_cast<const char *>(pCDS->lpData));
    if(threadId != 0)
    {
        HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, threadId);
        if (hThread)
        {
            processId = GetProcessIdOfThread(hThread);
            CloseHandle(hThread);
        }
    }
    //GetWindowThreadProcessId(hWnd, &processId);

    log.debug("Received COPYDATA from process ID {}", processId);

    // Now we need to get message from client
    HANDLE hMapping = OpenFileMapping(
        FILE_MAP_ALL_ACCESS,
        FALSE,
        static_cast<LPCSTR>(pCDS->lpData));

    if (!hMapping)
    {
        log.error("OpenFileMapping failed");
        return FALSE;
    }

    CSID expected = CSID::getUser();
    CSID expected_legacy = CSID::getDefault();
    CSID actual = CSID::getSidOfHandle(hMapping);

    if (!(expected == actual || expected_legacy == actual))
    {
        log.error("Client SID does not match expected SID");
        CloseHandle(hMapping);
        return FALSE;
    }

    LPVOID pMapView = MapViewOfFile(
        hMapping,
        FILE_MAP_WRITE,
        0,
        0,
        0);

    if (!pMapView)
    {
        log.error("MapViewOfFile failed");
        return FALSE;
    }

    size_t messageLength;

    MEMORY_BASIC_INFORMATION mbi;
    size_t mbiSize = VirtualQuery(pMapView, &mbi, sizeof(mbi));
    if (mbiSize == 0)
    {
        log.error("VirtualQuery failed");
        UnmapViewOfFile(pMapView);
        CloseHandle(hMapping);
        return FALSE;
    }

    if (mbiSize < (offsetof(MEMORY_BASIC_INFORMATION, RegionSize) + sizeof(mbi.RegionSize)))
    {
        log.error("VirtualQuery returned insufficient data");
        UnmapViewOfFile(pMapView);
        CloseHandle(hMapping);
        return FALSE;
    }

    messageLength = mbi.RegionSize;

    std::unordered_map<DWORD, PageantSession>::iterator session = m_sessions.find(processId);
    if (session == m_sessions.end())
    {
        m_sessions.emplace(processId, processId);
        session = m_sessions.find(processId);
    }

    KillTimer(1); // pause session cleanup during processing

    PageantSession &pageantSession = session->second;
    pageantSession.process(static_cast<uint8_t *>(pMapView), messageLength);
    pageantSession.touch();

    auto &response = pageantSession.getResponse();
    if (response.size() > messageLength)
    {
        log.error("Response size exceeds shared memory size");
        UnmapViewOfFile(pMapView);
        CloseHandle(hMapping);
        return FALSE;
    }
    memcpy(pMapView, response.data(), response.size());

    UnmapViewOfFile(pMapView);
    CloseHandle(hMapping);

    SetTimer(CleanupTimer, 1000); // resume session cleanup

    return TRUE;
}

LRESULT CPageantServer::OnTimer(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL &bHandled)
{
    if(wParam == CleanupTimer)
    {
    for (auto it = m_sessions.begin(); it != m_sessions.end(); )
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
