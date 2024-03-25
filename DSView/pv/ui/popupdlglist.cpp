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

#include "popupdlglist.h"
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <QTimer>

#include "../log.h"

namespace{
    std::vector<PopuDlgItem> g_popup_dlg_list;
    QScreen     *currentScreen = NULL;
}

void PopupDlgList::AddDlgTolist(QWidget *w)
{   
    if (w != nullptr){
        PopuDlgItem item;
        item.screen = currentScreen;
        item.widget = w;
        g_popup_dlg_list.push_back(item);
    }
}

void PopupDlgList::RemoveDlgFromList(QWidget *w)
{
    if (w != nullptr){
        for (auto it = g_popup_dlg_list.begin(); it != g_popup_dlg_list.end(); ++it)
        {
            if ((*it).widget == w){
                g_popup_dlg_list.erase(it);
                break;
            }
        }
    }
}

void PopupDlgList::TryCloseAllByScreenChanged(QScreen *windowScreen)
{
    int num = g_popup_dlg_list.size();
  
    for (int i=0; i<num; i++)
    {
        auto it = g_popup_dlg_list.begin() + i;
        auto w = (*it).widget;

        if (w->isVisible()){
            if ((*it).screen != windowScreen){
                w->close(); //Close the dialog.
                g_popup_dlg_list.erase(it);
                --num;
                --i;                 
            }
        }
    }
}

void PopupDlgList::SetCurrentScreen(QScreen *screen)
{
    currentScreen = screen;
}
