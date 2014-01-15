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

#include "dsi2c.h"

#include <math.h>

using namespace boost;
using namespace std;

namespace pv {
namespace decoder {

const QColor dsI2c::ColorTable[TableSize] = {
    QColor(255, 255, 255, 150),
    QColor(0, 255, 0, 150),
    QColor(255, 0, 0, 150),
    QColor(0, 255, 0, 150),
    QColor(255, 0, 0, 150),
    QColor(0, 0, 255, 150),
    QColor(0, 0, 255, 150),
    QColor(0, 255, 255, 150),
};

const QString dsI2c::StateTable[TableSize] = {
    "UNKNOWN",
    "START",
    "STOP",
    "ACK",
    "NAK",
    "READ @",
    "WRITE @",
    "DATA"
};

dsI2c::dsI2c(shared_ptr<data::Logic> data, std::list <int > _sel_probes, QMap<QString, QVariant> &_options, QMap<QString, int> _options_index) :
    Decoder(data, _sel_probes, _options_index)
{
    (void)_options;

    assert(_sel_probes.size() == 2);

    _scl_index = _sel_probes.front();
    _sda_index = _sel_probes.back();
}

dsI2c::~dsI2c()
{
}

QString dsI2c::get_decode_name()
{
    return "I2C";
}

void dsI2c::recode(std::list <int > _sel_probes, QMap <QString, QVariant>& _options, QMap <QString, int> _options_index)
{
    (void)_options;

    assert(_sel_probes.size() == 2);
    _scl_index = _sel_probes.front();
    _sda_index = _sel_probes.back();
    this->_sel_probes = _sel_probes;
    this->_options_index = _options_index;

    decode();
}

void dsI2c::decode()
{
    assert(_data);
    _max_width = 0;
    uint8_t cur_state = Unknown;

    const deque< shared_ptr<pv::data::LogicSnapshot> > &snapshots =
        _data->get_snapshots();
    if (snapshots.empty())
        return;

    const shared_ptr<pv::data::LogicSnapshot> &snapshot =
        snapshots.front();

    uint64_t flag_index;
    uint64_t start_index;
    uint64_t stop_index;
    bool edge;
    uint64_t left = 0;
    uint64_t right = snapshot->get_sample_count() - 1;

    if (!_state_index.empty())
        _state_index.clear();

    while(1)
    {
        // search start flag
        if (snapshot->get_first_edge(flag_index, edge, left, right, _sda_index, -1, _scl_index, 1) == SR_OK) {
            left = flag_index + 1;

            if (cur_state == Start) {
                stop_index = flag_index;
                snapshot->get_edges(_cur_edges, start_index, stop_index, _scl_index, 1);
                cmd_decode(snapshot);
                data_decode(snapshot);
                _cur_edges.clear();
            }

            if (edge == false) {
                cur_state = Start;
                _state_index.push_back(std::make_pair(std::make_pair(flag_index - 1, 2), std::make_pair(cur_state, 0)));
            } else {
                cur_state = Stop;
                _state_index.push_back(std::make_pair(std::make_pair(flag_index - 1, 2), std::make_pair(cur_state, 0)));
            }
            start_index = flag_index + 1;
            _max_width = max(_max_width, (uint64_t)2);
        } else {
            if (cur_state == Start) {
                stop_index = snapshot->get_sample_count() - 1;
                snapshot->get_edges(_cur_edges, start_index, stop_index, _scl_index, 1);
                cmd_decode(snapshot);
                data_decode(snapshot);
                _cur_edges.clear();
            }
            _cur_edges.clear();
            break;
        }
    }

}

void dsI2c::cmd_decode(const boost::shared_ptr<data::LogicSnapshot> &snapshot)
{
    uint8_t cur_state;
    const uint8_t *src_ptr;
    const int unit_size = snapshot->get_unit_size();
    //const uint8_t *const end_src_ptr = (uint8_t*)snapshot->get_data() +
    //    snapshot->get_sample_count() * unit_size;

    //const uint64_t scl_mask = 1ULL << _scl_index;
    const uint64_t sda_mask = 1ULL << _sda_index;

    if (_cur_edges.size() > 9) {
        uint8_t slave_addr = 0;
        bool read;
        bool nak;
        //int index = 0;
        src_ptr = (uint8_t*)snapshot->get_data();
        slave_addr = ((*(uint64_t*)(src_ptr + _cur_edges[0].first * unit_size) & sda_mask) != 0);
        slave_addr = (slave_addr << 1) + ((*(uint64_t*)(src_ptr + _cur_edges[1].first * unit_size) & sda_mask) != 0);
        slave_addr = (slave_addr << 1) + ((*(uint64_t*)(src_ptr + _cur_edges[2].first * unit_size) & sda_mask) != 0);
        slave_addr = (slave_addr << 1) + ((*(uint64_t*)(src_ptr + _cur_edges[3].first * unit_size) & sda_mask) != 0);
        slave_addr = (slave_addr << 1) + ((*(uint64_t*)(src_ptr + _cur_edges[4].first * unit_size) & sda_mask) != 0);
        slave_addr = (slave_addr << 1) + ((*(uint64_t*)(src_ptr + _cur_edges[5].first * unit_size) & sda_mask) != 0);
        slave_addr = (slave_addr << 1) + ((*(uint64_t*)(src_ptr + _cur_edges[6].first * unit_size) & sda_mask) != 0);
        read = ((*(uint64_t*)(src_ptr + _cur_edges[7].first * unit_size) & sda_mask) != 0);
        nak = ((*(uint64_t*)(src_ptr + _cur_edges[8].first * unit_size) & sda_mask) != 0);

        cur_state = read ? Read : Write;
        _state_index.push_back(std::make_pair(std::make_pair(_cur_edges.at(0).first - 1, _cur_edges.at(7).first - _cur_edges.at(0).first + 2),
                                              std::make_pair(cur_state, slave_addr)));
        cur_state = nak ? Nak : Ack;
        _state_index.push_back(std::make_pair(std::make_pair(_cur_edges.at(8).first - 1, 2),
                                              std::make_pair(cur_state, 0)));
        _max_width = max(_max_width, _cur_edges.at(7).first - _cur_edges.at(0).first + 2);
        //_cur_edges.erase(_cur_edges.begin(), _cur_edges.begin() + 9);
    }
}

void dsI2c::data_decode(const shared_ptr<data::LogicSnapshot> &snapshot)
{
    uint8_t cur_state;
    const uint8_t *src_ptr;
    const int unit_size = snapshot->get_unit_size();
    //const uint8_t *const end_src_ptr = (uint8_t*)snapshot->get_data() +
    //    snapshot->get_sample_count() * unit_size;

    //const uint64_t scl_mask = 1ULL << _scl_index;
    const uint64_t sda_mask = 1ULL << _sda_index;

    uint64_t edge_size = _cur_edges.size();
    uint64_t index = 9;

    while (edge_size > index + 9) {
        uint8_t data = 0;
        bool nak;

        src_ptr = (uint8_t*)snapshot->get_data();
        data = ((*(uint64_t*)(src_ptr + _cur_edges[index].first * unit_size) & sda_mask) != 0);
        data = (data << 1) + ((*(uint64_t*)(src_ptr + _cur_edges[index + 1].first * unit_size) & sda_mask) != 0);
        data = (data << 1) + ((*(uint64_t*)(src_ptr + _cur_edges[index + 2].first * unit_size) & sda_mask) != 0);
        data = (data << 1) + ((*(uint64_t*)(src_ptr + _cur_edges[index + 3].first * unit_size) & sda_mask) != 0);
        data = (data << 1) + ((*(uint64_t*)(src_ptr + _cur_edges[index + 4].first * unit_size) & sda_mask) != 0);
        data = (data << 1) + ((*(uint64_t*)(src_ptr + _cur_edges[index + 5].first * unit_size) & sda_mask) != 0);
        data = (data << 1) + ((*(uint64_t*)(src_ptr + _cur_edges[index + 6].first * unit_size) & sda_mask) != 0);
        data = (data << 1) + ((*(uint64_t*)(src_ptr + _cur_edges[index + 7].first * unit_size) & sda_mask) != 0);
        nak = ((*(uint64_t*)(src_ptr + _cur_edges[index + 8].first * unit_size) & sda_mask) != 0);

        cur_state = Data;
        _state_index.push_back(std::make_pair(std::make_pair(_cur_edges.at(index).first - 1, _cur_edges.at(index + 7).first - _cur_edges.at(index).first + 2),
                                              std::make_pair(cur_state, data)));
        cur_state = nak ? Nak : Ack;
        _state_index.push_back(std::make_pair(std::make_pair(_cur_edges.at(index + 8).first - 1, 2),
                                              std::make_pair(cur_state, 0)));
        _max_width = max(_max_width, _cur_edges.at(index + 7).first - _cur_edges.at(index).first + 2);
        //_cur_edges.erase(_cur_edges.begin(), _cur_edges.begin() + 9);
        index += 9;
    }
}

void dsI2c::fill_color_table(std::vector <QColor>& _color_table)
{
    int i;
    for(i = 0; i < TableSize; i++)
        _color_table.push_back(ColorTable[i]);
}

void dsI2c::fill_state_table(std::vector <QString>& _state_table)
{
    int i;
    for(i = 0; i < TableSize; i++)
        _state_table.push_back(StateTable[i]);
}

void dsI2c::get_subsampled_states(std::vector<struct ds_view_state> &states,
                           uint64_t start, uint64_t end,
                           float min_length)
{
    ds_view_state view_state;

    const deque< shared_ptr<pv::data::LogicSnapshot> > &snapshots =
        _data->get_snapshots();
    if (snapshots.empty())
        return;
    const shared_ptr<pv::data::LogicSnapshot> &snapshot =
        snapshots.front();

    assert(end <= snapshot->get_sample_count());
    assert(start <= end);
    assert(min_length > 0);

    if (!states.empty())
        states.clear();

    if (_state_index.empty())
        return;
    if (start > _state_index.at(_state_index.size() - 1).first.first)
        return;
    if (end < _state_index.at(0).first.first)
        return;

    if (min_length * _view_scale > _max_width) {
        view_state.index = _state_index.at(0).first.first;
        view_state.samples = _state_index.at(_state_index.size() - 1).first.first +
                             _state_index.at(_state_index.size() - 1).first.second - _state_index.at(0).first.first;
        view_state.type = DEC_NODETAIL;
        view_state.state = 0;
        view_state.data = 0;
        states.push_back(view_state);
        return;
    }

    uint64_t view_start = 0;
    uint64_t view_end = 0;
    uint64_t minIndex = 0;
    uint64_t maxIndex = _state_index.size() - 1;
    uint64_t i = start * 1.0f / snapshot->get_sample_count() * maxIndex;
    bool check_flag = false;
    int times = 0;
    while(times <= 32) {
        if (_state_index.at(i).first.first + _state_index.at(i).first.second >= start) {
            check_flag = true;
        } else {
            minIndex = i;
            i = ceil((i + maxIndex) / 2.0f);
        }

        if (check_flag) {
            if (i == 0) {
                view_start = i;
                break;
            } else if (_state_index.at(i-1).first.first  + _state_index.at(i-1).first.second < start) {
                view_start = i;
                break;
            } else {
                maxIndex = i;
                i = (i + minIndex) / 2;
            }
            check_flag = false;
        }
        times++;
    }
    i = view_start;
    check_flag = false;
    times = 0;
    minIndex = view_start;
    //maxIndex = _state_index.size() - 1;
    maxIndex = min(_state_index.size() - 1, (size_t)end);
    view_end = view_start;
    while(times <= 32) {
        if (_state_index.at(i).first.first <= end) {
            check_flag = true;
        } else {
            maxIndex = i;
            i = (i + minIndex) / 2;
        }

        if (check_flag) {
            if (i == maxIndex) {
                view_end = i;
                break;
            } else if (_state_index.at(i+1).first.first > end) {
                view_end = i;
                break;
            } else {
                minIndex = i;
                i = ceil((i + maxIndex) / 2.0f);
            }
            check_flag = false;
        }
        times++;
    }

    assert(view_end >= view_start);

    for (uint64_t i = view_start; i <= view_end; i++) {
        if (_state_index.at(i).first.second >= min_length * _view_scale) {
            view_state.index = _state_index.at(i).first.first;
            view_state.samples = _state_index.at(i).first.second;
            view_state.type = (_state_index.at(i).second.first == Read ||
                               _state_index.at(i).second.first == Write ||
                               _state_index.at(i).second.first == Data) ? DEC_DATA : DEC_CMD;
            view_state.state = _state_index.at(i).second.first;
            view_state.data = _state_index.at(i).second.second;
            states.push_back(view_state);
        } else {
            view_state.index = _state_index.at(i).first.first;
            view_state.samples = _state_index.at(i).first.second;
            view_state.type = DEC_NODETAIL;
            view_state.state = 0;
            view_state.data = 0;
            states.push_back(view_state);
        }
    }
}

} // namespace decoder
} // namespace pv
