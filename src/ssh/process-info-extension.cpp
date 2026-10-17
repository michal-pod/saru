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
#include "process-info-extension.h"

#include <format>
#include <stdexcept>

using namespace nglab::saru;

ProcessInfoExtension::ProcessInfoExtension() : LogEnabler("ProcessInfoExtension"),
                                                 m_uid(0),
                                                 m_gid(0)
{
    log.vdebug("ProcessInfoExtension created");
}

void ProcessInfoExtension::serialize(Serializer &s) const
{
    log.vdebug("Serializing ProcessInfoExtension: uid={}, user={}, gid={}, group={}",
              m_uid, m_user, m_gid, m_group);

    /*s.writeBE32(m_uid);
    s.writeString(m_user);
    s.writeBE32(m_gid);
    s.writeString(m_group);
    s.writeBE32(m_pid);
    s.writeString(m_process_name);*/
}

void ProcessInfoExtension::deserialize(Deserializer &d)
{
    log.vdebug("Deserializing ProcessInfoExtension, remaining data size: {}", d.remaining());


    m_uid = d.readBE32();
    m_user = readText(d, MaxNameLength, "user");
    m_gid = d.readBE32();
    m_group = readText(d, MaxNameLength, "group");
    m_distro = readText(d, MaxNameLength, "distro");

    const auto type = d.readByte();
    if (type != Linux && type != FreeBSD && type != Windows)
    {
        throw std::runtime_error(std::format("ProcessInfoExtension: invalid system type {}", type));
    }
    m_type = static_cast<ClientInfoSystemType>(type);

    // The first hop is the client process, callers rely on at least one
    auto hops = d.readBE32();
    if (hops == 0 || hops > MaxHops)
    {
        throw std::runtime_error(std::format("ProcessInfoExtension: invalid number of hops {}", hops));
    }

    log.debug("Deserialized ProcessInfoExtension: uid={}, user={}, gid={}, group={}, hops={}, distro={}, type={}",
              m_uid, m_user, m_gid, m_group, hops, m_distro, type);
    auto data = d.readBlob();
    log.vdebug("Read hops blob of size: {}", data.size());
    Deserializer hopDeser(data);
    std::vector<std::string> path;
    while(hops-- > 0){
        auto hop_pid = hopDeser.readBE32();
        auto hop_path = readText(hopDeser, MaxPathLength, "path");
        path.push_back(hop_path);
        log.vdebug("Read hop: pid={}, path={}", hop_pid, hop_path);
    }

    if (hopDeser.remaining() != 0 || d.remaining() != 0)
    {
        throw std::runtime_error("ProcessInfoExtension: unexpected trailing data");
    }

    m_path = std::move(path);
}

std::string ProcessInfoExtension::readText(Deserializer &d, size_t maxLength, const char *field)
{
    auto text = d.readString();
    if (text.size() > maxLength)
    {
        throw std::runtime_error(std::format("ProcessInfoExtension: {} is too long", field));
    }

    // Control characters (new lines in particular) could make a misleading confirmation dialog
    for (const unsigned char c : text)
    {
        if (c < 0x20 || c == 0x7F)
        {
            throw std::runtime_error(std::format("ProcessInfoExtension: {} contains control characters", field));
        }
    }
    return text;
}
