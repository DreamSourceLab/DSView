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

#include "header.h"
#include "ruler.h"
#include "signal.h"
#include "view.h"
#include "viewport.h"

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

const int View::WellPixelsPerSample = 1.0f;
const double View::MaxViewRate = 1.0f;

View::View(SigSession &session, QWidget *parent) :
	QAbstractScrollArea(parent),
	_session(session),
	_viewport(new Viewport(*this)),
	_ruler(new Ruler(*this)),
	_header(new Header(*this)),
	_data_length(0),
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
	_hover_point(-1, -1)
{
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	connect(horizontalScrollBar(), SIGNAL(valueChanged(int)),
		this, SLOT(h_scroll_value_changed(int)));
	connect(verticalScrollBar(), SIGNAL(valueChanged(int)),
		this, SLOT(v_scroll_value_changed(int)));

	connect(&_session, SIGNAL(signals_changed()),
		this, SLOT(signals_changed()));
	connect(&_session, SIGNAL(data_updated()),
		this, SLOT(data_updated()));
    connect(&_session, SIGNAL(sample_rate_changed(quint64)),
        this, SLOT(sample_rate_changed(quint64)));
    connect(&_session, SIGNAL(receive_data(quint64)),
            this, SLOT(receive_data(quint64)));
    connect(&_session, SIGNAL(receive_trigger(quint64)),
            this, SLOT(set_trig_pos(quint64)));

	connect(_header, SIGNAL(signals_moved()),
		this, SLOT(on_signals_moved()));
    connect(_header, SIGNAL(header_updated()),
        this, SLOT(header_updated()));
    connect(_header, SIGNAL(vDial_changed(quint16)),
        this, SLOT(vDial_changed(quint16)));
    connect(_header, SIGNAL(hDial_changed(quint16)),
        this, SLOT(hDial_changed(quint16)));
    connect(_header, SIGNAL(acdc_changed(quint16)),
        this, SLOT(acdc_changed(quint16)));
    connect(_header, SIGNAL(ch_changed(quint16)),
        this, SLOT(ch_changed(quint16)));

    setViewportMargins(headerWidth(), RulerHeight, 0, 0);
	setViewport(_viewport);

	_viewport->installEventFilter(this);
	_ruler->installEventFilter(this);
	_header->installEventFilter(this);

    _viewport->setObjectName(tr("ViewArea_viewport"));
    _ruler->setObjectName(tr("ViewArea_ruler"));
    _header->setObjectName(tr("ViewArea_header"));

    _show_trig_cursor = false;
    _trig_cursor = new Cursor(*this, Signal::dsLightRed, 0);
    _show_search_cursor = false;
    _search_pos = 0;
    _search_cursor = new Cursor(*this, Signal::dsLightBlue, _search_pos);
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
        if (_session.get_device()->mode != DSO) {
            _scale *= pow(3.0/2.0, -steps);
            _scale = max(min(_scale, _maxscale), _minscale);
        } else {
            const vector< shared_ptr<Signal> > sigs(_session.get_signals());
            if (steps > 0.5) {
                BOOST_FOREACH(const shared_ptr<Signal> s, sigs)
                    s->go_hDialNext();
            } else if(steps < -0.5) {
                BOOST_FOREACH(const shared_ptr<Signal> s, sigs)
                    s->go_hDialPre();
            }
            _scale = sigs.at(0)->get_hDialValue() * pow(10, -9) * Viewport::NumSpanX / _viewport->width();
        }
        _offset = cursor_offset - _scale * offset;
        const double MinOffset = -(_scale * (_viewport->width() * (1 - MaxViewRate)));
        const double MaxOffset = _data_length * 1.0f / _session.get_last_sample_rate() -
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

        if (_session.get_device()->mode != DSO)
            _scale = max(min(scale, _maxscale), _minscale);

        const double MinOffset = -(_scale * (_viewport->width() * (1 - MaxViewRate)));
        const double MaxOffset = _data_length * 1.0f / _session.get_last_sample_rate()
                - _scale * (_viewport->width() * MaxViewRate);
        _offset = max(min(offset, MaxOffset), MinOffset);

        if (_scale != _preScale || _offset != _preOffset) {
            update_scroll();
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
    const double time = trig_pos * 1.0f / _session.get_last_sample_rate();
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

    const double time = search_pos * 1.0f / _session.get_last_sample_rate();
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
	const vector< boost::shared_ptr<Signal> > sigs(_session.get_signals());

	int v_min = INT_MAX;
	BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs)
		v_min = min(s->get_v_offset(), v_min);

	const int delta = -min(v_min, 0);
	BOOST_FOREACH(boost::shared_ptr<Signal> s, sigs)
		s->set_v_offset(s->get_v_offset() + delta);

	verticalScrollBar()->setSliderPosition(_v_offset + delta);
	v_scroll_value_changed(verticalScrollBar()->sliderPosition());
}


