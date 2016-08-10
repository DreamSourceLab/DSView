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


#include "viewport.h"
#include "ruler.h"

#include "signal.h"
#include "dsosignal.h"
#include "logicsignal.h"
#include "mathtrace.h"
#include "../device/devinst.h"
#include "../data/logic.h"
#include "../data/logicsnapshot.h"
#include "../sigsession.h"
#include "../dialogs/dsomeasure.h"
#include "decodetrace.h"

#include <QMouseEvent>
#include <QStyleOption>


#include <math.h>

#include <boost/foreach.hpp>

using namespace boost;
using namespace std;

namespace pv {
namespace view {

const double Viewport::DragDamping = 1.05;
const double Viewport::MinorDragRateUp = 10;

Viewport::Viewport(View &parent, View_type type) :
    QWidget(&parent),
    _view(parent),
    _type(type),
    _need_update(false),
    _total_receive_len(0),
    _action_type(NO_ACTION),
    _measure_type(NO_MEASURE),
    _cur_sample(0),
    _nxt_sample(1),
    _cur_preX(0),
    _cur_aftX(1),
    _cur_midY(0),
    _hover_index(0),
    _hover_hit(false),
    _dso_xm_valid(false),
    _dso_ym_valid(false),
    _waiting_trig(0)
{
	setMouseTracking(true);
	setAutoFillBackground(true);
    setBackgroundRole(QPalette::Base);

    //setFixedSize(QSize(600, 400));
    _mm_width = "#####";
    _mm_period = "#####";
    _mm_freq = "#####";
    _mm_duty = "#####";
    _measure_en = true;
    triggered = false;
    timer_cnt = 0;

    // drag inertial
    _drag_strength = 0;
    _drag_timer.setSingleShot(true);

    connect(&_view, SIGNAL(traces_moved()),
        this, SLOT(on_traces_moved()));
    connect(&trigger_timer, SIGNAL(timeout()),
            this, SLOT(on_trigger_timer()));
    connect(&_drag_timer, SIGNAL(timeout()),
            this, SLOT(on_drag_timer()));

    connect(&_view.session(), &SigSession::receive_data,
            this, &Viewport::set_receive_len);
}

int Viewport::get_total_height() const
{
	int h = 0;

    const vector< boost::shared_ptr<Trace> > traces(_view.get_traces(_type));
    BOOST_FOREACH(const boost::shared_ptr<Trace> t, traces) {
        assert(t);
        h += (int)(t->get_totalHeight());
    }
    h += 2 * View::SignalMargin;

	return h;
}

QPoint Viewport::get_mouse_point() const
{
    return _mouse_point;
}

void Viewport::paintEvent(QPaintEvent *event)
{
    (void)event;

    using pv::view::Signal;

    QStyleOption o;
    o.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &o, &p, this);

    //p.begin(this);
    const vector< boost::shared_ptr<Trace> > traces(_view.get_traces(_type));
    BOOST_FOREACH(const boost::shared_ptr<Trace> t, traces)
    {
        assert(t);
        t->paint_back(p, 0, _view.get_view_width());
        if (t->enabled() && _view.session().get_device()->dev_inst()->mode == DSO)
            break;
    }

    p.setRenderHint(QPainter::Antialiasing, false);
    if (_view.session().get_device()->dev_inst()->mode == LOGIC ||
        _view.session().get_instant()) {
        switch(_view.session().get_capture_state()) {
        case SigSession::Init:
            break;

        case SigSession::Stopped:
            paintSignals(p);
            break;

        case SigSession::Running:
            if (_type == TIME_VIEW) {
                p.setRenderHint(QPainter::Antialiasing);
                paintProgress(p);
                p.setRenderHint(QPainter::Antialiasing, false);
            }
            break;
        }
    } else {
        paintSignals(p);
    }

    BOOST_FOREACH(const boost::shared_ptr<Trace> t, traces)
    {
        assert(t);
        if (t->enabled())
            t->paint_fore(p, 0, _view.get_view_width());
    }

    //p.setRenderHint(QPainter::Antialiasing, false);
    if (_view.get_signalHeight() != _curSignalHeight)
            _curSignalHeight = _view.get_signalHeight();

