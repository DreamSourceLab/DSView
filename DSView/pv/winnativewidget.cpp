
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


#include "winnativewidget.h"
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
#include <string.h>

#include "log.h"
#include "../config.h"
#include "winshadow.h"

#define FIXED_WIDTH(widget) (widget->minimumWidth() >= widget->maximumWidth())
#define FIXED_HEIGHT(widget) (widget->minimumHeight() >= widget->maximumHeight())
#define FIXED_SIZE(widget) (FIXED_WIDTH(widget) && FIXED_HEIGHT(widget))

namespace pv {

//-----------------------------WinNativeWidget 
WinNativeWidget::WinNativeWidget(const int x, const int y, const int width, 
        const int height, bool isDark)
{   
    _childWindow = nullptr;
    childWidget = nullptr;
    _hWnd = NULL;
    _event_callback = NULL;
   
    _titleBarWidget = NULL;
    _is_native_border = IsWin11OrGreater();
    _hCurrentMonitor = NULL;
    _shadow = NULL; 
    _border_color = QColor(0x80,0x80,0x80);

    HINSTANCE hInstance = GetModuleHandle(nullptr);
    WNDCLASSEX wcx;

    memset(&wcx, 0, sizeof(WNDCLASSEXW));

    wcx.cbSize = sizeof(WNDCLASSEX);
    wcx.style = CS_HREDRAW | CS_VREDRAW;
    wcx.hInstance = hInstance;
    wcx.lpfnWndProc = WndProc;
    wcx.cbClsExtra = 0;
    wcx.cbWndExtra = 0;
    wcx.lpszClassName = L"DSViewWindowClass";
    wcx.hCursor = LoadCursor(hInstance, IDC_ARROW);

    if (isDark){
        wcx.hbrBackground = CreateSolidBrush(RGB(38, 38, 38));
    }
    else{
        wcx.hbrBackground = CreateSolidBrush(RGB(248, 248, 248));
    }
 
    RegisterClassEx(&wcx);
    if (FAILED(RegisterClassEx(&wcx)))
    {
        dsv_info("ERROR: can't register window class");
        assert(false);
    }
 
    _hWnd = CreateWindow(L"DSViewWindowClass", L"DSView",
            //WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_CLIPCHILDREN,
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, 
            x, y, width, height,
            NULL, NULL, hInstance, NULL);

    if (!_hWnd)
    {
        dsv_info("ERROR: can't create naitive window");
        assert(false);
    }
    
    SetWindowLongPtr(_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    SetWindowPos(_hWnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);

    if (!_is_native_border){
        _shadow = new WinShadow(_hWnd, NULL);
        _shadow->createWinId();
    }
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
    else if (_shadow != NULL){
        _shadow->hideShadow();
        _shadow->close(); //Set null, the applictoin will exit.
    }
}

void WinNativeWidget::ReShowWindow()
{
    if (IsMaxsized()){
        ShowMax();
    }
    else{
        ShowNormal();                    
    }
    Show(true);
}

LRESULT CALLBACK WinNativeWidget::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    WinNativeWidget *self = reinterpret_cast<WinNativeWidget*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    if (self == NULL){
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
            }
            break;;
        }      
        case WM_NCCALCSIZE:
        { 
            if (!wParam || self->IsMaxsized()){
                return 0;
            }
            if (!self->_is_native_border){
                return 0;
            }
            
            int k = self->GetDevicePixelRatio(); 
            NCCALCSIZE_PARAMS* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam); 
            RECT* rect = &params->rgrc[0]; 

            int borderSize = 1 * k;
            rect->left += borderSize;
            rect->top += 1;
            rect->right -= borderSize;
            rect->bottom -= borderSize;

