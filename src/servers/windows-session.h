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
#include <windows.h>
#include <atlbase.h>

#include <libssha/utils/logger.h>
#include <libssha/agent/session.h>
#include <libssha/messages/extension.h>

#include "dialogs/ckeyconfirm.h"
#include "dialogs/ckeyselection.h"
#include "ssh/process-info-extension.h"
#include "key-constrains-loader.h"
#include "client-info.h"
#include "config.h"

namespace nglab
{
    namespace skym
    {
        using nglab::libssha::ExtensionMessage;
        using nglab::libssha::LogEnabler;

        enum WindowsSessionType : uint8_t
        {
            Pipe,
            Pageant,
            HyperV
        };

        class ConfirmMemory
        {
        public:
            enum RememberType
            {
                NotRemember,
                RememberedAsConfirmed,
                RememberedAsDenied
            };

            static ConfirmMemory &instance()
            {
                static ConfirmMemory instance;
                return instance;
            }

            static RememberType get(const std::string &fingerprint)
            {
                removeExpired();
                auto it = ms_memory.find(fingerprint);
                if (it != ms_memory.end())
                {
                    return it->second.m_type;
                }
                return NotRemember;
            }

            static void set(const std::string &fingerprint, RememberType type, int durationSeconds)
            {
                removeExpired();
                auto &entry = ms_memory[fingerprint];
                entry.m_type = type;
                entry.m_expiryTime = std::chrono::steady_clock::now() + std::chrono::seconds(durationSeconds);
            }

