
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


#include "WinNativeWidget.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QScreen>
#include <QGuiApplication>
#include <QWindow>
#include <dwmapi.h> 
#include <assert.h>
#include <QString>
#include <QtCore>
#include <QtGui>
#include <QtWidgets>
#include <qmath.h>

#include "log.h"
#include "../config.h"

#define FIXED_WIDTH(widget) (widget->minimumWidth() >= widget->maximumWidth())
#define FIXED_HEIGHT(widget) (widget->minimumHeight() >= widget->maximumHeight())
#define FIXED_SIZE(widget) (FIXED_WIDTH(widget) && FIXED_HEIGHT(widget))

namespace pv {

//-----------------------------WinNativeWidget 
WinNativeWidget::WinNativeWidget(const int x, const int y, const int width, const int height)
{   
    _childWindow = nullptr;
    childWidget = nullptr;
    _hWnd = NULL;
    _event_callback = NULL;
    _is_moving = false;
    _shadow = NULL;
    _cur_screen = NULL;

    HBRUSH windowBackground = CreateSolidBrush(RGB(0, 0, 0));
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    WNDCLASSEX wcx = { 0 };

    wcx.cbSize = sizeof(WNDCLASSEX);
    wcx.style = CS_HREDRAW | CS_VREDRAW;
    wcx.hInstance = hInstance;
    wcx.lpfnWndProc = WndProc;
    wcx.cbClsExtra = 0;
    wcx.cbWndExtra = 0;
    wcx.lpszClassName = L"DSViewWindowClass";
    wcx.hbrBackground = windowBackground;
    wcx.hCursor = LoadCursor(hInstance, IDC_ARROW);


    RegisterClassEx(&wcx);
    if (FAILED(RegisterClassEx(&wcx)))
    {
        dsv_info("ERROR: can't register window class");
        assert(false);
    }
 
    _hWnd = CreateWindow(L"DSViewWindowClass", L"DSView",
           // WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_CLIPCHILDREN,
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 
            x, y, width, height,
            NULL, NULL, hInstance, NULL);

    if (!_hWnd)
    {
        dsv_info("ERROR: can't create naitive window");
        assert(false);
    }

    SetWindowLongPtr(_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    SetWindowPos(_hWnd, NULL, x, y, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
}

WinNativeWidget::~WinNativeWidget()
{
    if (_hWnd){
        if (_shadow){
            _shadow->hide();
            delete _shadow;
            _shadow = NULL;
        }
        Show(false);
        DestroyWindow(_hWnd);
    }  
}

void WinNativeWidget::SetChildWidget(QWidget *w)
{ 
    childWidget = w;
    _childWindow = NULL;

    if (w != NULL){
        _childWindow = (HWND)w->winId();
        _shadow = new WinShadow(_hWnd, w);
        _shadow->createWinId();
    }    
}

LRESULT CALLBACK WinNativeWidget::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WinNativeWidget *self = reinterpret_cast<WinNativeWidget*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    if (self == NULL)
    {
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    switch (message)
    { 
        case WM_ACTIVATE:
        {
            switch (wParam)
            {
                case WA_ACTIVE:
                case WA_CLICKACTIVE:
                {
                    self->BeginShowShadow();
                    break;
                }
                case WA_INACTIVE:
                {    
                    break;
                }
            }
        }
        case WM_SYSCOMMAND:
        {
            if (wParam == SC_KEYMENU)
            {
                RECT winrect;
                GetWindowRect(hWnd, &winrect);
                TrackPopupMenu(GetSystemMenu(hWnd, false), TPM_TOPALIGN | TPM_LEFTALIGN, winrect.left + 5, winrect.top + 5, 0, hWnd, NULL);
            }
            else
            {
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
            break;;
        }
        case WM_NCCALCSIZE:
        { 
            return 0;
        } 
        case WM_CLOSE:
        {
            if (self->childWidget != NULL)
            {
                self->childWidget->close();
                return 0;
            }
            break;
        }
        case WM_DESTROY:
        {
           // PostQuitMessage(0);
            break;
        }
        case WM_SIZE:
        { 
            if (self->_childWindow != NULL){
                self->ResizeChild();

                if (self->_shadow){
                    if (self->IsNormalsized()){
                        self->_shadow->tryToShow();
                    }
                    else{
                        self->_shadow->hide();
                    }
                }
                
                self->MoveShadow();
            }

            break;
        }
        case WM_GETMINMAXINFO:
        {
            if (self->childWidget)
            {
                auto gw = self->childWidget;
                int k = gw->windowHandle()->devicePixelRatio();
                MINMAXINFO *mmi = reinterpret_cast<MINMAXINFO*>(lParam);

                int border_width = 0;
                QSize minimum = gw->minimumSize();
                QSize sizeHint = gw->minimumSizeHint();

                mmi->ptMinTrackSize.x = qFloor(qMax(minimum.width(), sizeHint.width()) *
                                            k) + border_width * 2;
                mmi->ptMinTrackSize.y = qFloor(qMax(minimum.height(), sizeHint.height()) *
                                            k) + border_width;

                QSize maximum = gw->maximumSize();

                mmi->ptMaxTrackSize.x = qFloor(maximum.width() * k) + border_width * 2;
                mmi->ptMaxTrackSize.y = qFloor(maximum.height() * k) + border_width;
            } 
            break;
        }
        case WM_DISPLAYCHANGE:            
        {
            dsv_info("Display changed.");

            if (self->_event_callback != NULL){
                self->_event_callback->OnParentNativeEvent(PARENT_EVENT_DISPLAY_CHANGED);
            }

            break;
        }
        case WM_WINDOWPOSCHANGING:
        {
            int st = 3;
            if (self->IsMaxsized())
                st = 1;
            else if (self->IsNormalsized())
                st = 2;
            
            if (self->childWidget != NULL && self->_shadow){
                if (st == 2){            
                    if (self->childWidget->isVisible() && !self->_is_moving){                        
                        self->_shadow->showLater();
                    }
                }
                else {
                    self->_shadow->hide();                   
                }
            }
            break;
        }
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

void WinNativeWidget::ResizeChild()
{
    if (_childWindow != NULL){ 
        RECT rc;
        GetWindowRect(_hWnd, &rc);

        int sp = 0;
        int x = sp;
        int y = sp;
        int w = rc.right - rc.left - sp * 2;
        int h = rc.bottom - rc.top - sp * 2;
 
        if (IsMaxsized()) { 
            w -= 8;
            h -= 8;
        }
                
        MoveWindow(_childWindow, x, y, w - 1 , h - 1 , 1);
        MoveWindow(_childWindow, x, y, w , h , 1); 
        childWidget->updateGeometry();
    }
}

void WinNativeWidget::ResizeSelf()
{
    if (_hWnd)
    {
        RECT rc;
        GetWindowRect(_hWnd, &rc);

        int x = rc.left;
        int y = rc.top;
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        static int times = 0;
        times++;

        if (times % 2 == 0){
            w += 2;
            h += 2;
        }
        else{
            w -= 2;
            h -= 2;
        }
        
        MoveWindow(_hWnd, x, y, w , h , 1);
    }
}

void WinNativeWidget::MoveShadow()
{
    if (!_shadow || !childWidget){
        return;
    }

    if (IsNormalsized())
    {  
    
        RECT rc;
        GetWindowRect(_hWnd, &rc);

        int bw = SHADOW_BORDER_WIDTH;

       // bw = 20;

        int x = rc.left;
        int y = rc.top;
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        x -= bw;
        y -= bw;
        w += bw * 2;
        h += bw * 2;

        MoveWindow((HWND)_shadow->winId(), x, y, w , h , 1);
        _shadow->setActive(isActiveWindow()); 
    } 
    else{
        _shadow->hide();
        _shadow->setActive(false);
        _shadow->update();
    }
}

bool WinNativeWidget::isActiveWindow()
{ 
    const HWND active_window = GetActiveWindow();
    return ((active_window == _hWnd) || IsChild(_hWnd, active_window)); 
}

void WinNativeWidget::setGeometry(const int x, const int y, const int width, const int height)
{
   // dsv_info("set parent window, x:%d, y:%d, w:%d, h:%d", x, y, width, height);
    if (_hWnd != NULL){
        MoveWindow(_hWnd, x, y, width, height, 1);
    }   
}

void WinNativeWidget::Show(bool bShow)
{
    if (_hWnd){
        ShowWindow(_hWnd, bShow ? SW_SHOW : SW_HIDE);

        if (!bShow && _shadow){
            _shadow->hide();
        }
    }
}

 void WinNativeWidget::Move(int x, int y)
 {
    RECT rc;
    GetClientRect(_hWnd, &rc);
    int w = rc.right;
    int h = rc.bottom;
    MoveWindow(_hWnd, x, y, w, h, 1);

    if (_shadow != NULL){
        auto scr = GetPointScreen();

        if (_cur_screen && scr && scr != _cur_screen){
            _shadow->windowHandle()->setScreen(scr);
        }
        _cur_screen = scr;

        MoveShadow();
    }
 }

void WinNativeWidget::ShowNormal()
{
    if (_hWnd){
        SendMessage(_hWnd, WM_SYSCOMMAND, SC_RESTORE, 0);
    }
}

void WinNativeWidget::ShowMax()
{
    if (_hWnd){
        SendMessage(_hWnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    }
}

void WinNativeWidget::ShowMin()
{
    if (_hWnd){
        SendMessage(_hWnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
    }
}

void WinNativeWidget::UpdateChildDpi()
{
    QScreen *scr = screenFromWindow(_hWnd);
    if (scr != NULL && childWidget != NULL){
        childWidget->windowHandle()->setScreen(scr);
        if (_shadow){
            _shadow->windowHandle()->setScreen(scr);
        }
    }
    else{
        dsv_info("ERROR: failed to update child's screen.");
    }
}

QScreen* WinNativeWidget::screenFromWindow(HWND hwnd)
{
    if (hwnd == NULL)
        return NULL;

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

    MONITORINFO monitor_info;
    memset(&monitor_info, 0, sizeof(MONITORINFO));
    monitor_info.cbSize = sizeof(MONITORINFO);

    GetMonitorInfoW(monitor, &monitor_info);

    QPoint top_left;
    top_left.setX(monitor_info.rcMonitor.left);
    top_left.setY(monitor_info.rcMonitor.top);

    for (QScreen *screen : QGuiApplication::screens())
    {
        if (screen->geometry().topLeft() == top_left)
        {
            return screen;
        }
    }
  
    return NULL;
}

QScreen* WinNativeWidget::GetPointScreen()
{
    return screenFromWindow(_hWnd);
}

void WinNativeWidget::OnDisplayChanged()
{ 
    UpdateChildDpi();
    ResizeChild();

    RECT rc;
    GetWindowRect(_hWnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    MoveWindow(_hWnd, rc.left, rc.top, w + 1, h + 1, 1);
}

bool WinNativeWidget::IsMaxsized()
{
    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(_hWnd, &wp);
    return wp.showCmd == SW_MAXIMIZE;
}

bool WinNativeWidget::IsMinsized()
{
    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(_hWnd, &wp);
    return wp.showCmd == SW_MINIMIZE;
}

bool WinNativeWidget::IsNormalsized()
{
    WINDOWPLACEMENT wp;
    wp.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(_hWnd, &wp);
    return wp.showCmd == SW_SHOWNORMAL;
}

void WinNativeWidget::BeginShowShadow()
{
    if (_shadow){
        _shadow->show();
        _shadow->showLater();
    }
}

}
