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
#include "dcc_generator.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

#include <libssha/extensions/openssh-restrict-destination.h>
#include <libssha/utils/serializer.h>
#include <libssha/utils/logger.h>
#include <external/base64.hpp>

using namespace std;
using namespace nglab::libssha;


// DccGenerator helper implementations
std::string DccGenerator::trim(const std::string &s) const
{
    size_t a = 0;
    while (a < s.size() && isspace((unsigned char)s[a])) ++a;
    size_t b = s.size();
    while (b > a && isspace((unsigned char)s[b-1])) --b;
    return s.substr(a, b-a);
}

std::vector<uint8_t> DccGenerator::base64_decode(const std::string &in) const
{
    try {
        return base64::decode_into<std::vector<uint8_t>>(std::string_view(in));
    } catch (const std::exception &e) {
        Logger::instance().warning("base64 decode error: {}", e.what());
        return {};
    }
}

bool DccGenerator::matchGlob(const std::string &pattern, const std::string &host) const
{
    size_t p = 0, h = 0;
    size_t star = string::npos, match = 0;
    while (h < host.size()) {
        if (p < pattern.size() && (pattern[p] == host[h] || pattern[p] == '?')) {
            ++p; ++h;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++;
            match = h;
        } else if (star != string::npos) {
            p = star + 1;
            h = ++match;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
}

DccGenerator::DccGenerator(std::string knownHostsPath, std::string outPath)
    : LogEnabler("skym-dcc"), m_knownHostsPath(std::move(knownHostsPath)), m_outPath(std::move(outPath))
{
}

bool DccGenerator::parseKnownHosts()
{
    ifstream in(m_knownHostsPath);
    if (!in) {
        Logger::instance().error("Failed to open input file: {}", m_knownHostsPath);
        return false;
    }

    string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0]=='#') continue;
        // tokens: hosts keytype keybase64 [comment]
        istringstream ss(line);
        string hostsTok, keytype, keyb64;
        if (!(ss >> hostsTok >> keytype >> keyb64)) continue;
        // hostsTok may be comma separated
        vector<string> hosts;
        size_t pos = 0, prev = 0;
        while ((pos = hostsTok.find(',', prev))!=string::npos) {
            hosts.push_back(hostsTok.substr(prev, pos-prev));
            prev = pos+1;
        }
        hosts.push_back(hostsTok.substr(prev));

        auto keybin = base64_decode(keyb64);
        for (auto &h : hosts) {
                // known_hosts entries must not contain wildcards here
                if (h.find('*') != string::npos || h.find('?') != string::npos) {
                    log.warning("Skipping host entry with wildcard in known_hosts: {}", h);
                continue;
            }
                if (keybin.empty()) {
                    log.warning("Skipping host '{}' due to base64 decode failure", h);
                continue;
            }
            m_hostKeys[h].push_back(keybin);
        }
    }

    return true;
}

void DccGenerator::processSpec(const std::string &spec, std::vector<OpenSSHSDestinationConstraint> &constraints)
{
    string left, right;
    size_t arrow = spec.find('>');
    if (arrow == string::npos) {
        left = ""; right = spec;
    } else {
        left = spec.substr(0, arrow);
        right = spec.substr(arrow+1);
    }

    auto parsePart = [](const string &s) -> pair<string,string> {
        size_t at = s.find('@');
        if (at==string::npos) return {"", s};
        return { s.substr(0, at), s.substr(at+1) };
    };

    auto leftp = parsePart(left);
    auto rightp = parsePart(right);

    vector<string> matchedRightHosts;
    for (const auto &kv : m_hostKeys) {
        const string &host = kv.first;
        if (matchGlob(rightp.second, host)) {
            matchedRightHosts.push_back(host);
        }
    }
    if (matchedRightHosts.empty()) {
        log.error("No keys found for destination pattern: {}", rightp.second);
        return;
    }

    vector<string> matchedLeftHosts;
    if (!left.empty()) {
        for (const auto &kv : m_hostKeys) {
            const string &host = kv.first;
            if (matchGlob(leftp.second, host)) {
                matchedLeftHosts.push_back(host);
            }
        }
        if (matchedLeftHosts.empty()) {
            log.error("No keys found for from pattern: {}", leftp.second);
            return;
        }
    }

    for (const auto &rhost : matchedRightHosts) {
        vector<OpenSSHHopKey> thisToKeys;
        for (const auto &b : m_hostKeys[rhost]) {
            OpenSSHHopKey hk; hk.key = b; hk.key_is_ca = false;
            thisToKeys.push_back(hk);
        }
        if (thisToKeys.empty()) continue;

        if (matchedLeftHosts.empty()) {
            OpenSSHHopDescriptor fromDesc(std::vector<OpenSSHHopKey>{}, std::string{}, std::string{});
            OpenSSHHopDescriptor toDesc(thisToKeys, rhost, rightp.first);
            constraints.push_back(OpenSSHSDestinationConstraint(fromDesc, toDesc));
        } else {
            for (const auto &lhost : matchedLeftHosts) {
                vector<OpenSSHHopKey> thisFromKeys;
                for (const auto &b : m_hostKeys[lhost]) {
                    OpenSSHHopKey hk; hk.key = b; hk.key_is_ca = false;
                    thisFromKeys.push_back(hk);
                }
                if (thisFromKeys.empty()) continue;
                OpenSSHHopDescriptor fromDesc(std::vector<OpenSSHHopKey>(thisFromKeys.begin(), thisFromKeys.end()), lhost, leftp.first);
                OpenSSHHopDescriptor toDesc(std::vector<OpenSSHHopKey>(thisToKeys.begin(), thisToKeys.end()), rhost, rightp.first);
                constraints.push_back(OpenSSHSDestinationConstraint(fromDesc, toDesc));
            }
        }
    }
}

bool DccGenerator::writeOutput(const std::vector<OpenSSHSDestinationConstraint> &constraints)
{
    Serializer out;
    for (const auto &c : constraints) {
        auto cb = c.serialize();
        out.writeBlob(cb);
    }

    Serializer top;
    top.writeBlob(out.data());
    auto data = top.data();
    ofstream fout(m_outPath, ios::binary);
    if (!fout) { log.error("Failed to open output: {}", m_outPath); return false; }
    fout.write(reinterpret_cast<const char*>(data.data()), data.size());
    fout.close();
    log.info("Wrote {} bytes to {}", data.size(), m_outPath);
    return true;
}

int DccGenerator::run(const std::vector<std::string>& specs)
{
    if (!parseKnownHosts()) return 3;

    vector<OpenSSHSDestinationConstraint> constraints;
    for (const auto &s : specs) processSpec(s, constraints);

    if (!writeOutput(constraints)) return 6;
    return 0;
}