        private:
            static void removeExpired()
            {
                auto now = std::chrono::steady_clock::now();
                for (auto it = ms_memory.begin(); it != ms_memory.end();)
                {
                    if (it->second.m_expiryTime <= now)
                    {
                        it = ms_memory.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
            static std::unordered_map<std::string, ConfirmMemory> ms_memory;
            std::chrono::steady_clock::time_point m_expiryTime;
            RememberType m_type;
        };

        template <typename T, WindowsSessionType SessionType>
        class WindowsSession : virtual public LogEnabler, virtual public nglab::libssha::Session
        {
        public:
            WindowsSession() {}
            bool findBinary()
            {
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, clientInfo.ClientPid);
                if (!hProcess)
                {
                    log.error("OpenProcess failed");
                    return false;
                }

                char pathBuffer[MAX_PATH];
                DWORD pathSize = sizeof(pathBuffer);
                if (!QueryFullProcessImageNameA(hProcess, 0, pathBuffer, &pathSize))
                {
                    log.error("QueryFullProcessImageName failed");
                    CloseHandle(hProcess);
                    return false;
                }
                clientInfo.ClientPath = pathBuffer;

                log.debug("Client PID={}, Path={}", clientInfo.ClientPid, clientInfo.ClientPath);

                CloseHandle(hProcess);

                return true;
            }

            bool confirmRequest(const KeyBase &key)
            {
                auto rememberStatus = ConfirmMemory::get(key.fingerprint());
                if (rememberStatus == ConfirmMemory::RememberedAsConfirmed || rememberStatus == ConfirmMemory::RememberedAsDenied)
                {
                    log.trace("Using remembered confirmation status for key {}: {}",
                             key.fingerprint(),
                             (rememberStatus == ConfirmMemory::RememberedAsConfirmed) ? "confirmed" : "denied");
                    return rememberStatus == ConfirmMemory::RememberedAsConfirmed;
                }

                clientInfo.IsForwarded = isForwarded();

                CKeyConfirm confirmDialog(
                    key,
                    clientInfo,
                    SessionType);

                auto dialogResult = confirmDialog.DoModal();

                const bool confirmed = dialogResult == IDYES;
                log.debug("Confirmation dialog result: {}", confirmed ? "confirmed" : "denied");

                if (confirmDialog.getRememberKey())
                {
                    int rememberTime = confirmDialog.getRememberTime();
                    log.info("User chose to remember key for {} seconds",
                             rememberTime);

                    ConfirmMemory::set(
                        key.fingerprint(),
                        confirmed ? ConfirmMemory::RememberedAsConfirmed : ConfirmMemory::RememberedAsDenied,
                        rememberTime);
                }

                log.info("Key {} was {}", key.fingerprint(), confirmed ? "confirmed" : "denied");
                return confirmed;
            }

            bool requiresConfirmation(const KeyBasePtr key) const
            {
                CRegKey reg_key;
                if (reg_key.Open(HKEY_CURRENT_USER, SKYM_KEY_ROOT, KEY_READ) == ERROR_SUCCESS)
                {
                    DWORD val;
                    if (reg_key.QueryDWORDValue("AlwaysConfirm", val) == ERROR_SUCCESS)
                    {
                        if(val != 0){
                            return true;
                        }                        
                    }
                }

                return KeyConstrainsLoader::needConfirmation(key);
            }

            void processRequestIdentities(const nglab::libssha::Message &msg)
            {
                CRegKey key;
                if (key.Open(HKEY_CURRENT_USER, SKYM_KEY_ROOT, KEY_READ) == ERROR_SUCCESS)
                {
                    DWORD showKeySelection = 0;
                    if (key.QueryDWORDValue("ShowKeySelection", showKeySelection) == ERROR_SUCCESS && showKeySelection == 1)
                    {
                        log.debug("Showing key selection dialog for identities request");

                        using nglab::libssha::IdentitiesAnswerMessage;
                        IdentitiesAnswerMessage response_msg;
                        auto &km = nglab::libssha::KeyManager::instance();
                        auto items = km.listKeys(*this);
                        if (items.size() > 1)
                        {
                            CKeySelectionDlg keySelectionDialog(
                                [this]()
                                {
                                    return nglab::libssha::KeyManager::instance().listKeys(*this);
                                },
                                clientInfo,
                                SessionType);
                            if (keySelectionDialog.DoModal() == IDOK)
                            {
                                const auto &item = keySelectionDialog.selectedItem();
                                log.debug("User selected key {}", item.fingerprint);
                                response_msg.addIdentity(item.blob, item.comment);
                            }
                        }
                        else if (items.size() == 1)
                        {
                            const auto &item = items[0];
                            log.debug("Only one key available, adding it without selection dialog");
                            response_msg.addIdentity(item.blob, item.comment);
                        }
                        auto response = response_msg.serialize();
                        send(response);
                    }
                    else
                    {
                        log.debug("Processing identities request without key selection dialog");
                        static_cast<nglab::libssha::Session &>(*this).nglab::libssha::Session::processRequestIdentities(msg);
                    }
                }
            }

            bool processExtensionMessage(const ExtensionMessage &msg)
            {
                if (msg.extensionName() != "proc-info@nglab.net")
                {
                    return false;
                }
                const ProcessInfoExtension *ext = dynamic_cast<const ProcessInfoExtension *>(msg.extension().get());
                if (!ext)
                {
                    log.error("Failed to cast extension to ProcessInfoExtension");
                    return false;
                }

                clientInfo.ClientInfo = std::format("{} (user {}/{})", ext->path().front(), ext->user(), ext->uid());
                clientInfo.ClientPath = ext->path().front();
                clientInfo.HopPaths = ext->path();
                clientInfo.User = std::format("{} ({})", ext->user(), ext->uid());
                clientInfo.Distro = ext->distro();
                clientInfo.SystemType = ext->type();

                return true;
            }

            std::string client() const
            {
                if (clientInfo.ClientInfo.size() > 0)
                {
                    return clientInfo.ClientInfo;
                }
                std::string sFileName = clientInfo.ClientPath.substr(clientInfo.ClientPath.find_last_of("\\/") + 1);

                if (sFileName.size() == 0)
                {
                    return "Unknown";
                }

                return sFileName;
            }

            const std::string VMName() const
            {
                return clientInfo.VMName;
            }

        protected:
            ClientInfo clientInfo;
        };
    }
}
