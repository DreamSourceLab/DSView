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


#include <extdef.h>

#include <QDebug>

#include <math.h>

#include "logicsignal.h"
#include "view.h"
#include "pv/data/logic.h"
#include "pv/data/logicsnapshot.h"
#include "view.h"

using namespace boost;
using namespace std;

namespace pv {
namespace view {

//const float LogicSignal::Oversampling = 2.0f;
const float LogicSignal::Oversampling = 1.0f;

const QColor LogicSignal::EdgeColour(0x80, 0x80, 0x80);
const QColor LogicSignal::HighColour(0x00, 0xC0, 0x00);
const QColor LogicSignal::LowColour(0xC0, 0x00, 0x00);

const QColor LogicSignal::DEFAULT_COLOR = QColor(150, 150, 150, 255);

const int LogicSignal::StateHeight = 12;
const int LogicSignal::StateRound = 5;

LogicSignal::LogicSignal(boost::shared_ptr<pv::device::DevInst> dev_inst,
                         boost::shared_ptr<data::Logic> data,
                         sr_channel *probe) :
    Signal(dev_inst, probe),
    _data(data),
    _trig(NONTRIG)
{
    //_colour = PROBE_COLORS[probe->index % countof(PROBE_COLORS)];
    _colour = DEFAULT_COLOR;
}

LogicSignal::LogicSignal(boost::shared_ptr<view::LogicSignal> s,
                         boost::shared_ptr<pv::data::Logic> data,
                         sr_channel *probe) :
    Signal(*s.get(), probe),
    _data(data),
    _trig(s->get_trig())
{
}

LogicSignal::~LogicSignal()
{
    _cur_edges.clear();
}

const sr_channel* LogicSignal::probe() const
{
    return _probe;
}

boost::shared_ptr<pv::data::SignalData> LogicSignal::data() const
{
    return _data;
}

boost::shared_ptr<pv::data::Logic> LogicSignal::logic_data() const
{
    return _data;
}

LogicSignal::LogicSetRegions LogicSignal::get_trig() const
{
    return _trig;
}

void LogicSignal::set_trig(int trig)
{
    if (trig > NONTRIG && trig <= EDGTRIG)
        _trig = (LogicSetRegions)trig;
    else
        _trig = NONTRIG;
}

void LogicSignal::commit_trig()
{

    if (_trig == NONTRIG)
        ds_trigger_probe_set(_index_list.front(), 'X', 'X');
    else {
        ds_trigger_set_en(true);
        if (_trig == POSTRIG)
            ds_trigger_probe_set(_index_list.front(), 'R', 'X');
        else if (_trig == HIGTRIG)
            ds_trigger_probe_set(_index_list.front(), '1', 'X');
        else if (_trig == NEGTRIG)
            ds_trigger_probe_set(_index_list.front(), 'F', 'X');
        else if (_trig == LOWTRIG)
            ds_trigger_probe_set(_index_list.front(), '0', 'X');
        else if (_trig == EDGTRIG)
            ds_trigger_probe_set(_index_list.front(), 'C', 'X');
    }
}

void LogicSignal::paint_mid(QPainter &p, int left, int right)
{
	using pv::view::View;

	QLineF *line;

	assert(_data);
    assert(_view);
	assert(right >= left);

    const int y = get_y() + _totalHeight * 0.5;
    const double scale = _view->scale();
    assert(scale > 0);
    const double offset = _view->offset();

    const float high_offset = y - _totalHeight + 0.5f;
	const float low_offset = y + 0.5f;

	const deque< boost::shared_ptr<pv::data::LogicSnapshot> > &snapshots =
		_data->get_snapshots();
    double samplerate = _data->samplerate();
    if (snapshots.empty() || samplerate == 0)
		return;

	const boost::shared_ptr<pv::data::LogicSnapshot> &snapshot =
		snapshots.front();
    if (snapshot->empty())
        return;

	const double pixels_offset = offset / scale;
	const double start_time = _data->get_start_time();
    const int64_t last_sample =  snapshot->get_sample_count() - 1;
	const double samples_per_pixel = samplerate * scale;
	const double start = samplerate * (offset - start_time);
	const double end = start + samples_per_pixel * (right - left);

    snapshot->get_subsampled_edges(_cur_edges,
		min(max((int64_t)floor(start), (int64_t)0), last_sample),
		min(max((int64_t)ceil(end), (int64_t)0), last_sample),
        samples_per_pixel / Oversampling, _probe->index);
    if (_cur_edges.size() < 2)
        return;

    // Paint the edges
    const unsigned int edge_count = 2 * _cur_edges.size() - 3;
    QLineF *const edge_lines = new QLineF[edge_count];
    line = edge_lines;

    double preX = ((*(_cur_edges.begin())).first / samples_per_pixel - pixels_offset) + left;
    double preY = (*(_cur_edges.begin())).second ? high_offset : low_offset;
    vector<pv::data::LogicSnapshot::EdgePair>::const_iterator i;
    for ( i = _cur_edges.begin() + 1; i != _cur_edges.end() - 1; i++) {
        const double x = ((*i).first / samples_per_pixel -
            pixels_offset) + left;
        const double y = (*i).second ? high_offset : low_offset;
        *line++ = QLineF(preX, preY, x, preY);
        *line++ = QLineF(x, high_offset, x, low_offset);
        preX = x;
        preY = y;
    }
    const double x = ((*i).first / samples_per_pixel -
            pixels_offset) + left;
    *line++ = QLineF(preX, preY, x, preY);

    p.setPen(_colour);
    p.drawLines(edge_lines, edge_count);
    delete[] edge_lines;
}

void LogicSignal::paint_caps(QPainter &p, QLineF *const lines,
    vector< pair<uint64_t, bool> > &edges, bool level,
	double samples_per_pixel, double pixels_offset, float x_offset,
	float y_offset)
{
	QLineF *line = lines;

    uint64_t curX = 0;
    uint64_t nxtX = 0;
	for (vector<pv::data::LogicSnapshot::EdgePair>::const_iterator i =
		edges.begin(); i != (edges.end() - 1); i++)
		if ((*i).second == level) {
            curX = ((*i).first / samples_per_pixel -
                    pixels_offset) + x_offset;
            nxtX = ((*(i+1)).first / samples_per_pixel -
                    pixels_offset) + x_offset;
            if (nxtX > curX)
                *line++ = QLineF(curX, y_offset, nxtX, y_offset);
		}

	p.drawLines(lines, line - lines);
}

const std::vector< std::pair<uint64_t, bool> > LogicSignal::cur_edges() const
{
    return _cur_edges;
}

void LogicSignal::paint_type_options(QPainter &p, int right, const QPoint pt)
{
    int y = get_y();
    const QRectF posTrig_rect  = get_rect(POSTRIG,  y, right);
    const QRectF higTrig_rect  = get_rect(HIGTRIG,  y, right);
    const QRectF negTrig_rect  = get_rect(NEGTRIG,  y, right);
    const QRectF lowTrig_rect  = get_rect(LOWTRIG,  y, right);
    const QRectF edgeTrig_rect = get_rect(EDGTRIG, y, right);

    p.setPen(Qt::NoPen);
    p.setBrush(posTrig_rect.contains(pt) ? dsBlue.lighter() :
               (_trig == POSTRIG) ? dsBlue : DARK_BACK);
    p.drawRect(posTrig_rect);
    p.setBrush(higTrig_rect.contains(pt) ? dsBlue.lighter() :
               (_trig == HIGTRIG) ? dsBlue : DARK_BACK);
    p.drawRect(higTrig_rect);
    p.setBrush(negTrig_rect.contains(pt) ? dsBlue.lighter() :
               (_trig == NEGTRIG) ? dsBlue : DARK_BACK);
    p.drawRect(negTrig_rect);
    p.setBrush(lowTrig_rect.contains(pt) ? dsBlue.lighter() :
               (_trig == LOWTRIG) ? dsBlue : DARK_BACK);
    p.drawRect(lowTrig_rect);
    p.setBrush(edgeTrig_rect.contains(pt) ? dsBlue.lighter() :
               (_trig == EDGTRIG) ? dsBlue : DARK_BACK);
    p.drawRect(edgeTrig_rect);

    p.setPen(QPen(DARK_FORE, 1, Qt::DashLine));
    p.setBrush(Qt::transparent);
//    p.drawLine(posTrig_rect.right(), posTrig_rect.top(),
//               posTrig_rect.right(), posTrig_rect.bottom());
//    p.drawLine(higTrig_rect.right(), higTrig_rect.top(),
//               higTrig_rect.right(), higTrig_rect.bottom());
//    p.drawLine(negTrig_rect.right(), negTrig_rect.top(),
//               negTrig_rect.right(), negTrig_rect.bottom());
//    p.drawLine(lowTrig_rect.right(), lowTrig_rect.top(),
//               lowTrig_rect.right(), lowTrig_rect.bottom());
    p.drawLine(posTrig_rect.left(), posTrig_rect.bottom(),
               edgeTrig_rect.right(), edgeTrig_rect.bottom());

    p.setPen(QPen(DARK_FORE, 2, Qt::SolidLine));
    p.setBrush(Qt::transparent);
    p.drawLine(posTrig_rect.left() + 5, posTrig_rect.bottom() - 5,
               posTrig_rect.center().x(), posTrig_rect.bottom() - 5);
    p.drawLine(posTrig_rect.center().x(), posTrig_rect.bottom() - 5,
               posTrig_rect.center().x(), posTrig_rect.top() + 5);
    p.drawLine(posTrig_rect.center().x(), posTrig_rect.top() + 5,
               posTrig_rect.right() - 5, posTrig_rect.top() + 5);

    p.drawLine(higTrig_rect.left() + 5, higTrig_rect.top() + 5,
               higTrig_rect.right() - 5, higTrig_rect.top() + 5);

    p.drawLine(negTrig_rect.left() + 5, negTrig_rect.top() + 5,
               negTrig_rect.center().x(), negTrig_rect.top() + 5);
    p.drawLine(negTrig_rect.center().x(), negTrig_rect.top() + 5,
               negTrig_rect.center().x(), negTrig_rect.bottom() - 5);
    p.drawLine(negTrig_rect.center().x(), negTrig_rect.bottom() - 5,
               negTrig_rect.right() - 5, negTrig_rect.bottom() - 5);

    p.drawLine(lowTrig_rect.left() + 5, lowTrig_rect.bottom() - 5,
               lowTrig_rect.right() - 5, lowTrig_rect.bottom() - 5);

    p.drawLine(edgeTrig_rect.left() + 5, edgeTrig_rect.top() + 5,
               edgeTrig_rect.center().x() - 2, edgeTrig_rect.top() + 5);
    p.drawLine(edgeTrig_rect.center().x() + 2 , edgeTrig_rect.top() + 5,
               edgeTrig_rect.right() - 5, edgeTrig_rect.top() + 5);
    p.drawLine(edgeTrig_rect.center().x(), edgeTrig_rect.top() + 7,
               edgeTrig_rect.center().x(), edgeTrig_rect.bottom() - 7);
    p.drawLine(edgeTrig_rect.left() + 5, edgeTrig_rect.bottom() - 5,
               edgeTrig_rect.center().x() - 2, edgeTrig_rect.bottom() - 5);
    p.drawLine(edgeTrig_rect.center().x() + 2, edgeTrig_rect.bottom() - 5,
               edgeTrig_rect.right() - 5, edgeTrig_rect.bottom() - 5);
}

bool LogicSignal::measure(const QPointF &p, uint64_t &index0, uint64_t &index1, uint64_t &index2) const
{
    const float gap = abs(p.y() - get_y());
    if (gap < get_totalHeight() * 0.5) {
        const deque< boost::shared_ptr<pv::data::LogicSnapshot> > &snapshots =
            _data->get_snapshots();
        if (snapshots.empty())
            return false;

        const boost::shared_ptr<pv::data::LogicSnapshot> &snapshot =
            snapshots.front();
        if (snapshot->empty())
            return false;

        uint64_t index = _data->samplerate() * (_view->offset() - _data->get_start_time() + p.x() * _view->scale());
        if (index == 0 || index >= (snapshot->get_sample_count() - 1))
            return false;

        const uint64_t sig_mask = 1ULL << get_index();
        bool sample = snapshot->get_sample(index) & sig_mask;
        index--;
        if (!snapshot->get_pre_edge(index, sample, 1, get_index()))
            return false;

        index0 = index;
        sample = snapshot->get_sample(index) & sig_mask;
        index++;
        if (!snapshot->get_nxt_edge(index, sample, snapshot->get_sample_count(), 1, get_index()))
            return false;

        index1 = index;
        sample = snapshot->get_sample(index) & sig_mask;
        index++;
        if (!snapshot->get_nxt_edge(index, sample, snapshot->get_sample_count(), 1, get_index()))
            index2 = 0;
        else
            index2 = index;

        return true;
    }
    return false;
}


bool LogicSignal::edges(const QPointF &p, uint64_t start, uint64_t &rising, uint64_t &falling) const
{
    uint64_t index, end;
    const float gap = abs(p.y() - get_y());
    if (gap < get_totalHeight() * 0.5) {
        const deque< boost::shared_ptr<pv::data::LogicSnapshot> > &snapshots =
            _data->get_snapshots();
        if (snapshots.empty())
            return false;

        const boost::shared_ptr<pv::data::LogicSnapshot> &snapshot =
            snapshots.front();
        if (snapshot->empty())
            return false;

        end = _data->samplerate() * (_view->offset() - _data->get_start_time() + p.x() * _view->scale());
        index = min(start, end);
        end = max(start, end);
        start = index;
        if (end > (snapshot->get_sample_count() - 1))
            return false;

        const uint64_t sig_mask = 1ULL << get_index();
        bool sample = snapshot->get_sample(start) & sig_mask;

        rising = 0;
        falling = 0;
        do {
            if (snapshot->get_nxt_edge(index, sample, snapshot->get_sample_count(), 1, get_index())) {
                if (index > end)
                    break;
                rising += !sample;
                falling += sample;
                sample = !sample;
            } else {
                break;
            }
        } while(index <= end);
        return true;
    }
    return false;
}

bool LogicSignal::mouse_press(int right, const QPoint pt)
{
    int y = get_y();
    const QRectF posTrig = get_rect(POSTRIG, y, right);
    const QRectF higTrig = get_rect(HIGTRIG, y, right);
    const QRectF negTrig = get_rect(NEGTRIG, y, right);
    const QRectF lowTrig = get_rect(LOWTRIG, y, right);
    const QRectF edgeTrig = get_rect(EDGTRIG, y, right);

    if (posTrig.contains(pt))
        set_trig((_trig == POSTRIG) ? NONTRIG : POSTRIG);
    else if (higTrig.contains(pt))
        set_trig((_trig == HIGTRIG) ? NONTRIG : HIGTRIG);
    else if (negTrig.contains(pt))
        set_trig((_trig == NEGTRIG) ? NONTRIG : NEGTRIG);
    else if (lowTrig.contains(pt))
        set_trig((_trig == LOWTRIG) ? NONTRIG : LOWTRIG);
    else if (edgeTrig.contains(pt))
        set_trig((_trig == EDGTRIG) ? NONTRIG : EDGTRIG);
    else
        return false;

    return true;
}

QRectF LogicSignal::get_rect(LogicSetRegions type, int y, int right)
{
    const QSizeF name_size(right - get_leftWidth() - get_rightWidth(), SquareWidth);

    if (type == POSTRIG)
        return QRectF(
            get_leftWidth() + name_size.width() + Margin,
            y - SquareWidth / 2,
            SquareWidth, SquareWidth);
    else if (type == HIGTRIG)
        return QRectF(
            get_leftWidth() + name_size.width() + SquareWidth + Margin,
            y - SquareWidth / 2,
            SquareWidth, SquareWidth);
    else if (type == NEGTRIG)
        return QRectF(
            get_leftWidth() + name_size.width() + 2 * SquareWidth + Margin,
            y - SquareWidth / 2,
            SquareWidth, SquareWidth);
    else if (type == LOWTRIG)
        return QRectF(
            get_leftWidth() + name_size.width() + 3 * SquareWidth + Margin,
            y - SquareWidth / 2,
            SquareWidth, SquareWidth);
    else if (type == EDGTRIG)
        return QRectF(
            get_leftWidth() + name_size.width() + 4 * SquareWidth + Margin,
            y - SquareWidth / 2,
            SquareWidth, SquareWidth);
    else
        return QRectF(0, 0, 0, 0);
}

} // namespace view
} // namespace pv
