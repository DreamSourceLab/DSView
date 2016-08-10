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


#include <assert.h>
#include <limits.h>
#include <math.h>

#include <boost/foreach.hpp>

#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QScrollBar>

#include "groupsignal.h"
#include "decodetrace.h"
#include "header.h"
#include "devmode.h"
#include "ruler.h"
#include "signal.h"
#include "dsosignal.h"
#include "view.h"
#include "viewport.h"
#include "mathtrace.h"

#include "../device/devinst.h"
#include "pv/sigsession.h"
#include "pv/data/logic.h"
#include "pv/data/logicsnapshot.h"
#include "pv/dialogs/calibration.h"

using namespace boost;
using namespace std;

namespace pv {
namespace view {

const int View::LabelMarginWidth = 70;
const int View::RulerHeight = 50;

const int View::MaxScrollValue = INT_MAX / 2;
const int View::MaxHeightUnit = 20;

//const int View::SignalHeight = 30;s
const int View::SignalMargin = 10;
const int View::SignalSnapGridSize = 10;

const QColor View::CursorAreaColour(220, 231, 243);

const QSizeF View::LabelPadding(4, 4);

View::View(SigSession &session, pv::toolbars::SamplingBar *sampling_bar, QWidget *parent) :
    QScrollArea(parent),
	_session(session),
    _sampling_bar(sampling_bar),
    _scale(1e-5),
    _preScale(1e-6),
    _maxscale(1e9),
    _minscale(1e-15),
	_offset(0),
    _preOffset(0),
	_updating_scroll(false),
	_show_cursors(false),
    _hover_point(-1, -1),
    _dso_auto(true)
{
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	connect(horizontalScrollBar(), SIGNAL(valueChanged(int)),
		this, SLOT(h_scroll_value_changed(int)));
	connect(verticalScrollBar(), SIGNAL(valueChanged(int)),
		this, SLOT(v_scroll_value_changed(int)));

    // trace viewport map
    _trace_view_map[SR_CHANNEL_LOGIC] = TIME_VIEW;
    _trace_view_map[SR_CHANNEL_GROUP] = TIME_VIEW;
    _trace_view_map[SR_CHANNEL_DECODER] = TIME_VIEW;
    _trace_view_map[SR_CHANNEL_ANALOG] = TIME_VIEW;
    _trace_view_map[SR_CHANNEL_DSO] = TIME_VIEW;
    _trace_view_map[SR_CHANNEL_FFT] = FFT_VIEW;

    _active_viewport = NULL;
    _ruler = new Ruler(*this);
    _header = new Header(*this);
    _devmode = new DevMode(this, session);

    setViewportMargins(headerWidth(), RulerHeight, 0, 0);
    //setViewport(_viewport);

    // windows splitter
    _time_viewport = new Viewport(*this, TIME_VIEW);
    _time_viewport->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    _time_viewport->setMinimumHeight(100);
    connect(_time_viewport, SIGNAL(measure_updated()),
            this, SLOT(on_measure_updated()));
    _fft_viewport = new Viewport(*this, FFT_VIEW);
    _fft_viewport->setVisible(false);
    _fft_viewport->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    _fft_viewport->setMinimumHeight(100);
    connect(_fft_viewport, SIGNAL(measure_updated()),
            this, SLOT(on_measure_updated()));

    _vsplitter = new QSplitter(this);
    _vsplitter->setOrientation(Qt::Vertical);
    _vsplitter->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    _viewport_list.push_back(_time_viewport);
    _vsplitter->addWidget(_time_viewport);
    _vsplitter->setCollapsible(0, false);
    _vsplitter->setStretchFactor(0, 2);
    _viewport_list.push_back(_fft_viewport);
    _vsplitter->addWidget(_fft_viewport);
    _vsplitter->setCollapsible(1, false);
    _vsplitter->setStretchFactor(1, 1);

    _viewcenter = new QWidget(this);
    _viewcenter->setContentsMargins(0,0,0,0);
    QGridLayout* layout = new QGridLayout(_viewcenter);
    layout->setSpacing(0);
    layout->setContentsMargins(0,0,0,0);
    _viewcenter->setLayout(layout);
    layout->addWidget(_vsplitter, 0, 0);
    _viewbottom = new widgets::ViewStatus(this);
    _viewbottom->setFixedHeight(StatusHeight);
    layout->addWidget(_viewbottom, 1, 0);
    setViewport(_viewcenter);
    connect(_vsplitter, SIGNAL(splitterMoved(int,int)),
        this, SLOT(splitterMoved(int, int)));

    connect(&_session, SIGNAL(device_setted()),
            _devmode, SLOT(set_device()));
    connect(&_session, SIGNAL(signals_changed()),
        this, SLOT(signals_changed()), Qt::DirectConnection);
    connect(&_session, SIGNAL(data_updated()),
        this, SLOT(data_updated()));
    connect(&_session, SIGNAL(receive_header()),
            this, SLOT(receive_header()));
    connect(&_session, SIGNAL(receive_trigger(quint64)),
            this, SLOT(receive_trigger(quint64)));
    connect(&_session, SIGNAL(frame_ended()),
            this, SLOT(receive_end()));
    connect(&_session, SIGNAL(frame_began()),
            this, SLOT(frame_began()));
    connect(&_session, SIGNAL(show_region(uint64_t,uint64_t)),
            this, SLOT(show_region(uint64_t, uint64_t)));
    connect(&_session, SIGNAL(show_wait_trigger()),
            _time_viewport, SLOT(show_wait_trigger()));

    connect(_devmode, SIGNAL(mode_changed()),
            parent, SLOT(mode_changed()), Qt::DirectConnection);

    connect(_header, SIGNAL(traces_moved()),
        this, SLOT(on_traces_moved()));
    connect(_header, SIGNAL(header_updated()),
        this, SLOT(header_updated()));

    _time_viewport->installEventFilter(this);
    _fft_viewport->installEventFilter(this);
	_ruler->installEventFilter(this);
	_header->installEventFilter(this);
    _devmode->installEventFilter(this);

    _viewcenter->setObjectName(tr("ViewArea_center"));
    _ruler->setObjectName(tr("ViewArea_ruler"));
    _header->setObjectName(tr("ViewArea_header"));

    _show_trig_cursor = false;
    _trig_cursor = new Cursor(*this, Trace::dsLightRed, 0);
    _show_search_cursor = false;
    _search_pos = 0;
    _search_cursor = new Cursor(*this, Trace::dsLightBlue, _search_pos);

    _cali = new pv::dialogs::Calibration(this);
    _cali->hide();
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
    zoom(steps, get_view_width() / 2);
}

void View::set_update(Viewport *viewport, bool need_update)
{
    viewport->set_need_update(need_update);
}

void View::set_all_update(bool need_update)
{
    BOOST_FOREACH(Viewport *viewport, _viewport_list)
        viewport->set_need_update(need_update);
}

void View::update_sample(bool instant)
{
    _session.get_device()->set_config(NULL, NULL, SR_CONF_INSTANT, g_variant_new_boolean(instant));
    BOOST_FOREACH(const boost::shared_ptr<pv::view::Signal> s, _session.get_signals()) {
        boost::shared_ptr<pv::view::DsoSignal> dsoSig;
        if (dsoSig = dynamic_pointer_cast<pv::view::DsoSignal>(s)) {
            dsoSig->go_hDialCur();
            break;
        }
    }
}

void View::set_sample_rate(uint64_t sample_rate, bool force)
{
    if (_session.get_capture_state() != pv::SigSession::Stopped || force) {
        _sampling_bar->set_sample_rate(sample_rate);
        _session.set_cur_samplerate(_session.get_device()->get_sample_rate());
    }
}

void View::set_sample_limit(uint64_t sample_limit, bool force)
{
    if (_session.get_capture_state() != pv::SigSession::Stopped || force) {
        _sampling_bar->set_sample_limit(sample_limit);
        const uint64_t final_limit = _session.get_device()->get_sample_limit();
        _trig_cursor->set_index(_trig_cursor->index() * 1.0 / _session.cur_samplelimits() * final_limit);
        _session.set_cur_samplelimits(final_limit);
    }
}

void View::zoom(double steps, int offset)
{
    //if (_session.get_capture_state() == SigSession::Stopped) {
        _preScale = _scale;
        _preOffset = _offset;

        const double cursor_offset = _offset + _scale * offset;
        if (_session.get_device()->dev_inst()->mode != DSO) {
            _scale *= std::pow(3.0/2.0, -steps);
            _scale = max(min(_scale, _maxscale), _minscale);
        }else {
            const vector< boost::shared_ptr<Signal> > sigs(_session.get_signals());
            bool setted = false;
            BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs) {
                boost::shared_ptr<DsoSignal> dsoSig;
                if (dsoSig = dynamic_pointer_cast<DsoSignal>(s)) {
                    if(steps > 0.5)
                        dsoSig->go_hDialPre(setted);
                    else if (steps < -0.5)
                        dsoSig->go_hDialNext(setted);
                    else
                        break;
                    setted = true;
                }
            }
        }

