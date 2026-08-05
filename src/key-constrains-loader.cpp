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
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <libssha/extensions/openssh-restrict-destination.h>

#include "key-constrains-loader.h"
#include "stdatl.h"
#include "system/directories.h"
#include "dialogs/cdialogexception.h"
#include "config.h"

using namespace nglab::skym;
using nglab::libssha::Deserializer;
using nglab::libssha::KeyBase;
using nglab::libssha::KeyBasePtr;
using nglab::libssha::Logger;
using nglab::libssha::OpenSSHHopDescriptor;
using nglab::libssha::OpenSSHSDestinationConstraint;
using nglab::libssha::OpenSSHSRestrictDestination;
using nglab::libssha::PubKeyBase;

KeyConstrainsLoader::KeyConstrainsLoader()
    : LogEnabler("KeyConstrainsLoader")
{
}

KeyConstrainsLoader &KeyConstrainsLoader::instance()
{
    static KeyConstrainsLoader instance;
    return instance;
}

bool KeyConstrainsLoader::loadDestinationConstrains(KeyBasePtr key, const std::string &path, bool persistent)
{
    return instance().loadConstraintsInt(key, path, persistent);
}

void KeyConstrainsLoader::unloadDestinationConstrains(KeyBasePtr key, bool deletePersistent)
{
    instance().unloadConstraintsInt(key, deletePersistent);
}

bool KeyConstrainsLoader::needConfirmation(KeyBasePtr key)
{
    return instance().needConfirmationInt(key);
}

void KeyConstrainsLoader::setNeedConfirmation(KeyBasePtr key, bool need)
{
    instance().setNeedConfirmationInt(key, need);
}

bool KeyConstrainsLoader::loadConstraintsInt(KeyBasePtr key, const std::string &path, bool persistent)
{
    if (!key)
    {
        log.error("Cannot load constraints: key is null");
        throw nglab::skym::DialogException("Invalid key|Cannot load constraints: key is null");
    }

    // If path is empty then check registry if there is a stored path and signature
    // then load from there, verify signature using provided key
    std::string load_path = path;
    std::vector<uint8_t> signature;

    if (path.empty())
    {
        log.vdebug("No path specified, checking registry for stored constraints for key {}", key->fingerprint(PubKeyBase::FingerprintFormat::Sha256Hex));
        std::string key_name(SKYM_KEY_ROOT "\\KeyConstraints\\");
        key_name += key->fingerprint(PubKeyBase::Sha256Hex);
        CRegKey reg_key;
        if (reg_key.Open(HKEY_CURRENT_USER, key_name.c_str(), KEY_READ) == ERROR_SUCCESS)
        {
            ULONG path_len = 0;
            if (reg_key.QueryStringValue("Path", nullptr, &path_len) == ERROR_SUCCESS && path_len > 0)
            {
                std::vector<char> path_buf(path_len);
                if (reg_key.QueryStringValue("Path", path_buf.data(), &path_len) == ERROR_SUCCESS)
                {
                    load_path = std::string(path_buf.data());
                }
            }
            else {
                log.vdebug("No stored path found in registry for key {}", key->fingerprint(PubKeyBase::FingerprintFormat::Sha256Hex));
                return false;
            }

            ULONG sig_len = 0;
            if (reg_key.QueryBinaryValue("Signature", nullptr, &sig_len) == ERROR_SUCCESS && sig_len > 0)
            {
                signature.resize(sig_len);
                if (reg_key.QueryBinaryValue("Signature", signature.data(), &sig_len) != ERROR_SUCCESS)
                {
                    signature.clear();
                }
            }

            if(signature.empty()){
                log.vdebug("No stored signature found in registry for key {}", key->fingerprint(PubKeyBase::FingerprintFormat::Sha256Hex));
                return false;
            }
        }
        else {
            log.vdebug("No stored constraints found in registry for key {}", key->fingerprint(PubKeyBase::FingerprintFormat::Sha256Hex));
            return false;
        }
    }

    std::ifstream fileStream(load_path, std::ios::binary);
    if (fileStream)
    {
        fileStream.seekg(0, std::ios::end);
        size_t fileSize = fileStream.tellg();
            if (fileSize == 0 || fileSize > 512 * 1024)
        {
            log.error("Selected file {} is {}", load_path, fileSize == 0 ? "empty" : "too large");
            throw nglab::skym::DialogException(std::format("File error|Selected file {} is {}", load_path, fileSize == 0 ? "empty" : "too large"));
        }
        fileStream.seekg(0, std::ios::beg);

        std::vector<uint8_t> data;

        data.resize(fileSize);
        fileStream.read(reinterpret_cast<char *>(data.data()), fileSize);

        if(persistent){
            // copy the selected file into %APPDATA%\SKYM\<fingerprint>.cdc and use that path
            try {
                std::string appdir = Directories::getAppDataDirectoryA();
                std::filesystem::path skymdir = std::filesystem::path(appdir) / "SKYM";
                std::error_code ec;
                std::filesystem::create_directories(skymdir, ec);
                if (ec) {
                    log.error("Failed to create appdata SKYM directory: {}", ec.message());
                    throw nglab::skym::DialogException(std::format("Filesystem error|Failed to create appdata SKYM directory: {}", ec.message()));
                }
                std::filesystem::path src(load_path);
                std::string fname = key->fingerprint(PubKeyBase::FingerprintFormat::Sha256Hex) + ".cdc";
                std::filesystem::path dst = skymdir / fname;
                std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
                if (ec) {
                    log.error("Failed to copy constraints file to {}: {}", dst.string(), ec.message());
                    throw nglab::skym::DialogException(std::format("Filesystem error|Failed to copy constraints file to {}: {}", dst.string(), ec.message()));
                }
                load_path = dst.string();
                log.vdebug("Copied constraints to {}", load_path);
            } catch (const std::exception &e) {
                log.error("Failed to copy file to appdata: {}", e.what());
                return false;
            }

        }

        Deserializer deserializer(data);
        OpenSSHSRestrictDestination ext;
        try
        {
            ext.deserialize(deserializer);


            if (signature.empty())
            {
                signature = key->sign(data, 0);

                log.vdebug("Calculated signature for loaded constraints for key {}", key->fingerprint());
            }

            if (key->pubKey().verify(data, signature) == false)
            {
                log.error("Signature verification failed for loaded constraints for key {}", key->fingerprint());
                throw nglab::skym::DialogException("Signature verification failed|Signature verification failed for loaded constraints");
            }

            log.trace("Signature verification succeeded for loaded constraints for key {}, size of signature {}",
                 key->fingerprint(), signature.size());

            key->setDestConstraints(ext.constraints());

            log.debug("Loaded {} constraints for key {}", ext.constraints().size(), key->fingerprint());
        }
        catch (const std::exception &e)
        {
            log.error("Failed to deserialize file: {}", e.what());
            throw nglab::skym::DialogException(std::string("Deserialize error|") + e.what());
        }
    }
    else
    {
        log.error("Failed to open file: {}", load_path);
        throw nglab::skym::DialogException(std::format("File open error|Failed to open file: {}", load_path));
    }
    // Load constraints from the specified path, calculate signature then
    /// store path and signature in registry if persistent is true.
    if (persistent)
    {
        CRegKey reg_key;
        std::string key_name = SKYM_KEY_ROOT "\\KeyConstraints\\" + key->fingerprint(PubKeyBase::FingerprintFormat::Sha256Hex);
        if (reg_key.Create(HKEY_CURRENT_USER, key_name.c_str()) == ERROR_SUCCESS)
        {
            reg_key.SetStringValue("Path", load_path.c_str());
            reg_key.SetBinaryValue("Signature", signature.data(), static_cast<ULONG>(signature.size()));

            log.trace("Stored persistent constraints for key {}", key->fingerprint());
        }
        else
        {
            log.error("Failed to open/create registry key for storing constraints for key {}", key->fingerprint());
            throw nglab::skym::DialogException("Registry error|Failed to open/create registry key for storing constraints");
        }
    }

    return true;
}

