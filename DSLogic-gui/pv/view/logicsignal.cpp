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

LogicSignal::LogicSignal(QString name, shared_ptr<data::Logic> data,
    int probe_index, int order) :
    Signal(name, probe_index, DS_LOGIC, order),
    _probe_index(probe_index),
    _data(data),
    _need_decode(false),
    _decoder(NULL)
{
	assert(_probe_index >= 0);
	_colour = SignalColours[_probe_index % countof(SignalColours)];
}

LogicSignal::~LogicSignal()
{
}

void LogicSignal::set_data(boost::shared_ptr<data::Logic> _logic_data,
                           boost::shared_ptr<pv::data::Analog> _analog_data,
                           boost::shared_ptr<data::Group> _group_data)
{
    (void)_analog_data;
    (void)_group_data;

    assert(_logic_data);

    if (!_cur_edges.empty())
        _cur_edges.clear();

    _data = _logic_data;
}

void LogicSignal::paint(QPainter &p, int y, int left, int right,
		double scale, double offset)
{
	using pv::view::View;

	QLineF *line;

	assert(scale > 0);
	assert(_data);
	assert(right >= left);

    const float high_offset = y - _signalHeight + 0.5f;
	const float low_offset = y + 0.5f;

	const deque< shared_ptr<pv::data::LogicSnapshot> > &snapshots =
		_data->get_snapshots();
	if (snapshots.empty())
		return;

	const shared_ptr<pv::data::LogicSnapshot> &snapshot =
		snapshots.front();

	double samplerate = _data->get_samplerate();

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
		samples_per_pixel / Oversampling, _probe_index);
    assert(_cur_edges.size() >= 2);

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

    if (_need_decode) {
        assert(_decoder);
        _decoder->get_subsampled_states(_cur_states,
                                       min(max((int64_t)floor(start), (int64_t)0), last_sample),
                                       min(max((int64_t)ceil(end), (int64_t)0), last_sample),
                                       samples_per_pixel);

        const float top_offset = y - (_signalHeight + StateHeight) / 2.0f;
        const uint64_t sig_mask = 1ULL << _probe_index;
        const uint8_t *const init_ptr = (uint8_t*)snapshot->get_data();
        const uint8_t *src_ptr;
        const int unit_size = snapshot->get_unit_size();
        uint64_t value;
        uint64_t finalValue = 0;

        if (!_cur_states.empty()) {
            _decoder->fill_color_table(_color_table);
            _decoder->fill_state_table(_state_table);

            vector<pv::decoder::ds_view_state>::const_iterator i;
            for ( i = _cur_states.begin(); i != _cur_states.end(); i++) {
                finalValue = 0;
                const uint64_t index = (*i).index;
                const uint64_t samples = (*i).samples;
                const int64_t x = (index / samples_per_pixel -
                    pixels_offset) + left;
                const int64_t width = samples / samples_per_pixel;

                if ((*i).type == decoder::DEC_DATA) {
                    src_ptr = init_ptr + index * unit_size;
                    for (uint64_t j = 0; j < samples; j++) {
                        value = (*(uint64_t*)src_ptr & sig_mask);
                        if (_probe_index - j > 0)
                            value = value >> (_probe_index - j);
                        else
                            value = value << (j - _probe_index);
                        finalValue |= value;
                        src_ptr += unit_size;
                    }
                }

                p.setBrush(_color_table.at((*i).state));
                const QRectF state_rect = QRectF(x, top_offset, width, StateHeight);
                p.drawRoundedRect(state_rect, StateRound, StateRound);
                p.setPen(Qt::black);
                if ((*i).type == decoder::DEC_CMD)
                    p.drawText(state_rect, Qt::AlignCenter | Qt::AlignVCenter,
                               _state_table.at((*i).state));
                else if ((*i).type == decoder::DEC_DATA)
                    p.drawText(state_rect, Qt::AlignCenter | Qt::AlignVCenter,
                               _state_table.at((*i).state) + "0x" + QString::number(finalValue, 16).toUpper());
            }
        }
    }
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

void LogicSignal::set_decoder(pv::decoder::Decoder *decoder)
{
    assert(decoder);
    _need_decode = true;
    _decoder = decoder;
}

decoder::Decoder *LogicSignal::get_decoder()
{
    return _decoder;
}

void LogicSignal::del_decoder()
{
    _need_decode = false;
    _decoder = NULL;
}

} // namespace view
} // namespace pv