	p.end();
}

void Viewport::paintSignals(QPainter &p)
{
    if (_view.session().get_data_lock())
        return;
    const vector< boost::shared_ptr<Trace> > traces(_view.get_traces(_type));
    if (_view.scale() != _curScale ||
        _view.offset() != _curOffset ||
        _view.get_signalHeight() != _curSignalHeight ||
        _need_update) {
        _curScale = _view.scale();
        _curOffset = _view.offset();
        _curSignalHeight = _view.get_signalHeight();

        pixmap = QPixmap(size());
        pixmap.fill(Qt::transparent);
        QPainter dbp(&pixmap);
        dbp.initFrom(this);
        //p.setRenderHint(QPainter::Antialiasing, false);
        BOOST_FOREACH(const boost::shared_ptr<Trace> t, traces)
        {
            assert(t);
            if (t->enabled())
                t->paint_mid(dbp, 0, t->get_view_rect().width());
        }
        _need_update = false;
    }
    p.drawPixmap(0, 0, pixmap);

    // plot cursors
    if (_view.cursors_shown() && _type == TIME_VIEW) {
        list<Cursor*>::iterator i = _view.get_cursorList().begin();
        double cursorX;
        const double samples_per_pixel = _view.session().cur_samplerate() * _view.scale();
        while (i != _view.get_cursorList().end()) {
            cursorX = (*i)->index()/samples_per_pixel - (_view.offset() / _view.scale());
            if (rect().contains(_view.hover_point().x(), _view.hover_point().y()) &&
                    qAbs(cursorX - _view.hover_point().x()) <= HitCursorMargin)
                (*i)->paint(p, rect(), 1);
            else
                (*i)->paint(p, rect(), 0);
            i++;
        }
    }

    if (_type == TIME_VIEW) {
        if (_view.trig_cursor_shown()) {
            _view.get_trig_cursor()->paint(p, rect(), 0);
        }
        if (_view.search_cursor_shown()) {
            _view.get_search_cursor()->paint(p, rect(), 0);
        }

        // plot zoom rect
        if (_action_type == LOGIC_ZOOM) {
            p.setPen(Qt::NoPen);
            p.setBrush(Trace::dsLightBlue);
            p.drawRect(QRectF(_mouse_down_point, _mouse_point));
        }

        //plot measure arrow
        paintMeasure(p);

        //plot trigger information
        if (_view.session().get_device()->dev_inst()->mode == DSO &&
            _view.session().get_capture_state() == SigSession::Running) {
            uint8_t type;
            bool roll = false;
            QString type_str="";
            GVariant *gvar = _view.session().get_device()->get_config(NULL, NULL, SR_CONF_ROLL);
            if (gvar != NULL) {
                roll = g_variant_get_boolean(gvar);
                g_variant_unref(gvar);
            }
            gvar = _view.session().get_device()->get_config(NULL, NULL, SR_CONF_TRIGGER_SOURCE);
            if (gvar != NULL) {
                type = g_variant_get_byte(gvar);
                g_variant_unref(gvar);
                if (type == DSO_TRIGGER_AUTO && roll) {
                    type_str = "Auto(Roll)";
                } else if (type == DSO_TRIGGER_AUTO && !_view.session().trigd()) {
                    type_str = "Auto";
                } else if (_waiting_trig > 0) {
                    type_str = "Waiting Trig";
                    for (int i = 1; i < _waiting_trig; i++)
                        if (i % (WaitLoopTime / SigSession::ViewTime) == 0)
                            type_str += ".";
                } else {
                    type_str = "Trig'd";
                }
            }
            p.setPen(Trace::DARK_FORE);
            p.drawText(_view.get_view_rect(), Qt::AlignLeft | Qt::AlignTop, type_str);
        }
    }
}

void Viewport::paintProgress(QPainter &p)
{
    using pv::view::Signal;

    const uint64_t _total_sample_len = _view.session().cur_samplelimits();

    double progress = -(_total_receive_len * 1.0 / _total_sample_len * 360 * 16);
    int captured_progress = 0;

    p.setPen(Qt::gray);
    p.setBrush(Qt::NoBrush);
    const QPoint cenPos = QPoint(_view.get_view_width() / 2, height() / 2);
    const int radius = min(0.3 * _view.get_view_width(), 0.3 * height());
    p.drawEllipse(cenPos, radius - 2, radius - 2);
    p.setPen(QPen(Trace::dsGreen, 4, Qt::SolidLine));
    p.drawArc(cenPos.x() - radius, cenPos.y() - radius, 2* radius, 2 * radius, 180 * 16, progress);

    p.setPen(Qt::gray);
    const QPoint logoPoints[] = {
        QPoint(cenPos.x() - 0.75 * radius, cenPos.y()),
        QPoint(cenPos.x() - 0.75 * radius, cenPos.y() + 0.15 * radius),
        QPoint(cenPos.x() - 0.6 * radius, cenPos.y()),
        QPoint(cenPos.x() - 0.6 * radius, cenPos.y() + 0.3 * radius),
        QPoint(cenPos.x() - 0.45 * radius, cenPos.y()),
        QPoint(cenPos.x() - 0.45 * radius, cenPos.y() + 0.45 * radius),
        QPoint(cenPos.x() - 0.3 * radius, cenPos.y()),
        QPoint(cenPos.x() - 0.3 * radius, cenPos.y() + 0.3 * radius),
        QPoint(cenPos.x() - 0.15 * radius, cenPos.y()),
        QPoint(cenPos.x() - 0.15 * radius, cenPos.y() + 0.15 * radius),
        QPoint(cenPos.x() + 0.15 * radius, cenPos.y()),
        QPoint(cenPos.x() + 0.15 * radius, cenPos.y() - 0.15 * radius),
        QPoint(cenPos.x() + 0.3 * radius, cenPos.y()),
        QPoint(cenPos.x() + 0.3 * radius, cenPos.y() - 0.3 * radius),
        QPoint(cenPos.x() + 0.45 * radius, cenPos.y()),
        QPoint(cenPos.x() + 0.45 * radius, cenPos.y() - 0.45 * radius),
        QPoint(cenPos.x() + 0.6 * radius, cenPos.y()),
        QPoint(cenPos.x() + 0.6 * radius, cenPos.y() - 0.3 * radius),
        QPoint(cenPos.x() + 0.75 * radius, cenPos.y()),
        QPoint(cenPos.x() + 0.75 * radius, cenPos.y() - 0.15 * radius)
    };
    const int logoRadius = 10;
    p.drawLine(logoPoints[0], logoPoints[1]);
    p.drawLine(logoPoints[2], logoPoints[3]);
    p.drawLine(logoPoints[4], logoPoints[5]);
    p.drawLine(logoPoints[6], logoPoints[7]);
    p.drawLine(logoPoints[8], logoPoints[9]);
    p.drawLine(logoPoints[10], logoPoints[11]);
    p.drawLine(logoPoints[12], logoPoints[13]);
    p.drawLine(logoPoints[14], logoPoints[15]);
    p.drawLine(logoPoints[16], logoPoints[17]);
    p.drawLine(logoPoints[18], logoPoints[19]);
    p.drawEllipse(logoPoints[1].x() - 0.5 * logoRadius, logoPoints[1].y(),
            logoRadius, logoRadius);
    p.drawEllipse(logoPoints[3].x() - 0.5 * logoRadius, logoPoints[3].y(),
            logoRadius, logoRadius);
    p.drawEllipse(logoPoints[5].x() - 0.5 * logoRadius, logoPoints[5].y(),
            logoRadius, logoRadius);
    p.drawEllipse(logoPoints[7].x() - 0.5 * logoRadius, logoPoints[7].y(),
            logoRadius, logoRadius);
    p.drawEllipse(logoPoints[9].x() - 0.5 * logoRadius, logoPoints[9].y(),
            logoRadius, logoRadius);
    p.drawEllipse(logoPoints[11].x() - 0.5 * logoRadius, logoPoints[11].y() - logoRadius,
            logoRadius, logoRadius);
    p.drawEllipse(logoPoints[13].x() - 0.5 * logoRadius, logoPoints[13].y() - logoRadius,
            logoRadius, logoRadius);
    p.drawEllipse(logoPoints[15].x() - 0.5 * logoRadius, logoPoints[15].y() - logoRadius,
            logoRadius, logoRadius);
    p.drawEllipse(logoPoints[17].x() - 0.5 * logoRadius, logoPoints[17].y() - logoRadius,
            logoRadius, logoRadius);
    p.drawEllipse(logoPoints[19].x() - 0.5 * logoRadius, logoPoints[19].y() - logoRadius,
            logoRadius, logoRadius);

    if (!triggered) {
        const int width = _view.get_view_width();
        const QPoint cenLeftPos = QPoint(width / 2 - 0.05 * width, height() / 2);
        const QPoint cenRightPos = QPoint(width / 2 + 0.05 * width, height() / 2);
        const int trigger_radius = min(0.02 * width, 0.02 * height());

        p.setPen(Qt::NoPen);
        p.setBrush((timer_cnt % 3) == 0 ? Trace::dsLightBlue : Trace::dsGray);
        p.drawEllipse(cenLeftPos, trigger_radius, trigger_radius);
        p.setBrush((timer_cnt % 3) == 1 ? Trace::dsLightBlue : Trace::dsGray);
        p.drawEllipse(cenPos, trigger_radius, trigger_radius);
        p.setBrush((timer_cnt % 3) == 2 ? Trace::dsLightBlue : Trace::dsGray);
        p.drawEllipse(cenRightPos, trigger_radius, trigger_radius);

        sr_status status;
        if (sr_status_get(_view.session().get_device()->dev_inst(), &status, SR_STATUS_TRIG_BEGIN, SR_STATUS_TRIG_END) == SR_OK){
            const bool triggred = status.trig_hit & 0x01;
            uint32_t captured_cnt = (status.captured_cnt0 +
                                          (status.captured_cnt1 << 8) +
                                          (status.captured_cnt2 << 16) +
                                          (status.captured_cnt3 << 24));
            if (_view.session().get_device()->dev_inst()->mode == DSO)
                captured_cnt = captured_cnt * _view.session().get_signals().size() / _view.session().get_ch_num(SR_CHANNEL_DSO);
            if (triggred)
                captured_progress = (_total_sample_len - captured_cnt) * 100.0 / _total_sample_len;
            else
                captured_progress = captured_cnt * 100.0 / _total_sample_len;


            p.setPen(Trace::dsLightBlue);
            QFont font=p.font();
            font.setPointSize(10);
            font.setBold(true);
            p.setFont(font);
            QRect status_rect = QRect(cenPos.x() - radius, cenPos.y() + radius * 0.4, radius * 2, radius * 0.5);
            if (triggred)
                p.drawText(status_rect,
                           Qt::AlignCenter | Qt::AlignVCenter,
                           "Triggered! " + QString::number(captured_progress)+"% Captured");
            else
                p.drawText(status_rect,
                           Qt::AlignCenter | Qt::AlignVCenter,
                           "Waiting for Trigger! " + QString::number(captured_progress)+"% Captured");
        }

    } else {
        const int progress100 = ceil(progress / -3.6 / 16);
        p.setPen(Trace::dsGreen);
        QFont font=p.font();
        font.setPointSize(50);
        font.setBold(true);
        p.setFont(font);
        p.drawText(rect(), Qt::AlignCenter | Qt::AlignVCenter, QString::number(progress100)+"%");
    }

    p.setPen(QPen(Trace::dsLightBlue, 4, Qt::SolidLine));
    const int int_radius = max(radius - 4, 0);
    p.drawArc(cenPos.x() - int_radius, cenPos.y() - int_radius, 2* int_radius, 2 * int_radius, 180 * 16, -captured_progress*3.6*16);
    QFont font;
    p.setFont(font);
}

void Viewport::mousePressEvent(QMouseEvent *event)
{
	assert(event);

	_mouse_down_point = event->pos();
	_mouse_down_offset = _view.offset();
    _drag_strength = 0;
    _time.restart();

    if (event->button() == Qt::LeftButton) {
        const vector< boost::shared_ptr<Signal> > sigs(_view.session().get_signals());
        BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs) {
            assert(s);
            if (!s->enabled())
                continue;
            boost::shared_ptr<DsoSignal> dsoSig;
            if (dsoSig = dynamic_pointer_cast<DsoSignal>(s)) {
                if (dsoSig->get_ms_show_hover()) {
                    dsoSig->set_ms_show(!dsoSig->get_ms_show());
                    break;
                } else if (dsoSig->get_ms_gear_hover()) {
                    pv::dialogs::DsoMeasure dsoMeasureDialog(this, dsoSig);
                    dsoMeasureDialog.exec();
                    break;
                }
            }
        }

        update();
    }

