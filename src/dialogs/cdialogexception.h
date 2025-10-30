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
#include <exception>
#include <atlstr.h>
#include <atlcrack.h>

#include "stdatl.h"
#include "utils/string-converter.h"

namespace nglab { namespace skym {

class DialogException : public std::exception {
public:
    DialogException(std::string title, std::string detail)
        : m_title(std::move(title)), m_detail(std::move(detail))
    {}

    DialogException(const std::string &combined) {
        auto pos = combined.find('|');
        if (pos == std::string::npos) {
            m_title = "Error";
            m_detail = combined;
        } else {
            m_title = combined.substr(0, pos);
            m_detail = combined.substr(pos+1);
        }
    }

    const char* what() const noexcept override {
        if (m_cached.empty()) {
            m_cached = m_title + "|" + m_detail;
        }
        return m_cached.c_str();
    }

    const std::string& title() const { return m_title; }
    const std::string& detail() const { return m_detail; }

    void show(HWND parent) const {
        CTaskDialog dlg(parent);
        auto wt = nglab::skym::StringConverter::u8w2(m_title);
        auto wd = nglab::skym::StringConverter::u8w2(m_detail);
        dlg.SetWindowTitle(wt.c_str());
        dlg.SetContentText(wd.c_str());
        dlg.SetMainIcon(TD_ERROR_ICON);
        dlg.DoModal(parent);
    }

private:
    std::string m_title;
    std::string m_detail;
    mutable std::string m_cached;
};

}} // namespace

