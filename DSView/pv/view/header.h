/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2013 DreamSourceLab <support@dreamsourcelab.com>
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


#ifndef DSVIEW_PV_VIEW_HEADER_H
#define DSVIEW_PV_VIEW_HEADER_H

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

#include <list>
#include <utility>

#include <QWidget>
#include <QLineEdit>

namespace pv {
namespace view {

class Trace;
class View;

class Header : public QWidget
{
	Q_OBJECT

public:
	Header(View &parent);

private:
    boost::shared_ptr<pv::view::Trace> get_mTrace(
        int &action,
		const QPoint &pt);

private:
	void paintEvent(QPaintEvent *event);

private:
	void mousePressEvent(QMouseEvent * event);
	void mouseReleaseEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
	void leaveEvent(QEvent *event);
    void wheelEvent(QWheelEvent *event);
    void contextMenuEvent(QContextMenuEvent *event);

    void changeName(QMouseEvent *event);
    void changeColor(QMouseEvent *event);

public:
    int get_nameEditWidth();
    void header_resize();

private slots:
	void on_action_set_name_triggered();

    void on_action_add_group_triggered();

    void on_action_del_group_triggered();

    void on_traces_moved();

signals:
    void traces_moved();
    void header_updated();
    void vDial_changed(quint16);
    void acdc_changed(quint16);
    void ch_changed(quint16);

private:
	View &_view;

    bool _moveFlag;
    bool _colorFlag;
    bool _nameFlag;

	QPoint _mouse_point;
	QPoint _mouse_down_point;

    QLineEdit *nameEdit;

    std::list<std::pair<boost::weak_ptr<Trace>, int> >
        _drag_traces;

    boost::shared_ptr<Trace> _context_trace;

    QAction *_action_add_group;
    QAction *_action_del_group;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_VIEW_HEADER_H
