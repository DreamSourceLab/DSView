/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2014 DreamSourceLab <support@dreamsourcelab.com>
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


#ifndef DSVIEW_PV_VIEW_DEVMODE_H
#define DSVIEW_PV_VIEW_DEVMODE_H

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

#include <list>
#include <utility>
#include <map>
#include <set>

#include <QWidget>
#include <QPushButton>
#include <QGridLayout>
#include <QVector>

#include <libsigrok4DSL/libsigrok.h>

namespace pv {

namespace device{
class DevInst;
}

class SigSession;

namespace view {

class DevMode : public QWidget
{
	Q_OBJECT

private:
    static const int GRID_COLS = 3;

public:
    DevMode(QWidget *parent, SigSession &session);

private:
	void paintEvent(QPaintEvent *event);

private:
	void mousePressEvent(QMouseEvent * event);
	void mouseReleaseEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void leaveEvent(QEvent *event);

public slots:
    void set_device();
    void on_mode_change();

private slots:

signals:
    void mode_changed();

private:
    SigSession &_session;

    QGridLayout *  _layout;
    std::map <QPushButton *, sr_dev_mode *> _mode_button_list;
    QPoint _mouse_point;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_VIEW_DEVMODE_H
