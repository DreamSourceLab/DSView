/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2013 DreamSourceLab <dreamsourcelab@dreamsourcelab.com>
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


#ifndef DSVIEW_PV_VIEW_VIEWPORT_H
#define DSVIEW_PV_VIEW_VIEWPORT_H

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

#include <QTimer>
#include <QWidget>
#include <stdint.h>

class QPainter;
class QPaintEvent;
class SigSession;

namespace pv {
namespace view {

class Signal;
class View;

class Viewport : public QWidget
{
	Q_OBJECT

public:
    static const int HitCursorMargin = 10;
    static constexpr double HitCursorTimeMargin;

public:
	explicit Viewport(View &parent);

	int get_total_height() const;

    QPoint get_mouse_point() const;

    void set_receive_len(quint64 length);

    QString get_measure(QString option);

    void set_measure_en(int enable);

    void start_trigger_timer(int msec);
    void stop_trigger_timer();

protected:
	void paintEvent(QPaintEvent *event);

private:
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
	void wheelEvent(QWheelEvent *event);
    void leaveEvent(QEvent *);

    void paintSignals(QPainter& p);
    void paintProgress(QPainter& p);
    void paintMeasure(QPainter &p);

    void measure();

private slots:
    void on_traces_moved();
    void on_trigger_timer();

signals:
    void mouse_measure();

private:
	View &_view;

    quint64 _total_receive_len;
    QPoint _mouse_point;
	QPoint _mouse_down_point;
	double _mouse_down_offset;
    double _curScale;
    double _curOffset;
    int _curSignalHeight;

    QPixmap pixmap;

    bool _zoom_rect_visible;
    QRectF _zoom_rect;

    bool _measure_en;
    bool _measure_shown;
    uint64_t _cur_sample;
    uint64_t _nxt_sample;
    uint64_t _thd_sample;
    double _cur_preX;
    double _cur_aftX;
    double _cur_thdX;
    double _cur_midY;
    QString _mm_width;
    QString _mm_period;
    QString _mm_freq;
    QString _mm_duty;

    QTimer trigger_timer;
    bool triggered;
    int timer_cnt;

    boost::shared_ptr<Signal> _drag_sig;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_VIEW_VIEWPORT_H
