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
#pragma once
#include <windows.h>

namespace nglab
{
    namespace skym
    {
        class AsyncServer
        {
        public:
            virtual ~AsyncServer() = default;
            virtual HANDLE hEventConnect() const = 0;
            virtual HANDLE hEventRead() const = 0;
            virtual HANDLE hEventWrite() const = 0;

            virtual bool onConnected() = 0;
            virtual bool onRead() = 0;
            virtual bool onWritten() = 0;

            virtual ULONG clientPid() const = 0;

            virtual char type() const = 0;
    
        };
    }
}
