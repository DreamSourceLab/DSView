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

 
#include "winshadow.h" 
#include "log.h"
#include "config/appconfig.h"
#include "mainframe.h"
#include "mainwindow.h"
  
 
namespace pv {

namespace
    {
    const QColor dark_border0 = QColor(80, 80, 80, 255);
    const QColor dark_border1 = QColor(48, 47, 47, 200);
    const QColor dark_border2 = QColor(48, 47, 47, 150);
    const QColor dark_border3 = QColor(48, 47, 47, 100);
    const QColor dark_border4 = QColor(48, 47, 47, 10);

    const QColor light_border0 = QColor(100, 100, 100, 255);
    const QColor light_border1 = QColor(150, 150, 150, 150);
    const QColor light_border2 = QColor(150, 150, 150, 100);
    const QColor light_border3 = QColor(150, 150, 150, 50);
    const QColor light_border4 = QColor(150, 150, 150, 0);

    const int Margin = 4;
    }

WinShadow::WinShadow(HWND hwnd, QWidget *parent) : QWidget(parent)
{ 
    m_hwnd = HWND(hwnd);
    m_active = true;
    m_timer = NULL;
    m_bClean = false;
    _frame = NULL;
    _hit_border = None;
    _bDraging = false;
    _freezing = false;
  
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
   
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &WinShadow::show);
    
    m_timer->setInterval(500);
    m_timer->setSingleShot(true); 

    _layout = new QGridLayout(this);
    _layout->setSpacing(0);
    _layout->setContentsMargins(0,0,0,0);

    setLayout(_layout);

    if (true)
    {
        _top_left = new widgets::Border (TopLeft, this);
        _top_left->setFixedSize(Margin, Margin);
        _top_left->installEventFilter(this);
        _top = new widgets::Border (Top, this);
        _top->setFixedHeight(Margin);
        _top->installEventFilter(this);
        _top_right = new widgets::Border (TopRight, this);
        _top_right->setFixedSize(Margin, Margin);
        _top_right->installEventFilter(this);

        _left = new widgets::Border (Left, this);
        _left->setFixedWidth(Margin);
        _left->installEventFilter(this);
        _right = new widgets::Border (Right, this);
        _right->setFixedWidth(Margin);
        _right->installEventFilter(this);

        _bottom_left = new widgets::Border (BottomLeft, this);
        _bottom_left->setFixedSize(Margin, Margin);
        _bottom_left->installEventFilter(this);
        _bottom = new widgets::Border (Bottom, this);
        _bottom->setFixedHeight(Margin);
        _bottom->installEventFilter(this);
        _bottom_right = new widgets::Border (BottomRight, this);
        _bottom_right->setFixedSize(Margin, Margin);
        _bottom_right->installEventFilter(this);

        _layout->addWidget(_top_left, 0, 0);
        _layout->addWidget(_top, 0, 1);
        _layout->addWidget(_top_right, 0, 2);
        _layout->addWidget(_left, 1, 0);

        _layout->addWidget(_right, 1, 2);
        _layout->addWidget(_bottom_left, 2, 0);
        _layout->addWidget(_bottom, 2, 1);
        _layout->addWidget(_bottom_right, 2, 2);
    }
}

int WinShadow::shadowWidth()
{ 
    return SHADOW_BORDER_WIDTH; 
}

void WinShadow::showLater()
{ 
    m_timer->stop();
    m_timer->start(); 
}