    if (_action_type == NO_ACTION &&
        event->button() == Qt::RightButton &&
        _view.session().get_capture_state() == SigSession::Stopped) {
        if (_view.session().get_device()->dev_inst()->mode == LOGIC) {
            _action_type = LOGIC_ZOOM;
        } else if (_view.session().get_device()->dev_inst()->mode == DSO) {
            if (_hover_hit) {
                uint64_t index = (_view.offset() + (event->pos().x() + 0.5) * _view.scale()) * _view.session().cur_samplerate();
                _view.add_cursor(view::Ruler::CursorColor[_view.get_cursorList().size() % 8], index);
                _view.show_cursors(true);
            }
        }
    }
}

void Viewport::mouseMoveEvent(QMouseEvent *event)
{
	assert(event);
    _hover_hit = false;

    if (event->buttons() & Qt::LeftButton) {
        if (_type == TIME_VIEW) {
            _view.set_scale_offset(_view.scale(),
                _mouse_down_offset +
                (_mouse_down_point - event->pos()).x() *
                _view.scale());
            _drag_strength = (_mouse_down_point - event->pos()).x();
        } else if (_type == FFT_VIEW) {
            BOOST_FOREACH(const boost::shared_ptr<view::MathTrace> t, _view.session().get_math_signals()) {
                assert(t);
                if(t->enabled()) {
                    double delta = (_mouse_point - event->pos()).x();
                    t->set_offset(delta);
                    break;
                }
            }
        }
    }

    if (_type == TIME_VIEW) {
        if (!(event->buttons() || Qt::NoButton)) {
            if (_action_type == DSO_TRIG_MOVE) {
                if (_drag_sig) {
                    boost::shared_ptr<view::DsoSignal> dsoSig;
                    if (dsoSig = dynamic_pointer_cast<view::DsoSignal>(_drag_sig))
                        dsoSig->set_trig_vpos(event->pos().y(), true);
                }
            }

            if (_action_type == CURS_MOVE) {
                uint64_t sample_rate = _view.session().cur_samplerate();
                TimeMarker* grabbed_marker = _view.get_ruler()->get_grabbed_cursor();
                if (_view.cursors_shown() && grabbed_marker) {
                    int curX = _view.hover_point().x();
                    uint64_t index0 = 0, index1 = 0, index2 = 0;
                    bool logic = false;
                    const vector< boost::shared_ptr<Signal> > sigs(_view.session().get_signals());
                    BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs) {
                        assert(s);
                        boost::shared_ptr<view::LogicSignal> logicSig;
                        boost::shared_ptr<view::DsoSignal> dsoSig;
                        if ((_view.session().get_device()->dev_inst()->mode == LOGIC) &&
                            (logicSig = dynamic_pointer_cast<view::LogicSignal>(s))) {
                            if (logicSig->measure(event->pos(), index0, index1, index2)) {
                                logic = true;
                                break;
                            }
                        }
                        if ((_view.session().get_device()->dev_inst()->mode == DSO) &&
                            (dsoSig = dynamic_pointer_cast<view::DsoSignal>(s))) {
                            curX = min(dsoSig->get_view_rect().right(), curX);
                            break;
                        }
                    }

                    const double cur_time = _view.offset() + curX * _view.scale();
                    const double pos = cur_time * sample_rate;
                    const double pos_delta = pos - (uint64_t)pos;
                    const double samples_per_pixel = sample_rate * _view.scale();
                    const double index_offset = _view.offset() / _view.scale();
                    const double curP = index0 / samples_per_pixel - index_offset;
                    const double curN = index1 / samples_per_pixel - index_offset;
                    if (logic && (curX - curP < SnapMinSpace || curN - curX < SnapMinSpace)) {
                        if (curX - curP < curN - curX)
                            grabbed_marker->set_index(index0);
                        else
                            grabbed_marker->set_index(index1);
                    } else if ( pos_delta < 0.5) {
                        grabbed_marker->set_index((uint64_t)floor(pos));
                    } else {
                        grabbed_marker->set_index((uint64_t)ceil(pos));
                    }
                }
            }

            if (_action_type == DSO_XM_STEP1 || _action_type == DSO_XM_STEP2) {
                BOOST_FOREACH(const boost::shared_ptr<Signal> s, _view.session().get_signals()) {
                    assert(s);
                    if (!s->get_view_rect().contains(event->pos())) {
                        _dso_xm_valid = false;
                        _mm_width = "#####";
                        _mm_period = "#####";
                        _mm_freq = "#####";
                        _mm_duty = "#####";
                        _action_type = NO_ACTION;
                    }
                    break;
                }
            }

            if (_action_type == DSO_YM)
                _dso_ym_end = event->pos().y();
        }
    }

    _mouse_point = event->pos();

    measure();
    update();
}

