/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2016 DreamSourceLab <support@dreamsourcelab.com>
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


#include "messagebox.h"

#include <QVBoxLayout>
#include <QAbstractButton>

namespace pv {
namespace dialogs {

MessageBox::MessageBox(QWidget *parent) :
    QDialog(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);

    _msg = new QMessageBox(this);
    _msg->setWindowFlags(Qt::FramelessWindowHint | Qt::Widget);

    _titlebar = new toolbars::TitleBar(false, parent);
    _titlebar->setTitle(tr("Message"));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(_titlebar);
    layout->addWidget(_msg);
    setLayout(layout);

    //connect(_msg, SIGNAL(finished(int)), this, SLOT(accept()));
    connect(_msg, SIGNAL(buttonClicked(QAbstractButton*)), this, SLOT(on_button(QAbstractButton*)));
}

void MessageBox::accept()
{
    using namespace Qt;

    QDialog::accept();
}

void MessageBox::reject()
{
    using namespace Qt;

    QDialog::reject();
}

QMessageBox* MessageBox::mBox()
{
    return _msg;
}

int MessageBox::exec()
{
    //_msg->show();
    return QDialog::exec();
}

void MessageBox::on_button(QAbstractButton *btn)
{
    QMessageBox::ButtonRole role = _msg->buttonRole(btn);
    if (role == QMessageBox::AcceptRole)
        accept();
    else
        reject();
}

} // namespace dialogs
} // namespace pv