int View::get_spanY()
{
    return _spanY;
}

int View::get_signalHeight()
{
    return SignalHeight;
}

void View::get_scroll_layout(double &length, double &offset) const
{
	const boost::shared_ptr<data::SignalData> sig_data = _session.get_data();
	if (!sig_data)
		return;

	length = _data_length / (sig_data->get_samplerate() * _scale);
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
		_viewport->get_total_height() + SignalMargin -
		areaSize.height());
}

void View::reset_signal_layout()
{
    if (_session.get_signals().size())
        SignalHeight =
                ((_viewport->height() - horizontalScrollBar()->height()) /
                 _session.get_signals().size())
                - 2 * SignalMargin;

	int offset = SignalMargin + SignalHeight;
    _spanY = SignalHeight + 2 * SignalMargin;

	const vector< boost::shared_ptr<Signal> > sigs(_session.get_signals());
	BOOST_FOREACH(boost::shared_ptr<Signal> s, sigs) {
        s->set_signalHeight(SignalHeight);
        s->set_windowHeight(_viewport->height());
        //s->set_v_offset(offset);
        //offset += SignalHeight + 2 * SignalMargin;
        s->set_v_offset(offset + s->get_order() * _spanY);
        s->set_zeroPos(_viewport->height()*0.5);
	}
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
    int fontWidth=fm.width("A");

    const vector< shared_ptr<Signal> > sigs(_session.get_signals());
    if (!sigs.empty()){
        BOOST_FOREACH(const shared_ptr<Signal> s, sigs) {
            maxNameWidth = max(s->get_name().length() * fontWidth, maxNameWidth);
            maxLeftWidth = max(s->get_leftWidth(), maxLeftWidth);
            maxRightWidth = max(s->get_rightWidth(), maxRightWidth);
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
    if (_session.get_capture_state() == SigSession::Stopped) {
        _maxscale = (_data_length * 1.0f / _session.get_last_sample_rate()) / (_viewport->width() * MaxViewRate);
        _scale = min(_scale, _maxscale);
    }
    reset_signal_layout();
    _header->header_resize();
    _need_update = true;
}

void View::h_scroll_value_changed(int value)
{
	if (_updating_scroll)
		return;

    _preOffset = _offset;

    const double MinOffset = -(_scale * (_viewport->width() * (1 - MaxViewRate)));
    const double MaxOffset = _data_length * 1.0f / _session.get_last_sample_rate()
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

void View::signals_changed()
{
	reset_signal_layout();
}

void View::data_updated()
{
	// Get the new data length
    _data_length = max(_session.get_total_sample_len(), (quint64)1000);
    _maxscale = (_data_length * 1.0f / _session.get_last_sample_rate()) / (_viewport->width() * MaxViewRate);
    if(_session.get_device()->mode != DSO)
        _scale = min(_scale, _maxscale);

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

void View::sample_rate_changed(quint64 sample_rate)
{
    assert(sample_rate > 0);

    if (_session.get_device()->mode != DSO)
        _scale = (1.0f / sample_rate) / WellPixelsPerSample;

    _minscale = (1.0f / sample_rate) / (_viewport->width() * MaxViewRate);
    _offset = 0;
    _preScale = _scale;
    _preOffset = _offset;

    _ruler->update();
    _viewport->update();
}

void View::marker_time_changed()
{
	_ruler->update();
	_viewport->update();
}

void View::on_signals_moved()
{
	update_scroll();
    _viewport->update();
    //signals_moved();
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

QString View::get_cm_delta_cnt(int index1, int index2)
{
    if (index1 == index2)
        return "0";

    return QString::number(
                floor(abs(get_cursor_time(index1) -
                          get_cursor_time(index2)) * _session.get_data()->get_samplerate()));
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

void View::vDial_changed(uint16_t channel)
{
    if (channel == 0)
        _session.set_dso_ctrl(SR_CONF_VDIV0);
    else
        _session.set_dso_ctrl(SR_CONF_VDIV1);
}

void View::hDial_changed(uint16_t channel)
{
    const vector< shared_ptr<Signal> > sigs(_session.get_signals());
    _session.set_dso_ctrl(SR_CONF_TIMEBASE);
    _scale = sigs.at(channel)->get_hDialValue() * pow(10, -9) * Viewport::NumSpanX / _viewport->width();
    _ruler->update();
    _viewport->update();
    update_scroll();
}

void View::acdc_changed(uint16_t channel)
{
    if (channel == 0)
        _session.set_dso_ctrl(SR_CONF_COUPLING0);
    else
        _session.set_dso_ctrl(SR_CONF_COUPLING1);
}

void View::ch_changed(uint16_t channel)
{
    if (channel == 0)
        _session.set_dso_ctrl(SR_CONF_EN_CH0);
    else
        _session.set_dso_ctrl(SR_CONF_EN_CH1);
}

} // namespace view
} // namespace pv
