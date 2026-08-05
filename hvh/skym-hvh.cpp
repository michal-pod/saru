/*
 SKYM - SSH KeY Manager
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
#include <stdio.h>
#include <atlbase.h>
#include <comdef.h>
#include <Wbemidl.h>
#include <string>
#include <config.h>

std::wstring VariantToWString(const CComVariant &var)
{
    if (var.vt == VT_BSTR && var.bstrVal != NULL)
    {
        return std::wstring(var.bstrVal, SysStringLen(var.bstrVal));
    }
    return L"";
}

DWORD VariantToDWORD(const CComVariant &var, DWORD defaultValue = 0)
{
    if (var.vt == VT_I4)
    {
        return static_cast<DWORD>(var.lVal);
    }
    return defaultValue;
}

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
//int wmain(int argc, wchar_t *argv[])
{
    HRESULT hr = S_OK;
    hr = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        fprintf(stderr, "Failed to initialize COM library: 0x%08X\n", hr);
        return -1;
    }

    hr = CoInitializeSecurity(
        NULL,
        -1,
        NULL,
        NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE,
        NULL);
    if (FAILED(hr))
    {
        fprintf(stderr, "Failed to initialize COM security: 0x%08X\n", hr);
        CoUninitialize();
        return -1;
    }

    CComPtr<IWbemLocator> pLoc;
    hr = CoCreateInstance(
        CLSID_WbemLocator,
        0,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID *)&pLoc);

    if (FAILED(hr))
    {
        fprintf(stderr, "Failed to create IWbemLocator instance: 0x%08X\n", hr);
        CoUninitialize();
        return -1;
    }

    CComPtr<IWbemServices> pSvc;
    _bstr_t bstrNamespace = L"ROOT\\virtualization\\v2";

    hr = pLoc->ConnectServer(
        bstrNamespace,
        NULL,
        NULL,
        0,
        NULL,
        0,
        0,
        &pSvc);

    if (FAILED(hr))
    {
        fprintf(stderr, "Failed to connect to WMI namespace: 0x%08X\n", hr);
        CoUninitialize();
        return -1;
    }

    hr = CoSetProxyBlanket(
        pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE);

    if (FAILED(hr))
    {
        fprintf(stderr, "Failed to set proxy blanket: 0x%08X\n", hr);
        CoUninitialize();
        return -1;
    }

    // założenia: m_spSvc jest IWbemServices (po ConnectServer i CoSetProxyBlanket)

    CComBSTR queryVssd(L"SELECT ElementName, InstanceID FROM Msvm_VirtualSystemSettingData");
    CComPtr<IEnumWbemClassObject> spEnumVssd;
    hr = pSvc->ExecQuery(CComBSTR(L"WQL"), queryVssd,
                                    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &spEnumVssd);
    if (FAILED(hr))
    {
        fprintf(stderr, "Failed to execute WMI query: 0x%08X\n", hr);
        CoUninitialize();        
        return hr;
    }

    CComPtr<IWbemClassObject> spVssdObj;
    ULONG uReturned = 0;
    while (SUCCEEDED(spEnumVssd->Next(WBEM_INFINITE, 1, &spVssdObj, &uReturned)) && uReturned)
    {
        CComVariant vElementName, vInstanceID, vPath;
        spVssdObj->Get(CComBSTR(L"ElementName"), 0, &vElementName, NULL, NULL);
        spVssdObj->Get(CComBSTR(L"InstanceID"), 0, &vInstanceID, NULL, NULL);
        // Get object's __RELPATH or __PATH system property to use in ASSOCIATORS query
        spVssdObj->Get(CComBSTR(L"__RELPATH"), 0, &vPath, NULL, NULL);
        std::wstring elementName = VariantToWString(vElementName);
        std::wstring instanceID = VariantToWString(vInstanceID);
        std::wstring relPath = VariantToWString(vPath); // np. "Msvm_VirtualSystemSettingData.InstanceID=\"...\""

        // Build associators query (ResultClass=Msvm_ComputerSystem)
        // Note: __RELPATH may contain quotes; wrap in braces
        std::wstring assocQuery = L"ASSOCIATORS OF {";
        assocQuery += relPath;
        assocQuery += L"} WHERE ResultClass = Msvm_ComputerSystem";

        CComPtr<IEnumWbemClassObject> spAssocEnum;
        hr = pSvc->ExecQuery(CComBSTR(L"WQL"), CComBSTR(assocQuery.c_str()),
                                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &spAssocEnum);
        if (SUCCEEDED(hr) && spAssocEnum)     
        {
            CComPtr<IWbemClassObject> spCsObj;
            ULONG uGot = 0;
            hr = spAssocEnum->Next(WBEM_INFINITE, 1, &spCsObj, &uGot);
            if (SUCCEEDED(hr) && uGot)
            {
                // mamy powiązany ComputerSystem — pobierz Name (GUID) i EnabledState
                CComVariant vName, vEnabled, vDescription;
                spCsObj->Get(CComBSTR(L"Name"), 0, &vName, NULL, NULL);
                spCsObj->Get(CComBSTR(L"EnabledState"), 0, &vEnabled, NULL, NULL);
                spCsObj->Get(CComBSTR(L"Description"), 0, &vDescription, NULL, NULL);

                std::wstring vmGuid = VariantToWString(vName);
                DWORD enabledState = VariantToDWORD(vEnabled, 0);
                std::wstring description = VariantToWString(vDescription);
                fwprintf(stdout, L"VM: %s\n  ElementName: %s\n  InstanceID: %s\n  EnabledState: %u\n  Description: %s\n\n",
                        vmGuid.c_str(), elementName.c_str(), instanceID.c_str(), enabledState, description.c_str());
                CRegKey regKey;
                std::wstring regPath = std::wstring(SKYM_KEY_ROOT_W) + L"\\HyperVVMs\\" + vmGuid;
                // Create the registry key
                bool created = false;
                DWORD dwDisposition;
                if (regKey.Create(HKEY_CURRENT_USER, regPath.c_str(), 0, 0, KEY_WRITE, NULL, &dwDisposition) == ERROR_SUCCESS)
                {

                    created = (dwDisposition == REG_CREATED_NEW_KEY);
                    fprintf(stdout, "Created registry key: %ls dwDisposition: %lu vs %lu created: %d\n",
                         regPath.c_str(), dwDisposition, REG_CREATED_NEW_KEY, created);
                    // Successfully created or opened the key
                }
                
                    fprintf(stdout, "Opened registry key: %ls\n", regPath.c_str());
                    if(created){
                        fprintf(stdout, "Setting enabled state to 0.\n");
                        regKey.SetDWORDValue(L"Enabled", 0);
                    }

                    regKey.SetStringValue(L"Description", elementName.c_str());
                    regKey.Close();

                // teraz zapisz vmGuid, elementName, enabledState do rejestru...
            }
        }
        else
        {
            // brak powiązanego ComputerSystem — to może być wskazówka, że to nie VM albo WMI inaczej skonfigurowane
        }

        spVssdObj.Release();
    }

    CoUninitialize();
    return 0;
}
