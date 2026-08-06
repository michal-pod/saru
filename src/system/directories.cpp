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
#include "directories.h"

#include "stdatl.h"

namespace nglab
{
    namespace saru
    {
        std::string Directories::getExecutableDirectoryA()
        {
            char buffer[MAX_PATH];
            GetModuleFileNameA(NULL, buffer, MAX_PATH);
            std::string executablePath = buffer;
            return executablePath.substr(0, executablePath.find_last_of("\\/"));
        }

        std::wstring Directories::getExecutableDirectoryW()
        {
            wchar_t buffer[MAX_PATH];
            GetModuleFileNameW(NULL, buffer, MAX_PATH);
            std::wstring executablePath = buffer;
            return executablePath.substr(0, executablePath.find_last_of(L"\\/"));
        }        

        std::string Directories::getAppDataDirectoryA()
        {
            char buffer[MAX_PATH];
            SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, buffer);
            return buffer;
        }

        std::wstring Directories::getAppDataDirectoryW()
        {
            wchar_t buffer[MAX_PATH];
            SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, buffer);
            return buffer;
        }

    } // namespace saru
} // namespace nglab

