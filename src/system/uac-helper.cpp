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
#include "uac-helper.h"

#include <windows.h>

using namespace nglab::skym;

bool UACHelper::isElevated()
{
    BOOL isElevated = FALSE;
    HANDLE token = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        TOKEN_ELEVATION elevation;
        DWORD size;
        if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size))
        {
            isElevated = elevation.TokenIsElevated;
        }
        CloseHandle(token);
    }
    return isElevated != FALSE;
}

bool UACHelper::canElevate()
{
    if (isElevated())
        return true;
        
    TOKEN_ELEVATION_TYPE type = GetTokenElevationType();
    if (type == TokenElevationTypeLimited)
    {
        // Proces ma token ograniczony (użytkownik jest w Administrators ale działa bez elevation).
        return true;
    }

    BOOL bIsMember = FALSE;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID pAdminGroup = NULL;

    if (AllocateAndInitializeSid(
            &NtAuthority,
            2,
            SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0,
            &pAdminGroup))
    {
        if (!CheckTokenMembership(NULL, pAdminGroup, &bIsMember))
        {
            bIsMember = FALSE;
        }
        FreeSid(pAdminGroup);
    }
    return bIsMember != FALSE;
}

TOKEN_ELEVATION_TYPE UACHelper::GetTokenElevationType()
{
    TOKEN_ELEVATION_TYPE elevationType = TokenElevationTypeDefault;
    HANDLE token = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
    {
        TOKEN_ELEVATION_TYPE type;
        DWORD size;
        if (GetTokenInformation(token, TokenElevationType, &type, sizeof(type), &size))
        {
            elevationType = type;
        }
        CloseHandle(token);
    }
    return elevationType;
}
