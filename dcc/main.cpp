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
#include "dcc_generator.h"
#include <vector>
#include <string>
#include <libssha/utils/logger.h>

int main(int argc, char **argv)
{
    using nglab::libssha::Logger;
    if (argc < 4) {
        Logger::instance().info("Usage: saru-dcc <known_hosts> <outfile> <constraints...>");
        return 2;
    }

    std::string inpath = argv[1];
    std::string outpath = argv[2];
    std::vector<std::string> specs;
    for (int i = 3; i < argc; ++i) specs.push_back(argv[i]);

    DccGenerator gen(inpath, outpath);
    return gen.run(specs);
}

