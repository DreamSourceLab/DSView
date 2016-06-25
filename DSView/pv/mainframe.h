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

namespace pv {

class DeviceManager;
class MainWindow;

namespace toolbars {
class TitleBar;
}

class MainFrame : public QFrame
{
    Q_OBJECT
public:
    static const int minWidth = 800;
    static const int minHeight = 680;

public:
    static const int Margin = 8;

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
    MainFrame(DeviceManager &device_manager,
        const char *open_file_name = NULL);

    void showMaxRestore();

protected:
    void changeEvent(QEvent* event);
    void resizeEvent(QResizeEvent *event);
    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *object, QEvent *event);

public slots:
    void unfreezing();
    void showNormal();
    void showMaximized();
    void showMinimized();

private:
    void hide_border();
    void show_border();

    void writeSettings();
    void readSettings();

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

    bool _moving;
    bool _draging;
    QPoint _lastMousePosition;
    QRect _dragStartGeometry;
    int _startPos;
    QTimer _timer;
    bool _freezing;
    bool _minimized;
};

} // namespace pv

#endif // DSVIEW_PV_MAINFRAME_H
