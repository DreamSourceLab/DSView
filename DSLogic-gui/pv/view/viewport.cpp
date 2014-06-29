/*
 * This file is part of the DSLogic-gui project.
 * DSLogic-gui is based on PulseView.
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


#include "view.h"
#include "viewport.h"
#include "ruler.h"

#include "signal.h"
#include "../data/logic.h"
#include "../data/logicsnapshot.h"
#include "../sigsession.h"

#include <QMouseEvent>
#include <QStyleOption>

#include <boost/foreach.hpp>

using namespace boost;
using namespace std;

namespace pv {
namespace view {

const int Viewport::HitCursorMargin = 3;
const int Viewport::NumSpanY = 5;
const int Viewport::NumMiniSpanY = 5;
const int Viewport::NumSpanX = 10;

Viewport::Viewport(View &parent) :
	QWidget(&parent),
        _view(parent),
    _total_receive_len(0),
    _zoom_rect_visible(false),
    _measure_shown(false),
    _cur_sample(0),
    _nxt_sample(1),
    _cur_preX(0),
    _cur_aftX(1),
    _cur_midY(0)
{
	setMouseTracking(true);
	setAutoFillBackground(true);
	setBackgroundRole(QPalette::Base);

    //setFixedSize(QSize(600, 400));
    _mm_width = "#####";
    _mm_period = "#####";
    _mm_freq = "#####";
    _measure_en = true;
    triggered = false;
    timer_cnt = 0;

	connect(&_view, SIGNAL(signals_moved()),
		this, SLOT(on_signals_moved()));
    connect(&trigger_timer, SIGNAL(timeout()),
            this, SLOT(on_trigger_timer()));
}

int Viewport::get_total_height() const
{
	int h = 0;
	const vector< boost::shared_ptr<Signal> > sigs(
		_view.session().get_signals());
	BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs) {
		assert(s);
        //h = max(s->get_v_offset() + _view.get_signalHeight(), h);
        h = max(s->get_v_offset(), h);
	}

	return h;
}

void Viewport::paintEvent(QPaintEvent *event)
{
    (void)event;

    using pv::view::Signal;
    int i, j;
    QStyleOption o;
    o.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &o, &p, this);

    //QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (_view.session().get_device()->mode == LOGIC) {
        switch(_view.session().get_capture_state()) {
        case SigSession::Init:
            break;

        case SigSession::Stopped:
            paintSignals(p);
            break;

        case SigSession::Running:
            //p.setRenderHint(QPainter::Antialiasing);
            paintProgress(p);
            break;
        }
    } else {
        paintSignals(p);
    }

    p.setRenderHint(QPainter::Antialiasing, false);
    if (_view.get_signalHeight() != _curSignalHeight)
            _curSignalHeight = _view.get_signalHeight();
    const vector< boost::shared_ptr<Signal> > sigs(
        _view.session().get_signals());

    BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs) {
        assert(s);
        //paint_axis(p, y, left, right);
        p.setPen(Signal::dsGray);
        const double sigY = s->get_v_offset() - _view.v_offset();

        if (s->get_type() == Signal::DS_ANALOG) {
            p.drawLine(0, sigY, width(), sigY);
            const double spanY = (s->get_signalHeight()) * 1.0f / NumSpanY;
            for (i = 1; i <= NumSpanY; i++) {
                const double posY = sigY - spanY * i;
                p.drawLine(0, posY, width(), posY);
                const double miniSpanY = spanY / NumMiniSpanY;
                for (j = 1; j < NumMiniSpanY; j++) {
                    p.drawLine(width() / 2.0f - 10, posY + miniSpanY * j,
                               width() / 2.0f + 10, posY + miniSpanY * j);
                }
            }
            const double spanX = width() * 1.0f / NumSpanX;
            for (i = 1; i < NumSpanX; i++) {
                p.drawLine(0 + spanX * i, sigY,
                           0 + spanX * i, sigY - s->get_signalHeight());
            }
        } else if (s->get_type() == Signal::DS_LOGIC) {
            p.drawLine(0, sigY + 10, width(), sigY + 10);
        }
    }

    if (_view.session().get_device()->mode == DSO) {

        p.setPen(Signal::dsGray);
        p.setPen(Qt::DotLine);

        const double spanY =height() * 1.0f / 10;
        for (i = 1; i < 11; i++) {
            const double posY = spanY * i;
            p.drawLine(0, posY, width(), posY);
            const double miniSpanY = spanY / 5;
            for (j = 1; j < 5; j++) {
                p.drawLine(width() / 2.0f - 10, posY - miniSpanY * j,
                           width() / 2.0f + 10, posY - miniSpanY * j);
            }
        }
        const double spanX = width() * 1.0f / 10;
        for (i = 1; i < 11; i++) {
            const double posX = spanX * i;
            p.drawLine(posX, 0,
                       posX, height());
            const double miniSpanX = spanX / 5;
            for (j = 1; j < 5; j++) {
                p.drawLine(posX - miniSpanX * j, height() / 2.0f - 10,
                           posX - miniSpanX * j, height() / 2.0f + 10);
            }
        }
    }

	p.end();
}

void Viewport::paintSignals(QPainter &p)
{
    const vector< boost::shared_ptr<Signal> > sigs(
        _view.session().get_signals());
//    const vector< boost::shared_ptr<Signal> > pro_sigs(
//        _view.session().get_pro_signals());
    // Plot the signal
    const int v_offset = _view.v_offset();
    if (_view.scale() != _curScale ||
        _view.offset() != _curOffset ||
        _view.get_signalHeight() != _curSignalHeight ||
        _view.need_update()) {
        _curScale = _view.scale();
        _curOffset = _view.offset();
        _curSignalHeight = _view.get_signalHeight();

        pixmap = QPixmap(size());
        pixmap.fill(Qt::transparent);
        QPainter dbp(&pixmap);
        dbp.initFrom(this);
        p.setRenderHint(QPainter::Antialiasing, false);
        BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs) {
            assert(s);
            if (s->get_active())
                s->paint(dbp, ((s->get_type() == Signal::DS_DSO) ? s->get_zeroPos() + height()*0.5 : s->get_v_offset() - v_offset), 0, width(),
                    _view.scale(), _view.offset());
        }
//        p.setRenderHint(QPainter::Antialiasing);
//        BOOST_FOREACH(const boost::shared_ptr<Signal> s, pro_sigs) {
//            assert(s);
//            s->paint(dbp, s->get_v_offset() - v_offset, 0, width(),
//                _view.scale(), _view.offset());
//        }
        _view.set_need_update(false);
    }
    p.drawPixmap(0, 0, pixmap);

    // plot trig line in DSO mode
    BOOST_FOREACH(const shared_ptr<Signal> s, sigs) {
        assert(s);
        if (s->get_active() && s->get_type() == Signal::DS_DSO)
            s->paint_trig(p, 0, width(), qAbs(_mouse_point.y() - s->get_trig_vpos()) <= HitCursorMargin );
    }

    // plot cursors
    if (_view.cursors_shown()) {
        list<Cursor*>::iterator i = _view.get_cursorList().begin();
        double cursorX;
        while (i != _view.get_cursorList().end()) {
            cursorX = ((*i)->time() - _view.offset()) / _view.scale();
            if (rect().contains(_view.hover_point().x(), _view.hover_point().y()) &&
                    qAbs(cursorX - _view.hover_point().x()) <= HitCursorMargin)
                (*i)->paint(p, rect(), 1);
            else
                (*i)->paint(p, rect(), 0);
            if (!_view.session().get_data()->get_snapshots().empty()) {
                uint64_t _hit_sample = floor((*i)->time() * _view.session().get_last_sample_rate());
                if (_hit_sample > _total_receive_len) {
                    (*i)->set_time(0);
                } else {
                    QRectF valueRect = QRectF(cursorX + 3, height()-20, 100, 20);
                    p.setPen(Qt::black);
                    p.drawLine(cursorX, height()-13, cursorX + 3, height()-10);
                    p.drawLine(cursorX, height()-7, cursorX + 3, height()-10);
                    p.drawText(valueRect, Qt::AlignLeft | Qt::AlignVCenter,
                               "Value: " +
                               QString::number((uint16_t)_view.session().get_data()->get_snapshots().front()->get_sample(_hit_sample)));
                }
            }

            i++;
        }
    }
    if (_view.trig_cursor_shown()) {
        _view.get_trig_cursor()->paint(p, rect(), 0);
    }
    if (_view.search_cursor_shown()) {
        _view.get_search_cursor()->paint(p, rect(), 0);
    }

    // plot zoom rect
    if (_zoom_rect_visible) {
        p.setPen(Qt::NoPen);
        p.setBrush(Signal::dsLightBlue);
        p.drawRect(_zoom_rect);
    }

    //plot measure arrow
    if (_measure_shown) {
        paintMeasure(p);
    }
}

void Viewport::paintProgress(QPainter &p)
{
    using pv::view::Signal;

    const quint64 _total_sample_len = _view.session().get_total_sample_len();
    double progress = -(_total_receive_len * 1.0f / _total_sample_len * 360 * 16);

    p.setPen(Qt::gray);
    const QPoint cenPos = QPoint(width() / 2, height() / 2);
    const int radius = min(0.3 * width(), 0.3 * height());
    p.drawEllipse(cenPos, radius - 2, radius - 2);
    p.setPen(QPen(Signal::dsGreen, 4, Qt::SolidLine));
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

        const QPoint cenLeftPos = QPoint(width() / 2 - 0.05 * width(), height() / 2);
        const QPoint cenRightPos = QPoint(width() / 2 + 0.05 * width(), height() / 2);
        const int trigger_radius = min(0.02 * width(), 0.02 * height());

        p.setPen(Qt::NoPen);
        p.setBrush((timer_cnt % 3) == 0 ? Signal::dsLightBlue : Signal::dsGray);
        p.drawEllipse(cenLeftPos, trigger_radius, trigger_radius);
        p.setBrush((timer_cnt % 3) == 1 ? Signal::dsLightBlue : Signal::dsGray);
        p.drawEllipse(cenPos, trigger_radius, trigger_radius);
        p.setBrush((timer_cnt % 3) == 2 ? Signal::dsLightBlue : Signal::dsGray);
        p.drawEllipse(cenRightPos, trigger_radius, trigger_radius);
    } else {
        const int progress100 = ceil(progress / -3.6 / 16);
        p.setPen(QColor(0, 0, 0, 50));
        QFont font=p.font();
        font.setPointSize(50);
        font.setBold(true);
        p.setFont(font);
        p.drawText(rect(), Qt::AlignCenter | Qt::AlignVCenter, QString::number(progress100)+"%");
    }
}

void Viewport::mousePressEvent(QMouseEvent *event)
{
	assert(event);

	_mouse_down_point = event->pos();
	_mouse_down_offset = _view.offset();

    if (event->buttons() & Qt::LeftButton) {
        if (_view.cursors_shown()) {
            list<Cursor*>::iterator i = _view.get_cursorList().begin();
            double cursorX;
            while (i != _view.get_cursorList().end()) {
                cursorX = ((*i)->time() - _view.offset()) / _view.scale();
                if ((*i)->grabbed())
                    _view.get_ruler()->rel_grabbed_cursor();
                else if (qAbs(cursorX - event->pos().x()) <= HitCursorMargin) {
                    _view.get_ruler()->set_grabbed_cursor(*i);
                    break;
                }
                i++;
            }

        }
//        if (!_view.get_ruler()->get_grabbed_cursor()) {
//            _zoom_rect_visible = true;
//        }

        const vector< shared_ptr<Signal> > sigs(_view.session().get_signals());
        BOOST_FOREACH(const shared_ptr<Signal> s, sigs) {
            assert(s);
            if (s->get_active() &&
                s->get_type() == Signal::DS_DSO &&
                qAbs(_mouse_point.y() - s->get_trig_vpos()) <= HitCursorMargin) {
                if (_drag_sig)
                    _drag_sig.reset();
                else
                    _drag_sig = s;
                break;
            }
        }
        update();
    }
}

void Viewport::mouseMoveEvent(QMouseEvent *event)
{
	assert(event);
    _mouse_point = event->pos();
    if (event->buttons() & Qt::RightButton) {
        _zoom_rect = QRectF(_mouse_down_point, event->pos());
        _zoom_rect_visible = true;
	}

    if (event->buttons() & Qt::LeftButton) {
        _view.set_scale_offset(_view.scale(),
            _mouse_down_offset +
            (_mouse_down_point - event->pos()).x() *
            _view.scale());
        measure();
    }

    if (!(event->buttons() || Qt::NoButton)) {
        if (_drag_sig) {
            uint16_t trig_value = 0;
            int vpos = _mouse_point.y();
            if (vpos < 0)
                vpos = 0;
            else if (vpos > height())
                vpos = height();
            _drag_sig->set_trig_vpos(vpos);

            const vector< shared_ptr<Signal> > sigs(_view.session().get_signals());
            BOOST_FOREACH(const shared_ptr<Signal> s, sigs) {
                assert(s);
                if (s->get_active() &&
                    s->get_type() == Signal::DS_DSO) {
                    trig_value += (((uint16_t)(255 - s->get_trig_vpos()*1.0/height()*255)) << 8*s->get_index());
                }
            }
            sr_config_set(_view.session().get_device(),
                          SR_CONF_TRIGGER_VALUE, g_variant_new_uint16(trig_value));
        }

        TimeMarker* grabbed_marker = _view.get_ruler()->get_grabbed_cursor();
        if (_view.cursors_shown() && grabbed_marker) {
            grabbed_marker->set_time(_view.offset() + _view.hover_point().x() * _view.scale());
        }
        measure();
    }

    update();
}

void Viewport::mouseReleaseEvent(QMouseEvent *event)
{
    assert(event);

    if (_zoom_rect_visible) {
        _zoom_rect_visible = false;
        const double newOffset = _view.offset() + (min(event->pos().x(), _mouse_down_point.x()) + 0.5) * _view.scale();
        const double newScale = max(min(_view.scale() * (event->pos().x() - _mouse_down_point.x()) / width(),
                                        _view.get_maxscale()), _view.get_minscale());
        if (newScale != _view.scale())
            _view.set_scale_offset(newScale, newOffset);
    }

    update();
}

void Viewport::mouseDoubleClickEvent(QMouseEvent *event)
{
    assert (event);
    if (_view.scale() == _view.get_maxscale())
        _view.set_preScale_preOffset();
    else
        _view.set_scale_offset(_view.get_maxscale(), 0);

    update();
}

void Viewport::wheelEvent(QWheelEvent *event)
{
	assert(event);

	if (event->orientation() == Qt::Vertical) {
		// Vertical scrolling is interpreted as zooming in/out
        _view.zoom(event->delta() / 80, event->x());
	} else if (event->orientation() == Qt::Horizontal) {
		// Horizontal scrolling is interpreted as moving left/right
		_view.set_scale_offset(_view.scale(),
				       event->delta() * _view.scale()
				       + _view.offset());
	}

    measure();
}

void Viewport::leaveEvent(QEvent *)
{
    _measure_shown = false;
    _mouse_point = QPoint(-1, -1);
    //_view.show_cursors(false);
    update();
}

void Viewport::on_signals_moved()
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
        if (_total_receive_len + length > _view.session().get_total_sample_len())
            _total_receive_len = _view.session().get_total_sample_len();
        else
            _total_receive_len += length;
    }
    update();
}

void Viewport::measure()
{
    const vector< boost::shared_ptr<Signal> > sigs(_view.session().get_signals());
    BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs) {
        assert(s);
        const int curY = _view.hover_point().y();
        const double curX = _view.hover_point().x();
        if (curY <= View::SignalMargin || s->get_type() != Signal::DS_LOGIC) {
            _measure_shown = false;
            break;
        } else if ( curY < s->get_v_offset() &&
                    curY > (s->get_v_offset() - _view.get_signalHeight())) {
            if (s->cur_edges().size() > 2) {
                const double pixels_offset = _view.offset() / _view.scale();
                const double samples_per_pixel = _view.session().get_last_sample_rate() * _view.scale();

                uint64_t findIndex = curX / width() * s->cur_edges().size();
                uint64_t left_findIndex = 0;
                uint64_t right_findIndex = s->cur_edges().size() - 1;
                int times = 0;
                while(!s->cur_edges().empty() && times < 20) {
                    findIndex = min(findIndex, (uint64_t)(s->cur_edges().size() - 2));
                    const double pre_edge_x =
                            s->cur_edges().at(findIndex).first / samples_per_pixel - pixels_offset;
                    const double aft_edge_x =
                            s->cur_edges().at(findIndex + 1).first / samples_per_pixel - pixels_offset;
                    if ( curX >= pre_edge_x && curX <= aft_edge_x) {
                        if (aft_edge_x - pre_edge_x < 2 ||
                                findIndex == 0 ||
                                findIndex == s->cur_edges().size() - 2) {
                            _measure_shown = false;
                            break;
                        } else {
                            _measure_shown = true;
                            _cur_sample = s->cur_edges().at(findIndex).first;
                            _nxt_sample = s->cur_edges().at(findIndex + 1).first;
                            _cur_preX = pre_edge_x;
                            _cur_aftX = aft_edge_x;
                            //if (findIndex >= 0 && findIndex <= s->cur_edges().size() - 4) {
                            if(findIndex <= s->cur_edges().size() - 4) {
                                _thd_sample = s->cur_edges().at(findIndex + 2).first;
                                _cur_thdX =
                                        s->cur_edges().at(findIndex + 2).first / samples_per_pixel - pixels_offset;
                            } else {
                                _thd_sample = 0;
                                _cur_thdX = 0;

                            }
                            _cur_midY = s->get_v_offset() - 0.5 * _view.get_signalHeight();
                            break;
                        }
                    } else if (curX < pre_edge_x) {
                        right_findIndex = findIndex;
                        findIndex = (left_findIndex + findIndex) / 2;
                    } else if (curX > aft_edge_x) {
                        left_findIndex = findIndex;
                        findIndex = (right_findIndex + findIndex) / 2;
                    }
                    times++;
                }
            }
            break;
        } else if (curY >= s->get_v_offset() &&
                   curY <= (s->get_v_offset() + 2 * View::SignalMargin)){
            _measure_shown = false;
            break;
        }else {
            _measure_shown = false;
        }
    }

    if (_measure_shown == true) {
        const uint64_t delta_sample = _nxt_sample - _cur_sample;
        const uint64_t delta1_sample = _thd_sample - _cur_sample;
        //assert(delta_sample >= 0);
        const double delta_time = delta_sample * 1.0f / _view.session().get_last_sample_rate();
        const double delta1_time = delta1_sample * 1.0f / _view.session().get_last_sample_rate();
        const int order = (int)floorf(log10f(delta_time));
        unsigned int prefix = (15 + order) / 3;
        assert(prefix < 9);

        _mm_width = _view.get_ruler()->format_time(delta_time, prefix);
        _mm_period = _thd_sample != 0 ? _view.get_ruler()->format_time(delta1_time, prefix) :
                                        "#####";
        _mm_freq = _thd_sample != 0 ? _view.get_ruler()->format_freq(delta1_time) :
                                      "#####";
    } else {
        _mm_width = "#####";
        _mm_period = "#####";
        _mm_freq = "#####";
    }

    _view.on_mouse_moved();
}

void Viewport::paintMeasure(QPainter &p)
{
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
        double typical_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
            Qt::AlignLeft | Qt::AlignTop, _mm_width).width() + 150;
        QRectF measure_rect = QRectF(_view.hover_point().x(), _view.hover_point().y(),
                                     (double)typical_width, 60.0);
        QRectF measure1_rect = QRectF(_view.hover_point().x(), _view.hover_point().y(),
                                     (double)typical_width, 20.0);
        QRectF measure2_rect = QRectF(_view.hover_point().x(), _view.hover_point().y() + 20,
                                     (double)typical_width, 20.0);
        QRectF measure3_rect = QRectF(_view.hover_point().x(), _view.hover_point().y() + 40,
                                     (double)typical_width, 20.0);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(17, 133, 209,  150));
        p.drawRect(measure_rect);

        p.setPen(Qt::black);
        p.drawText(measure1_rect, Qt::AlignRight | Qt::AlignVCenter,
                   "Width: " + _mm_width);
        p.drawText(measure2_rect, Qt::AlignRight | Qt::AlignVCenter,
                   "Period: " + _mm_period);
        p.drawText(measure3_rect, Qt::AlignRight | Qt::AlignVCenter,
                   "Frequency: " + _mm_freq);
    }
}

QString Viewport::get_mm_width()
{
    return _mm_width;
}

QString Viewport::get_mm_period()
{
    return _mm_period;
}

QString Viewport::get_mm_freq()
{
    return _mm_freq;
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

} // namespace view
} // namespace pv
