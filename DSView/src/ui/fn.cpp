/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2023 DreamSourceLab <support@dreamsourcelab.com>
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

#include "fn.h"
#include <assert.h>
#include <QPushButton>
#include <QToolButton>
#include <QComboBox>
#include <QLabel>
#include <QAction>
#include <QToolBar>
#include <QWidget>
#include <QLineEdit>
#include <QTabWidget>
#include <QGroupBox>
#include <QTextEdit>
#include <QRadioButton>
#include <QCheckBox>

#include "../config/appconfig.h"

namespace ui
{

    void set_font_param(QFont &font, struct FontParam &param)
    {
        font.setPointSizeF(param.size >= 9.0f ? param.size : 9.0f);

        if (param.name != ""){
            font.setFamily(param.name);
        }
    }

    void set_toolbar_font(QToolBar *bar, QFont font)
    {
        assert(bar);

        auto buttons = bar->findChildren<QToolButton*>();
        for(auto o : buttons)
        { 
            o->setFont(font);
        }

        auto buttons2 = bar->findChildren<QPushButton*>();
        for(auto o : buttons2)
        { 
            o->setFont(font);
        }

        auto comboxs = bar->findChildren<QComboBox*>();
        for(auto o : comboxs)
        { 
            o->setFont(font);
        }

        auto labels = bar->findChildren<QLabel*>();
        for(auto o : labels)
        { 
            o->setFont(font);
        }

        auto actions = bar->findChildren<QAction*>();
        for(auto o : actions)
        { 
            o->setFont(font);
        }
    }
    
    void set_form_font(QWidget *wid, QFont font)
    {
        assert(wid);

        auto buttons2 = wid->findChildren<QPushButton*>();
        for(auto o : buttons2)
        { 
            o->setFont(font);
        }

        auto comboxs = wid->findChildren<QComboBox*>();
        for(auto o : comboxs)
        { 
            o->setFont(font);
        }

        auto labels = wid->findChildren<QLabel*>();
        for(auto o : labels)
        { 
            o->setFont(font);
        }

        auto edits = wid->findChildren<QLineEdit*>();
        for(auto o : edits)
        { 
            o->setFont(font);
        }

        auto textEdits = wid->findChildren<QTextEdit*>();
        for(auto o : textEdits)
        { 
            o->setFont(font);
        }

        auto radios = wid->findChildren<QRadioButton*>();
        for(auto o : radios)
        { 
            o->setFont(font);
        }

        auto checks = wid->findChildren<QCheckBox*>();
        for(auto o : checks)
        { 
            o->setFont(font);
        }

        // Magnify the size.
        font.setPointSizeF(font.pointSizeF() + 1);

        auto tabs = wid->findChildren<QTabWidget*>();
        for(auto o : tabs)
        { 
            o->setFont(font); 
        }

        auto groups = wid->findChildren<QGroupBox*>();
        for(auto o : groups)
        { 
            o->setFont(font);
        }
    }

} // namespace ui