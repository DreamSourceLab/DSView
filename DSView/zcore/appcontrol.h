/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 * 
 * Copyright (C) 2021 DreamSourceLab <support@dreamsourcelab.com>
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

#pragma once

#include <string>
#include <vector>

struct sr_context;
class QWidget;
class IFontForm;

namespace pv{ 
    class SigSession;
}

class AppControl
{
private:
    explicit AppControl();
    ~AppControl();
    AppControl(AppControl &o);

public:
    static AppControl* Instance();

    void Destroy();

    bool Init();

    bool Start();

    void Stop();

    void UnInit(); 

    inline pv::SigSession*  GetSession(){
        return _session;
    } 

    inline void SetTopWindow(QWidget *w){
        _topWindow = w;
    }

    inline QWidget* GetTopWindow(){
        return _topWindow;
    }

    bool TopWindowIsMaximized();

    void add_font_form(IFontForm *form);
    void remove_font_form(IFontForm *form);
    void update_font_forms();

public:
    std::string        _open_file_name; 

private: 
    pv::SigSession      *_session;
    QWidget             *_topWindow;
    std::vector<IFontForm*> _font_forms;
};
