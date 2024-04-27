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

#include "xtoolbutton.h"
#include <QMenu>
#include <QApplication>
#include <QTimer>
#include "../log.h" 

#ifdef _WIN32
#include "../winnativewidget.h"
#endif

namespace
{
    static int _click_times = 0;
    static XToolButton *_lst_button = NULL;
} 

XToolButton::XToolButton(QWidget *parent)
    :QToolButton(parent)
{
    _menu = NULL;
    _is_mouse_down = false; 
}

void XToolButton::mousePressEvent(QMouseEvent *event)
{
    _is_mouse_down = true;

    if (_lst_button != this){
        _click_times = 0;
    }

    _lst_button = this;

#ifdef _WIN32
    _menu = this->menu();
    if (_menu == NULL){
        QToolButton::mousePressEvent(event); // is not a popup menu.
        return;
    }

    _click_times++;     

    if (event->button() != Qt::LeftButton){
        setCheckable(true);
        setChecked(false);
        setCheckable(false);        
        return;
    }

    if (_click_times % 2 == 1){
        setCheckable(true);
        setChecked(true);
        QPoint pt = mapToGlobal(rect().bottomLeft());
        connect(_menu, SIGNAL(aboutToHide()), this, SLOT(onHidePopupMenu()));
        pv::WinNativeWidget::EnalbeNoClientArea(false);
        _menu->popup(pt);
    }
    else{
        setCheckable(true);
        setChecked(false);
        setCheckable(false);
    }
    
#else
    QToolButton::mousePressEvent(event);
#endif
}

void XToolButton::mouseReleaseEvent(QMouseEvent *event)
{
    _is_mouse_down = false;
    QToolButton::mouseReleaseEvent(event);
}

void XToolButton::onHidePopupMenu()
{   
#ifdef _WIN32 

    setCheckable(true);
    setChecked(false);
    setCheckable(false);
 
    QWidget *widgetUnderMouse = qApp->widgetAt(QCursor::pos());
    if (widgetUnderMouse != this){
        _is_mouse_down = false;
    }

    if (!_is_mouse_down || _lst_button != this){
       _click_times = 0;
    }
   
    if (_menu != NULL){
        disconnect(_menu, SIGNAL(aboutToHide()), this, SLOT(onHidePopupMenu()));
    } 

    QTimer::singleShot(300, this, [this](){
                pv::WinNativeWidget::EnalbeNoClientArea(true);
            });

#endif
}
