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
#include <format>

#include "stdatl.h"
#include "config.h"
class CAboutDlg : public CDialogImpl<CAboutDlg>
{
public:
    enum
    {
        IDD = IDD_ABOUT
    };

    CTabCtrl m_tab;
    CEdit m_edit;
    static bool m_created;

    BEGIN_MSG_MAP(CAboutDlg)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    NOTIFY_HANDLER(IDC_ABOUT_TAB, TCN_SELCHANGE, OnTabSelChange)
    COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
    END_MSG_MAP()

    CAboutDlg() {
        if (m_created) {
            throw std::runtime_error("CAboutDlg instance already created");
        }
        m_created = true;
    }
    ~CAboutDlg() {
        m_created = false;
    }



    LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL &)
    {
        // Ustaw ikonę
         HICON hIcon = AtlLoadIconImage(IDI_ICON1, LR_DEFAULTCOLOR, 32, 32);
         SetIcon(hIcon, TRUE);

        CRect rcTab;
                m_tab.Attach(GetDlgItem(IDC_ABOUT_TAB));
        m_edit.Attach(GetDlgItem(IDC_ABOUT_CONTENT));


        // Opis programu
        SetDlgItemText(IDC_ABOUT_DESCRIPTION, "SSH Key Manager\nVersion " VERSION_STRING "\nSSH key management application.");

        // Tab control
        m_tab.InsertItem(0, "Release");
        m_tab.InsertItem(1, "ChangeLog");
        m_tab.InsertItem(2, "License");
        m_tab.InsertItem(3, "Build Info");


        // Pole edycyjne do wyświetlania treści
        m_edit.SetReadOnly(TRUE);

        LoadResourceText(IDR_RELEASE); // domyślnie pierwszy tab

        return TRUE;
    }

    LRESULT OnTabSelChange(int, LPNMHDR, BOOL &)
    {
        int sel = m_tab.GetCurSel();
        switch (sel)
        {
        case 0:
            LoadResourceText(IDR_RELEASE);
            break;
        case 1:
            LoadResourceText(IDR_CHANGELOG);
            break;
        case 2:
            LoadResourceText(IDR_LICENSE);
            break;
        case 3:
            {
                std::string buildInfo = std::format(
                    "Build Information:\r\n"
                    "Version: {}\r\n"
                    "Build type: {}\r\n"
                    "Build date: {}\r\n"
                    "Compiler: {}\r\n"
                    "Architecture: {}\r\n",
                    VERSION_STRING,
                    SKYM_BUILD_TYPE,
                    __DATE__ " " __TIME__,
                    SKYM_COMPILER,
                    SKYM_ARCHITECTURE
                );
                m_edit.SetWindowText(buildInfo.c_str());
            }
            break;
        }
        return 0;
    }

    void LoadResourceText(UINT id)
    {
        HRSRC hRes = ::FindResource(_Module.GetResourceInstance(), MAKEINTRESOURCE(id), RT_RCDATA);
        if (!hRes)
            return;
        HGLOBAL hData = ::LoadResource(_Module.GetResourceInstance(), hRes);
        if (!hData)
            return;

        LPCSTR pData = (LPCSTR)::LockResource(hData);

        m_edit.SetWindowText(pData);
    }

    LRESULT OnCloseCmd(WORD, WORD wID, HWND, BOOL &)
    {
        EndDialog(wID);
        return 0;
    }
};

bool CAboutDlg::m_created = false;

