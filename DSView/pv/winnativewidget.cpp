
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
#include "log.h"
#include "../config.h"

#define NAI_WIN_CREATE_STYLE WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_CLIPCHILDREN

namespace pv {
 
WinNativeWidget::WinNativeWidget(const int x, const int y, const int width, const int height)
{   
    _childWindow = nullptr;
    childWidget = nullptr;
    _hWnd = NULL;
    _event_callback = NULL;
    _is_moving = false;

    HBRUSH windowBackground = CreateSolidBrush(RGB(0, 0, 0));
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    WNDCLASSEX wcx = { 0 };

    wcx.cbSize = sizeof(WNDCLASSEX);
    wcx.style = CS_HREDRAW | CS_VREDRAW;
    wcx.hInstance = hInstance;
    wcx.lpfnWndProc = WndProc;
    wcx.cbClsExtra = 0;
    wcx.cbWndExtra = 0;
    wcx.lpszClassName = L"WindowClass";
    wcx.hbrBackground = windowBackground;
    wcx.hCursor = LoadCursor(hInstance, IDC_ARROW);


    RegisterClassEx(&wcx);
    if (FAILED(RegisterClassEx(&wcx)))
    {
        dsv_info("ERROR: can't register window class");
        assert(false);
    }

    QString title = QApplication::applicationName() + " v" + QApplication::applicationVersion();
    wchar_t title_string_buffer[50];
    int title_len = title.toWCharArray(title_string_buffer);
    title_string_buffer[title_len] = 0;
 
    _hWnd = CreateWindow(L"WindowClass", title_string_buffer,
            NAI_WIN_CREATE_STYLE, x, y, 
            width, height, 0, 0, hInstance, nullptr);

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
        case WM_SYSCOMMAND:
        {
            if (wParam == SC_KEYMENU)
            {
                RECT winrect;
                GetWindowRect(hWnd, &winrect);
                TrackPopupMenu(GetSystemMenu(hWnd, false), TPM_TOPALIGN | TPM_LEFTALIGN, winrect.left + 5, winrect.top + 5, 0, hWnd, NULL);
                break;
            }
            else
            {
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
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
            }

            break;
        }
        case WM_DPICHANGED:
            if (self->_is_moving == false){
                self->UpdateChildDpi();
                self->ResizeChild();
                dsv_info("Dpi was changed.");
            } 
            break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

void WinNativeWidget::ResizeChild()
{
    if (_childWindow != NULL){ 
        RECT rc;
        GetClientRect(_hWnd, &rc);

        int w = rc.right;
        int h = rc.bottom;

        WINDOWPLACEMENT wp;
        wp.length = sizeof(WINDOWPLACEMENT);
        GetWindowPlacement(_hWnd, &wp);

        if (wp.showCmd == SW_MAXIMIZE) { 
            w -= 8;
            h -= 8;
        }
        
        //childWidget->adjustSize();           
        MoveWindow(_childWindow, 0, 0, w - 1 , h - 1 , 1);
        MoveWindow(_childWindow, 0, 0, w , h , 1); 
        childWidget->updateGeometry();  
    }
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
    }
}

 void WinNativeWidget::Move(int x, int y)
 {
    RECT rc;
    GetClientRect(_hWnd, &rc);
    MoveWindow(_hWnd, x, y, rc.right, rc.bottom, 0);
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

}
