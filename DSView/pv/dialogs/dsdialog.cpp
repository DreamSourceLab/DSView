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


#include "dsdialog.h"
#include "shadow.h"

#include <QObject>
#include <QEvent>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QAbstractButton>

namespace pv {
namespace dialogs {

DSDialog::DSDialog(QWidget *parent, bool hasClose) :
    QDialog(parent),
    _moving(false)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);

    build_main(hasClose);

    _layout = new QVBoxLayout(this);
    _layout->addWidget(_main);
    setLayout(_layout);
}

void DSDialog::accept()
{
    using namespace Qt;

    QDialog::accept();
}

void DSDialog::reject()
{
    using namespace Qt;

    QDialog::reject();
}

bool DSDialog::eventFilter(QObject *object, QEvent *event)
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
#ifndef _WIN32
            _startPos = mouse_event->pos() +
                        QPoint(_layout->margin(), _layout->margin()) +
                        QPoint(_layout->spacing(), _layout->spacing()) +
                        QPoint(_mlayout->margin(), _mlayout->margin()) +
                        QPoint(_mlayout->spacing(), _mlayout->spacing());
#else
            _startPos = mouse_event->pos() +
                        QPoint(_layout->margin(), _layout->margin()) +
                        QPoint(_layout->spacing(), _layout->spacing());
#endif
        }
    } else if (type == QEvent::MouseButtonRelease) {
        if (mouse_event->buttons().testFlag(Qt::LeftButton)) {
            _moving = false;
        }
    }
    return false;
}

QVBoxLayout* DSDialog::layout()
{
    return _mlayout;
}

QWidget* DSDialog::mainWidget()
{
    return _main;
}

void DSDialog::setTitle(QString title)
{
    _titlebar->setTitle(title);
}

void DSDialog::reload(bool hasClose)
{
    QString title;
    if (_titlebar)
        title = _titlebar->title();
    if (_main)
        delete _main;

    build_main(hasClose);
    _titlebar->setTitle(title);
    _layout->addWidget(_main);
}

void DSDialog::build_main(bool hasClose)
{
    _main = new QWidget(this);
    _mlayout = new QVBoxLayout(_main);
    _main->setLayout(_mlayout);
    //_mlayout->setMargin(5);
    //_mlayout->setSpacing(5);

    Shadow *bodyShadow = new Shadow(_main);
    bodyShadow->setBlurRadius(10.0);
    bodyShadow->setDistance(3.0);
    bodyShadow->setColor(QColor(0, 0, 0, 80));
    _main->setAutoFillBackground(true);
    _main->setGraphicsEffect(bodyShadow);

    _titlebar = new toolbars::TitleBar(false, this, hasClose);
    _titlebar->installEventFilter(this);
    _mlayout->addWidget(_titlebar);
}

} // namespace dialogs
} // namespace pv
