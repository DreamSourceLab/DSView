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
 
#include <list>
#include <utility>
#include <map>
#include <set>
#include <QWidget>
#include <QPushButton>
#include <QVector>
#include <QToolButton>
#include <QLabel>
#include <libsigrok.h> 

#include "../interface/icallbacks.h"

struct dev_mode_name{
    int _mode;
    const char *_logo;
};
 
class DeviceAgent;

namespace pv {

class SigSession;

namespace view {

//devece work mode select list
class DevMode : public QWidget, public IFontForm
{
	Q_OBJECT

private:
    static const int GRID_COLS = 3;

public:
    DevMode(QWidget *parent, SigSession *session);

private:
	void paintEvent(QPaintEvent *event);

private:
	void mousePressEvent(QMouseEvent * event);
	void mouseReleaseEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void leaveEvent(QEvent *event);
    void changeEvent(QEvent *event);
    const dev_mode_name* get_mode_name(int mode);

    //IFontForm
    void update_font() override;

public slots:
    void set_device();
    void on_mode_change();
    void on_close();

private slots:

 

private:
    SigSession *_session;
    std::map <QAction *, const sr_dev_mode *> _mode_list;
    QToolButton     *_mode_btn;
    QMenu           *_pop_menu;
    QPoint          _mouse_point;
    QToolButton     *_close_button;
    bool            _bFile;

    DeviceAgent     *_device_agent;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_VIEW_DEVMODE_H
