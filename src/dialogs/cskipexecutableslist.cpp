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
#include "cskipexecutableslist.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>

#include <shellapi.h>
#include <shobjidl.h>

#include "config.h"
#include "factories/icon-factory.h"

namespace nglab::saru
{
    CSkipExecutablesList::CSkipExecutablesList()
        : LogEnabler("CSkipExecutablesList")
    {
    }

    std::vector<std::filesystem::path> CSkipExecutablesList::buildDefaullOriginatingProcessSkipList()
    {
        const auto environmentVariable = [](const char *name) -> std::optional<std::string>
        {
            const DWORD requiredSize = GetEnvironmentVariableA(name, nullptr, 0);
            if (requiredSize == 0)
            {
                return std::nullopt;
            }

            std::string value(requiredSize, '\0');
            const DWORD length = GetEnvironmentVariableA(name, value.data(), requiredSize);
            if (length == 0 || length >= requiredSize)
            {
                return std::nullopt;
            }
            value.resize(length);
            return value;
        };

        const auto isExecutableFile = [](const std::filesystem::path &path)
        {
            std::error_code error;
            return std::filesystem::is_regular_file(path, error);
        };

        std::vector<std::filesystem::path> skippedExecutables;

        std::array<char, MAX_PATH> systemDirectory{};
        const UINT systemDirectoryLength = GetSystemDirectoryA(
            systemDirectory.data(),
            static_cast<UINT>(systemDirectory.size()));
        if (systemDirectoryLength > 0 && systemDirectoryLength < static_cast<UINT>(systemDirectory.size()))
        {
            const std::filesystem::path systemDirectoryPath(systemDirectory.data());
            skippedExecutables.emplace_back(systemDirectoryPath / "OpenSSH" / "ssh.exe");
            skippedExecutables.emplace_back(systemDirectoryPath / "cmd.exe");
        }
        else
        {
            skippedExecutables.emplace_back("ssh.exe");
            skippedExecutables.emplace_back("cmd.exe");
        }

        std::vector<std::filesystem::path> programFilesDirectories;
        for (const char *variable : {"ProgramW6432", "ProgramFiles", "ProgramFiles(x86)"})
        {
            if (const auto directory = environmentVariable(variable))
            {
                const std::filesystem::path path(*directory);
                if (std::find(programFilesDirectories.begin(), programFilesDirectories.end(), path) == programFilesDirectories.end())
                {
                    programFilesDirectories.push_back(path);
                }
            }
        }

        for (const auto &directory : programFilesDirectories)
        {
            const auto sshExecutable = directory / "OpenSSH" / "ssh.exe";
            if (isExecutableFile(sshExecutable))
            {
                skippedExecutables.push_back(sshExecutable);
            }
        }

        std::vector<std::filesystem::path> plinkCandidates;
        for (const auto &directory : programFilesDirectories)
        {
            plinkCandidates.emplace_back(directory / "PuTTY" / "plink.exe");
        }
        if (const auto localAppData = environmentVariable("LocalAppData"))
        {
            plinkCandidates.emplace_back(std::filesystem::path(*localAppData) / "Programs" / "PuTTY" / "plink.exe");
        }

        for (const auto &candidate : plinkCandidates)
        {
            if (isExecutableFile(candidate))
            {
                skippedExecutables.push_back(candidate);
                return skippedExecutables;
            }
        }

        std::array<char, 32768> path{};
        const DWORD pathLength = SearchPathA(
            nullptr,
            "plink.exe",
            nullptr,
            static_cast<DWORD>(path.size()),
            path.data(),
            nullptr);
        if (pathLength > 0 && pathLength < static_cast<DWORD>(path.size()) && isExecutableFile(path.data()))
        {
            skippedExecutables.emplace_back(std::string(path.data(), pathLength));
        }

        return skippedExecutables;
    }

