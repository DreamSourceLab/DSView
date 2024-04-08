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

#include "dscombobox.h"
#include <QFontMetrics>
#include <QString>
#include <QGuiApplication>
#include <QScreen>
#include "../config/appconfig.h"

DsComboBox::DsComboBox(QWidget *parent) 
    :QComboBox(parent)
{
    _bPopup = false;
    QComboBox::setSizeAdjustPolicy(QComboBox::AdjustToContents);   
}

DsComboBox::~DsComboBox()
{

}

void DsComboBox::measureSize()
{
    int num = this->count();
    int maxWidth = 0;
    int height = 30;
    QFontMetrics fm = this->fontMetrics();

    for (int i=0; i<num; i++){
        QString text = this->itemText(i);
        QRect rc = fm.boundingRect(text);
 
        if (rc.width() > maxWidth){
            maxWidth = rc.width();
        }
        height = rc.height();
    }

    QString style = QString("QAbstractItemView{min-width:%1px; min-height:%2px;}")
                .arg(maxWidth + 30)
                .arg(height + 5);
    this->setStyleSheet(style);
}

void DsComboBox::showPopup()
{
    _bPopup = true;

#ifdef Q_OS_DARWIN

    measureSize();
    QComboBox::showPopup();

    QWidget *popup = this->findChild<QFrame*>();
    auto rc = popup->geometry();
    int x = rc.left() + 6;
    int y = rc.top();
    int w = rc.right() - rc.left();
    int h = rc.bottom() - rc.top() + 20;
    popup->setGeometry(x, y, w, h);

    int sy = QGuiApplication::primaryScreen()->size().height(); 
    if (sy <= 1080){
        popup->setMaximumHeight(750); 
    }

    popup->setStyleSheet("background-color:" + AppConfig::Instance().GetStyleColor().name());

#else
    QComboBox::showPopup();
#endif

}

void DsComboBox::hidePopup()
{
    QComboBox::hidePopup();
    _bPopup = false;
}
 