void Viewport::mouseReleaseEvent(QMouseEvent *event)
{
    assert(event);

    if (_type == TIME_VIEW) {
        if ((_action_type == NO_ACTION) &&
            (event->button() == Qt::LeftButton)) {
            // priority 0
            if (_action_type == NO_ACTION && _view.cursors_shown()) {
                list<Cursor*>::iterator i = _view.get_cursorList().begin();
                double cursorX;
                const double samples_per_pixel = _view.session().cur_samplerate() * _view.scale();
                while (i != _view.get_cursorList().end()) {
                    cursorX = (*i)->index()/samples_per_pixel - (_view.offset() / _view.scale());
                    if ((*i)->grabbed()) {
                        _view.get_ruler()->rel_grabbed_cursor();
                    } else if (qAbs(cursorX - event->pos().x()) <= HitCursorMargin) {
                        _view.get_ruler()->set_grabbed_cursor(*i);
                        _action_type = CURS_MOVE;
                        break;
                    }
                    i++;
                }
            }

            if (_view.session().get_device()->dev_inst()->mode == LOGIC &&
                _view.session().get_capture_state() == SigSession::Stopped) {
                // priority 1
                if (_action_type == NO_ACTION) {
                    const double strength = _drag_strength*DragTimerInterval*1.0/_time.elapsed();
                    if (_time.elapsed() < 200 &&
                        abs(_drag_strength) < MinorDragOffsetUp &&
                        abs(strength) > MinorDragRateUp) {
                        _drag_timer.start(DragTimerInterval);
                        _action_type = LOGIC_MOVE;
                    } else if (_time.elapsed() < 200 &&
                               abs(strength) > DragTimerInterval) {
                        _drag_strength = strength * 5;
                        _drag_timer.start(DragTimerInterval);
                        _action_type = LOGIC_MOVE;
                    }
                }

                // priority 2
                if (_action_type == NO_ACTION) {
                    if (_mouse_down_point.x() == event->pos().x()) {
                        const vector< boost::shared_ptr<Signal> > sigs(_view.session().get_signals());
                        BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs) {
                            assert(s);
                            if (abs(event->pos().y() - s->get_y()) < _view.get_signalHeight()) {
                                _action_type = LOGIC_EDGE;
                                _edge_start = (_view.offset() + (event->pos().x() + 0.5) * _view.scale()) * _view.session().cur_samplerate();
                                break;
                            }
                        }
                    }
                }
            } else if (_view.session().get_device()->dev_inst()->mode == DSO) {
                // priority 0
                if (_action_type == NO_ACTION && _hover_hit) {
                    _action_type = DSO_YM;
                    _dso_ym_valid = true;
                    _dso_ym_sig_index = _hover_sig_index;
                    _dso_ym_sig_value = _hover_sig_value;
                    _dso_ym_index = _hover_index;
                    _dso_ym_start = event->pos().y();
                }

                // priority 1
                if (_action_type == NO_ACTION) {
                    const vector< boost::shared_ptr<Signal> > sigs(_view.session().get_signals());
                    BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs) {
                        assert(s);
                        if (!s->enabled())
                            continue;
                        boost::shared_ptr<DsoSignal> dsoSig;
                        if (dsoSig = dynamic_pointer_cast<DsoSignal>(s)) {
                            if (dsoSig->get_trig_rect(0, _view.get_view_width()).contains(_mouse_point)) {
                                _drag_sig = s;
                                _action_type = DSO_TRIG_MOVE;
                                break;
                            }
                        }
                    }
                }
            }
        } else if (_action_type == DSO_YM) {
            if (event->button() == Qt::LeftButton) {
                _dso_ym_end = event->pos().y();
                _action_type = NO_ACTION;
            } else if (event->button() == Qt::RightButton) {
                _action_type = NO_ACTION;
                _dso_ym_valid = false;
            }
        } else if (_action_type == DSO_TRIG_MOVE) {
            if (event->button() == Qt::LeftButton) {
                _drag_sig.reset();
                _action_type = NO_ACTION;
            }
        } else if (_action_type == DSO_XM_STEP0) {
            if (event->button() == Qt::LeftButton) {
                _action_type = DSO_XM_STEP1;
                _dso_xm_valid = true;
            }
        } else if (_action_type == DSO_XM_STEP1) {
            if (event->button() == Qt::LeftButton) {
                const uint64_t sample_rate = _view.session().cur_samplerate();
                const double scale = _view.scale();
                const double samples_per_pixel =  sample_rate * scale;

                _dso_xm_index[1] = event->pos().x() * samples_per_pixel + _view.offset() * sample_rate;
                const uint64_t max_index = max(_dso_xm_index[0], _dso_xm_index[1]);
                _dso_xm_index[0] = min(_dso_xm_index[0], _dso_xm_index[1]);
                _dso_xm_index[1] = max_index;

                _action_type = DSO_XM_STEP2;
            } else if (event->button() == Qt::RightButton) {
                _action_type = NO_ACTION;
                _dso_xm_valid = false;
                _mm_width = "#####";
                _mm_period = "#####";
                _mm_freq = "#####";
                _mm_duty = "#####";
                measure_updated();
            }
        } else if (_action_type == DSO_XM_STEP2) {
            if (event->button() == Qt::LeftButton) {
                const uint64_t sample_rate = _view.session().cur_samplerate();
                const double scale = _view.scale();
                const double samples_per_pixel =  sample_rate * scale;
                _dso_xm_index[2] = event->pos().x() * samples_per_pixel + _view.offset() * sample_rate;
                uint64_t max_index = max(_dso_xm_index[1], _dso_xm_index[2]);
                _dso_xm_index[1] = min(_dso_xm_index[1], _dso_xm_index[2]);
                _dso_xm_index[2] = max_index;

                max_index = max(_dso_xm_index[0], _dso_xm_index[1]);
                _dso_xm_index[0] = min(_dso_xm_index[0], _dso_xm_index[1]);
                _dso_xm_index[1] = max_index;

                _action_type = NO_ACTION;
            } else if (event->button() == Qt::RightButton) {
                _action_type = NO_ACTION;
                _dso_xm_valid = false;
                _mm_width = "#####";
                _mm_period = "#####";
                _mm_freq = "#####";
                _mm_duty = "#####";
                measure_updated();
            }
        } else if (_action_type == CURS_MOVE) {
            _action_type = NO_ACTION;
            if (_view.cursors_shown()) {
                list<Cursor*>::iterator i = _view.get_cursorList().begin();
                while (i != _view.get_cursorList().end()) {
                    if ((*i)->grabbed()) {
                        _view.get_ruler()->rel_grabbed_cursor();
                    }
                    i++;
                }
            }
        } else if (_action_type == LOGIC_EDGE) {
            _action_type = NO_ACTION;
            _edge_rising = 0;
            _edge_falling = 0;
        } else if (_action_type == LOGIC_MOVE) {
            if (_mouse_down_point == event->pos()) {
                _drag_strength = 0;
                _drag_timer.stop();
                _action_type = NO_ACTION;
            } else {
                const double strength = _drag_strength*DragTimerInterval*1.0/_time.elapsed();
                if (_time.elapsed() < 200 &&
                    abs(_drag_strength) < MinorDragOffsetUp &&
                    abs(strength) > MinorDragRateUp) {
                    _drag_timer.start(DragTimerInterval);
                } else if (_time.elapsed() < 200 &&
                           abs(strength) > DragTimerInterval) {
                    _drag_strength = strength * 5;
                    _drag_timer.start(DragTimerInterval);
                }
            }
        } else if (_action_type == LOGIC_ZOOM) {
            if (event->pos().x() != _mouse_down_point.x()) {
                const double newOffset = _view.offset() + (min(event->pos().x(), _mouse_down_point.x()) + 0.5) * _view.scale();
                const double newScale = max(min(_view.scale() * abs(event->pos().x() - _mouse_down_point.x()) / _view.get_view_width(),
                                                _view.get_maxscale()), _view.get_minscale());
                if (newScale != _view.scale())
                    _view.set_scale_offset(newScale, newOffset);
            }
            _action_type = NO_ACTION;
        }
    }
    update();
}