        _offset = cursor_offset - _scale * offset;
        _offset = max(min(_offset, get_max_offset()), get_min_offset());

        if (_scale != _preScale || _offset != _preOffset) {
            _header->update();
            _ruler->update();
            viewport_update();
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
        _offset = max(min(offset, get_max_offset()), get_min_offset());

        if (_scale != _preScale || _offset != _preOffset) {
            update_scroll();
            _header->update();
            _ruler->update();
            viewport_update();
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

vector< boost::shared_ptr<Trace> > View::get_traces(int type)
{
    const vector< boost::shared_ptr<Signal> > sigs(_session.get_signals());
    const vector< boost::shared_ptr<GroupSignal> > groups(_session.get_group_signals());
#ifdef ENABLE_DECODE
    const vector< boost::shared_ptr<DecodeTrace> > decode_sigs(
        _session.get_decode_signals());
#endif
    const vector< boost::shared_ptr<MathTrace> > maths(_session.get_math_signals());

    vector< boost::shared_ptr<Trace> > traces;
    BOOST_FOREACH(boost::shared_ptr<Trace> t, sigs) {
        if (type == ALL_VIEW || _trace_view_map[t->get_type()] == type)
            traces.push_back(t);
    }
#ifdef ENABLE_DECODE
    BOOST_FOREACH(boost::shared_ptr<Trace> t, decode_sigs) {
        if (type == ALL_VIEW || _trace_view_map[t->get_type()] == type)
            traces.push_back(t);
    }
#endif
    BOOST_FOREACH(boost::shared_ptr<Trace> t, groups) {
        if (type == ALL_VIEW || _trace_view_map[t->get_type()] == type)
            traces.push_back(t);
    }

    BOOST_FOREACH(boost::shared_ptr<Trace> t, maths) {
        if (type == ALL_VIEW || _trace_view_map[t->get_type()] == type)
            traces.push_back(t);
    }

    stable_sort(traces.begin(), traces.end(), compare_trace_v_offsets);
    return traces;
}

bool View::compare_trace_v_offsets(const boost::shared_ptr<Trace> &a,
    const boost::shared_ptr<Trace> &b)
{
    assert(a);
    assert(b);
    if (a->get_type() != b->get_type())
        return a->get_type() > b->get_type();
    else
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
    viewport_update();
}

void View::show_trig_cursor(bool show)
{
    _show_trig_cursor = show;
    _ruler->update();
    viewport_update();
}

void View::show_search_cursor(bool show)
{
    _show_search_cursor = show;
    _ruler->update();
    viewport_update();
}

void View::status_clear()
{
    _viewbottom->clear();
}

void View::receive_header()
{
    status_clear();
}

void View::frame_began()
{
    if (_session.get_device()->dev_inst()->mode == LOGIC)
        _viewbottom->set_trig_time(_session.get_trigger_time());
}

void View::receive_end()
{
    if (_session.get_device()->dev_inst()->mode == LOGIC) {
        GVariant *gvar = _session.get_device()->get_config(NULL, NULL, SR_CONF_RLE);
        if (gvar != NULL) {
            bool rle = g_variant_get_boolean(gvar);
            g_variant_unref(gvar);
            if (rle) {
                gvar = _session.get_device()->get_config(NULL, NULL, SR_CONF_ACTUAL_SAMPLES);
                if (gvar != NULL) {
                    uint64_t actual_samples = g_variant_get_uint64(gvar);
                    g_variant_unref(gvar);
                    if (actual_samples != _session.cur_samplelimits()) {
                        _viewbottom->set_rle_depth(actual_samples);
                    }
                }
            }
        }
    }
    _time_viewport->unshow_wait_trigger();
}

void View::receive_trigger(quint64 trig_pos)
{
    const double time = trig_pos * 1.0 / _session.cur_samplerate();
    _trig_cursor->set_index(trig_pos);
    if (ds_trigger_get_en() ||
        _session.get_device()->name() == "virtual-session" ||
        _session.get_device()->dev_inst()->mode == DSO) {
        _show_trig_cursor = true;
        set_scale_offset(_scale,  time - _scale * get_view_width() / 2);
    }

    _ruler->update();
    viewport_update();
}

void View::set_trig_pos(int percent)
{
    uint64_t index = _session.cur_samplelimits() * percent / 100;
    receive_trigger(index);
}

void View::set_search_pos(uint64_t search_pos)
{
    //assert(search_pos >= 0);

    const double time = search_pos * 1.0 / _session.cur_samplerate();
    _search_pos = search_pos;
    _search_cursor->set_index(search_pos);
    set_scale_offset(_scale,  time - _scale * get_view_width() / 2);
    _ruler->update();
    viewport_update();
}

uint64_t View::get_search_pos()
{
    return _search_pos;
}

const QPoint& View::hover_point() const
{
	return _hover_point;
}

void View::normalize_layout()
{
    const vector< boost::shared_ptr<Trace> > traces(get_traces(ALL_VIEW));

	int v_min = INT_MAX;
    BOOST_FOREACH(const boost::shared_ptr<Trace> t, traces)
        v_min = min(t->get_v_offset(), v_min);

	const int delta = -min(v_min, 0);
    BOOST_FOREACH(boost::shared_ptr<Trace> t, traces)
        t->set_v_offset(t->get_v_offset() + delta);

    verticalScrollBar()->setSliderPosition(delta);
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
    const set< boost::shared_ptr<data::SignalData> > data_set = _session.get_data();
    if (data_set.empty())
		return;

    length = _session.get_device()->get_sample_time() / _scale;
	offset = _offset / _scale;
}

void View::update_scroll()
{
    assert(_viewcenter);

    const QSize areaSize = _viewcenter->size();

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
    verticalScrollBar()->setRange(0,0);
}

void View::update_scale_offset()
{
    const uint64_t sample_rate = _session.cur_samplerate();
    assert(sample_rate > 0);

    if (_session.get_device()->dev_inst()->mode != DSO) {
        //_scale = (1.0 / sample_rate) / WellPixelsPerSample;
        _maxscale = _session.cur_sampletime() / (get_view_width() * MaxViewRate);
        _minscale = (1.0 / sample_rate) / MaxPixelsPerSample;
    } else {
        _scale = _session.get_device()->get_time_base() * 10.0 / get_view_width() * std::pow(10.0, -9.0);
        _maxscale = 1e9;
        _minscale = 1e-15;
    }


    _scale = max(min(_scale, _maxscale), _minscale);
    _offset = max(min(_offset, get_max_offset()), get_min_offset());

    _preScale = _scale;
    _preOffset = _offset;

    //_trig_cursor->set_index(_session.get_trigger_pos());

    _ruler->update();
    viewport_update();
}

void View::signals_changed()
{
    int total_rows = 0;
    uint8_t max_height = MaxHeightUnit;
    vector< boost::shared_ptr<Trace> > time_traces;
    vector< boost::shared_ptr<Trace> > fft_traces;
    int bits = 8;

    BOOST_FOREACH(const boost::shared_ptr<Trace> t, get_traces(ALL_VIEW)) {
        if (_trace_view_map[t->get_type()] == TIME_VIEW)
            time_traces.push_back(t);
        else if (_trace_view_map[t->get_type()] == FFT_VIEW)
            if (t->enabled())
                fft_traces.push_back(t);
    }

    if (!fft_traces.empty()) {
        if (!_fft_viewport->isVisible()) {
            _fft_viewport->setVisible(true);
            _fft_viewport->clear_measure();
            _viewport_list.push_back(_fft_viewport);
            _vsplitter->refresh();
        }
        BOOST_FOREACH(boost::shared_ptr<Trace> t, fft_traces) {
            t->set_view(this);
            t->set_viewport(_fft_viewport);
            t->set_totalHeight(_fft_viewport->height());
            t->set_v_offset(_fft_viewport->geometry().bottom());
        }
    } else {
        _fft_viewport->setVisible(false);
        _vsplitter->refresh();

        // Find the decoder in the stack
        std::list< Viewport *>::iterator iter = _viewport_list.begin();
        for(unsigned int i = 0; i < _viewport_list.size(); i++, iter++)
            if ((*iter) == _fft_viewport)
                break;
        // Delete the element
        if (iter != _viewport_list.end())
            _viewport_list.erase(iter);
    }

    if (!time_traces.empty() && _time_viewport) {
        BOOST_FOREACH(const boost::shared_ptr<Trace> t, time_traces) {
            assert(t);
            if (dynamic_pointer_cast<DsoSignal>(t) ||
                t->enabled())
                total_rows += t->rows_size();
        }

        const double height = (_time_viewport->height()
                               - 2 * SignalMargin * time_traces.size()) * 1.0 / total_rows;

        if (_session.get_device()->dev_inst()->mode == LOGIC) {
            GVariant* gvar = _session.get_device()->get_config(NULL, NULL, SR_CONF_MAX_HEIGHT_VALUE);
            if (gvar != NULL) {
                max_height = (g_variant_get_byte(gvar) + 1) * MaxHeightUnit;
                g_variant_unref(gvar);
            }
            _signalHeight = (int)((height <= 0) ? 1 : (height >= max_height) ? max_height : height);
        } else if (_session.get_device()->dev_inst()->mode == DSO) {
            _signalHeight = (_header->height()
                             - horizontalScrollBar()->height()
                             - 2 * SignalMargin * time_traces.size()) * 1.0 / total_rows;
        } else {
            _signalHeight = (int)((height <= 0) ? 1 : height);
        }
        _spanY = _signalHeight + 2 * SignalMargin;
        int next_v_offset = SignalMargin;
        BOOST_FOREACH(boost::shared_ptr<Trace> t, time_traces) {
            t->set_view(this);
            t->set_viewport(_time_viewport);
            const double traceHeight = _signalHeight*t->rows_size();
            t->set_totalHeight((int)traceHeight);
            t->set_v_offset(next_v_offset + 0.5 * traceHeight + SignalMargin);
            next_v_offset += traceHeight + 2 * SignalMargin;

            boost::shared_ptr<view::DsoSignal> dsoSig;
            if (dsoSig = dynamic_pointer_cast<view::DsoSignal>(t)) {
                GVariant *gvar = _session.get_device()->get_config(NULL, NULL, SR_CONF_DSO_BITS);
                if (gvar != NULL) {
                    bits = g_variant_get_byte(gvar);
                    g_variant_unref(gvar);
                }
                dsoSig->set_scale(dsoSig->get_view_rect().height() * 1.0f / (1 << bits));
            }
        }
        _time_viewport->clear_measure();
    }

    header_updated();
    normalize_layout();
    update_scale_offset();
    data_updated();
}

bool View::eventFilter(QObject *object, QEvent *event)
{
	const QEvent::Type type = event->type();
	if (type == QEvent::MouseMove) {
		const QMouseEvent *const mouse_event = (QMouseEvent*)event;
        if (object == _ruler || object == _time_viewport || object == _fft_viewport) {
            //_hover_point = QPoint(mouse_event->x(), 0);
            double cur_periods = (mouse_event->pos().x() * _scale + _offset) / _ruler->get_min_period();
            int integer_x = (round(cur_periods) * _ruler->get_min_period() - _offset ) / _scale;
            double cur_deviate_x = qAbs(mouse_event->pos().x() - integer_x);
            if (cur_deviate_x < 10)
                _hover_point = QPoint(integer_x, mouse_event->pos().y());
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
    int maxNameWidth = 25;
    int maxLeftWidth = 0;
    int maxRightWidth = 0;

    QFont font = QApplication::font();
    QFontMetrics fm(font);

    const vector< boost::shared_ptr<Trace> > traces(get_traces(ALL_VIEW));
    if (!traces.empty()){
        BOOST_FOREACH(const boost::shared_ptr<Trace> t, traces) {
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
    setViewportMargins(headerWidth(), RulerHeight, 0, 0);
    update_margins();
    update_scroll();
    signals_changed();
    if (_session.get_device()->dev_inst()->mode == DSO)
        _scale = _session.get_device()->get_time_base() * std::pow(10.0, -9.0) * DS_CONF_DSO_HDIVS / get_view_width();

    if (_session.get_device()->dev_inst()->mode != DSO)
        _maxscale = _session.cur_sampletime() / (get_view_width() * MaxViewRate);
    else
        _maxscale = 1e9;

    _scale = min(_scale, _maxscale);

    _ruler->update();
    _header->header_resize();
    set_update(_time_viewport, true);
    set_update(_fft_viewport, true);
}

void View::h_scroll_value_changed(int value)
{
	if (_updating_scroll)
		return;

    _preOffset = _offset;

	const int range = horizontalScrollBar()->maximum();
	if (range < MaxScrollValue)
		_offset = _scale * value;
	else {
		double length = 0, offset;
		get_scroll_layout(length, offset);
		_offset = _scale * length * value / MaxScrollValue;
	}

    _offset = max(min(_offset, get_max_offset()), get_min_offset());

    if (_offset != _preOffset) {
        _ruler->update();
        viewport_update();
    }
}

void View::v_scroll_value_changed(int value)
{
    (void)value;
	_header->update();
    viewport_update();
}

void View::data_updated()
{
    setViewportMargins(headerWidth(), RulerHeight, 0, 0);
    update_margins();

	// Update the scroll bars
	update_scroll();

	// Repaint the view
    _time_viewport->unshow_wait_trigger();
    set_update(_time_viewport, true);
    set_update(_fft_viewport, true);
    viewport_update();
}

void View::update_margins()
{
    _ruler->setGeometry(_viewcenter->x(), 0,
        get_view_width(), _viewcenter->y());
    _header->setGeometry(0, _viewcenter->y(),
        _viewcenter->x(), _viewcenter->height());
    _devmode->setGeometry(0, 0,
        _viewcenter->x(), _viewcenter->y());
}

void View::header_updated()
{
    headerWidth();
    update_margins();

    // Update the scroll bars
    update_scroll();

    viewport_update();
    _header->update();
}

void View::marker_time_changed()
{
	_ruler->update();
    viewport_update();
}

void View::on_traces_moved()
{
	update_scroll();
    set_update(_time_viewport, true);
    viewport_update();
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

void View::add_cursor(QColor color, uint64_t index)
{
    Cursor *newCursor = new Cursor(*this, color, index);
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
    set_scale_offset(_scale, (*i)->index() * 1.0 / _session.cur_samplerate() - _scale * get_view_width() / 2);
}

void View::on_measure_updated()
{
    _active_viewport = dynamic_cast<Viewport *>(sender());
    measure_updated();
}

QString View::get_measure(QString option)
{
    if (_active_viewport) {
        return _active_viewport->get_measure(option);
    }
    return "#####";
}

QString View::get_cm_time(int index)
{
    return _ruler->format_real_time(get_cursor_samples(index), _session.cur_samplerate());
}

QString View::get_cm_delta(int index1, int index2)
{
    if (index1 == index2)
        return "0";

    uint64_t samples1 = get_cursor_samples(index1);
    uint64_t samples2 = get_cursor_samples(index2);
    uint64_t delta_sample = (samples1 > samples2) ? samples1 - samples2 : samples2 - samples1;
    return _ruler->format_real_time(delta_sample, _session.cur_samplerate());
}

uint64_t View::get_cursor_samples(int index)
{
    assert(index < (int)_cursorList.size());

    int curIndex = 0;
    for (list<Cursor*>::iterator i = _cursorList.begin();
         i != _cursorList.end(); i++) {
        if (index == curIndex) {
            return (*i)->index();
        }
        curIndex++;
    }
}

void View::on_cursor_moved()
{
    cursor_moved();
}

void View::set_measure_en(int enable)
{
    BOOST_FOREACH(Viewport *viewport, _viewport_list)
            viewport->set_measure_en(enable);
}

void View::on_state_changed(bool stop)
{
    if (stop) {
        BOOST_FOREACH(Viewport *viewport, _viewport_list)
            viewport->stop_trigger_timer();
    }
    update_scale_offset();
}

QRect View::get_view_rect()
{
    if (_session.get_device()->dev_inst()->mode == DSO) {
        const vector< boost::shared_ptr<Signal> > sigs(_session.get_signals());
        BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs) {
            return s->get_view_rect();
        }
    } else {
        return _viewcenter->rect();
    }
}

int View::get_view_width()
{
    int view_width = 0;
    if (_session.get_device()->dev_inst()->mode == DSO) {
        const vector< boost::shared_ptr<Signal> > sigs(_session.get_signals());
        BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs) {
            view_width = max(view_width, s->get_view_rect().width());
        }
    } else {
        view_width = _viewcenter->width();
    }

    return view_width;
}

int View::get_view_height()
{
    int view_height = 0;
    if (_session.get_device()->dev_inst()->mode == DSO) {
        const vector< boost::shared_ptr<Signal> > sigs(_session.get_signals());
        BOOST_FOREACH(const boost::shared_ptr<Signal> s, sigs) {
            view_height = max(view_height, s->get_view_rect().height());
        }
    } else {
        view_height = _viewcenter->height();
    }

    return view_height;
}

double View::get_min_offset()
{
    return -(_scale * (get_view_width() * (1 - MaxViewRate)));
}

double View::get_max_offset()
{
    return _session.get_device()->get_sample_time()
            - _scale * (get_view_width() * MaxViewRate);
}

// -- calibration dialog
void View::show_calibration()
{
    _cali->set_device(_session.get_device());
    _cali->show();
}

void View::hide_calibration()
{
    _cali->hide();
}

void View::update_calibration()
{
    if (_cali->isVisible()) {
        _cali->set_device(_session.get_device());
    }
}

void View::show_region(uint64_t start, uint64_t end)
{
    assert(start <= end);
    const double ideal_scale = (end-start) * 2.0 / _session.cur_samplerate() / get_view_width();
    const double new_scale = max(min(ideal_scale, _maxscale), _minscale);
    const double new_off = (start + end)  * 0.5 / _session.cur_samplerate() - new_scale * get_view_width() / 2;
    set_scale_offset(new_scale, new_off);
}

void View::viewport_update()
{
    _viewcenter->update();
    BOOST_FOREACH(Viewport *viewport, _viewport_list)
        viewport->update();
}

void View::splitterMoved(int pos, int index)
{
    (void)pos;
    (void)index;
    signals_changed();
}

void View::reload()
{
    show_trig_cursor(false);

    /*
     * if headerwidth not change, viewport height will not be updated
     * lead to a wrong signal height
     */
    if (_session.get_device()->dev_inst()->mode == LOGIC)
        _viewbottom->setFixedHeight(StatusHeight);
    else
        _viewbottom->setFixedHeight(10);


}

} // namespace view
} // namespace pv
