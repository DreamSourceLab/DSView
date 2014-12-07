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


#include <extdef.h>

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

const QColor LogicSignal::SignalColours[8] = {
    QColor(0x16, 0x19, 0x1A),	// Black
    QColor(0x8F, 0x52, 0x02),	// Brown
    QColor(0xCC, 0x00, 0x00),	// Red
    QColor(0xF5, 0x79, 0x00),	// Orange
    QColor(0xED, 0xD4, 0x00),	// Yellow
    QColor(0x73, 0xD2, 0x16),	// Green
    QColor(0x34, 0x65, 0xA4),	// Blue
    QColor(0x75, 0x50, 0x7B),	// Violet
//    QColor(17, 133, 209),
//    QColor(17, 133, 209),
//    QColor(17, 133, 209),
//    QColor(17, 133, 209),
//    QColor(17, 133, 209),
//    QColor(17, 133, 209),
//    QColor(17, 133, 209),
//    QColor(17, 133, 209),
};

const int LogicSignal::StateHeight = 12;
const int LogicSignal::StateRound = 5;

LogicSignal::LogicSignal(boost::shared_ptr<pv::device::DevInst> dev_inst,
                         boost::shared_ptr<data::Logic> data,
                         const sr_channel * const probe) :
    Signal(dev_inst, probe, DS_LOGIC),
    _data(data)
{
    assert(probe->index >= 0);
    _colour = SignalColours[probe->index % countof(SignalColours)];
}

LogicSignal::~LogicSignal()
{
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

void LogicSignal::paint_mid(QPainter &p, int left, int right)
{
	using pv::view::View;

	QLineF *line;

	assert(_data);
    assert(_view);
	assert(right >= left);

    const int y = get_y() + _signalHeight * 0.5;
    const double scale = _view->scale();
    assert(scale > 0);
    const double offset = _view->offset();

    const float high_offset = y - _signalHeight + 0.5f;
	const float low_offset = y + 0.5f;

	const deque< boost::shared_ptr<pv::data::LogicSnapshot> > &snapshots =
		_data->get_snapshots();
	if (snapshots.empty())
		return;

	const boost::shared_ptr<pv::data::LogicSnapshot> &snapshot =
		snapshots.front();
    if (snapshot->buf_null())
        return;

    double samplerate = _data->samplerate();

	// Show sample rate as 1Hz when it is unknown
	if (samplerate == 0.0)
		samplerate = 1.0;

	const double pixels_offset = offset / scale;
	const double start_time = _data->get_start_time();
    const int64_t last_sample = snapshot->get_sample_count() - 1;
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

void LogicSignal::paint_type_options(QPainter &p, int right, bool hover, int action)
{
    int y = get_y();
    const QRectF posTrig_rect  = get_rect("posTrig",  y, right);
    const QRectF higTrig_rect  = get_rect("higTrig",  y, right);
    const QRectF negTrig_rect  = get_rect("negTrig",  y, right);
    const QRectF lowTrig_rect  = get_rect("lowTrig",  y, right);
    const QRectF edgeTrig_rect = get_rect("edgeTrig", y, right);

    p.setPen(Qt::transparent);
    p.setBrush(((hover && action == POSTRIG) || (_trig == POSTRIG)) ?
                   dsYellow :
                   dsBlue);
    p.drawRect(posTrig_rect);
    p.setBrush(((hover && action == HIGTRIG) || (_trig == HIGTRIG)) ?
                   dsYellow :
                   dsBlue);
    p.drawRect(higTrig_rect);
    p.setBrush(((hover && action == NEGTRIG) || (_trig == NEGTRIG)) ?
                   dsYellow :
                   dsBlue);
    p.drawRect(negTrig_rect);
    p.setBrush(((hover && action == LOWTRIG) || (_trig == LOWTRIG)) ?
                   dsYellow :
                   dsBlue);
    p.drawRect(lowTrig_rect);
    p.setBrush(((hover && action == EDGETRIG) || (_trig == EDGETRIG)) ?
                   dsYellow :
                   dsBlue);
    p.drawRect(edgeTrig_rect);

    p.setPen(QPen(Qt::blue, 1, Qt::DotLine));
    p.setBrush(Qt::transparent);
    p.drawLine(posTrig_rect.right(), posTrig_rect.top() + 3,
               posTrig_rect.right(), posTrig_rect.bottom() - 3);
    p.drawLine(higTrig_rect.right(), higTrig_rect.top() + 3,
               higTrig_rect.right(), higTrig_rect.bottom() - 3);
    p.drawLine(negTrig_rect.right(), negTrig_rect.top() + 3,
               negTrig_rect.right(), negTrig_rect.bottom() - 3);
    p.drawLine(lowTrig_rect.right(), lowTrig_rect.top() + 3,
               lowTrig_rect.right(), lowTrig_rect.bottom() - 3);

    p.setPen(QPen(Qt::white, 2, Qt::SolidLine));
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

} // namespace view
} // namespace pv