void Viewport::mouseDoubleClickEvent(QMouseEvent *event)
{
    assert (event);
    (void)event;

    if (_view.session().get_device()->dev_inst()->mode == LOGIC &&
        _view.session().get_capture_state() == SigSession::Stopped) {
        if (event->button() == Qt::RightButton) {
            if (_view.scale() == _view.get_maxscale())
                _view.set_preScale_preOffset();
            else
                _view.set_scale_offset(_view.get_maxscale(), 0);
        } else if (event->button() == Qt::LeftButton) {
            uint64_t index = (_view.offset() + (event->pos().x() + 0.5) * _view.scale()) * _view.session().cur_samplerate();
            _view.add_cursor(view::Ruler::CursorColor[_view.get_cursorList().size() % 8], index);
            _view.show_cursors(true);
        }
        update();
    } else if (_view.session().get_device()->dev_inst()->mode == DSO &&
               _view.session().get_capture_state() != SigSession::Init &&
               event->button() == Qt::LeftButton) {
        if (_dso_xm_valid) {
            _dso_xm_valid = false;
            _action_type = NO_ACTION;
            _mm_width = "#####";
            _mm_period = "#####";
            _mm_freq = "#####";
            _mm_duty = "#####";
            measure_updated();
        } else if (_action_type == NO_ACTION) {
            BOOST_FOREACH(const boost::shared_ptr<Signal> s, _view.session().get_signals()) {
                assert(s);
                if (s->get_view_rect().contains(event->pos())) {
                    uint64_t sample_rate = _view.session().cur_samplerate();
                    double scale = _view.scale();
                    const double samples_per_pixel =  sample_rate * scale;
                    _dso_xm_index[0] = event->pos().x() * samples_per_pixel +
                                            _view.offset() * sample_rate;
                    _dso_xm_y = event->pos().y();
                    _action_type = DSO_XM_STEP0;
                }
                break;
            }
        }
    }
}

void Viewport::wheelEvent(QWheelEvent *event)
{
	assert(event);

    if (_type == FFT_VIEW) {
        BOOST_FOREACH(const boost::shared_ptr<view::MathTrace> t, _view.session().get_math_signals()) {
            assert(t);
            if(t->enabled()) {
                t->zoom(event->delta() / 80, event->x());
                break;
            }
        }
    } else if (_type == TIME_VIEW){
        if (event->orientation() == Qt::Vertical) {
            // Vertical scrolling is interpreted as zooming in/out
            const double offset = event->x();
            _view.zoom(event->delta() / 80, offset);
        } else if (event->orientation() == Qt::Horizontal) {
            // Horizontal scrolling is interpreted as moving left/right
            _view.set_scale_offset(_view.scale(),
                           event->delta() * _view.scale()
                           + _view.offset());
        }
    }

    measure();
}

void Viewport::leaveEvent(QEvent *)
{
    _mouse_point = QPoint(-1, -1);
    //_view.show_cursors(false);

    if (_action_type == LOGIC_EDGE) {
        _edge_rising = 0;
        _edge_falling = 0;
        _action_type = NO_ACTION;
    } else if (_action_type == LOGIC_MOVE) {
        _drag_strength = 0;
        _drag_timer.stop();
        _action_type = NO_ACTION;
    } else if (_action_type == DSO_XM_STEP1 || _action_type == DSO_XM_STEP2) {
        _dso_xm_valid = false;
        _mm_width = "#####";
        _mm_period = "#####";
        _mm_freq = "#####";
        _mm_duty = "#####";
        _action_type = NO_ACTION;
    } else if (_action_type == DSO_YM) {
        _dso_ym_valid = false;
        _action_type = NO_ACTION;
    }

    measure();
    update();
}

void Viewport::on_traces_moved()
{
	update();
}

void Viewport::set_receive_len(quint64 length)
{
    if (length == 0) {
        _total_receive_len = 0;
        start_trigger_timer(333);
    } else {
        stop_trigger_timer();
        if (_total_receive_len + length > _view.session().cur_samplelimits())
            _total_receive_len = _view.session().cur_samplelimits();
        else
            _total_receive_len += length;
    }
    update();
}

void Viewport::clear_measure()
{
    _measure_type = NO_MEASURE;
    update();
}

