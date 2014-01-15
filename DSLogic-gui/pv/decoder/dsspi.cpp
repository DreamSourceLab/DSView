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


#include "dsspi.h"

#include <math.h>

using namespace boost;
using namespace std;

namespace pv {
namespace decoder {

const QColor dsSpi::ColorTable[TableSize] = {
    QColor(255, 255, 255, 150),
    QColor(255, 0, 0, 150),
    QColor(255, 0, 0, 150),
    QColor(0, 255, 255, 150),
};

const QString dsSpi::StateTable[TableSize] = {
    "UNKNOWN",
    "CLOCK POLARITY ERROR",
    "TOO FEW BITS",
    "DATA"
};

dsSpi::dsSpi(shared_ptr<data::Logic> data, std::list <int > _sel_probes, QMap<QString, QVariant> &_options, QMap<QString, int> _options_index) :
    Decoder(data, _sel_probes, _options_index)
{
    _cpol = _options.value("cpol").toBool();
    _cpha = _options.value("cpha").toBool();
    _order = _options.value("order").toBool();
    _bits = _options.value("bits").toUInt();
    _ssn = _options.value("ssn").toInt();

    if (_ssn == -1)
        assert(_sel_probes.size() == 3);
    else
        assert(_sel_probes.size() == 4);

    if (_ssn != -1) {
        _ssn_index = _sel_probes.front();
        _sel_probes.pop_front();
    }else{
        _ssn_index = 0;
    }
    _sclk_index = _sel_probes.front();
    _sel_probes.pop_front();
    _mosi_index = _sel_probes.front();
    _sel_probes.pop_front();
    _miso_index = _sel_probes.front();
    _sel_probes.pop_front();
}

dsSpi::~dsSpi()
{
}

QString dsSpi::get_decode_name()
{
    return "SPI";
}

void dsSpi::recode(std::list <int > _sel_probes, QMap <QString, QVariant>& _options, QMap<QString, int> _options_index)
{
    _cpol = _options.value("cpol").toBool();
    _cpha = _options.value("cpha").toBool();
    _order = _options.value("order").toBool();
    _bits = _options.value("bits").toUInt();
    _ssn = _options.value("ssn").toInt();

    if (_ssn == -1)
        assert(_sel_probes.size() == 3);
    else
        assert(_sel_probes.size() == 4);

    if (_ssn != -1) {
        _ssn_index = _sel_probes.front();
        _sel_probes.pop_front();
        _sel_probes.push_back(_ssn_index);
    }else{
        _ssn_index = 0;
    }
    _sclk_index = _sel_probes.front();
    _sel_probes.pop_front();
    _sel_probes.push_back(_sclk_index);
    _mosi_index = _sel_probes.front();
    _sel_probes.pop_front();
    _sel_probes.push_back(_mosi_index);
    _miso_index = _sel_probes.front();
    _sel_probes.pop_front();
    _sel_probes.push_back(_miso_index);


    this->_sel_probes = _sel_probes;
    this->_options_index = _options_index;

    decode();
}

void dsSpi::decode()
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

    const uint64_t ssn_mask = 1ULL << _ssn_index;
    const uint64_t sclk_mask = 1ULL << _sclk_index;
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
        if (_ssn != -1) {
            if (_ssn == ((snapshot->get_sample(left) & ssn_mask) != 0)) {
                if (snapshot->get_first_edge(flag_index, edge, left, right, _ssn_index, !_ssn, _ssn_index, -1) == SR_OK) {
                    start_index = left;
                    stop_index = flag_index - 1;
                } else {
                    start_index = left;
                    stop_index = right;
                }
            } else {
                if (snapshot->get_first_edge(flag_index, edge, left, right, _ssn_index, _ssn, _ssn_index, -1) == SR_OK)
                    start_index = flag_index;
                else
                    break;

                if (snapshot->get_first_edge(flag_index, edge, left, right, _ssn_index, !_ssn, _ssn_index, -1) == SR_OK)
                    stop_index = flag_index - 1;
                else
                    stop_index = right;
            }
        } else {
            start_index = left;
            stop_index = right;
        }

        if (stop_index - start_index > 15) {
            if (_cpol == ((snapshot->get_sample(start_index) & sclk_mask) != 0)) {
                snapshot->get_edges(_cur_edges, start_index, stop_index, _sclk_index, !(_cpha ^ _cpol));
                data_decode(snapshot);
            } else {
                cur_state = ClockErr;
                _state_index.push_back(std::make_pair(std::make_pair(start_index, stop_index - start_index),
                                                      std::make_pair(cur_state, 0)));
                _max_width = max(_max_width, stop_index - start_index);
            }
        }

        left = stop_index + 1;
        if (left >= right)
            break;
    }
}

void dsSpi::data_decode(const shared_ptr<data::LogicSnapshot> &snapshot)
{
    uint8_t cur_state;
    const uint8_t *src_ptr;
    const int unit_size = snapshot->get_unit_size();

    const uint64_t miso_mask = 1ULL << _miso_index;
    const uint64_t mosi_mask = 1ULL << _mosi_index;

    uint64_t edge_size = _cur_edges.size();
    uint64_t index = 0;
    src_ptr = (uint8_t*)snapshot->get_data();

    while (edge_size >= index + _bits) {
        uint8_t mosi = 0;
        uint8_t miso = 0;
        int i;

        if (_order) {
            for (i = 0; i < _bits; i++) {
                mosi = (mosi << 1) + ((*(uint64_t*)(src_ptr + _cur_edges[index + i].first * unit_size) & mosi_mask) != 0);
                miso = (miso << 1) + ((*(uint64_t*)(src_ptr + _cur_edges[index + i].first * unit_size) & miso_mask) != 0);
            }
        } else {
            for (i = 0; i < _bits; i++) {
                mosi = mosi + (((*(uint64_t*)(src_ptr + _cur_edges[index + i].first * unit_size) & mosi_mask) != 0) << i);
                miso = miso + (((*(uint64_t*)(src_ptr + _cur_edges[index + i].first * unit_size) & miso_mask) != 0) << i);
            }
        }

        cur_state = Data;
        _state_index.push_back(std::make_pair(std::make_pair(_cur_edges.at(index).first - 1, _cur_edges.at(index + _bits - 1).first - _cur_edges.at(index).first + 2),
                                              std::make_pair(cur_state, mosi)));

        _max_width = max(_max_width, _cur_edges.at(index + _bits - 1).first - _cur_edges.at(index).first + 2);
        index += 8;
    }

    if (edge_size > index + 1 && edge_size < index + _bits) {
        cur_state = BitsErr;
        _state_index.push_back(std::make_pair(std::make_pair(_cur_edges.at(index + 1).first - 1, _cur_edges.at(_cur_edges.size() - 1).first - _cur_edges.at(index + 1).first + 2),
                                              std::make_pair(cur_state, 0)));
    }
}

void dsSpi::fill_color_table(std::vector <QColor>& _color_table)
{
    int i;
    for(i = 0; i < TableSize; i++)
        _color_table.push_back(ColorTable[i]);
}

void dsSpi::fill_state_table(std::vector <QString>& _state_table)
{
    int i;
    for(i = 0; i < TableSize; i++)
        _state_table.push_back(StateTable[i]);
}

void dsSpi::get_subsampled_states(std::vector<struct ds_view_state> &states,
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
            view_state.type = (_state_index.at(i).second.first == Data) ? DEC_DATA : DEC_CMD;
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
