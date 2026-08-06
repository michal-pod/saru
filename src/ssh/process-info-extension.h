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
#include <memory>
#include <string>
#include <vector>

#include <libssha/extensions/extension.h>
#include <libssha/utils/serializer.h>
#include <libssha/utils/deserializer.h>
#include <libssha/utils/logger.h>

#include "servers/client-info.h"
namespace nglab
{
    namespace saru
    {
        using nglab::libssha::Extension;
        using nglab::libssha::ExtensionType;
        using nglab::libssha::Serializer;
        using nglab::libssha::Deserializer;
        using nglab::libssha::LogEnabler;

        namespace
        {
            constexpr const char process_info_ext_name[] = "proc-info@nglab.net";
        }


        class ProcessInfoExtension : public Extension<ProcessInfoExtension,
                                                      process_info_ext_name,
                                                      ExtensionType::MessageExtension>,
                                     public LogEnabler
        {
        public:
            ProcessInfoExtension();

            virtual void serialize(Serializer &s) const override;
            virtual void deserialize(Deserializer &d) override;

            uint32_t uid() const { return m_uid; }
            void setUid(uint32_t uid) { m_uid = uid; }

            std::string user() const { return m_user; }
            void setUser(const std::string &user) { m_user = user; }

            uint32_t gid() const { return m_gid; }
            void setGid(uint32_t gid) { m_gid = gid; }

            std::string group() const { return m_group; }
            void setGroup(const std::string &group) { m_group = group; }

            const std::vector<std::string>& path() const { return m_path; }
            void addPath(const std::string &path) { m_path.push_back(path); }

            std::string distro() const { return m_distro; }
            void setDistro(const std::string &distro) { m_distro = distro; }

            ClientInfoSystemType type() const { return m_type; }
            void setType(ClientInfoSystemType type) { m_type = type; }

        private:
            uint32_t m_uid;
            std::string m_user;
            uint32_t m_gid;
            std::string m_group;
            std::string m_distro;
            ClientInfoSystemType m_type;
            std::vector<std::string> m_path;
            
        };
    } // namespace saru
} // namespace nglab