            return WVR_VALIDRECTS;
        }
        case WM_NCACTIVATE:
        {   
            // Is activing.
            if (wParam){
                return 0;
            }

            HWND hForegWnd = GetForegroundWindow();
            HWND hActiveWnd = GetActiveWindow();

            // Is not the foreground window.
            if (hForegWnd != hActiveWnd){
                return 0;
            }
            
            //Is the foreground window, but is not actived, maybe opening a child dialog.
            SetWindowRedraw(hWnd, FALSE);
            LRESULT result = DefWindowProc(hWnd, message, wParam, lParam);
            SetWindowRedraw(hWnd, TRUE);
            return result;
        }
        case WM_CLOSE:
        {
            if (self->childWidget != NULL) {
                self->childWidget->close();
                return 0;
            }
            break;
        }
        case WM_MOVE:
        {
            if (IsIconic(hWnd) == FALSE) {
                HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);

                if (hMonitor == NULL){
                    dsv_info("ERROR: WM_MOVE: get an invalid monitor.");
                }
                else if (self->_hCurrentMonitor == NULL){
                    RECT rc = GetMonitorArea(hMonitor, true);

                    dsv_info("WM_MOVE: get a monitor, handle:%x, x:%d, y:%d, w:%d, h:%d", 
                        hMonitor, rc.left, rc.top,
                        rc.right - rc.left,
                        rc.bottom - rc.top);
                }

                if (hMonitor != self->_hCurrentMonitor && self->_hCurrentMonitor && hMonitor)
                {
                    RECT rc = GetMonitorArea(hMonitor, true);

                    dsv_info("WM_MOVE: display be changed, handle:%x, x:%d, y:%d, w:%d, h:%d", 
                        hMonitor, rc.left, rc.top,
                        rc.right - rc.left,
                        rc.bottom - rc.top);
                }
                
                self->_hCurrentMonitor = hMonitor;
                self->ResizeChild();
            }
            break;
        }
        case WM_DPICHANGED:
        case WM_DISPLAYCHANGE:            
        {
            if (message == WM_DPICHANGED){
                dsv_info("dpi changed.");
            }
            else{
                dsv_info("display changed.");
            }

            if (self->_event_callback != NULL && self->_childWindow != NULL){ 
                self->_event_callback->OnParentNativeEvent(PARENT_EVENT_DISPLAY_CHANGED);
            }
            break;
        }
        case WM_SIZE:
        {   
            if (wParam != SIZE_MINIMIZED && self->_childWindow != NULL){ 
               self->ResizeChild();
            }
            break;
        }
        case WM_WINDOWPOSCHANGING:
        {
            static int lst_state = -1;
            int st = 3;
            if (self->IsMaxsized())
                st = 1;
            else if (self->IsNormalsized())
                st = 2;
        
            if (self->childWidget != NULL && self->_shadow && lst_state != st){
                if (st == 2){            
                    if (self->childWidget->isVisible()){                        
                        self->_shadow->showLater();
                        self->showBorder();
                    }
                }
                else {
                    self->_shadow->hide();
                    self->hideBorder();
                }

                lst_state = st;
            }
            break;
        }
        case WM_GETMINMAXINFO:
        {
            if (self->childWidget && self->_hCurrentMonitor)
            {
                int maxWidth = 0;
                int maxHeight = 0;
                 
                if (self->getMonitorWorkArea(self->_hCurrentMonitor, &maxWidth, &maxHeight))
                {
                    auto gw = self->childWidget;
                    int k = self->GetDevicePixelRatio();
                    QSize minimum = gw->minimumSize();
                    QSize sizeHint = gw->minimumSizeHint();

                    MINMAXINFO *mmi = reinterpret_cast<MINMAXINFO*>(lParam);
                    mmi->ptMinTrackSize.x = qFloor(qMax(minimum.width(), sizeHint.width()) * k);
                    mmi->ptMinTrackSize.y = qFloor(qMax(minimum.height(), sizeHint.height()) * k);
                    mmi->ptMaxTrackSize.x = maxWidth;
                    mmi->ptMaxTrackSize.y = maxHeight;
                }                
            } 
            break;
        }
        case WM_NCHITTEST:
        {
            auto childWidget = self->childWidget;
            if (childWidget == NULL)
                break;

            
            int k = self->GetDevicePixelRatio();
            const LONG borderWidth = 8 * k;
            RECT winrect;
            GetWindowRect(hWnd, &winrect);
            long x = GET_X_LPARAM(lParam);
            long y = GET_Y_LPARAM(lParam);

            // Check if the size can to resize.
            if (!self->IsMaxsized())
            {
                //bottom left corner
                if (x >= winrect.left && x < winrect.left + borderWidth &&
                    y < winrect.bottom && y >= winrect.bottom - borderWidth)
                {
                    return HTBOTTOMLEFT;
                }
                //bottom right corner
                if (x < winrect.right && x >= winrect.right - borderWidth &&
                    y < winrect.bottom && y >= winrect.bottom - borderWidth)
                {
                    return HTBOTTOMRIGHT;
                }
                //top left corner
                if (x >= winrect.left && x < winrect.left + borderWidth &&
                    y >= winrect.top && y < winrect.top + borderWidth)
                {
                    return HTTOPLEFT;
                }
                //top right corner
                if (x < winrect.right && x >= winrect.right - borderWidth &&
                    y >= winrect.top && y < winrect.top + borderWidth)
                {
                    return HTTOPRIGHT;
                }
                //left border
                if (x >= winrect.left && x < winrect.left + borderWidth)
                {
                    return HTLEFT;
                }
                //right border
                if (x < winrect.right && x >= winrect.right - borderWidth)
                {
                    return HTRIGHT;
                }
                //bottom border
                if (y < winrect.bottom && y >= winrect.bottom - borderWidth)
                {
                    return HTBOTTOM;
                }
                //top border
                if (y >= winrect.top && y < winrect.top + borderWidth)
                {
                    return HTTOP;
                }
            } 

            // Check unble move.
            if (self->_titleBarWidget)
            {
                QRect titleRect = self->_titleBarWidget->geometry(); 

                int titleWidth = titleRect.width() * k - 55 * k;
                int titleHeight = titleRect.height() * k;
            
                if (x > winrect.left + 2 * k && x < winrect.left + titleWidth)
                {
                    if (y > winrect.top + 2 * k && y < winrect.top + titleHeight){                        
                        return HTCAPTION;
                    }
                }
            }

            break;
        }
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

