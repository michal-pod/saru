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
#include <windows.h>
#include <Aclapi.h>
#include <stdexcept>

#include <libssha/utils/logger.h>

namespace nglab {
    namespace saru
    {
        class CSID : public nglab::libssha::LogEnabler
        {
            public:

            static CSID getUser();
            static CSID getDefault();
            static CSID getSidOfHandle(HANDLE hObject);
            static CSID getSidOfProcess(DWORD processId);

            CSID& operator=(CSID&& other) noexcept;
            CSID(CSID&& other) noexcept;
            ~CSID();

            bool operator==(const CSID &other) const;

            PSID psid() const { return m_sid; }

            private:
            // Stores a copy of the SID, the caller keeps ownership of sid
            CSID(PSID sid);
            static CSID readProcessUser();
            PSID m_sid;
        };

    } // namespace saru
} // namespace nglab