void Viewport::measure()
{
    if (_view.session().get_data_lock())
        return;
    _measure_type = NO_MEASURE;
    if (_type == TIME_VIEW) {
        const uint64_t sample_rate = _view.session().cur_samplerate();
        const vector< boost::shared_ptr<Signal> > sigs(_view.session().get_signals());
        BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs) {
            assert(s);
            boost::shared_ptr<view::LogicSignal> logicSig;
            boost::shared_ptr<view::DsoSignal> dsoSig;
            if (logicSig = dynamic_pointer_cast<view::LogicSignal>(s)) {
                if (_action_type == NO_ACTION) {
                    if (logicSig->measure(_mouse_point, _cur_sample, _nxt_sample, _thd_sample)) {
                        _measure_type = LOGIC_FREQ;

                        _mm_width = _view.get_ruler()->format_real_time(_nxt_sample - _cur_sample, sample_rate);
                        _mm_period = _thd_sample != 0 ? _view.get_ruler()->format_real_time(_thd_sample - _cur_sample, sample_rate) : "#####";
                        _mm_freq = _thd_sample != 0 ? _view.get_ruler()->format_real_freq(_thd_sample - _cur_sample, sample_rate) : "#####";

                        const double pixels_offset =  _view.offset() / _view.scale();
                        const double samples_per_pixel = sample_rate * _view.scale();
                        _cur_preX = _cur_sample / samples_per_pixel - pixels_offset;
                        _cur_aftX = _nxt_sample / samples_per_pixel - pixels_offset;
                        _cur_thdX = _thd_sample / samples_per_pixel - pixels_offset;
                        _cur_midY = logicSig->get_y();

                        _mm_duty = _thd_sample != 0 ? QString::number((_nxt_sample - _cur_sample) * 100.0 / (_thd_sample - _cur_sample), 'f', 2)+"%" :
                                                     "#####";
                        break;
                    } else {
                        _measure_type = NO_MEASURE;
                        _mm_width = "#####";
                        _mm_period = "#####";
                        _mm_freq = "#####";
                        _mm_duty = "#####";
                    }
                } else if (_action_type == LOGIC_EDGE) {
                    if (logicSig->edges(_view.hover_point(), _edge_start, _edge_rising, _edge_falling)) {
                        const double pixels_offset =  _view.offset() / _view.scale();
                        const double samples_per_pixel = sample_rate * _view.scale();
                        _cur_preX = _edge_start / samples_per_pixel - pixels_offset;
                        _cur_aftX = _view.hover_point().x();
                        _cur_midY = logicSig->get_y() - logicSig->get_totalHeight()/2 - 5;

                        _em_rising = tr("Rising: ") + QString::number(_edge_rising);
                        _em_falling = tr("Falling: ") + QString::number(_edge_falling);
                        _em_edges = tr("Edges: ") + QString::number(_edge_rising + _edge_falling);

                        break;
                    }
                }
            } else if (dsoSig = dynamic_pointer_cast<view::DsoSignal>(s)) {
                if (_measure_en && dsoSig->measure(_view.hover_point())) {
                    _measure_type = DSO_VALUE;
                    break;
                } else {
                    _measure_type = NO_MEASURE;
                }
            }
        }
    } else if (_type == FFT_VIEW) {
        BOOST_FOREACH(const boost::shared_ptr<view::MathTrace> t, _view.session().get_math_signals()) {
            assert(t);
            if(t->enabled()) {
                t->measure(_mouse_point);
            }
        }
    }

    measure_updated();
}

