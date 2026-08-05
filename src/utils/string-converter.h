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
#include <string>
#include <windows.h>

namespace nglab
{
    namespace skym
    {
        class StringConverter
        {
        public:
            static std::string w2u8(const std::wstring &input);
            static std::wstring u8w2(const std::string &input);
            static std::string fromGUIDA(const GUID &guid);
            static std::wstring fromGUIDW(const GUID &guid);
        };
    }
}
