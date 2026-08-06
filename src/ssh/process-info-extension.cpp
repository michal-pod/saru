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
    m_user = d.readString();
    m_gid = d.readBE32();
    m_group = d.readString();
    m_distro = d.readString();    
    m_type = static_cast<ClientInfoSystemType>(d.readByte());
    auto hops = d.readBE32(); // number of hops, currently unused

    log.debug("Deserialized ProcessInfoExtension: uid={}, user={}, gid={}, group={}, hops={}, distro={}, type={}",
              m_uid, m_user, m_gid, m_group, hops, m_distro, static_cast<uint8_t>(m_type));
    auto data = d.readBlob();
    log.vdebug("Read hops blob of size: {}", data.size());
    Deserializer hopDeser(data);
    while(hops-- > 0){
        auto hop_pid = hopDeser.readBE32();
        auto hop_path = hopDeser.readString();
        m_path.push_back(hop_path);
        log.vdebug("Read hop: pid={}, path={}", hop_pid, hop_path);
    }

    log.vdebug("remaining data size after deserialization: {}", d.remaining());
    
}
