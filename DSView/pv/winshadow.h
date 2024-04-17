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

class IShadowCallback
{
public:
    virtual void OnForeWindowLosed()=0;
};

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

    inline bool IsActived(){
        return m_bActived;
    }

    inline void SetCallback(IShadowCallback *callback){
        m_callback = callback;
    }

signals:
    void showSignal();
     
private slots:
    void showShadow();     
    void onMoveSelf();
    void onCheckForeWindow();

private:
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override; 
    void paintEvent(QPaintEvent *event) override;
 
    QWidget     *m_parent;
    QTimer      *m_timer;
    QTimer      *m_checkTimer;
    HWND        m_hwnd;
    int         m_scale;
    bool        m_bActived;
    IShadowCallback *m_callback;
};

} // namespace pv

#endif
