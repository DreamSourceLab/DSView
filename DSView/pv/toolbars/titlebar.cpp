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

#include "titlebar.h"

#include <QStyle>
#include <QLabel>
#include <QToolButton>
#include <QHBoxLayout>
#include <QEvent>
#include <QMouseEvent>
#include <QApplication>
#include <QPainter>

#ifdef Q_OS_WIN
#pragma comment(lib, "user32.lib")
#include <qt_windows.h>
#endif

namespace pv {
namespace toolbars {

TitleBar::TitleBar(bool top, QWidget *parent, bool hasClose) :
    QWidget(parent),
    _moving(false),
    _isTop(top),
    _hasClose(hasClose)
{
    setObjectName("TitleBar");
    setFixedHeight(28);

    _title = new QLabel(this);
    QHBoxLayout *hbox = new QHBoxLayout(this);
    hbox->addWidget(_title);

    if (_isTop) {
        _minimizeButton = new QToolButton(this);
        _minimizeButton->setObjectName("MinimizeButton");
        _minimizeButton->setIcon(QIcon::fromTheme("titlebar",
                                 QIcon(":/icons/minimize.png")));
        _maximizeButton = new QToolButton(this);
        _maximizeButton->setObjectName("MaximizeButton");
        _maximizeButton->setIcon(QIcon::fromTheme("titlebar",
                                 QIcon(":/icons/maximize.png")));

        hbox->addWidget(_minimizeButton);
        hbox->addWidget(_maximizeButton);

        connect(this, SIGNAL( normalShow() ), parent, SLOT(showNormal() ) );
        connect(this, SIGNAL( maximizedShow() ), parent, SLOT(showMaximized() ) );
        connect(_minimizeButton, SIGNAL( clicked() ), parent, SLOT(showMinimized() ) );
        connect(_maximizeButton, SIGNAL( clicked() ), this, SLOT(showMaxRestore() ) );
    }

    if (_isTop || _hasClose) {
        _closeButton= new QToolButton(this);
        _closeButton->setObjectName("CloseButton");
        _closeButton->setIcon(QIcon::fromTheme("titlebar",
                                 QIcon(":/icons/close.png")));
        hbox->addWidget(_closeButton);
        connect(_closeButton, SIGNAL( clicked() ), parent, SLOT(close() ) );
    }

    hbox->insertStretch(0, 500);
    hbox->insertStretch(2, 500);
    hbox->setMargin(0);
    hbox->setSpacing(0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void TitleBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QColor(48, 47, 47, 255));
    p.setBrush(QColor(48, 47, 47, 255));
    p.drawRect(rect());

    const int xgap = 2.5;
    const int xstart = 10;
    p.setPen(QPen(QColor(213, 15, 37, 255), 2, Qt::SolidLine));
    p.drawLine(xstart + xgap*0,  height()*0.50, xstart + xgap*0,  height()*0.66);
    p.drawLine(xstart + xgap*18, height()*0.34, xstart + xgap*18, height()*0.50);

    p.setPen(QPen(QColor(238, 178, 17, 255), 2, Qt::SolidLine));
    p.drawLine(xstart + xgap*2,  height()*0.50, xstart + xgap*2,  height()*0.83);
    p.drawLine(xstart + xgap*16, height()*0.17, xstart + xgap*16, height()*0.50);

    p.setPen(QPen(QColor(17, 133, 209,  255), 2, Qt::SolidLine));
    p.drawLine(xstart + xgap*4,  height()*0.50, xstart + xgap*4,  height()*1.00);
    p.drawLine(xstart + xgap*14, height()*0.00, xstart + xgap*14, height()*0.50);

    p.setPen(QPen(QColor(0, 153, 37, 200), 2, Qt::SolidLine));
    p.drawLine(xstart + xgap*6,  height()*0.50, xstart + xgap*6,  height()*0.83);
    p.drawLine(xstart + xgap*12, height()*0.17, xstart + xgap*12, height()*0.50);

    p.setPen(QPen(QColor(109, 50, 156, 255), 2, Qt::SolidLine));
    p.drawLine(xstart + xgap*8,  height()*0.50, xstart + xgap*8,  height()*0.66);
    p.drawLine(xstart + xgap*10, height()*0.34, xstart + xgap*10, height()*0.50);
}

void TitleBar::setTitle(QString title)
{
    _title->setText(title);
}

QPoint TitleBar::get_startPos() const
{
    return _startPos;
}

QString TitleBar::title() const
{
    return _title->text();
}

void TitleBar::showMaxRestore()
{
    if (parentWidget()->isMaximized()) {
        _maximizeButton->setIcon(QIcon::fromTheme("titlebar",
                                 QIcon(":/icons/maximize.png")));
        normalShow();
    } else {
        _maximizeButton->setIcon(QIcon::fromTheme("titlebar",
                                 QIcon(":/icons/restore.png")));
        maximizedShow();
    }   
}

void TitleBar::setRestoreButton(bool max)
{
    if (!max) {
        _maximizeButton->setIcon(QIcon::fromTheme("titlebar",
                                 QIcon(":/icons/maximize.png")));
    } else {
        _maximizeButton->setIcon(QIcon::fromTheme("titlebar",
                                 QIcon(":/icons/restore.png")));
    }
}

void TitleBar::mousePressEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton && !parentWidget()->isMaximized()) {
        _moving = true;
        _startPos = mapToParent(event->pos());
    }
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if(_moving && event->buttons().testFlag(Qt::LeftButton)) {
        parentWidget()->move(event->globalPos() - _startPos);
    }
}

void TitleBar::mouseReleaseEvent(QMouseEvent* event)
{
    if(event->button() == Qt::LeftButton) {
        _moving = false;
    }
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (_isTop)
        showMaxRestore();
    QWidget::mouseDoubleClickEvent(event);
}

} // namespace toolbars
} // namespace pv
