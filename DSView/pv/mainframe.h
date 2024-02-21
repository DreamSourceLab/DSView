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


#ifndef DSVIEW_PV_MAINFRAME_H
#define DSVIEW_PV_MAINFRAME_H

#include "widgets/border.h"

#include <QFrame>
#include <QGridLayout>
#include <QTimer>
#include <QRect>

#ifdef _WIN32
#include <QWinTaskbarButton>
#include <QWinTaskbarProgress>
#endif

#include "toolbars/titlebar.h"

namespace pv {
 
class MainWindow;
class WinNativeWidget;

namespace dialogs {
class DSMessageBox;
class DSDialog;
}
 
struct Point
{
    int x;
    int y;
};

struct FormRegion{
    int x;
    int y;
    int w;
    int h;
};

struct FormInitInfo
{
    FormRegion r;
    bool isMaxSize;
};

class MainFrame : public QFrame, public ITitleParent
{
    Q_OBJECT

public:
    static const int Margin = 5;

    enum borderTypes{
        None,
        TopLeft,
        Left,
        BottomLeft,
        Bottom,
        BottomRight,
        Right,
        TopRight,
        Top
    }borderTypes;

public:
    MainFrame();
 
    void ShowFormInit();
    void ShowHelpDocAsync();
    
    void PrintRegionProc();
    void PrintRegion();

protected: 
    void resizeEvent(QResizeEvent *event);
    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *object, QEvent *event);
#ifdef _WIN32
    void showEvent(QShowEvent *event);
#endif

    void changeEvent(QEvent *event) override; 

public slots:
    void unfreezing();   
    void show_doc();
    void setTaskbarProgress(int progress);

    void showNormal();
    void showMaximized();
    void showMinimized();
    void moveToWinNaitiveNormal();

private:
    void hide_border();
    void show_border();
    void writeSettings();
    void saveNormalRegion();

    void ReadSettings();
    void AttachNativeWindow();

    //ITitleParent
    void MoveWindow(int x, int y) override;
    QPoint GetParentPos() override;
    bool ParentIsMaxsized() override;
    void MoveBegin() override;
    void MoveEnd() override;

    void SetFormRegion(int x, int y, int w, int h);
    QRect GetFormRegion();

private:
    toolbars::TitleBar *_titleBar;
    MainWindow *_mainWindow;

    QGridLayout *_layout;
    widgets::Border *_left;
    widgets::Border *_right;
    widgets::Border *_top;
    widgets::Border *_bottom;
    widgets::Border *_top_left;
    widgets::Border *_top_right;
    widgets::Border *_bottom_left;
    widgets::Border *_bottom_right;
 
    bool    _bDraging;  
    int     _hit_border;
    QTimer  _timer;
    bool    _freezing; 
    // Taskbar Progress Effert for Win7 and Above
#ifdef _WIN32
    QWinTaskbarButton *_taskBtn;
    QWinTaskbarProgress *_taskPrg;
#endif

    bool    _is_native_title;
    bool    _is_resize_ready;    

    WinNativeWidget *_parentNativeWidget; 
    FormInitInfo    _initWndInfo;
    FormRegion      _normalRegion;
    bool            _is_max_status;
    QPoint          _clickPos;
    QRect           _dragStartRegion; 
    QRect           _move_screen_region;
};

} // namespace pv

#endif // DSVIEW_PV_MAINFRAME_H