void WinShadow::show()
{ 
    m_bClean = false;
    
    if (m_timer->isActive())
        return;

    if (!IsWindowEnabled(m_hwnd))
        return;

    if (GetForegroundWindow() != m_hwnd)
        return;

    Q_EMIT showSignal();

    QWidget::show();
    QWidget::raise();

    SetWindowPos(m_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
  
}

bool WinShadow::tryToShow()
{
    if (!isVisible()){
        show();

        if (!m_timer->isActive()){
            showLater();
        }
        return true;
    }
    return false;
}

void WinShadow::SetClean(bool bClean)
{
    m_bClean = bClean;
    if (isVisible()){
        repaint();
    } 
}

void WinShadow::hide()
{ 
    SetClean(true);
    m_timer->stop();
 
    if (!isVisible())
        return;
 
    QWidget::hide(); 
}

void WinShadow::setActive(bool active)
{  
    m_active = active;
    repaint(); 
}

bool WinShadow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{ 
    MSG *msg = static_cast<MSG*>(message);

    switch (msg->message)
    {
    case WM_ACTIVATE:
    {
        switch (msg->wParam)
        {
        case WA_ACTIVE:
        case WA_CLICKACTIVE:
        { 
            SetForegroundWindow(m_hwnd);
            break;
        }
        default:
            break;
        }

        break;
    }
    case WM_MOUSEACTIVATE:
    { 
        SetForegroundWindow(m_hwnd); 
        *result = MA_NOACTIVATE;
        return true;
    }
    /*
    case WM_NCMOUSEMOVE:
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONUP:
    case WM_NCLBUTTONDBLCLK:
    case WM_NCHITTEST:
    { 
        *result = long(SendMessageW(m_hwnd, msg->message, msg->wParam, msg->lParam));
        return true;
    }
    */
    default:
        break;
    }
 
    return QWidget::nativeEvent(eventType, message, result);
}

bool WinShadow::eventFilter(QObject *object, QEvent *event)
{ 
    const QEvent::Type type = event->type();
    const QMouseEvent *const mouse_event = (QMouseEvent*)event;
    int newWidth = 0;
    int newHeight = 0;
    int newLeft = 0;
    int newTop = 0;

    if (_frame == NULL){
        return QWidget::eventFilter(object, event);
    }

    if (type != QEvent::MouseMove 
        && type != QEvent::MouseButtonPress 
        && type != QEvent::MouseButtonRelease
        && type != QEvent::Leave){
        return QWidget::eventFilter(object, event);
    }

    //when window is maximized, or is moving, call return 
    if (_frame->IsMaxsized() || _frame->IsMoving()){
       return QWidget::eventFilter(object, event);
    }
 
    if (!_bDraging && type == QEvent::MouseMove && (!(mouse_event->buttons() | Qt::NoButton))){
           if (object == _top_left) {
                _hit_border = TopLeft;
                setCursor(Qt::SizeFDiagCursor);
            } else if (object == _bottom_right) {
                _hit_border = BottomRight;
                setCursor(Qt::SizeFDiagCursor);
            } else if (object == _top_right) {
                _hit_border = TopRight;
                setCursor(Qt::SizeBDiagCursor);
            } else if (object == _bottom_left) {
                _hit_border = BottomLeft;
                setCursor(Qt::SizeBDiagCursor);
            } else if (object == _left) {
                _hit_border = Left;
                setCursor(Qt::SizeHorCursor);
            } else if (object == _right) {
                _hit_border = Right;
                setCursor(Qt::SizeHorCursor);
            } else if (object == _bottom) {
                _hit_border = Bottom;
                setCursor(Qt::SizeVerCursor);
            } else if (object == _top) {
                _hit_border = Top;
                setCursor(Qt::SizeVerCursor);
            } else {
                _hit_border = None;
                setCursor(Qt::ArrowCursor);
            }

            return QWidget::eventFilter(object, event);
    }

  if (type == QEvent::MouseMove) {
 
        QPoint pt;
        int k =  _frame->GetDevicePixelRatio();
 
        POINT p;
        GetCursorPos(&p);
        pt.setX(p.x);
        pt.setY(p.y);

         
        int datX = pt.x() - _clickPos.x();
        int datY = pt.y() - _clickPos.y();
        datX /= k;
        datY /= k;
        
        int l = _dragStartRegion.left();
        int t = _dragStartRegion.top();
        int r = _dragStartRegion.right();
        int b = _dragStartRegion.bottom();

         if(mouse_event->buttons().testFlag(Qt::LeftButton)) {

            // Do nothing this time.
            if (_freezing){
                return QWidget::eventFilter(object, event);         
            }

            int minW = MainWindow::Min_Width;
            int minH = MainWindow::Min_Height;
          
            switch (_hit_border) {
                case TopLeft:
                    l += datX;
                    t += datY;
                    if (r - l < minW)
                        l = r - minW;
                    if (b - t < minH)
                        t = b - minH;
                   break;

                case BottomLeft:
                    l += datX;
                    b += datY;
                    if (r - l < minW)
                        l = r - minW;
                    if (b - t < minH)
                        b = t + minH;
                   break;

                case TopRight:
                    r += datX;
                    t += datY;
                    if (r - l < minW)
                        r = l + minW;
                    if (b - t < minH)
                        t = b - minH;
                   break;

                case BottomRight:
                    r += datX;
                    b += datY;
                    if (r - l < minW)
                        r = l + minW;
                    if (b - t < minH)
                        b = t + minH;
                   break;

                case Left:
                    l += datX;
                    if (r - l < minW)
                        l = r - minW;                    
                   break;

                case Right:
                    r += datX; 
                    if (r - l < minW)
                        r = l + minW;                     
                   break;

                case Top: 
                    t += datY; 
                    if (b - t < minH)
                        t = b - minH;                    
                   break;

                case Bottom: 
                    b += datY; 
                    if (b - t < minH)
                        b = t + minH;                    
                   break;

                default:
                    r = l;
                   break;
            }

            if (r != l){
                _frame->SetFormRegion(l, t, r-l, b-t);
                _frame->saveNormalRegion();
            }            
         
            return true;
        }
    }
    else if (type == QEvent::MouseButtonPress) {
        if (mouse_event->button() == Qt::LeftButton) 
        if (_hit_border != None)
            _bDraging = true;
        _timer.start(50); 

 
        POINT p;
        GetCursorPos(&p);
        _clickPos.setX(p.x);
        _clickPos.setY(p.y);


        _dragStartRegion = _frame->GetFormRegion();
    } 
    else if (type == QEvent::MouseButtonRelease) {
        if (mouse_event->button() == Qt::LeftButton) {         
            _bDraging = false;
            _timer.stop(); 
        }
    }
    else if (!_bDraging && type == QEvent::Leave) {
        _hit_border = None;
        setCursor(Qt::ArrowCursor);
    } 
    
    return QWidget::eventFilter(object, event);
}

}