RECT WinNativeWidget::GetMonitorArea(HMONITOR hMonitor, bool isPhysics)
{
    assert(hMonitor);

    MONITORINFO monitorInfo;
    memset(&monitorInfo, 0, sizeof(MONITORINFO));
    monitorInfo.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(hMonitor, &monitorInfo); 

    if (isPhysics){
        return monitorInfo.rcMonitor;
    }
    else{
        return monitorInfo.rcWork;
    }
}

bool WinNativeWidget::getMonitorWorkArea(HMONITOR hMonitor, int *outWidth, int *outHeight)
{  
    assert(outWidth);
    assert(outHeight);
    
    RECT rc = GetMonitorArea(hMonitor, false);
    
    int borderSizeX = GetSystemMetrics(SM_CXSIZEFRAME);
    int borderSizeY = GetSystemMetrics(SM_CYSIZEFRAME);
    int workAreaWidth =  rc.right -  rc.left;
    int workAreaHeight =  rc.bottom -  rc.top;
    int maxWidth = workAreaWidth + borderSizeX * 2;
    int maxHeight = workAreaHeight + borderSizeY * 2;

    if (workAreaWidth > 0){
        *outWidth = maxWidth;
        *outHeight = maxHeight;
        return true;
    }

    return false;
}

void WinNativeWidget::resizeSelf()
{
    if (_hWnd)
    {
        RECT rc;
        GetWindowRect(_hWnd, &rc);
        int x = rc.left;
        int y = rc.top;
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        MoveWindow(_hWnd, x, y, w - 1 , h - 1 , 1);
        MoveWindow(_hWnd, x, y, w , h , 1);
        ShowWindow(_hWnd, SW_SHOW);
    }
}

void WinNativeWidget::ResizeChild()
{
    if (_childWindow != NULL){ 

        int k =  GetDevicePixelRatio();
        
        if (_shadow != NULL){
            _shadow->setScale(k);
            _shadow->moveShadow();
        }

        RECT rc;
        GetClientRect(_hWnd, &rc);

        int x = 0;
        int y = 0;
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
  
        if (IsMaxsized()){
            int borderSizeX = GetSystemMetrics(SM_CXSIZEFRAME) - 2;
            int borderSizeY = GetSystemMetrics(SM_CYSIZEFRAME) - 2;
            x += borderSizeX;
            y += borderSizeY;
            w -= borderSizeX * 2;
            h -= borderSizeY * 2;
        }

        MoveWindow(_childWindow, x, y, w + 1 , h + 1 , 1);
        MoveWindow(_childWindow, x, y, w , h , 1);
        childWidget->updateGeometry();
    }
}

bool WinNativeWidget::isActiveWindow()
{ 
    const HWND active_window = GetActiveWindow();
    return ((active_window == _hWnd) || IsChild(_hWnd, active_window)); 
}

void WinNativeWidget::setGeometry(const int x, const int y, const int width, const int height)
{ 
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
    int w = rc.right;
    int h = rc.bottom;
    MoveWindow(_hWnd, x, y, w, h, 1); 
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
    QScreen *screen = GetPointScreen();

    if (screen == NULL){
        dsv_info("ERROR: failed to get pointing screen, will select the primary.");
        screen = QGuiApplication::primaryScreen();
    }

    if (screen != NULL && childWidget != NULL){
        childWidget->windowHandle()->setScreen(screen);
        if (_shadow != NULL){
            _shadow->windowHandle()->setScreen(screen);
        }
    }
}

