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
#include <cstring>
#include <unordered_map>

#include "bitmap-factory.h"
#include "stdatl.h"
#include "icon-factory.h"

using namespace nglab::skym;

// define static cache
std::unordered_map<int, HBITMAP> BitmapFactory::m_bitmapMap;

HBITMAP BitmapFactory::get(int resourceId)
{
    auto it = m_bitmapMap.find(resourceId);
    if (it != m_bitmapMap.end())
    {
        return it->second;
    }

    // Try to load a native bitmap resource first
    HBITMAP hBitmap = static_cast<HBITMAP>(LoadImage(_Module.GetResourceInstance(),
        MAKEINTRESOURCEA(resourceId), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR));
    if (hBitmap)
    {
        m_bitmapMap[resourceId] = hBitmap;
        return hBitmap;
    }

    // If no bitmap resource, try to get an icon and convert it to a 32bpp HBITMAP.
    HICON hIcon = IconFactory::get(resourceId);
    if (!hIcon)
    {
        // As a last resort, try loading an icon resource directly
        hIcon = static_cast<HICON>(LoadImage(_Module.GetResourceInstance(),
            MAKEINTRESOURCEA(resourceId), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR));
    }

    if (hIcon)
    {
        int w = GetSystemMetrics(SM_CXSMICON);
        int h = GetSystemMetrics(SM_CYSMICON);

        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = w;
        bi.bmiHeader.biHeight = -h; // top-down DIB
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32; // 32bpp to hold alpha
        bi.bmiHeader.biCompression = BI_RGB;

        void* pvBits = nullptr;
        HDC screenDC = GetDC(NULL);
        HDC memDC = CreateCompatibleDC(screenDC);
        HBITMAP hDib = CreateDIBSection(memDC, &bi, DIB_RGB_COLORS, &pvBits, NULL, 0);
        if (hDib && pvBits)
        {
            // Clear to transparent
            memset(pvBits, 0, w * h * 4);

            HBITMAP hOld = (HBITMAP)SelectObject(memDC, hDib);
            // Draw the icon into the DIB; DI_NORMAL should render color + alpha into 32bpp DIB
            DrawIconEx(memDC, 0, 0, hIcon, w, h, 0, NULL, DI_NORMAL);
            SelectObject(memDC, hOld);

            m_bitmapMap[resourceId] = hDib;
            // clean up
            DeleteDC(memDC);
            ReleaseDC(NULL, screenDC);
            return hDib;
        }

        // cleanup if failed
        if (memDC) DeleteDC(memDC);
        ReleaseDC(NULL, screenDC);
    }

    return nullptr;
}