void KeyConstrainsLoader::unloadConstraintsInt(KeyBasePtr key, bool deletePersistent)
{
    if (!key)
    {
        log.error("Cannot unload constraints: key is null");
        return;
    }

    // Unload constraints from memory
    key->setDestConstraints({});

    // If deletePersistent is true, remove stored path and signature from registry
    CRegKey reg_key;
    std::string key_name = SKYM_KEY_ROOT "\\KeyConstraints\\" + key->fingerprint(PubKeyBase::FingerprintFormat::Sha256Hex);
    if (deletePersistent && reg_key.Open(HKEY_CURRENT_USER, key_name.c_str(), KEY_WRITE) == ERROR_SUCCESS)
    {
        if (deletePersistent)
        {
            reg_key.DeleteValue("Path");
            reg_key.DeleteValue("Signature");

            log.trace("Deleted persistent constraints for key {}", key->fingerprint(PubKeyBase::FingerprintFormat::Sha256Hex));
        }
    }
}

bool KeyConstrainsLoader::needConfirmationInt(KeyBasePtr key)
{
    if (!key)
    {
        throw std::invalid_argument("Cannot check need confirmation: key is null");
    }

    CRegKey reg_key;
    std::string key_name = SKYM_KEY_ROOT "\\KeyConstraints\\" + key->fingerprint(PubKeyBase::FingerprintFormat::Sha256Hex);
    if (reg_key.Open(HKEY_CURRENT_USER, key_name.c_str(), KEY_READ) == ERROR_SUCCESS)
    {
        DWORD needConfirm = 0;
        if (reg_key.QueryDWORDValue("NeedConfirmation", needConfirm) == ERROR_SUCCESS)
        {
            return needConfirm != 0;
        }
    }

    return false;
}


void KeyConstrainsLoader::setNeedConfirmationInt(KeyBasePtr key, bool need)
{
    if (!key)
    {
        throw std::invalid_argument("Cannot set need confirmation: key is null");
    }

    CRegKey reg_key;
    std::string key_name = SKYM_KEY_ROOT "\\KeyConstraints\\" + key->fingerprint(PubKeyBase::FingerprintFormat::Sha256Hex);
    if (reg_key.Create(HKEY_CURRENT_USER, key_name.c_str()) == ERROR_SUCCESS)
    {
        reg_key.SetDWORDValue("NeedConfirmation", need ? 1 : 0);

        log.trace("Set need confirmation to {} for key {}", need, key->fingerprint(PubKeyBase::FingerprintFormat::Sha256Hex));
    }
    else
    {
        log.error("Failed to open/create registry key for setting need confirmation for key {}", key->fingerprint());
        throw nglab::skym::DialogException("Registry error|Failed to open/create registry key for setting need confirmation");
    }
}
