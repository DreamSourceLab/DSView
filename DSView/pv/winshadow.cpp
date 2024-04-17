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
 
#define COLOR1 QColor(0, 0, 0, 75)
#define COLOR2 QColor(0, 0, 0, 30)
#define COLOR3 QColor(0, 0, 0, 1)

namespace pv {

WinShadow::WinShadow(HWND hwnd, QWidget *parent) 
    : QWidget(parent)
{
 
    m_hwnd = HWND(hwnd);
    m_parent = parent;
    m_scale = 1;
    m_bActived = false;
    m_callback = NULL;

    setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                   (!m_parent ? Qt::Tool : Qt::WindowFlags(0)));

    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
 
    m_timer = new QTimer(this);
    m_timer->setInterval(100);
    m_timer->setSingleShot(true);

    m_checkTimer = new QTimer(this);
    m_checkTimer->setInterval(300);
    m_checkTimer->start();

    connect(m_timer, &QTimer::timeout, this, &WinShadow::showShadow);
    connect(this, &WinShadow::showSignal, this, &WinShadow::onMoveSelf);
    connect(m_checkTimer, &QTimer::timeout, this, &WinShadow::onCheckForeWindow);
}

void WinShadow::onMoveSelf()
{
    moveShadow();
}

void WinShadow::onCheckForeWindow()
{
    if (m_callback != NULL){
        HWND hw = GetForegroundWindow();
        if (hw != m_hwnd){
            m_callback->OnForeWindowLosed();
        }
    }
}

void WinShadow::showLater()
{
    m_timer->stop();
    m_timer->start();
    m_bActived = true;
}

void WinShadow::showShadow()
{ 
    if (m_timer->isActive())
        return;

    if (!IsWindowEnabled(m_hwnd))
        return;

    Q_EMIT showSignal();

    QWidget::show();
    QWidget::raise();

    SetWindowPos(m_hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE);
}

void WinShadow::hideShadow()
{
    m_timer->stop();

    if (!isVisible())
        return;

    QWidget::hide(); 
    m_bActived = false;
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
            }
            break;
        }
        case WM_MOUSEACTIVATE:
        { 
            SetForegroundWindow(m_hwnd); 
            *result = MA_NOACTIVATE;
            return true;
        }
        case WM_NCMOUSEMOVE:
        case WM_NCLBUTTONDOWN:
        case WM_NCLBUTTONUP:
        case WM_NCLBUTTONDBLCLK:
        case WM_NCHITTEST:
        { 
            *result = long(SendMessageW(m_hwnd, msg->message, msg->wParam, msg->lParam));
            return true;
        }
    }
    
    return QWidget::nativeEvent(eventType, message, result);
}
 
void WinShadow::moveShadow()
{
    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(m_hwnd, &wp);

    if (wp.showCmd == SW_SHOWNORMAL){
    
        RECT rc;
        GetWindowRect(m_hwnd, &rc);

        int bw = (SHADOW_BORDER_WIDTH - 1) * m_scale;
  
        int x = rc.left;
        int y = rc.top;
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        x -= bw;
        y -= bw;
        w += bw * 2;
        h += bw * 2;

        const HWND active_window = GetActiveWindow();
        bool isActiveWindow = ((active_window == m_hwnd) 
                        || IsChild(m_hwnd, active_window)); 

        MoveWindow((HWND)winId(), x, y, w , h , 1); 
    } 
}

void WinShadow::paintEvent(QPaintEvent *event)
{  
    const int shadow_width = SHADOW_BORDER_WIDTH;
    QPainter painter(this);
    painter.setCompositionMode(QPainter::CompositionMode_Source);

    QPixmap radial_gradient = QPixmap(shadow_width * 2, shadow_width * 2);
    {  
        radial_gradient.fill(QColor(0, 0, 0, 1));

        QPainter painter(&radial_gradient);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setCompositionMode(QPainter::CompositionMode_Source);

        QRadialGradient gradient(shadow_width, shadow_width, shadow_width);
        gradient.setColorAt(0.0, COLOR1);
        gradient.setColorAt(0.2, COLOR2);
        gradient.setColorAt(0.5, COLOR3);

        QPen pen(Qt::transparent, 0);
        painter.setPen(pen);
        painter.setBrush(gradient);
        painter.drawEllipse(0, 0, shadow_width * 2, shadow_width * 2);
    }

    QRect rect1 = rect().adjusted(shadow_width, shadow_width, -shadow_width, -shadow_width);

    painter.drawPixmap(0, 0, shadow_width, shadow_width, radial_gradient, 0, 0, shadow_width, shadow_width);
    painter.drawPixmap(rect().width() - shadow_width, 0, radial_gradient, shadow_width, 0, shadow_width, shadow_width); //Top-right corner
    painter.drawPixmap(0, rect().height() - shadow_width, radial_gradient, 0, shadow_width, shadow_width, shadow_width); //Bottom-left corner
    painter.drawPixmap(rect().width() - shadow_width, rect().height() - shadow_width, radial_gradient, shadow_width, shadow_width, shadow_width, shadow_width); //Bottom-right corner

    painter.drawPixmap(shadow_width, 0, rect1.width(), shadow_width, radial_gradient, shadow_width, 0, 1, shadow_width); //Top
    painter.drawPixmap(0, shadow_width, shadow_width, rect1.height(), radial_gradient, 0, shadow_width, shadow_width, 1); //Left
    painter.drawPixmap(rect1.width() + shadow_width, shadow_width, shadow_width, rect1.height(), radial_gradient, shadow_width, shadow_width, shadow_width, 1); //Right
    painter.drawPixmap(shadow_width, rect1.height() + shadow_width, rect1.width(), shadow_width, radial_gradient, shadow_width, shadow_width, 1, SHADOW_BORDER_WIDTH); //Bottom
}

}// namespace pv