void Viewport::paintMeasure(QPainter &p)
{
    _hover_hit = false;
    if (_action_type == NO_ACTION &&
        _measure_type == LOGIC_FREQ) {
        p.setPen(QColor(17, 133, 209,  255));
        p.drawLine(QLineF(_cur_preX, _cur_midY, _cur_aftX, _cur_midY));
        p.drawLine(QLineF(_cur_preX, _cur_midY, _cur_preX + 2, _cur_midY - 2));
        p.drawLine(QLineF(_cur_preX, _cur_midY, _cur_preX + 2, _cur_midY + 2));
        p.drawLine(QLineF(_cur_aftX - 2, _cur_midY - 2, _cur_aftX, _cur_midY));
        p.drawLine(QLineF(_cur_aftX - 2, _cur_midY + 2, _cur_aftX, _cur_midY));
        if (_thd_sample != 0) {
            p.drawLine(QLineF(_cur_aftX, _cur_midY, _cur_thdX, _cur_midY));
            p.drawLine(QLineF(_cur_aftX, _cur_midY, _cur_aftX + 2, _cur_midY - 2));
            p.drawLine(QLineF(_cur_aftX, _cur_midY, _cur_aftX + 2, _cur_midY + 2));
            p.drawLine(QLineF(_cur_thdX - 2, _cur_midY - 2, _cur_thdX, _cur_midY));
            p.drawLine(QLineF(_cur_thdX - 2, _cur_midY + 2, _cur_thdX, _cur_midY));
        }

        if (_measure_en) {
            int typical_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
                Qt::AlignLeft | Qt::AlignTop, _mm_width).width();
            typical_width = max(typical_width, p.boundingRect(0, 0, INT_MAX, INT_MAX,
                Qt::AlignLeft | Qt::AlignTop, _mm_period).width());
            typical_width = max(typical_width, p.boundingRect(0, 0, INT_MAX, INT_MAX,
                Qt::AlignLeft | Qt::AlignTop, _mm_freq).width());
            typical_width = max(typical_width, p.boundingRect(0, 0, INT_MAX, INT_MAX,
                Qt::AlignLeft | Qt::AlignTop, _mm_duty).width());
            typical_width = typical_width + 100;

            const double width = _view.get_view_width();
            const double height = _view.viewport()->height();
            const double left = _view.hover_point().x();
            const double top = _view.hover_point().y();
            const double right = left + typical_width;
            const double bottom = top + 80;
            QPointF org_pos = QPointF(right > width ? left - typical_width : left, bottom > height ? top - 80 : top);
            QRectF measure_rect = QRectF(org_pos.x(), org_pos.y(), (double)typical_width, 80.0);
            QRectF measure1_rect = QRectF(org_pos.x(), org_pos.y(), (double)typical_width, 20.0);
            QRectF measure2_rect = QRectF(org_pos.x(), org_pos.y()+20, (double)typical_width, 20.0);
            QRectF measure3_rect = QRectF(org_pos.x(), org_pos.y()+40, (double)typical_width, 20.0);
            QRectF measure4_rect = QRectF(org_pos.x(), org_pos.y()+60, (double)typical_width, 20.0);

            p.setPen(Qt::NoPen);
            p.setBrush(QColor(17, 133, 209,  150));
            p.drawRect(measure_rect);

            p.setPen(Qt::black);
            p.drawText(measure1_rect, Qt::AlignRight | Qt::AlignVCenter,
                       tr("Width: ") + _mm_width);
            p.drawText(measure2_rect, Qt::AlignRight | Qt::AlignVCenter,
                       tr("Period: ") + _mm_period);
            p.drawText(measure3_rect, Qt::AlignRight | Qt::AlignVCenter,
                       tr("Frequency: ") + _mm_freq);
            p.drawText(measure4_rect, Qt::AlignRight | Qt::AlignVCenter,
                       tr("Duty Cycle: ") + _mm_duty);
        }
    } 

    const vector< boost::shared_ptr<Signal> > sigs(_view.session().get_signals());
    if (_action_type == NO_ACTION &&
        _measure_type == DSO_VALUE) {
        BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs) {
            boost::shared_ptr<view::DsoSignal> dsoSig;
            if (dsoSig = dynamic_pointer_cast<view::DsoSignal>(s)) {
                uint64_t index;
                double value;
                QPointF hpoint;
                const int arrow_size = 5;
                const int mark_radius = 10;
                const int mark_width = 20;
                const int mark_cursor_height = 30;
                if (dsoSig->get_hover(index, hpoint, value)) {
                    p.setPen(dsoSig->get_colour());
                    const QRectF hpoint_rect = QRectF(hpoint.x()-mark_radius/2, hpoint.y()-mark_radius/2, mark_radius, mark_radius);
                    if (hpoint_rect.contains(_view.hover_point())) {
                        p.setBrush(dsoSig->get_colour());
                        const int cursor_up = hpoint.y()-mark_cursor_height;
                        const int cursor_dn = hpoint.y()+mark_cursor_height;
                        const int cursor_lf = hpoint.x()-arrow_size;
                        const int cursor_md = hpoint.x();
                        const int cursor_rt = hpoint.x()+arrow_size;

                        const QPointF up_arrow[3] = {
                            QPointF(cursor_lf, cursor_up+arrow_size),
                            QPointF(cursor_md, cursor_up),
                            QPointF(cursor_rt, cursor_up+arrow_size),
                        };
                        const QPointF dn_arrow[3] = {
                            QPointF(cursor_lf, cursor_dn-arrow_size),
                            QPointF(cursor_md, cursor_dn),
                            QPointF(cursor_rt, cursor_dn-arrow_size),
                        };
                        p.drawPolyline(up_arrow, 3);
                        p.drawPolyline(dn_arrow, 3);
                        p.drawLine(cursor_md, cursor_up, cursor_md, cursor_dn);
                        _hover_hit = true;
                        _hover_sig_index = dsoSig->get_index();
                        _hover_sig_value = value;
                        _hover_index = index;
                    } else {
                        p.setBrush(Qt::NoBrush);
                    }
                    p.drawEllipse(hpoint, mark_radius, mark_radius);
                    QString value_c = abs(value) > 1000 ? QString::number(value/1000.0, 'f', 2) + "V" : QString::number(value, 'f', 2) + "mV";
                    int value_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
                                                       Qt::AlignLeft | Qt::AlignTop, value_c).width();
                    const bool right = dsoSig->get_index()%2 ? hpoint.x() < value_width : hpoint.x() < _view.get_view_width() - value_width;
                    const bool up = hpoint.y() > 50;
                    const QPointF hpoint_sec = QPointF(hpoint.x() - (right ? -mark_width : mark_width), hpoint.y() - (up ? mark_width : -mark_width));
                    p.drawLine(hpoint, hpoint_sec);
                    p.drawLine(hpoint_sec, QPointF(hpoint_sec.x() + (right ? value_width : -value_width), hpoint_sec.y()));
                    p.drawText(QRectF(right ? hpoint_sec.x() : hpoint_sec.x() - value_width, hpoint_sec.y() - mark_width, value_width, mark_width),
                               Qt::AlignLeft | Qt::AlignBottom,
                               value_c);
                }
            }
        }
    }

    if (_dso_ym_valid) {
        BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs) {
            boost::shared_ptr<view::DsoSignal> dsoSig;
            if (dsoSig = dynamic_pointer_cast<view::DsoSignal>(s)) {
                if (dsoSig->get_index() == _dso_ym_sig_index) {
                    p.setPen(QPen(dsoSig->get_colour(), 1, Qt::DotLine));
                    const int text_height = p.boundingRect(0, 0, INT_MAX, INT_MAX,
                        Qt::AlignLeft | Qt::AlignTop, "W").height();
                    const uint64_t sample_rate = _view.session().cur_samplerate();
                    const double x = (_dso_ym_index / (sample_rate * _view.scale())) -
                        _view.offset() /_view.scale();
                    p.drawLine(x-10, _dso_ym_start,
                               x+10, _dso_ym_start);
                    p.drawLine(x, _dso_ym_start,
                               x, _dso_ym_end);
                    p.drawLine(0, _dso_ym_end,
                               _view.get_view_width(), _dso_ym_end);

                    // -- vertical delta value
                    double hrate = (_dso_ym_start - _dso_ym_end) * 1.0f / _view.get_view_height();
                    double value = hrate  * dsoSig->get_vDialValue() * dsoSig->get_factor() * DS_CONF_DSO_VDIVS;
                    QString value_str = abs(value) > 1000 ? QString::number(value/1000.0, 'f', 2) + "V" : QString::number(value, 'f', 2) + "mV";
                    int value_rect_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
                                                      Qt::AlignLeft | Qt::AlignVCenter, value_str).width();
                    p.drawText(QRect(x+10, abs(_dso_ym_start+_dso_ym_end)/2, value_rect_width, text_height),
                               value_str);

                    // -- start value
                    value_str = abs(_dso_ym_sig_value) > 1000 ? QString::number(_dso_ym_sig_value/1000.0, 'f', 2) + "V" : QString::number(_dso_ym_sig_value, 'f', 2) + "mV";
                    value_rect_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
                                                      Qt::AlignLeft | Qt::AlignVCenter, value_str).width();
                    int str_y = value > 0 ? _dso_ym_start : _dso_ym_start - text_height;
                    p.drawText(QRect(x-0.5*value_rect_width, str_y, value_rect_width, text_height),
                               value_str);

                    // -- end value
                    double end_value = _dso_ym_sig_value + value;
                    value_str = abs(end_value) > 1000 ? QString::number(end_value/1000.0, 'f', 2) + "V" : QString::number(end_value, 'f', 2) + "mV";
                    value_rect_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
                                                      Qt::AlignLeft | Qt::AlignVCenter, value_str).width();
                    str_y = value > 0 ? _dso_ym_end-text_height : _dso_ym_end;
                    p.drawText(QRect(x-0.5*value_rect_width, str_y, value_rect_width, text_height),
                               value_str);
                    break;
                }
            }
        }
    }

    if (_dso_xm_valid) {
        p.setPen(QPen(Qt::red, 1, Qt::DotLine));
        int measure_line_count = 6;
        const int text_height = p.boundingRect(0, 0, INT_MAX, INT_MAX,
            Qt::AlignLeft | Qt::AlignTop, "W").height();
        const uint64_t sample_rate = _view.session().cur_samplerate();
        QLineF *line;
        QLineF *const measure_lines = new QLineF[measure_line_count];
        line = measure_lines;
        double x[DsoMeasureStages];
        int dso_xm_stage = 0;
        if (_action_type == DSO_XM_STEP1)
            dso_xm_stage = 1;
        else if(_action_type == DSO_XM_STEP2)
            dso_xm_stage = 2;
        else
            dso_xm_stage = 3;

        for (int i = 0; i < dso_xm_stage; i++) {
            x[i] = (_dso_xm_index[i] / (sample_rate * _view.scale())) -
                _view.offset() /_view.scale();
        }
        measure_line_count = 0;
        if (dso_xm_stage > 0) {
            *line++ = QLineF(x[0], _dso_xm_y - 10,
                           x[0], _dso_xm_y + 10);
            measure_line_count += 1;
        }
        if (dso_xm_stage > 1) {
            *line++ = QLineF(x[1], _dso_xm_y - 10,
                           x[1], _dso_xm_y + 10);
            *line++ = QLineF(x[0], _dso_xm_y,
                           x[1], _dso_xm_y);
            _mm_width = _view.get_ruler()->format_real_time(_dso_xm_index[1] - _dso_xm_index[0], sample_rate);

            // -- width show
            const QString w_ctr = "W="+_mm_width;
            int w_rect_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
                                              Qt::AlignLeft | Qt::AlignVCenter, w_ctr).width();
            p.drawText(QRect(x[0]+10, _dso_xm_y - text_height, w_rect_width, text_height), w_ctr);
            measure_line_count += 2;
        }
        if (dso_xm_stage > 2) {
            *line++ = QLineF(x[0], _dso_xm_y + 20,
                           x[0], _dso_xm_y + 40);
            *line++ = QLineF(x[0], _dso_xm_y + 30,
                           x[2], _dso_xm_y + 30);
            *line++ = QLineF(x[2], _dso_xm_y + 20,
                           x[2], _dso_xm_y + 40);
            _mm_period = _view.get_ruler()->format_real_time(_dso_xm_index[2] - _dso_xm_index[0], sample_rate);
            _mm_freq = _view.get_ruler()->format_real_freq(_dso_xm_index[2] - _dso_xm_index[0], sample_rate);
            _mm_duty = QString::number((_dso_xm_index[1] - _dso_xm_index[0]) * 100.0 / (_dso_xm_index[2] - _dso_xm_index[0]), 'f', 2)+"%";

            // -- period show
            const QString p_ctr = "P="+_mm_period;
            int p_rect_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
                                              Qt::AlignLeft | Qt::AlignVCenter, p_ctr).width();
            p.drawText(QRect(x[0]+10, _dso_xm_y + 30 - text_height, p_rect_width, text_height), p_ctr);

            // -- frequency show
            const QString f_ctr = "F="+_mm_freq;
            int f_rect_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
                                              Qt::AlignLeft | Qt::AlignVCenter, f_ctr).width();
            p.drawText(QRect(x[0]+20 + p_rect_width, _dso_xm_y + 30 - text_height, f_rect_width, text_height), f_ctr);

            // -- duty show
            const QString d_ctr = "D="+_mm_duty;
            int d_rect_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
                                              Qt::AlignLeft | Qt::AlignVCenter, d_ctr).width();
            p.drawText(QRect(x[1]+10, _dso_xm_y - 0.5*text_height, d_rect_width, text_height), d_ctr);

            measure_line_count += 3;
        }
        p.drawLines(measure_lines, measure_line_count);
        if (dso_xm_stage < DsoMeasureStages) {
            p.drawLine(x[dso_xm_stage-1], _dso_xm_y,
                       _mouse_point.x(), _dso_xm_y);
            p.drawLine(_mouse_point.x(), 0,
                       _mouse_point.x(), height());
        }
        measure_updated();
    }

    if (_action_type == LOGIC_EDGE) {
        p.setPen(QColor(17, 133, 209,  255));

        p.drawLine(QLineF(_cur_preX, _cur_midY-5, _cur_preX, _cur_midY+5));
        p.drawLine(QLineF(_cur_aftX, _cur_midY-5, _cur_aftX, _cur_midY+5));
        p.drawLine(QLineF(_cur_preX, _cur_midY, _cur_aftX, _cur_midY));

        int typical_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
            Qt::AlignLeft | Qt::AlignTop, _em_edges).width();
        typical_width = max(typical_width, p.boundingRect(0, 0, INT_MAX, INT_MAX,
            Qt::AlignLeft | Qt::AlignTop, _em_rising).width());
        typical_width = max(typical_width, p.boundingRect(0, 0, INT_MAX, INT_MAX,
            Qt::AlignLeft | Qt::AlignTop, _em_falling).width());

        typical_width = typical_width + 30;

        const double width = _view.get_view_width();
        const double height = _view.viewport()->height();
        const double left = _view.hover_point().x();
        const double top = _view.hover_point().y();
        const double right = left + typical_width;
        const double bottom = top + 60;
        QPointF org_pos = QPointF(right > width ? left - typical_width : left, bottom > height ? top - 80 : top);
        QRectF measure_rect = QRectF(org_pos.x(), org_pos.y(), (double)typical_width, 60.0);
        QRectF measure1_rect = QRectF(org_pos.x(), org_pos.y(), (double)typical_width, 20.0);
        QRectF measure2_rect = QRectF(org_pos.x(), org_pos.y()+20, (double)typical_width, 20.0);
        QRectF measure3_rect = QRectF(org_pos.x(), org_pos.y()+40, (double)typical_width, 20.0);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(17, 133, 209,  150));
        p.drawRect(measure_rect);

        p.setPen(Qt::black);
        p.drawText(measure1_rect, Qt::AlignRight | Qt::AlignVCenter, _em_edges);
        p.drawText(measure2_rect, Qt::AlignRight | Qt::AlignVCenter, _em_rising);
        p.drawText(measure3_rect, Qt::AlignRight | Qt::AlignVCenter, _em_falling);

    }
}

