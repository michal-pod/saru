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
#include <format>
#include <windows.h>

#include "string-converter.h"

namespace nglab
{
    namespace saru
    {

        std::string StringConverter::w2u8(const std::wstring &input)
        {
            if (input.empty())
            {
                return std::string();
            }
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), (int)input.size(), NULL, 0, NULL, NULL);
            std::string result(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, input.c_str(), (int)input.size(), &result[0], size_needed, NULL, NULL);
            return result;
        }

        std::wstring StringConverter::u8w2(const std::string &input)
        {
            if (input.empty())
            {
                return std::wstring();
            }
            int size_needed = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), (int)input.size(), NULL, 0);
            std::wstring result(size_needed, 0);
            MultiByteToWideChar(CP_UTF8, 0, input.c_str(), (int)input.size(), &result[0], size_needed);
            return result;
        }

        std::string StringConverter::fromGUIDA(const GUID &guid)
        {
            return std::format("{:08X}-{:04X}-{:04X}-{:04X}-{:012X}",
                               guid.Data1,
                               guid.Data2,
                               guid.Data3,
                               (guid.Data4[0] << 8) | guid.Data4[1],
                               (static_cast<uint64_t>(guid.Data4[2]) << 40) |
                                   (static_cast<uint64_t>(guid.Data4[3]) << 32) |
                                   (static_cast<uint64_t>(guid.Data4[4]) << 24) |
                                   (static_cast<uint64_t>(guid.Data4[5]) << 16) |
                                   (static_cast<uint64_t>(guid.Data4[6]) << 8) |
                                   (static_cast<uint64_t>(guid.Data4[7])));
        }

        std::wstring StringConverter::fromGUIDW(const GUID &guid)
        {
            return std::format(L"{:08X}-{:04X}-{:04X}-{:04X}-{:012X}",
                               guid.Data1,
                               guid.Data2,
                               guid.Data3,
                               (guid.Data4[0] << 8) | guid.Data4[1],
                               (static_cast<uint64_t>(guid.Data4[2]) << 40) |
                                   (static_cast<uint64_t>(guid.Data4[3]) << 32) |
                                   (static_cast<uint64_t>(guid.Data4[4]) << 24) |
                                   (static_cast<uint64_t>(guid.Data4[5]) << 16) |
                                   (static_cast<uint64_t>(guid.Data4[6]) << 8) |
                                   (static_cast<uint64_t>(guid.Data4[7])));
        }

    } // namespace saru
} // namespace nglab
