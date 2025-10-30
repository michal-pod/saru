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
#include <windows.h>
#include <cstdio>
#include <stdexcept>

#include "debug-console.h"
#include "stdatl.h"
#include "config.h"

using namespace nglab::skym;

DebugConsole::DebugConsole() : LogEnabler("DebugConsole")
{
    CRegKey key;
    if (key.Open(HKEY_CURRENT_USER, _T(SKYM_KEY_ROOT), KEY_READ) == ERROR_SUCCESS)
    {
        DWORD val = 0;
        if (key.QueryDWORDValue("EnableDebugConsole", val) == ERROR_SUCCESS)
        {
            m_enabled = (val != 0);
        }
        key.Close();
    }
    if (!m_enabled)
    {
        return;
    }
    AllocConsole();

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;

    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);

    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    dwMode = 0;
    GetConsoleMode(hErr, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hErr, dwMode);

    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    freopen("CONIN$", "r", stdin);

    m_hwndConsole = GetConsoleWindow();

    if (m_hwndConsole == nullptr)
    {
        throw std::runtime_error("Failed to obtain console window handle.");
    }

    HMENU hMenu = GetSystemMenu(m_hwndConsole, FALSE);
    if (hMenu)
    {
        DeleteMenu(hMenu, SC_CLOSE, MF_BYCOMMAND);
    }

    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hInput, &dwMode);
    dwMode &= ~(ENABLE_QUICK_EDIT_MODE);
    SetConsoleMode(hInput, dwMode);

    ::ShowWindow(m_hwndConsole, SW_HIDE);
}

DebugConsole::~DebugConsole()
{
    if (!m_enabled)
    {
        return;
    }
    fclose(stdout);
    fclose(stderr);
    fclose(stdin);
    FreeConsole();
}
