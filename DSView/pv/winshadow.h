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

#ifndef WINSHADOW_H
#define WINSHADOW_H

#include <QtCore>
#include <QtGui>
#include <QtWidgets> 
#include <windows.h>
#include <QGridLayout>

#include "widgets/border.h"


#define SHADOW_BORDER_WIDTH 4
 
namespace pv {

class MainFrame;

class WinShadow : public QWidget
{
    Q_OBJECT

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
    explicit WinShadow(HWND hwnd, QWidget *parent);

    bool tryToShow();

    void SetClean(bool bClean);

    inline void SetFrame(MainFrame *frame){
        _frame = frame;
    }

Q_SIGNALS:
    void showSignal();

public Q_SLOTS:
    void showLater();
    void show();
    void hide();
    void setActive(bool active);
    int shadowWidth();

private:
    //Functions
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;

    bool eventFilter(QObject *object, QEvent *event) override;
   

private:

    QTimer      *m_timer; 
    HWND        m_hwnd;
    bool        m_active;
    bool        m_bClean;

    QGridLayout *_layout;
    widgets::Border *_left;
    widgets::Border *_right;
    widgets::Border *_top;
    widgets::Border *_bottom;
    widgets::Border *_top_left;
    widgets::Border *_top_right;
    widgets::Border *_bottom_left;
    widgets::Border *_bottom_right;

    MainFrame   *_frame;

    int     _hit_border;
    bool    _bDraging; 
    QTimer  _timer;
    bool    _freezing; 
    QPoint  _clickPos;
    QRect   _dragStartRegion;
 
}; 

}

#endif // SHADOW_H
