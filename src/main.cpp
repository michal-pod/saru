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
#include <windows.h>

#include <libssha/utils/logger.h>
#include <libssha/key/key-manager.h>
#include <libssha/providers/botan/botan-lock-provider.h>

#include "config.h"
#include "debug-console.h"
#include "stdatl.h"
#include "dialogs/ckeylist.h"
#include "dialogs/csettings.h"
#include "servers/pipe-server.h"
#include "servers/pageant-server.h"
#include "ssh/process-info-extension.h"
using namespace nglab::saru;
using namespace nglab::libssha;

CAppModule _Module;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    InitCommonControls();

    if(FindWindow(_T("#32770"), _T("Loaded SSH keys list")) != NULL)
    {
        MessageBox(NULL, _T("SARU is already running."), _T("Information"), MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    WSAData wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        MessageBox(NULL, _T("WSAStartup failed"), _T("Error"), MB_OK | MB_ICONERROR);
        return -1;
    }

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    CSettingsSheet::initDefault();

    Logger log(Logger::instance(), "saru");

    // Read debug level from registry
    CRegKey key;
    if (key.Open(HKEY_CURRENT_USER, _T(SARU_KEY_ROOT), KEY_READ) == ERROR_SUCCESS)
    {
        DWORD val = 0;
        if (key.QueryDWORDValue(_T("DebugLevel"), val) == ERROR_SUCCESS)
        {
            Logger::instance().setLevel(static_cast<Logger::Level>(val));
            log.info("Log level set to {}", Logger::getLevelName(static_cast<Logger::Level>(val)));
        }
        key.Close();
    }

    ProcessInfoExtension::registerType();

    _Module.Init(NULL, hInstance);

    DebugConsole::instance().hide();
    Logger::instance().info("Application started");
    CMessageLoop theLoop;
    _Module.AddMessageLoop(&theLoop);

    [[ maybe_unused ]] auto &key_manager = KeyManager::instance();
    KeyFactory::initializeKeyTypes();
    ExtensionFactory::initializeExtensions();
    KeyManager::instance().setLockProvider(new BotanLockProvider);

    CKeyList& keyList = CKeyList::instance();
    if (keyList.Create(NULL) == NULL)
    {
        log.error("Failed to create Key List dialog");
        return -1;
    }
    theLoop.AddMessageFilter(&keyList);

    PipeServer server(R"(\\.\pipe\openssh-ssh-agent)");
    server.start();
    
    CPageantServer pageantServer;
    if (!pageantServer.Start())
    {
        log.error("Failed to initialize Pageant server");
    }
    
    theLoop.Run();

    theLoop.RemoveMessageFilter(&keyList);

    keyList.DestroyWindow();

    if (pageantServer.isRunning())
        pageantServer.Stop();

    server.stop();    

    _Module.RemoveMessageLoop();  

    CoUninitialize();

    _Module.Term();
    

    return 0;
}
