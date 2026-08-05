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
#include <unordered_map>

#include "icon-factory.h"
#include "stdatl.h"

using namespace nglab::skym;

// define static cache
std::unordered_map<int, HICON> IconFactory::m_iconMap;

HICON IconFactory::get(int resourceId)
{
    auto it = m_iconMap.find(resourceId);
    if (it != m_iconMap.end())
    {
        return it->second;
    }

    HICON hIcon = static_cast<HICON>(LoadImage(_Module.GetResourceInstance(),
        MAKEINTRESOURCEA(resourceId), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
    if (hIcon)
    {
        m_iconMap[resourceId] = hIcon;
    }
    return hIcon;
}

std::unordered_map<int, HICON> IconFactory::m_largeIconMap;

HICON IconFactory::getLarge(int resourceId)
{
    auto it = m_largeIconMap.find(resourceId);
    if (it != m_largeIconMap.end())
    {
        return it->second;
    }

    HICON hIcon = static_cast<HICON>(LoadImage(_Module.GetResourceInstance(),
        MAKEINTRESOURCEA(resourceId), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR));
    if (hIcon)
    {
        m_largeIconMap[resourceId] = hIcon;
    }
    return hIcon;
}

