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
#pragma once
#include "stdatl.h"

namespace nglab
{
    namespace skym {
        class HVDetector
        {
        public:
            static bool isIntegrationServiceInstalled()
            {
                CRegKey key;
                return (key.Open(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Virtualization\\GuestCommunicationServices\\00002E70-FACB-11E6-BD58-64006A7986D3"), KEY_READ) == ERROR_SUCCESS);
            }

            static bool isHyperVRunning()
            {
                static int state = -1;
                if (state != -1)
                {
                    return state == 1;
                }

                SC_HANDLE hSCM = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
                if (hSCM)
                {
                    SC_HANDLE hService = OpenService(hSCM, _T("vmms"), SERVICE_QUERY_STATUS);
                    if (hService)
                    {
                        SERVICE_STATUS_PROCESS ssp = {};
                        DWORD bytesNeeded = 0;
                        if (QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &bytesNeeded))
                        {
                            CloseServiceHandle(hService);
                            CloseServiceHandle(hSCM);
                            state = (ssp.dwCurrentState == SERVICE_RUNNING) ? 1 : 0;
                            return state == 1;
                        }
                        CloseServiceHandle(hService);
                    }
                    CloseServiceHandle(hSCM);
                    
                    return false;                    
                }
                state = 0;
                return false;
            }
        };
    }
}