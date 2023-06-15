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

#include <QString>

class QWidget;

namespace pv{
    namespace dialogs{
        class DSMessageBox;
    }
}

class MsgBox
{
public:
    static void Show(const QString text);
    static void Show(const QString title, const QString text, QWidget *parent=0);
    static void Show(const QString title, const QString text, const QString infoText); 
    static void Show(const QString title, const QString text, QWidget *parent, pv::dialogs::DSMessageBox **box);
    static void Show(const QString title, const QString text, const QString infoText, QWidget *parent, pv::dialogs::DSMessageBox **box);

    static bool Confirm(const QString text, QWidget *parent=0);
    static bool Confirm(const QString text, const QString infoText, pv::dialogs::DSMessageBox **box=0, QWidget *parent=0);
};