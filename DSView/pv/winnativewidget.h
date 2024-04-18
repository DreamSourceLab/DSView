
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


#ifndef WINNATIVEWINDOW_H
#define WINNATIVEWINDOW_H


#include <Windows.h>
#include <Windowsx.h>
#include <QWidget>
#include <QByteArray>
#include <QColor>

#include "interface/icallbacks.h"
#include "winshadow.h"

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

#ifndef SM_CXPADDEDBORDER
#define SM_CXPADDEDBORDER 105
#endif


namespace pv {

class MainFrame;

class WinNativeWidget: public IShadowCallback
{
public:
    WinNativeWidget(const int x, const int y, const int width, const int heigh, QColor backColor);
    ~WinNativeWidget();
 
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    void SetChildWidget(MainFrame *w);

    void setGeometry(const int x, const int y, const int width, const int height);

    inline HWND Handle(){
        return _hWnd;
    }

    void Show(bool bShow);
    void Move(int x, int y);

    void ShowNormal();
    void ShowMax();
    void ShowMin();

    void UpdateChildDpi();
    void ResizeChild(); 
 
    QScreen* GetPointScreen();

    inline void SetNativeEventCallback(IParentNativeEventCallback *callback){
        _event_callback = callback;
    }
 
    bool IsMaxsized();
    bool IsMinsized();
    bool IsNormalsized(); 
    bool isActiveWindow();

    void SetBorderColor(QColor color);
    bool IsWin11OrGreater();

    inline void SetTitleBarWidget(QWidget *w){
        _titleBarWidget = w;
    }

    int GetDevicePixelRatio();
    bool IsVisible();

    void ReShowWindow();

private:   
    QScreen* screenFromCurrentMonitorHandle();
    bool isWinXOrGreater(DWORD major_version, DWORD minor_version, DWORD build_number);
    void showBorder();
    void hideBorder();
    static RECT GetMonitorArea(HMONITOR hMonitor, bool isPhysics);
    static bool getMonitorWorkArea(HMONITOR  hMonitor, int *outWidth, int *outHeight);
    
    void setShadowStatus(int windowStatus);

    //IShadowCallback
    void OnForeWindowLosed() override;

    LRESULT hitTest(HWND hWnd, WPARAM wParam, LPARAM lParam);

private: 
    MainFrame*  _childWidget;
    HWND        _childWindow;
    HWND        _hWnd;
    QWidget     *_titleBarWidget;
    IParentNativeEventCallback *_event_callback;
   
    bool        _is_native_border;
    HMONITOR    _hCurrentMonitor;
    WinShadow   *_shadow;
    QColor      _border_color;
    bool        _is_lose_foreground;
};

}


#endif // WINNATIVEWINDOW_H