    void CSkipExecutablesList::initDefault()
    {
        CRegKey key;
        if (key.Create(HKEY_CURRENT_USER, SARU_KEY_ROOT) != ERROR_SUCCESS)
        {
            return;
        }

        ULONG valueLength = 0;
        if (key.QueryMultiStringValue(
                _T("OriginatingProcessSkipExecutables"),
                nullptr,
                &valueLength) != ERROR_FILE_NOT_FOUND)
        {
            return;
        }

        const auto skippedExecutables = buildDefaullOriginatingProcessSkipList();
        std::vector<char> multiString;
        for (const auto &executable : skippedExecutables)
        {
            const auto path = executable.string();
            multiString.insert(multiString.end(), path.begin(), path.end());
            multiString.push_back('\0');
        }
        multiString.push_back('\0');
        key.SetMultiStringValue(
            _T("OriginatingProcessSkipExecutables"),
            multiString.data());
    }

    LRESULT CSkipExecutablesList::OnInitDialog(UINT, WPARAM, LPARAM, BOOL &)
    {
        m_list.Attach(GetDlgItem(IDC_SKIP_EXECUTABLES_LIST));
        m_list.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        CRect listClientRect;
        m_list.GetClientRect(&listClientRect);
        m_list.InsertColumn(0, _T("Executable path"), LVCFMT_LEFT, listClientRect.Width());

        m_addButton.Attach(GetDlgItem(IDC_SKIP_EXECUTABLES_ADD));
        m_addButton.SetIcon(IconFactory::get(IDI_ADD));
        m_removeButton.Attach(GetDlgItem(IDC_SKIP_EXECUTABLES_REMOVE));
        m_removeButton.SetIcon(IconFactory::get(IDI_BIN));
        m_cancelButton.Attach(GetDlgItem(IDCANCEL));
        m_cancelButton.SetIcon(IconFactory::get(IDI_CROSS));
        m_saveButton.Attach(GetDlgItem(IDOK));
        m_saveButton.SetIcon(IconFactory::get(IDI_ACCEPT));

        loadExecutables();
        refreshList();
        updateRemoveButton();
        CenterWindow(GetParent());
        return TRUE;
    }

    LRESULT CSkipExecutablesList::OnSelectionChanged(int, LPNMHDR, BOOL &)
    {
        updateRemoveButton();
        return 0;
    }

    LRESULT CSkipExecutablesList::OnAdd(WORD, WORD, HWND, BOOL &)
    {
        CComPtr<IFileOpenDialog> fileDialog;
        HRESULT result = CoCreateInstance(
            CLSID_FileOpenDialog,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&fileDialog));
        if (FAILED(result))
        {
            log.error("Failed to create executable file dialog: {}", static_cast<unsigned long>(result));
            return 0;
        }

        DWORD options = 0;
        if (SUCCEEDED(fileDialog->GetOptions(&options)))
        {
            fileDialog->SetOptions(options | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM);
        }
        const COMDLG_FILTERSPEC filters[] = {
            {L"Executable files", L"*.exe"},
            {L"All files", L"*.*"}};
        fileDialog->SetFileTypes(ARRAYSIZE(filters), filters);
        fileDialog->SetDefaultExtension(L"exe");

        result = fileDialog->Show(m_hWnd);
        if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        {
            return 0;
        }
        if (FAILED(result))
        {
            log.error("Failed to show executable file dialog: {}", static_cast<unsigned long>(result));
            return 0;
        }

        CComPtr<IShellItem> selectedItem;
        if (FAILED(fileDialog->GetResult(&selectedItem)))
        {
            return 0;
        }

        PWSTR selectedPath = nullptr;
        if (FAILED(selectedItem->GetDisplayName(SIGDN_FILESYSPATH, &selectedPath)))
        {
            return 0;
        }

        const std::filesystem::path executablePath(selectedPath);
        CoTaskMemFree(selectedPath);
        if (contains(executablePath))
        {
            return 0;
        }

        m_executables.push_back(executablePath);
        refreshList();
        const int addedIndex = static_cast<int>(m_executables.size() - 1);
        m_list.SetItemState(addedIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        m_list.EnsureVisible(addedIndex, FALSE);
        updateRemoveButton();
        return 0;
    }

