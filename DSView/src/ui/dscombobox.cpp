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

DsComboBox::DsComboBox(QWidget *parent) : QComboBox(parent)
{
    _contentWidth = 0;
    _bPopup = false;
    QComboBox::setSizeAdjustPolicy(QComboBox::AdjustToContents);   
}
  
 void DsComboBox::addItem(const QString &atext, const QVariant &auserData)
 {
      QComboBox::addItem(atext, auserData);

#ifdef Q_OS_DARWIN
      if (!atext.isEmpty()){
          QFontMetrics fm = this->fontMetrics();
          int w = fm.boundingRect(atext).width();
          if (w > _contentWidth){
              _contentWidth = w;                                
              this->setStyleSheet("QAbstractItemView{min-width:" + QString::number(w + 30) + "px;}");
          }
      }
#endif
 }

 void DsComboBox::showPopup()
 {
	QComboBox::showPopup();
    _bPopup = true;

    QWidget *popup = this->findChild<QFrame*>();
    auto rc = popup->geometry();
    int x = rc.left();
    int y = rc.top();
    int w = rc.right() - rc.left() + 2;
    int h = rc.bottom() - rc.top() + 20;

#ifdef Q_OS_DARWIN
    x += 6;
#endif

#ifndef _WIN32
    w += 3;
#endif

    popup->setGeometry(x, y, w, h);

    int sy = QGuiApplication::primaryScreen()->size().height(); 
    if (sy <= 1080){
        popup->setMaximumHeight(750); 
    }
    
    if (AppConfig::Instance().frameOptions.style == THEME_STYLE_DARK){       
        popup->setStyleSheet("background-color:#262626;");
    }
    else{
        popup->setStyleSheet("background-color:#white;");
    }
 }

 void DsComboBox::hidePopup()
 {
     QComboBox::hidePopup();
     _bPopup = false;
 }
 
