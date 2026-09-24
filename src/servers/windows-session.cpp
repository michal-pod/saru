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
#include "windows-session.h"

#include <chrono>
#include <format>
#include <unordered_map>

#include <windows.h>
#include <atlbase.h>

#include <libssha/key/key.h>
#include <libssha/messages/extension.h>

#include "config.h"
#include "dialogs/ckeyconfirm.h"
#include "dialogs/ckeyselection.h"
#include "key-constrains-loader.h"
#include "ssh/process-info-extension.h"

namespace nglab::saru
{
    namespace
    {
        enum ConfirmationStatus
        {
            NotRemembered,
            RememberedAsConfirmed,
            RememberedAsDenied
        };

        struct ConfirmationEntry
        {
            ConfirmationStatus status;
            std::chrono::steady_clock::time_point expiryTime;
        };

        std::unordered_map<std::string, ConfirmationEntry> confirmationMemory;

        void removeExpiredConfirmations()
        {
            const auto now = std::chrono::steady_clock::now();
            for (auto it = confirmationMemory.begin(); it != confirmationMemory.end();)
            {
                if (it->second.expiryTime <= now)
                {
                    it = confirmationMemory.erase(it);
                }
                else
                {
                    ++it;
                }
            }
        }

        ConfirmationStatus rememberedConfirmation(const std::string &fingerprint)
        {
            removeExpiredConfirmations();
            const auto it = confirmationMemory.find(fingerprint);
            if (it != confirmationMemory.end())
            {
                return it->second.status;
            }
            return NotRemembered;
        }

        void rememberConfirmation(const std::string &fingerprint, ConfirmationStatus status, int durationSeconds)
        {
            removeExpiredConfirmations();
            auto &entry = confirmationMemory[fingerprint];
            entry.status = status;
            entry.expiryTime = std::chrono::steady_clock::now() + std::chrono::seconds(durationSeconds);
        }
    }

    template <typename T, WindowsSessionType SessionType>
    WindowsSession<T, SessionType>::WindowsSession() = default;

    template <typename T, WindowsSessionType SessionType>
    bool WindowsSession<T, SessionType>::findBinary()
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

    template <typename T, WindowsSessionType SessionType>
    bool WindowsSession<T, SessionType>::confirmRequest(const KeyBase &key)
    {
        const auto confirmationStatus = rememberedConfirmation(key.fingerprint());
        if (confirmationStatus == RememberedAsConfirmed || confirmationStatus == RememberedAsDenied)
        {
            log.trace("Using remembered confirmation status for key {}: {}",
                      key.fingerprint(),
                      confirmationStatus == RememberedAsConfirmed ? "confirmed" : "denied");
            return confirmationStatus == RememberedAsConfirmed;
        }

        clientInfo.IsForwarded = isForwarded();

        CKeyConfirm confirmDialog(key, clientInfo, SessionType);
        const auto dialogResult = confirmDialog.DoModal();
        const bool confirmed = dialogResult == IDYES;
        log.debug("Confirmation dialog result: {}", confirmed ? "confirmed" : "denied");

        if (confirmDialog.getRememberKey())
        {
            const int rememberTime = confirmDialog.getRememberTime();
            log.info("User chose to remember key for {} seconds", rememberTime);

            rememberConfirmation(
                key.fingerprint(),
                confirmed ? RememberedAsConfirmed : RememberedAsDenied,
                rememberTime);
        }

        log.info("Key {} was {}", key.fingerprint(), confirmed ? "confirmed" : "denied");
        return confirmed;
    }

    template <typename T, WindowsSessionType SessionType>
    bool WindowsSession<T, SessionType>::requiresConfirmation(const KeyBasePtr key) const
    {
        CRegKey regKey;
        if (regKey.Open(HKEY_CURRENT_USER, SARU_KEY_ROOT, KEY_READ) == ERROR_SUCCESS)
        {
            DWORD value;
            if (regKey.QueryDWORDValue("AlwaysConfirm", value) == ERROR_SUCCESS && value != 0)
            {
                return true;
            }
        }

        return KeyConstrainsLoader::needConfirmation(key);
    }

    template <typename T, WindowsSessionType SessionType>
    void WindowsSession<T, SessionType>::processRequestIdentities(const nglab::libssha::Message &msg)
    {
        CRegKey key;
        if (key.Open(HKEY_CURRENT_USER, SARU_KEY_ROOT, KEY_READ) != ERROR_SUCCESS)
        {
            return;
        }

        DWORD showKeySelection = 0;
        if (key.QueryDWORDValue("ShowKeySelection", showKeySelection) == ERROR_SUCCESS && showKeySelection == 1)
        {
            log.debug("Showing key selection dialog for identities request");

            nglab::libssha::IdentitiesAnswerMessage responseMessage;
            auto &keyManager = nglab::libssha::KeyManager::instance();
            const auto items = keyManager.listKeys(*this);
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
                    responseMessage.addIdentity(item.blob, item.comment);
                }
            }
            else if (items.size() == 1)
            {
                const auto &item = items[0];
                log.debug("Only one key available, adding it without selection dialog");
                responseMessage.addIdentity(item.blob, item.comment);
            }
            auto response = responseMessage.serialize();
            send(response);
            return;
        }

        log.debug("Processing identities request without key selection dialog");
        static_cast<nglab::libssha::Session &>(*this).nglab::libssha::Session::processRequestIdentities(msg);
    }

    template <typename T, WindowsSessionType SessionType>
    bool WindowsSession<T, SessionType>::processExtensionMessage(const ExtensionMessage &msg)
    {
        if (msg.extensionName() != "proc-info@nglab.net")
        {
            return false;
        }
        const auto *extension = dynamic_cast<const ProcessInfoExtension *>(msg.extension().get());
        if (!extension)
        {
            log.error("Failed to cast extension to ProcessInfoExtension");
            return false;
        }

        clientInfo.ClientInfo = std::format("{} (user {}/{})", extension->path().front(), extension->user(), extension->uid());
        clientInfo.ClientPath = extension->path().front();
        clientInfo.HopPaths = extension->path();
        clientInfo.User = std::format("{} ({})", extension->user(), extension->uid());
        clientInfo.Distro = extension->distro();
        clientInfo.SystemType = extension->type();

        return true;
    }

    template <typename T, WindowsSessionType SessionType>
    std::string WindowsSession<T, SessionType>::client() const
    {
        if (!clientInfo.ClientInfo.empty())
        {
            return clientInfo.ClientInfo;
        }

        const std::string fileName = clientInfo.ClientPath.substr(clientInfo.ClientPath.find_last_of("\\/") + 1);
        if (fileName.empty())
        {
            return "Unknown";
        }
        return fileName;
    }

    template <typename T, WindowsSessionType SessionType>
    const std::string WindowsSession<T, SessionType>::VMName() const
    {
        return clientInfo.VMName;
    }

    template class WindowsSession<NamedPipeSession, WindowsSessionType::Pipe>;
    template class WindowsSession<PageantSession, WindowsSessionType::Pageant>;
    template class WindowsSession<HyperVSession, WindowsSessionType::HyperV>;
}