QScreen* WinNativeWidget::screenFromCurrentMonitorHandle()
{  
    assert(_hWnd);

    HMONITOR hMonitor = _hCurrentMonitor;
    RECT rc;

    if (hMonitor != NULL)
    {
        rc = GetMonitorArea(hMonitor, true);

        if (rc.right - rc.left == 0){
          //  dsv_info("ERROR: Got an invalid monitor information from current monitor handle.");
            hMonitor = NULL;
        }
    }
 
    if (hMonitor == NULL){
        hMonitor = MonitorFromWindow(_hWnd, MONITOR_DEFAULTTONEAREST);

        if (hMonitor == NULL){
            dsv_info("ERROR: MonitorFromWindow() can't returns a handle.");
            return NULL;
        }

        rc = GetMonitorArea(hMonitor, true);

        if (rc.right - rc.left == 0){
          //  dsv_info("ERROR: Got an invalid monitor information from window handle.");
            return NULL;
        }
    } 

    QPoint top_left;
    top_left.setX(rc.left);
    top_left.setY(rc.top);

    for (QScreen *screen : QGuiApplication::screens())
    {
        if (screen->geometry().topLeft() == top_left)
        {
            return screen;
        }
    }

   // dsv_info("ERROR: can't match a monitor.");
  
    return NULL;
}

QScreen* WinNativeWidget::GetPointScreen()
{
    return screenFromCurrentMonitorHandle();
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

void WinNativeWidget::SetBorderColor(QColor color)
{
    _border_color = color;

    if (_hWnd)
    { 
        if (_is_native_border)
        {
            const DWORD DWMWINDOWATTRIBUTE_DWMWA_BORDER_COLOR = 34;
            COLORREF COLOR = RGB(color.red(), color.green(), color.blue());
            typedef HRESULT(WINAPI *tDwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
            tDwmSetWindowAttribute pDwmSetWindowAttribute =
                tDwmSetWindowAttribute(QLibrary::resolve("dwmapi", "DwmSetWindowAttribute"));
            if (pDwmSetWindowAttribute){
                pDwmSetWindowAttribute(_hWnd, DWMWINDOWATTRIBUTE_DWMWA_BORDER_COLOR, &COLOR, sizeof(COLOR));
            }
        }
        else if (childWidget != NULL){
            if (IsMaxsized()){
                hideBorder();
            }
            else{
                showBorder();
            }
        } 
    }
}

void WinNativeWidget::showBorder()
{
    if (childWidget != NULL && !_is_native_border){
        childWidget->setObjectName("DSViewFrame");
        QString borderCss = "#DSViewFrame {border-radius:0px; border:1px solid %1;}";
        QString borderStyle = borderCss.arg(_border_color.name());
        childWidget->setStyleSheet(borderStyle);
    }
}

void WinNativeWidget::hideBorder()
{
     if (childWidget != NULL && !_is_native_border){
        childWidget->setObjectName("DSViewFrame");
        QString borderCss = "#DSViewFrame {border-radius:0px; border:0px solid %1;}";
        QString borderStyle = borderCss.arg(_border_color.name());
        childWidget->setStyleSheet(borderStyle);
    }
}

bool WinNativeWidget::isWinXOrGreater(DWORD major_version, DWORD minor_version, DWORD build_number)
{
    bool is_win_x_or_greater = false;

    typedef NTSTATUS(WINAPI *tRtlGetVersion)(LPOSVERSIONINFOEXW);
    tRtlGetVersion pRtlGetVersion = tRtlGetVersion(QLibrary::resolve("ntdll", "RtlGetVersion"));

    if (pRtlGetVersion)
    {
        OSVERSIONINFOEXW os_info;
        memset(&os_info, 0, sizeof(OSVERSIONINFOEXW));
        os_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
        NTSTATUS status = pRtlGetVersion(&os_info);
        if (status == 0)
        {
            is_win_x_or_greater = (os_info.dwMajorVersion >= major_version &&
                                   os_info.dwMinorVersion >= minor_version &&
                                   os_info.dwBuildNumber >= build_number);
        }
    }

    return is_win_x_or_greater;
}

bool WinNativeWidget::IsWin11OrGreater()
{
    return isWinXOrGreater(10, 0, 22000);
}

int WinNativeWidget::GetDevicePixelRatio()
{
    auto screen = GetPointScreen();
    if (screen != NULL){
        return screen->devicePixelRatio();
    }
    else{
        return QGuiApplication::primaryScreen()->devicePixelRatio();
    } 
}

bool WinNativeWidget::IsVisible()
{
    if (_hWnd){
        return IsWindowVisible(_hWnd);
    }
    return false;
}

}
