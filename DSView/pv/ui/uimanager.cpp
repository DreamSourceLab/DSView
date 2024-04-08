/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2024 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "uimanager.h"
#include <assert.h>

UiManager::UiManager()
{
}

UiManager* UiManager::Instance()
{
    static UiManager *ins = nullptr;

    if (ins == nullptr){
        ins = new UiManager();
    }

    return ins;
}

void UiManager::AddWindow(IUiWindow *w)
{
    assert(w);

    for (auto it = m_widgets.begin(); it != m_widgets.end(); ++it)
    {
        if ((*it) == w){
            return;
        }
    }

    m_widgets.push_back(w);
    
    //Init the ui.
    w->UpdateFont();
    w->UpdateLanguage();
    w->UpdateTheme();
}

void UiManager::RemoveWindow(IUiWindow *w)
{
    assert(w);

    for (auto it = m_widgets.begin(); it != m_widgets.end(); ++it)
    {
        if ((*it) == w){
            m_widgets.erase(it);
            break;
        }
    } 
}

void UiManager::Update(UiUpdateAction action)
{
    for (auto it = m_widgets.begin(); it != m_widgets.end(); ++it)
    {
        IUiWindow *w = (*it);

        if (action == UI_UPDATE_ACTION_LANG){
            w->UpdateLanguage();
        }
        else if (action == UI_UPDATE_ACTION_THEME){
            w->UpdateTheme();
        }
        else if (action == UI_UPDATE_ACTION_FONT){
            w->UpdateFont();
        }
    } 
}