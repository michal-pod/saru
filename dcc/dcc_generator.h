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

#include <string>
#include <vector>
#include <map>
#include <libssha/utils/logger.h>

namespace nglab { namespace libssha { struct OpenSSHHopKey; class OpenSSHHopDescriptor; class OpenSSHSDestinationConstraint; } }

class DccGenerator : public nglab::libssha::LogEnabler {
public:
    DccGenerator(std::string knownHostsPath, std::string outPath);
    int run(const std::vector<std::string>& specs);

private:
    bool parseKnownHosts();
    void processSpec(const std::string &spec, std::vector<nglab::libssha::OpenSSHSDestinationConstraint> &constraints);
    bool writeOutput(const std::vector<nglab::libssha::OpenSSHSDestinationConstraint> &constraints);

    // helper methods (moved from globals)
    std::string trim(const std::string &s) const;
    std::vector<uint8_t> base64_decode(const std::string &in) const;
    bool matchGlob(const std::string &pattern, const std::string &host) const;

    std::string m_knownHostsPath;
    std::string m_outPath;
    std::map<std::string, std::vector<std::vector<uint8_t>>> m_hostKeys;
};

