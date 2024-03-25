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

#ifndef SHADOW_H
#define SHADOW_H

#include <QtCore>
#include <QtGui>
#include <QtWidgets>
#include <windows.h>

#define SHADOW_BORDER_WIDTH 11

namespace pv {

class WinShadow : public QWidget
{
    Q_OBJECT
public:
    explicit WinShadow(HWND hwnd, QWidget *parent);

    void moveShadow();

    inline void setScale(int k){
        m_scale = k;
    }

    void showLater();
    void hideShadow();

Q_SIGNALS:
    void showSignal();
     
public Q_SLOTS:   
    void showShadow();     
    void onMoveSelf();

private:
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override; 
    void paintEvent(QPaintEvent *event) override;
    void setActive(bool active);
 
    QWidget     *m_parent;
    QTimer      *m_timer; 
    HWND        m_hwnd;
    bool        m_active;
    int         m_scale; 
};

} // namespace pv

#endif
