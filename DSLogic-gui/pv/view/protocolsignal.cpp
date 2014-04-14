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

#include "protocolsignal.h"
#include "view.h"
#include "pv/data/logic.h"
#include "pv/data/logicsnapshot.h"



using namespace boost;
using namespace std;

namespace pv {
namespace view {

const int ProtocolSignal::StateHeight = 16;
const int ProtocolSignal::StateRound = 3;

ProtocolSignal::ProtocolSignal(QString name, shared_ptr<data::Logic> data,
                               pv::decoder::Decoder *decoder, std::list<int> probe_index_list, int order, int protocol_index) :
    Signal(name, probe_index_list, DS_PROTOCOL, order, protocol_index),
    _probe_index_list(probe_index_list),
    _data(data),
    _decoder(decoder)
{
    assert(_probe_index_list.size() > 0);
}

ProtocolSignal::~ProtocolSignal()
{
}

void ProtocolSignal::set_data(boost::shared_ptr<data::Logic> _logic_data, boost::shared_ptr<data::Dso> _dso_data,
                           boost::shared_ptr<pv::data::Analog> _analog_data,
                           boost::shared_ptr<data::Group> _group_data)
{
    (void)_dso_data;
    (void)_analog_data;
    (void)_group_data;

    assert(_logic_data);

    if (!_cur_states.empty())
        _cur_states.clear();

    _data = _logic_data;
    _decoder->set_data(_logic_data);
}

void ProtocolSignal::paint(QPainter &p, int y, int left, int right, double scale, double offset)
{
    using pv::view::View;

    //QLineF *line;

    assert(scale > 0);
    assert(_data);
    assert(right >= left);

    //const float top_offset = y - (_signalHeight + StateHeight) / 2.0f;
    const float high_offset = y - _signalHeight + 0.5f;
    const float middle_offset = y - _signalHeight / 2 + 0.5f;
    //const float low_offset = y + 0.5f;

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

    _decoder->get_subsampled_states(_cur_states,
                                   min(max((int64_t)floor(start), (int64_t)0), last_sample),
                                   min(max((int64_t)ceil(end), (int64_t)0), last_sample),
                                   samples_per_pixel);

    int64_t preX = left;
    if (!_cur_states.empty()) {
        _decoder->fill_color_table(_color_table);
        _decoder->fill_state_table(_state_table);

//        const unsigned int edge_count = 2 * _cur_states.size();
//        QLineF *const edge_lines = new QLineF[edge_count];
//        line = edge_lines;

        vector<pv::decoder::ds_view_state>::const_iterator i;
        const int counter_init = 1;
        int counter = counter_init;
        for ( i = _cur_states.begin(); i != _cur_states.end(); i++) {
            const uint64_t index = (*i).index;
            const uint64_t samples = (*i).samples;
            const int64_t x = (index / samples_per_pixel -
                pixels_offset) + left;
            const int64_t width = samples / samples_per_pixel;

//            *line++ = QLineF(x, high_offset, x, low_offset);
//            *line++ = QLineF(x + width, high_offset, x + width, low_offset);

            p.setPen(dsBlue);
            p.setBrush(_color_table.at((*i).state));
            const QRectF state_rect = QRectF(x, high_offset, width, _signalHeight);
            //const QRectF cmd_rect = QRectF(x + width / 2 - 5, low_offset - 12, 10, 10);
            p.drawRoundedRect(state_rect, StateRound, StateRound);
            p.setPen(Qt::black);
            if ((*i).type == decoder::DEC_CMD) {
                counter = counter_init;
                p.drawText(state_rect, Qt::AlignCenter | Qt::AlignCenter,
                           _state_table.at((*i).state));
                //p.drawRect(cmd_rect);
            } else if ((*i).type == decoder::DEC_DATA) {
                counter = counter_init;
                p.drawText(state_rect, Qt::AlignCenter | Qt::AlignCenter,
                           _state_table.at((*i).state) + " 0x" + QString::number((*i).data, 16).toUpper());
            } else if ((*i).type == decoder::DEC_CNT) {
                p.drawText(state_rect, Qt::AlignCenter | Qt::AlignCenter,
                           _state_table.at((*i).state) + QString::number(counter) + ": 0x" + QString::number((*i).data, 16).toUpper());
                counter++;
            }
            if (x > preX)
                p.drawLine(preX, middle_offset, x, middle_offset);
            preX = x + width;
        }

//        p.setPen(_colour);
//        p.drawLines(edge_lines, edge_count);
//        delete[] edge_lines;
    }
    p.drawLine(preX, middle_offset, right, middle_offset);
}

const std::vector< std::pair<uint64_t, bool> > ProtocolSignal::cur_edges() const
{

}

void ProtocolSignal::set_decoder(pv::decoder::Decoder *decoder)
{
    (void)decoder;
}

decoder::Decoder *ProtocolSignal::get_decoder()
{
    return _decoder;
}

void ProtocolSignal::del_decoder()
{
}

} // namespace view
} // namespace pv

