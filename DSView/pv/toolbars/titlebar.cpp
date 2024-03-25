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
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QEvent>
#include <QMouseEvent> 
#include <QPainter>
#include <QStyleOption>
#include <assert.h>
#include <QTimer>

#include "../config/appconfig.h"
#include "../appcontrol.h"
#include "../dsvdef.h"
#include "../ui/fn.h"

namespace pv {
namespace toolbars {

namespace{
    static bool _is_able_drag = true;
}

TitleBar::TitleBar(bool top, QWidget *parent, ITitleParent *titleParent, bool hasClose) :
    QWidget(parent)
{ 
   _minimizeButton = NULL;
   _maximizeButton = NULL;
   _closeButton = NULL;
   _moving = false;
   _is_draging = false;
   _parent = parent;
   _isTop = top;
   _hasClose = hasClose;
   _title = NULL;
   _is_native = false;
   _titleParent = titleParent;
   _is_done_moved = false; 

    assert(parent);

    setObjectName("TitleBar");
    setContentsMargins(0,0,0,0);
    setFixedHeight(32); 

    QHBoxLayout *lay1 = new QHBoxLayout(this);

    _title = new QLabel(this);
    lay1->addWidget(_title);

    if (_isTop) {
        _minimizeButton = new XToolButton(this);
        _minimizeButton->setObjectName("MinimizeButton");
        _maximizeButton = new XToolButton(this);
        _maximizeButton->setObjectName("MaximizeButton");

        lay1->addWidget(_minimizeButton);
        lay1->addWidget(_maximizeButton);

        connect(this, SIGNAL(normalShow()), parent, SLOT(showNormal()));
        connect(this, SIGNAL( maximizedShow()), parent, SLOT(showMaximized()));
        connect(_minimizeButton, SIGNAL( clicked()), parent, SLOT(showMinimized()));
        connect(_maximizeButton, SIGNAL( clicked()), this, SLOT(showMaxRestore()));
    }

    if (_isTop || _hasClose) {
        _closeButton= new XToolButton(this);
        _closeButton->setObjectName("CloseButton");
        lay1->addWidget(_closeButton);
        connect(_closeButton, SIGNAL( clicked()), parent, SLOT(close()));
    }

    lay1->insertStretch(0, 500);
    lay1->insertStretch(2, 500);
    lay1->setContentsMargins(0,0,0,0);
    lay1->setSpacing(0);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed); 

    update_font();
}

TitleBar::~TitleBar(){ 
    DESTROY_QT_OBJECT(_minimizeButton);
    DESTROY_QT_OBJECT(_maximizeButton);
    DESTROY_QT_OBJECT(_closeButton);
}

void TitleBar::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::StyleChange)
        reStyle();
    QWidget::changeEvent(event);
}

void TitleBar::reStyle()
{
    QString iconPath = GetIconPath();

    if (_isTop) {
        _minimizeButton->setIcon(QIcon(iconPath+"/minimize.svg"));
        if (ParentIsMaxsized())
            _maximizeButton->setIcon(QIcon(iconPath+"/restore.svg"));
        else
            _maximizeButton->setIcon(QIcon(iconPath+"/maximize.svg"));
    }
    if (_isTop || _hasClose)
        _closeButton->setIcon(QIcon(iconPath+"/close.svg"));
}

bool TitleBar::ParentIsMaxsized()
{
    if (_titleParent != NULL){
        return _titleParent->ParentIsMaxsized();
    } 
    else{
        return parentWidget()->isMaximized();
    }
}

void TitleBar::paintEvent(QPaintEvent *event)
{ 
    //draw logo icon
    QStyleOption o;
    o.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &o, &p, this);

    p.setRenderHint(QPainter::Antialiasing, true);

    const int xgap = 2;
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

    QWidget::paintEvent(event);
}

void TitleBar::setTitle(QString title)
{
    if (!_is_native){
        _title->setText(title);
    }
    else if (_parent != NULL){
        _parent->setWindowTitle(title);
    }    
}
  
QString TitleBar::title()
{
    if (!_is_native){
        return _title->text();
    }
    else if (_parent != NULL){
        return _parent->windowTitle();
    }
    return "";
}

void TitleBar::showMaxRestore()
{
    QString iconPath = GetIconPath();
    if (ParentIsMaxsized()) {
        _maximizeButton->setIcon(QIcon(iconPath+"/maximize.svg"));
        normalShow();
    } else {
        _maximizeButton->setIcon(QIcon(iconPath+"/restore.svg"));
        maximizedShow();
    }   
}

void TitleBar::setRestoreButton(bool max)
{
    QString iconPath = GetIconPath();
    if (!max) {
        _maximizeButton->setIcon(QIcon(iconPath+"/maximize.svg"));
    } else {
        _maximizeButton->setIcon(QIcon(iconPath+"/restore.svg"));
    }
}
  
void TitleBar::mousePressEvent(QMouseEvent* event)
{ 
    bool ableMove = !ParentIsMaxsized();

    if(event->button() == Qt::LeftButton && ableMove && _is_able_drag) 
    {
        int x = event->pos().x();
        int y = event->pos().y(); 
        
        bool bTopWidow = AppControl::Instance()->GetTopWindow() == _parent;
        bool bClick = (x >= 6 && y >= 5 && x <= width() - 6);  //top window need resize hit check
 
        if (!bTopWidow || bClick ){
            _is_draging = true;             

            _clickPos = event->globalPos(); 

            if (_titleParent != NULL){
                _oldPos = _titleParent->GetParentPos();
            }
            else{
                _oldPos = _parent->pos(); 
            }

            _is_done_moved = false;
                
            event->accept();
            return;
        } 
    }  
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{  
    if(_is_draging){ 

        int datX = 0;
        int datY = 0;

        datX = (event->globalPos().x() - _clickPos.x());
        datY = (event->globalPos().y() - _clickPos.y());

        int x = _oldPos.x() + datX;
        int y = _oldPos.y() + datY;

        if (!_moving){
            if (ABS_VAL(datX) >= 2 || ABS_VAL(datY) >= 2){
                _moving = true;
            }
            else{
                return;
            }
        }

        if (_titleParent != NULL){

            if (!_is_done_moved){
                _is_done_moved = true;
                _titleParent->MoveBegin();
            }

            _titleParent->MoveWindow(x, y);
        }
        else{
            _parent->move(x, y);  
        }
        
        event->accept();
        return;
    } 
    QWidget::mouseMoveEvent(event);
}

void TitleBar::mouseReleaseEvent(QMouseEvent* event)
{
    if (_moving && _titleParent != NULL){
        _titleParent->MoveEnd();
    }
    _moving = false;
    _is_draging = false;
    QWidget::mouseReleaseEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{  
    QWidget::mouseDoubleClickEvent(event); 

    if (_isTop){ 

      QTimer::singleShot(200, this, [this](){
                showMaxRestore();
            });
    }
}

void TitleBar::update_font()
{  
    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize+1);
    _title->setFont(font);
}

void TitleBar::EnableAbleDrag(bool bEnabled)
{
    _is_able_drag = bEnabled;
}
 
} // namespace toolbars
} // namespace pv
