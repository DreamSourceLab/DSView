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


#include "dsmessagebox.h"
#include "shadow.h"

#include <QObject>
#include <QEvent>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QAbstractButton>

namespace pv {
namespace dialogs {

DSMessageBox::DSMessageBox(QWidget *parent) :
    QDialog(parent),
    _moving(false)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);
    _main = new QWidget(this);
    QVBoxLayout *mlayout = new QVBoxLayout(_main);
    _main->setLayout(mlayout);

    Shadow *bodyShadow = new Shadow();
    bodyShadow->setBlurRadius(10.0);
    bodyShadow->setDistance(3.0);
    bodyShadow->setColor(QColor(0, 0, 0, 80));
    _main->setAutoFillBackground(true);
    _main->setGraphicsEffect(bodyShadow);

    _msg = new QMessageBox(this);
    _msg->setWindowFlags(Qt::FramelessWindowHint | Qt::Widget);

    _titlebar = new toolbars::TitleBar(false, this);
    _titlebar->setTitle(tr("Message"));
    _titlebar->installEventFilter(this);

    mlayout->addWidget(_titlebar);
    mlayout->addWidget(_msg);

    _layout = new QVBoxLayout(this);
    _layout->addWidget(_main);
    setLayout(_layout);

    //connect(_msg, SIGNAL(finished(int)), this, SLOT(accept()));
    connect(_msg, SIGNAL(buttonClicked(QAbstractButton*)), this, SLOT(on_button(QAbstractButton*)));
}

void DSMessageBox::accept()
{
    using namespace Qt;

    QDialog::accept();
}

void DSMessageBox::reject()
{
    using namespace Qt;

    QDialog::reject();
}

bool DSMessageBox::eventFilter(QObject *object, QEvent *event)
{
    (void)object;
    const QEvent::Type type = event->type();
    const QMouseEvent *const mouse_event = (QMouseEvent*)event;
    if (type == QEvent::MouseMove) {
        if (_moving && mouse_event->buttons().testFlag(Qt::LeftButton)) {
            move(mouse_event->globalPos() - _startPos);
        }
        return true;
    } else if (type == QEvent::MouseButtonPress) {
        if (mouse_event->buttons().testFlag(Qt::LeftButton)) {
            _moving = true;
            _startPos = mouse_event->pos() +
                        QPoint(_layout->margin(), _layout->margin()) +
                        QPoint(_layout->spacing(), _layout->spacing());
        }
    } else if (type == QEvent::MouseButtonRelease) {
        if (mouse_event->buttons().testFlag(Qt::LeftButton)) {
            _moving = false;
        }
    }
    return false;
}

QMessageBox* DSMessageBox::mBox()
{
    return _msg;
}

int DSMessageBox::exec()
{
    //_msg->show();
    return QDialog::exec();
}

void DSMessageBox::on_button(QAbstractButton *btn)
{
    QMessageBox::ButtonRole role = _msg->buttonRole(btn);
    if (role == QMessageBox::AcceptRole)
        accept();
    else
        reject();
}

} // namespace dialogs
} // namespace pv
