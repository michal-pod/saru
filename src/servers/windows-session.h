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

#include <cstdint>
#include <string>

#include <libssha/agent/session.h>

#include "client-info.h"

namespace nglab
{
    namespace saru
    {
        using nglab::libssha::ExtensionMessage;
        using nglab::libssha::KeyBase;
        using nglab::libssha::KeyBasePtr;
        using nglab::libssha::LogEnabler;

        enum WindowsSessionType : uint8_t
        {
            Pipe,
            Pageant,
            HyperV
        };

        template <typename T, WindowsSessionType SessionType>
        class WindowsSession : virtual public LogEnabler, virtual public nglab::libssha::Session
        {
        public:
            WindowsSession();
            bool findBinary();
            bool confirmRequest(const KeyBase &key);
            bool requiresConfirmation(const KeyBasePtr key) const;
            void processRequestIdentities(const nglab::libssha::Message &msg);
            bool processExtensionMessage(const ExtensionMessage &msg);
            std::string client() const;
            const std::string VMName() const;

        protected:
            ClientInfo clientInfo;
        };

        class NamedPipeSession;
        class PageantSession;
        class HyperVSession;

        extern template class WindowsSession<NamedPipeSession, WindowsSessionType::Pipe>;
        extern template class WindowsSession<PageantSession, WindowsSessionType::Pageant>;
        extern template class WindowsSession<HyperVSession, WindowsSessionType::HyperV>;
    }
}
