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

#include <QObject>
#include <QWidget>
#include <QStringList>

class QComboBox;

struct FontBindInfo
{
    QComboBox   *name_box;
    QComboBox   *size_box;
    QString     lang_id;
    char        *lang_def;
    QString     *ptr_name;
    float         *ptr_size;
};

namespace pv
{
 namespace dialogs
{

    class ApplicationParamDlg
    { 
    public:
        ApplicationParamDlg();
        ~ApplicationParamDlg();

        bool ShowDlg(QWidget *parent);
 
    private:
        void bind_font_name_list(QComboBox *box, QString v);

        void bind_font_size_list(QComboBox *box, float size);

    private:
        QStringList _font_name_list; 
    };

}//
}//
