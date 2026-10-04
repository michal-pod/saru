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
#pragma once

#include <stdexcept>

#include "stdatl.h"
#include "cskipexecutableslist.h"
#include "system/hv.h"
#include "settings/chypervpage.h"
#include "settings/cintegrationpage.h"
#include "settings/cnotificationpage.h"

namespace nglab
{
    namespace saru
    {

        class CSettingsSheet : public CPropertySheetImpl<CSettingsSheet>
        {
        public:
            static void initDefault()
            {
                CIntegrationPage::initDefault();
                CNotificationPage::initDefault();
                CSkipExecutablesList::initDefault();
                CHyperVPage::initDefault();
            }

            CSettingsSheet(HWND hWnd) : CPropertySheetImpl<CSettingsSheet>(_T("Application Settings"), 0, hWnd)
            {
                if (m_created)
                {
                    throw std::runtime_error("CSettingsSheet instance already created");
                }
                m_created = true;
                AddPage(m_pageIntegration);
                AddPage(m_pageNotification);                
                
                if (HVDetector::isHyperVRunning() && HVDetector::isIntegrationServiceInstalled())
                {
                    AddPage(m_pageHyperV);
                }
            }

            ~CSettingsSheet()
            {
                m_created = false;
            }

            BEGIN_MSG_MAP(CSettingsSheet)
            CHAIN_MSG_MAP(CPropertySheetImpl<CSettingsSheet>)
            END_MSG_MAP()
        private:
            CIntegrationPage m_pageIntegration;
            CNotificationPage m_pageNotification;
            CHyperVPage m_pageHyperV;
            inline static bool m_created = false;
        };
    }
}
