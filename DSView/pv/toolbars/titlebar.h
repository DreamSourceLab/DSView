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

#ifndef DSVIEW_PV_TOOLBARS_TITLEBAR_H
#define DSVIEW_PV_TOOLBARS_TITLEBAR_H

#include <QWidget>
class QLabel;
class QToolButton;

namespace pv {
namespace toolbars {

class TitleBar : public QWidget
{
    Q_OBJECT

public:
    TitleBar(bool top, QWidget *parent, bool hasClose = false);
    void setTitle(QString title);
    QPoint get_startPos() const;
    QString title() const;

signals:
    void normalShow();
    void maximizedShow();

public slots:
    void showMaxRestore();
    void setRestoreButton(bool max);

protected:
    void paintEvent(QPaintEvent *);
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);

    QLabel *_title;
    QToolButton *_minimizeButton;
    QToolButton *_maximizeButton;
    QToolButton *_closeButton;

    bool _moving;
    bool _isTop;
    bool _hasClose;
    QPoint _startPos;
};

} // namespace toolbars
} // namespace pv

#endif // DSVIEW_PV_TOOLBARS_TITLEBAR_H
