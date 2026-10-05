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
#pragma once

#include <filesystem>
#include <shlobj.h>

#include "stdatl.h"
#include "resources/resource.h"

namespace nglab::saru
{
    // The shell scales an available frame when the exact size is absent.
    // Returned icons belong to the caller.
    inline HICON loadExecutableIcon(const std::filesystem::path &path, int size)
    {
        HICON icon = nullptr;
        SHDefExtractIconW(path.c_str(), 0, 0, &icon, nullptr, MAKELONG(size, 0));
        if (icon)
            return icon;

        SHSTOCKICONINFO info{};
        info.cbSize = sizeof(info);
        if (SUCCEEDED(SHGetStockIconInfo(SIID_APPLICATION, SHGSI_ICONLOCATION, &info)))
            SHDefExtractIconW(info.szPath, info.iIcon, 0, &icon, nullptr, MAKELONG(size, 0));
        if (icon)
            return icon;

        return static_cast<HICON>(LoadImage(
            _Module.GetResourceInstance(), MAKEINTRESOURCE(IDI_ICON1),
            IMAGE_ICON, size, size, LR_DEFAULTCOLOR));
    }
}
