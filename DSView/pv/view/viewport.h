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


#ifndef DSVIEW_PV_VIEW_VIEWPORT_H
#define DSVIEW_PV_VIEW_VIEWPORT_H

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

#include <QTime>
#include <QTimer>
#include <QWidget>

#include <stdint.h>

#include "../../extdef.h"
#include "../view/view.h"

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
    static const double HitCursorTimeMargin;
    static const int DragTimerInterval = 100;
    static const int MinorDragOffsetUp = 100;
    static const int DsoMeasureStages = 3;
    static const double MinorDragRateUp;
    static const double DragDamping;
    static const int SnapMinSpace = 10;
    static const int WaitLoopTime = 400;
    enum ActionType {
        NO_ACTION,

        CURS_MOVE,

        LOGIC_EDGE,
        LOGIC_MOVE,
        LOGIC_ZOOM,

        DSO_XM_STEP0,
        DSO_XM_STEP1,
        DSO_XM_STEP2,
        DSO_YM,
        DSO_TRIG_MOVE
    };

    enum MeasureType {
        NO_MEASURE,
        LOGIC_FREQ,
        LOGIC_EDGE_CNT,
        DSO_VALUE
    };

public:
    explicit Viewport(View &parent, View_type type);

	int get_total_height() const;

    QPoint get_mouse_point() const;

    QString get_measure(QString option);

    void set_measure_en(int enable);

    void start_trigger_timer(int msec);
    void stop_trigger_timer();

    void clear_measure();

    void set_need_update(bool update);

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
    void on_drag_timer();
    void set_receive_len(quint64 length);

public slots:
    void show_wait_trigger();
    void unshow_wait_trigger();

signals:
    void measure_updated();

private:
	View &_view;
    View_type _type;
    bool _need_update;

    uint64_t _total_receive_len;
    QPoint _mouse_point;
	QPoint _mouse_down_point;
	double _mouse_down_offset;
    double _curScale;
    double _curOffset;
    int _curSignalHeight;

    QPixmap pixmap;

    bool _measure_en;
    ActionType _action_type;
    MeasureType _measure_type;
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

    uint64_t _edge_rising;
    uint64_t _edge_falling;
    uint64_t _edge_start;
    QString _em_rising;
    QString _em_falling;
    QString _em_edges;

    QTimer trigger_timer;
    bool triggered;
    int timer_cnt;

    boost::shared_ptr<Signal> _drag_sig;

    uint64_t _hover_index;
    bool _hover_hit;
    uint16_t _hover_sig_index;
    double _hover_sig_value;

    QTime _time;
    QTimer _drag_timer;
    int _drag_strength;

    bool _dso_xm_valid;
    int _dso_xm_y;
    uint64_t _dso_xm_index[DsoMeasureStages];

    bool _dso_ym_valid;
    uint16_t _dso_ym_sig_index;
    double _dso_ym_sig_value;
    uint64_t _dso_ym_index;
    int _dso_ym_start;
    int _dso_ym_end;

    int _waiting_trig;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_VIEW_VIEWPORT_H
