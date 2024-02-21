
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

namespace pv {

class WinNativeWidget
{
public:

    WinNativeWidget(const int x, const int y, const int width, const int height);
    ~WinNativeWidget();

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    void SetChildWidget(QWidget *w);

    void setGeometry(const int x, const int y, const int width, const int height);

    inline HWND Handle(){
        return _hWnd;
    }

    void Show(bool bShow);
    void Move(int x, int y);

    void ShowNormal();
    void ShowMax();
    void ShowMin();

    void ResizeChild(bool bManul);

private: 
    QWidget*    childWidget;
    HWND        _childWindow;
    HWND        _hWnd;
};

}


#endif // WINNATIVEWINDOW_H
