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

#include "msgbox.h"
#include "../dialogs/dsmessagebox.h"
#include <assert.h>
#include <QMessageBox>
#include <QString>
#include "../dsvdef.h"
#include "../appcontrol.h"

//QMessageBox::information(NULL, "Title", "Content",QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
//QMessageBox::information(NULL, "Title", "Content",QMessageBox::Yes|QMessageBox::No);
//QMessageBox::information(NULL, "Title", "Content");
//QMessageBox::information(NULL, "Title", "Content",QMessageBox::Yes|QMessageBox::No|QMessageBox::Abort);

void MsgBox::Show(const char *title, const char *text, QWidget *parent)
{
    assert(text);

    QString str;
    str.append("\n");
    str.append(text);

    if (parent == NULL){
        parent = AppControl::Instance()->GetTopWindow();
    }

    pv::dialogs::DSMessageBox msg(parent, title);
     msg.mBox()->setText(str);
   // msg.mBox()->setInformativeText(QString(text));
    msg.mBox()->setStandardButtons(QMessageBox::Ok);
    msg.mBox()->setIcon(QMessageBox::Warning);
    msg.exec();
}

bool MsgBox::Confirm(const char *text, QWidget *parent)
{
    assert(text);

    QString str;
    str.append("\n");
    str.append(text);

    if (parent == NULL){
        parent = AppControl::Instance()->GetTopWindow();
    }

    pv::dialogs::DSMessageBox msg(parent, "Question");
    msg.mBox()->setText(str);
    msg.mBox()->setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msg.mBox()->setIcon(QMessageBox::Question);
    msg.exec();
    return msg.IsYes();
}
