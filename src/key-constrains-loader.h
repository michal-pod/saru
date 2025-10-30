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
#include <string>

#include <libssha/key/key-manager.h>
#include <libssha/utils/logger.h>

namespace nglab
{
    namespace skym
    {
        using nglab::libssha::KeyBasePtr;
        using nglab::libssha::LogEnabler;

        class KeyConstrainsLoader : LogEnabler
        {
        public:
            static bool loadDestinationConstrains(KeyBasePtr key, const std::string &path = "", bool persistent = false);
            static void unloadDestinationConstrains(KeyBasePtr key, bool deletePersistent);

            static bool needConfirmation(KeyBasePtr key);
            static void setNeedConfirmation(KeyBasePtr key, bool need);

        private:
            KeyConstrainsLoader();

            static KeyConstrainsLoader &instance();
            bool loadConstraintsInt(KeyBasePtr key, const std::string &path = "", bool persistent = false);
            void unloadConstraintsInt(KeyBasePtr key, bool deletePersistent);

            bool needConfirmationInt(KeyBasePtr key);
            void setNeedConfirmationInt(KeyBasePtr key, bool need);
        };
    }
}