    LRESULT CSkipExecutablesList::OnRemove(WORD, WORD, HWND, BOOL &)
    {
        const int selectedIndex = m_list.GetNextItem(-1, LVNI_SELECTED);
        if (selectedIndex < 0 || selectedIndex >= static_cast<int>(m_executables.size()))
        {
            return 0;
        }

        m_executables.erase(m_executables.begin() + selectedIndex);
        refreshList();
        if (!m_executables.empty())
        {
            const int nextIndex = std::min(selectedIndex, static_cast<int>(m_executables.size() - 1));
            m_list.SetItemState(nextIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        }
        updateRemoveButton();
        return 0;
    }

    LRESULT CSkipExecutablesList::OnSave(WORD, WORD, HWND, BOOL &)
    {
        if (!saveExecutables())
        {
            MessageBox(
                _T("Failed to save the skipped executables list."),
                _T("Save failed"),
                MB_OK | MB_ICONERROR);
            return 0;
        }

        EndDialog(IDOK);
        return 0;
    }

    LRESULT CSkipExecutablesList::OnCancel(WORD, WORD, HWND, BOOL &)
    {
        EndDialog(IDCANCEL);
        return 0;
    }

    void CSkipExecutablesList::loadExecutables()
    {
        m_executables.clear();

        CRegKey key;
        if (key.Open(HKEY_CURRENT_USER, SARU_KEY_ROOT, KEY_READ) != ERROR_SUCCESS)
        {
            return;
        }

        ULONG valueLength = 0;
        if (key.QueryMultiStringValue(
                _T("OriginatingProcessSkipExecutables"),
                nullptr,
                &valueLength) != ERROR_SUCCESS ||
            valueLength < 2)
        {
            return;
        }

        std::vector<char> value(valueLength);
        if (key.QueryMultiStringValue(
                _T("OriginatingProcessSkipExecutables"),
                value.data(),
                &valueLength) != ERROR_SUCCESS)
        {
            return;
        }

        for (const char *entry = value.data(); *entry != '\0'; entry += std::char_traits<char>::length(entry) + 1)
        {
            m_executables.emplace_back(entry);
        }
    }

    bool CSkipExecutablesList::saveExecutables()
    {
        CRegKey key;
        if (key.Create(HKEY_CURRENT_USER, SARU_KEY_ROOT) != ERROR_SUCCESS)
        {
            return false;
        }

        std::vector<char> value;
        for (const auto &executable : m_executables)
        {
            const auto path = executable.string();
            value.insert(value.end(), path.begin(), path.end());
            value.push_back('\0');
        }
        value.push_back('\0');
        if (m_executables.empty())
        {
            value.push_back('\0');
        }

        return key.SetValue(
                   _T("OriginatingProcessSkipExecutables"),
                   REG_MULTI_SZ,
                   value.data(),
                   static_cast<ULONG>(value.size())) == ERROR_SUCCESS;
    }

    void CSkipExecutablesList::refreshList()
    {
        m_list.DeleteAllItems();

        for (size_t index = 0; index < m_executables.size(); ++index)
        {
            const auto path = m_executables[index].string();
            SHFILEINFOA fileInfo{};
            UINT flags = SHGFI_SYSICONINDEX | SHGFI_LARGEICON;
            DWORD attributes = 0;
            std::error_code error;
            if (!std::filesystem::is_regular_file(m_executables[index], error))
            {
                flags |= SHGFI_USEFILEATTRIBUTES;
                attributes = FILE_ATTRIBUTE_NORMAL;
            }

            const auto imageList = reinterpret_cast<HIMAGELIST>(
                SHGetFileInfoA(path.c_str(), attributes, &fileInfo, sizeof(fileInfo), flags));
            if (!m_systemImageList && imageList)
            {
                m_systemImageList = imageList;
                m_list.SetImageList(m_systemImageList, LVSIL_SMALL);
            }

            m_list.InsertItem(
                static_cast<int>(index),
                path.c_str(),
                imageList ? fileInfo.iIcon : 0);
        }
    }

    void CSkipExecutablesList::updateRemoveButton()
    {
        m_removeButton.EnableWindow(m_list.GetSelectedCount() > 0);
    }

    bool CSkipExecutablesList::contains(const std::filesystem::path &path) const
    {
        const auto normalizedPath = path.lexically_normal().wstring();
        for (const auto &executable : m_executables)
        {
            const auto normalizedExecutable = executable.lexically_normal().wstring();
            if (CompareStringOrdinal(
                    normalizedPath.c_str(),
                    static_cast<int>(normalizedPath.size()),
                    normalizedExecutable.c_str(),
                    static_cast<int>(normalizedExecutable.size()),
                    TRUE) == CSTR_EQUAL)
            {
                return true;
            }
        }
        return false;
    }
}
