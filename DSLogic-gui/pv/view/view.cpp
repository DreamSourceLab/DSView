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


#include <assert.h>
#include <limits.h>
#include <math.h>

#include <boost/foreach.hpp>

#include <QtGui/QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QScrollBar>

#include "decodetrace.h"
#include "header.h"
#include "devmode.h"
#include "ruler.h"
#include "signal.h"
#include "dsosignal.h"
#include "view.h"
#include "viewport.h"

#include "../device/devinst.h"
#include "pv/sigsession.h"
#include "pv/data/logic.h"
#include "pv/data/logicsnapshot.h"

using namespace boost;
using namespace std;

namespace pv {
namespace view {

const int View::LabelMarginWidth = 70;
const int View::RulerHeight = 50;

const int View::MaxScrollValue = INT_MAX / 2;

//const int View::SignalHeight = 30;s
const int View::SignalMargin = 10;
const int View::SignalSnapGridSize = 10;

const QColor View::CursorAreaColour(220, 231, 243);

const QSizeF View::LabelPadding(4, 4);

View::View(SigSession &session, QWidget *parent) :
	QAbstractScrollArea(parent),
	_session(session),
	_viewport(new Viewport(*this)),
	_ruler(new Ruler(*this)),
	_header(new Header(*this)),
    _devmode(new DevMode(*this)),
    _scale(1e-8),
    _preScale(1e-6),
    _maxscale(1e9),
    _minscale(1e-15),
	_offset(0),
    _preOffset(0),
	_v_offset(0),
	_updating_scroll(false),
    _need_update(false),
	_show_cursors(false),
    _trig_pos(0),
	_hover_point(-1, -1)
{
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	connect(horizontalScrollBar(), SIGNAL(valueChanged(int)),
		this, SLOT(h_scroll_value_changed(int)));
	connect(verticalScrollBar(), SIGNAL(valueChanged(int)),
		this, SLOT(v_scroll_value_changed(int)));

    setViewportMargins(headerWidth(), RulerHeight, 0, 0);
    setViewport(_viewport);

	connect(&_session, SIGNAL(signals_changed()),
		this, SLOT(signals_changed()));
	connect(&_session, SIGNAL(data_updated()),
		this, SLOT(data_updated()));
    connect(&_session, SIGNAL(receive_data(quint64)),
            this, SLOT(receive_data(quint64)));
    connect(&_session, SIGNAL(receive_trigger(quint64)),
            this, SLOT(set_trig_pos(quint64)));

    connect(&_session, SIGNAL(device_setted()),
            _devmode, SLOT(set_device()));
    connect(_devmode, SIGNAL(mode_changed()),
            this, SIGNAL(mode_changed()));

    connect(_header, SIGNAL(traces_moved()),
        this, SLOT(on_traces_moved()));
    connect(_header, SIGNAL(header_updated()),
        this, SLOT(header_updated()));

	_viewport->installEventFilter(this);
	_ruler->installEventFilter(this);
	_header->installEventFilter(this);
    _devmode->installEventFilter(this);

    _viewport->setObjectName(tr("ViewArea_viewport"));
    _ruler->setObjectName(tr("ViewArea_ruler"));
    _header->setObjectName(tr("ViewArea_header"));

    _show_trig_cursor = false;
    _trig_cursor = new Cursor(*this, Trace::dsLightRed, 0);
    _show_search_cursor = false;
    _search_pos = 0;
    _search_cursor = new Cursor(*this, Trace::dsLightBlue, _search_pos);
}

SigSession& View::session()
{
	return _session;
}

double View::scale() const
{
	return _scale;
}

double View::offset() const
{
	return _offset;
}

int View::v_offset() const
{
	return _v_offset;
}

double View::get_minscale() const
{
    return _minscale;
}

double View::get_maxscale() const
{
    return _maxscale;
}

void View::zoom(double steps)
{
    zoom(steps, (width() - headerWidth()) / 2);
}

void View::set_need_update(bool need_update)
{
    _need_update = need_update;
}

bool View::need_update() const
{
    return _need_update;
}

void View::zoom(double steps, int offset)
{
    //if (_session.get_capture_state() == SigSession::Stopped) {
        _preScale = _scale;
        _preOffset = _offset;

        const double cursor_offset = _offset + _scale * offset;
        if (_session.get_device()->dev_inst()->mode != DSO) {
            _scale *= pow(3.0/2.0, -steps);
            _scale = max(min(_scale, _maxscale), _minscale);
        }else {
            const vector< shared_ptr<Signal> > sigs(_session.get_signals());
            BOOST_FOREACH(const shared_ptr<Signal> s, sigs) {
                shared_ptr<DsoSignal> dsoSig;
                if (dsoSig = dynamic_pointer_cast<DsoSignal>(s)) {
                    if(steps > 0.5)
                        dsoSig->go_hDialPre();
                    else if (steps < -0.5)
                        dsoSig->go_hDialNext();
                }
            }
        }
        _offset = cursor_offset - _scale * offset;
        const double MinOffset = -(_scale * (_viewport->width() * (1 - MaxViewRate)));
        const double MaxOffset = _session.get_device()->get_sample_time() -
                _scale * (_viewport->width() * MaxViewRate);
        _offset = max(min(_offset, MaxOffset), MinOffset);

        if (_scale != _preScale || _offset != _preOffset) {
            _header->update();
            _ruler->update();
            _viewport->update();
            update_scroll();
        }
    //}
}


void View::set_scale_offset(double scale, double offset)
{
    //if (_session.get_capture_state() == SigSession::Stopped) {
        _preScale = _scale;
        _preOffset = _offset;

        _scale = max(min(scale, _maxscale), _minscale);

        const double MinOffset = -(_scale * (_viewport->width() * (1 - MaxViewRate)));
        const double MaxOffset = _session.get_device()->get_sample_time()
                - _scale * (_viewport->width() * MaxViewRate);
        _offset = max(min(offset, MaxOffset), MinOffset);

        if (_scale != _preScale || _offset != _preOffset) {
            update_scroll();
            _header->update();
            _ruler->update();
            _viewport->update();
        }
    //}
}

void View::set_preScale_preOffset()
{
    //assert(_preScale <= _maxscale);
    //assert(_preScale >= _minscale);
    //assert(_preOffset >= 0);

    set_scale_offset(_preScale, _preOffset);
}

vector< shared_ptr<Trace> > View::get_traces() const
{
    const vector< shared_ptr<Signal> > sigs(_session.get_signals());
#ifdef ENABLE_DECODE
    const vector< shared_ptr<DecodeTrace> > decode_sigs(
        _session.get_decode_signals());
    vector< shared_ptr<Trace> > traces(
        sigs.size() + decode_sigs.size());
#else
    vector< shared_ptr<Trace> > traces(sigs.size());
#endif

    vector< shared_ptr<Trace> >::iterator i = traces.begin();
    i = copy(sigs.begin(), sigs.end(), i);
#ifdef ENABLE_DECODE
    i = copy(decode_sigs.begin(), decode_sigs.end(), i);
#endif

    stable_sort(traces.begin(), traces.end(), compare_trace_v_offsets);
    return traces;
}

bool View::compare_trace_v_offsets(const shared_ptr<Trace> &a,
    const shared_ptr<Trace> &b)
{
    assert(a);
    assert(b);
    return a->get_v_offset() < b->get_v_offset();
}

bool View::cursors_shown() const
{
	return _show_cursors;
}

bool View::trig_cursor_shown() const
{
    return _show_trig_cursor;
}

bool View::search_cursor_shown() const
{
    return _show_search_cursor;
}

void View::show_cursors(bool show)
{
	_show_cursors = show;
	_ruler->update();
	_viewport->update();
}

void View::show_trig_cursor(bool show)
{
    _show_trig_cursor = show;
    _ruler->update();
    _viewport->update();
}

void View::show_search_cursor(bool show)
{
    _show_search_cursor = show;
    _ruler->update();
    _viewport->update();
}

void View::set_trig_pos(quint64 trig_pos)
{
    const double time = trig_pos * 1.0f / _session.get_device()->get_sample_rate();
    _trig_pos = trig_pos;
    _trig_cursor->set_time(time);
    _show_trig_cursor = true;
    set_scale_offset(_scale,  time - _scale * _viewport->width() / 2);
    _ruler->update();
    _viewport->update();
}

void View::set_search_pos(uint64_t search_pos)
{
    //assert(search_pos >= 0);

    const double time = search_pos * 1.0f / _session.get_device()->get_sample_rate();
    _search_pos = search_pos;
    _search_cursor->set_time(time);
    set_scale_offset(_scale,  time - _scale * _viewport->width() / 2);
    _ruler->update();
    _viewport->update();
}

uint64_t View::get_trig_pos()
{
    return _trig_pos;
}

uint64_t View::get_search_pos()
{
    return _search_pos;
}

const QPointF& View::hover_point() const
{
	return _hover_point;
}

void View::normalize_layout()
{
    const vector< shared_ptr<Trace> > traces(get_traces());

	int v_min = INT_MAX;
    BOOST_FOREACH(const shared_ptr<Trace> t, traces)
        v_min = min(t->get_v_offset(), v_min);

	const int delta = -min(v_min, 0);
    BOOST_FOREACH(shared_ptr<Trace> t, traces)
        t->set_v_offset(t->get_v_offset() + delta);

	verticalScrollBar()->setSliderPosition(_v_offset + delta);
	v_scroll_value_changed(verticalScrollBar()->sliderPosition());
}


int View::get_spanY()
{
    return _spanY;
}

int View::get_signalHeight()
{
    return _signalHeight;
}

void View::get_scroll_layout(double &length, double &offset) const
{
    const set< shared_ptr<data::SignalData> > data_set = _session.get_data();
    if (data_set.empty())
		return;

    length = _session.get_device()->get_sample_time() / _scale;
	offset = _offset / _scale;
}

void View::update_scroll()
{
	assert(_viewport);

	const QSize areaSize = _viewport->size();

	// Set the horizontal scroll bar
	double length = 0, offset = 0;
	get_scroll_layout(length, offset);
	length = max(length - areaSize.width(), 0.0);

	horizontalScrollBar()->setPageStep(areaSize.width() / 2);

	_updating_scroll = true;

	if (length < MaxScrollValue) {
		horizontalScrollBar()->setRange(0, length);
		horizontalScrollBar()->setSliderPosition(offset);
	} else {
		horizontalScrollBar()->setRange(0, MaxScrollValue);
		horizontalScrollBar()->setSliderPosition(
			_offset * MaxScrollValue / (_scale * length));
	}

	_updating_scroll = false;

	// Set the vertical scrollbar
	verticalScrollBar()->setPageStep(areaSize.height());
	verticalScrollBar()->setRange(0,
        _viewport->get_total_height() - areaSize.height());
}

void View::update_scale()
{
    const uint64_t sample_rate = _session.get_device()->get_sample_rate();
    assert(sample_rate > 0);

    if (_session.get_device()->dev_inst()->mode != DSO) {
        _scale = (1.0f / sample_rate) / WellPixelsPerSample;
        _maxscale = _session.get_device()->get_sample_time() / (get_max_width() * MaxViewRate);
    } else {
        _scale = _session.get_device()->get_time_base() * 10.0f / get_max_width() * pow(10, -9);
        _maxscale = 1e9;
    }

    _minscale = (1.0f / sample_rate) / MaxPixelsPerSample;
    _offset = 0;
    _preScale = _scale;
    _preOffset = _offset;

    const double time = _trig_pos * 1.0f / sample_rate;
    _trig_cursor->set_time(time);

    _ruler->update();
    _viewport->update();
}

void View::signals_changed()
{
    int total_rows = 0;
    const vector< shared_ptr<Trace> > traces(get_traces());
    BOOST_FOREACH(const shared_ptr<Trace> t, traces)
    {
        assert(t);
        if (dynamic_pointer_cast<DsoSignal>(t) ||
            t->enabled())
            total_rows += t->rows_size();
    }

    const double height = (_viewport->height()
                           - horizontalScrollBar()->height()
                           - 2 * SignalMargin * traces.size()) * 1.0 / total_rows;

    _signalHeight = (int)((height <= 0) ? 1 : height);
    _spanY = _signalHeight + 2 * SignalMargin;
    int next_v_offset = SignalMargin;
    BOOST_FOREACH(shared_ptr<Trace> t, traces) {
        t->set_view(this);
        const double traceHeight = _signalHeight*t->rows_size();
        t->set_signalHeight((int)traceHeight);
        t->set_v_offset(next_v_offset + 0.5 * traceHeight + SignalMargin);
        next_v_offset += traceHeight + 2 * SignalMargin;
	}

    header_updated();
    normalize_layout();
}

bool View::eventFilter(QObject *object, QEvent *event)
{
	const QEvent::Type type = event->type();
	if (type == QEvent::MouseMove) {

		const QMouseEvent *const mouse_event = (QMouseEvent*)event;
        if (object == _ruler || object == _viewport) {
            //_hover_point = QPoint(mouse_event->x(), 0);
            double cur_periods = (mouse_event->pos().x() * _scale + _offset) / _ruler->get_min_period();
            double integer_x = (round(cur_periods) * _ruler->get_min_period() - _offset ) / _scale;
            double cur_deviate_x = qAbs(mouse_event->pos().x() - integer_x);
            if (cur_deviate_x < 10)
                _hover_point = QPointF(integer_x, mouse_event->pos().y());
            else
                _hover_point = mouse_event->pos();
        } else if (object == _header)
			_hover_point = QPoint(0, mouse_event->y());
		else
			_hover_point = QPoint(-1, -1);

		hover_point_changed();

	} else if (type == QEvent::Leave) {
		_hover_point = QPoint(-1, -1);
		hover_point_changed();
	}

	return QObject::eventFilter(object, event);
}

bool View::viewportEvent(QEvent *e)
{
	switch(e->type()) {
	case QEvent::Paint:
	case QEvent::MouseButtonPress:
	case QEvent::MouseButtonRelease:
	case QEvent::MouseButtonDblClick:
	case QEvent::MouseMove:
	case QEvent::Wheel:
		return false;

	default:
		return QAbstractScrollArea::viewportEvent(e);
	}
}

int View::headerWidth()
{
    int headerWidth;
    int maxNameWidth = 0;
    int maxLeftWidth = 0;
    int maxRightWidth = 0;

    QFont font = QApplication::font();
    QFontMetrics fm(font);

    const vector< shared_ptr<Trace> > traces(get_traces());
    if (!traces.empty()){
        BOOST_FOREACH(const shared_ptr<Trace> t, traces) {
            maxNameWidth = max(fm.boundingRect(t->get_name()).width(), maxNameWidth);
            maxLeftWidth = max(t->get_leftWidth(), maxLeftWidth);
            maxRightWidth = max(t->get_rightWidth(), maxRightWidth);
        }
    }
    maxNameWidth = max(_header->get_nameEditWidth(), maxNameWidth);
    headerWidth = maxLeftWidth + maxNameWidth + maxRightWidth;

    setViewportMargins(headerWidth, RulerHeight, 0, 0);

    return headerWidth;
}

void View::resizeEvent(QResizeEvent*)
{
    update_margins();
    update_scroll();
    if (_session.get_device()->dev_inst()->mode == DSO)
        _scale = _session.get_device()->get_time_base() * pow(10, -9) * DS_CONF_DSO_HDIVS / get_max_width();

    _maxscale = _session.get_device()->get_sample_time() / (get_max_width() * MaxViewRate);
    _scale = min(_scale, _maxscale);

    signals_changed();
    _ruler->update();
    _header->header_resize();
    _need_update = true;
}

void View::h_scroll_value_changed(int value)
{
	if (_updating_scroll)
		return;

    _preOffset = _offset;

    const double MinOffset = -(_scale * (_viewport->width() * (1 - MaxViewRate)));
    const double MaxOffset = _session.get_device()->get_sample_time()
            - _scale * (_viewport->width() * MaxViewRate);

	const int range = horizontalScrollBar()->maximum();
	if (range < MaxScrollValue)
		_offset = _scale * value;
	else {
		double length = 0, offset;
		get_scroll_layout(length, offset);
		_offset = _scale * length * value / MaxScrollValue;
	}

    _offset = max(min(_offset, MaxOffset), MinOffset);

    if (_offset != _preOffset) {
        _ruler->update();
        _viewport->update();
    }
}

void View::v_scroll_value_changed(int value)
{
	_v_offset = value;
	_header->update();
	_viewport->update();
}

void View::data_updated()
{
    setViewportMargins(headerWidth(), RulerHeight, 0, 0);
    update_margins();

	// Update the scroll bars
	update_scroll();

	// Repaint the view
    _need_update = true;
	_viewport->update();
}

void View::update_margins()
{
    _ruler->setGeometry(_viewport->x(), 0,
        _viewport->width(), _viewport->y());
    _header->setGeometry(0, _viewport->y(),
        _viewport->x(), _viewport->height());
    _devmode->setGeometry(0, 0,
        _viewport->x(), _viewport->y());
}

void View::header_updated()
{
    headerWidth();
    update_margins();

    // Update the scroll bars
    update_scroll();

    _viewport->update();
    _header->update();
}

void View::marker_time_changed()
{
	_ruler->update();
	_viewport->update();
}

void View::on_traces_moved()
{
	update_scroll();
    _need_update = true;
    _viewport->update();
    //traces_moved();
}

/*
 * cursorList
 */
std::list<Cursor*>& View::get_cursorList()
{
    return _cursorList;
}

Cursor* View::get_trig_cursor()
{
    return _trig_cursor;
}

Cursor* View::get_search_cursor()
{
    return _search_cursor;
}

Ruler* View::get_ruler()
{
    return _ruler;
}

void View::add_cursor(QColor color, double time)
{
    Cursor *newCursor = new Cursor(*this, color, time);
    _cursorList.push_back(newCursor);
    cursor_update();
}

void View::del_cursor(Cursor* cursor)
{
    assert(cursor);

    _cursorList.remove(cursor);
    delete cursor;
    cursor_update();
}

void View::set_cursor_middle(int index)
{
    assert(index < (int)_cursorList.size());

    list<Cursor*>::iterator i = _cursorList.begin();
    while (index-- != 0)
            i++;
    set_scale_offset(_scale, (*i)->time() - _scale * _viewport->width() / 2);
}

void View::receive_data(quint64 length)
{
    _viewport->set_receive_len(length);
}

QString View::get_mm_width()
{
    return _viewport->get_mm_width();
}

QString View::get_mm_period()
{
    return _viewport->get_mm_period();
}

QString View::get_mm_freq()
{
    return _viewport->get_mm_freq();
}

QString View::get_cm_time(int index)
{
    return _ruler->format_time(get_cursor_time(index));
}

QString View::get_cm_delta(int index1, int index2)
{
    if (index1 == index2)
        return "0";

    return _ruler->format_time(abs(get_cursor_time(index1) -
                                   get_cursor_time(index2)));
}

double View::get_cursor_time(int index)
{
    assert(index < (int)_cursorList.size());

    int curIndex = 0;
    for (list<Cursor*>::iterator i = _cursorList.begin();
         i != _cursorList.end(); i++) {
        if (index == curIndex) {
            return (*i)->time();
        }
        curIndex++;
    }
}

uint64_t View::get_cursor_samples(int index)
{
    const double time = get_cursor_time(index);
    const uint64_t sample_rate = _session.get_device()->get_sample_limit();
    assert(sample_rate !=0);

    return time*sample_rate;
}

void View::on_mouse_moved()
{
    mouse_moved();
}
void View::on_cursor_moved()
{
    cursor_moved();
}

void View::set_measure_en(int enable)
{
    _viewport->set_measure_en(enable);
}

void View::on_state_changed(bool stop)
{
    if (stop)
        _viewport->stop_trigger_timer();
}

int View::get_max_width()
{
    int max_width = 0;
    const vector< shared_ptr<Signal> > sigs(_session.get_signals());
    BOOST_FOREACH(const shared_ptr<Signal> s, sigs) {
        max_width = max((double)max_width, s->get_view_rect().width());
    }

    return max_width;
}

} // namespace view
} // namespace pv
