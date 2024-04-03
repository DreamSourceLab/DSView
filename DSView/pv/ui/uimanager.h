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

#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <vector>

class IUiWindow
{
public:
    virtual void UpdateLanguage()=0;
    virtual void UpdateTheme()=0;
    virtual void UpdateFont()=0;
};

enum UiUpdateAction
{
    UI_UPDATE_ACTION_LANG = 0,
    UI_UPDATE_ACTION_THEME = 1,
    UI_UPDATE_ACTION_FONT = 2,
};

class UiManager
{
private:
    UiManager();

public:
    static UiManager* Instance();

    void AddWindow(IUiWindow *w);
    void RemoveWindow(IUiWindow *w);
    void Update(UiUpdateAction action);

private:
    std::vector<IUiWindow*> m_widgets;
};

#define ADD_UI(o)   UiManager::Instance()->AddWindow(o);
#define REMOVE_UI(o)    UiManager::Instance()->RemoveWindow(o);

#endif