QString Viewport::get_measure(QString option)
{
    if(option.compare("width") == 0)
        return _mm_width;
    else if (option.compare("period") == 0)
        return _mm_period;
    else if (option.compare("frequency") == 0)
        return _mm_freq;
    else if (option.compare("duty") == 0)
        return _mm_duty;
    else
        return "#####";
}

void Viewport::set_measure_en(int enable)
{
    if (enable == 0)
        _measure_en = false;
    else
        _measure_en = true;
}

void Viewport::start_trigger_timer(int msec)
{
    assert(msec > 0);
    triggered = false;
    timer_cnt = 0;
    trigger_timer.start(msec);
}

void Viewport::stop_trigger_timer()
{
    triggered = true;
    timer_cnt = 0;
    trigger_timer.stop();
}

void Viewport::on_trigger_timer()
{
    timer_cnt++;
    update();
}

void Viewport::on_drag_timer()
{
    const double offset = _view.offset();
    const double scale = _view.scale();
    if (_view.session().get_capture_state() == SigSession::Stopped &&
        _drag_strength != 0 &&
        offset < _view.get_max_offset() &&
        offset > _view.get_min_offset()) {
        _view.set_scale_offset(scale, offset + _drag_strength * scale);
        _drag_strength /= DragDamping;
        if (_drag_strength != 0)
            _drag_timer.start(DragTimerInterval);
    } else if (offset == _view.get_max_offset() ||
               offset == _view.get_min_offset()) {
        _drag_strength = 0;
        _drag_timer.stop();
        _action_type = NO_ACTION;
    }else if (_action_type == NO_ACTION){
        _drag_strength = 0;
        _drag_timer.stop();
    }
}

void Viewport::set_need_update(bool update)
{
    _need_update = update;
}

void Viewport::show_wait_trigger()
{
    _waiting_trig %= (WaitLoopTime / SigSession::ViewTime) * 4;
    _waiting_trig++;
    update();
}

void Viewport::unshow_wait_trigger()
{
    _waiting_trig = 0;
    update();
}


} // namespace view
} // namespace pv
