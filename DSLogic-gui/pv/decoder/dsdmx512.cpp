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


#include "dsdmx512.h"

#include <math.h>

using namespace boost;
using namespace std;

namespace pv {
namespace decoder {

const QColor dsDmx512::ColorTable[TableSize] = {
    QColor(255, 255, 255, 150),
    QColor(0, 255, 0, 150),
    QColor(255, 0, 0, 150),
    QColor(0, 255, 0, 150),
    QColor(255, 0, 0, 150),
    QColor(0, 0, 255, 150),
    QColor(0, 0, 255, 150),
    QColor(0, 255, 255, 150),
};

const QString dsDmx512::StateTable[TableSize] = {
    "UNKNOWN",
    "BREAK",
    "MAB",
    "START",
    "START CODE",
    "STOP",
    "MARK",
    "SLOT"
};

dsDmx512::dsDmx512(boost::shared_ptr<data::Logic> data, std::list <int > _sel_probes, QMap<QString, QVariant> &_options, QMap<QString, int> _options_index) :
    Decoder(data, _sel_probes, _options_index)
{
    (void)_options;

    assert(_sel_probes.size() == 1);

    _dmx_index = _sel_probes.front();
}

dsDmx512::~dsDmx512()
{
}

QString dsDmx512::get_decode_name()
{
    return "DMX512";
}

void dsDmx512::recode(std::list <int > _sel_probes, QMap <QString, QVariant>& _options, QMap<QString, int> _options_index)
{
    (void)_options;

    assert(_sel_probes.size() == 1);
    _dmx_index = _sel_probes.front();

    this->_sel_probes = _sel_probes;
    this->_options_index = _options_index;

    decode();
}

void dsDmx512::decode()
{
    assert(_data);
    _max_width = 0;
    uint8_t cur_state = Unknown;

    const deque< boost::shared_ptr<pv::data::LogicSnapshot> > &snapshots =
        _data->get_snapshots();
    if (snapshots.empty())
        return;

    const boost::shared_ptr<pv::data::LogicSnapshot> &snapshot =
        snapshots.front();

    //uint64_t flag_index;
    uint64_t start_index;
    uint64_t stop_index;
    //bool edge;
    uint64_t left = 0;
    uint64_t right = snapshot->get_sample_count() - 1;
    const uint64_t samplerate = _data->get_samplerate();
    double pulse_width;
    bool valid;

    if (!_state_index.empty())
        _state_index.clear();

    while(1)
    {
        // search Break flag

        pulse_width = get_next_pulse_width(0, samplerate, left, right, stop_index, snapshot);
        start_index = left;
        if (pulse_width >= 0.088 && pulse_width <= 1000) { // Break
            cur_state = Break;
            _state_index.push_back(std::make_pair(std::make_pair(start_index, stop_index - start_index), std::make_pair(cur_state, 0)));
            _max_width = max(_max_width, stop_index - start_index);
        } else if (pulse_width == 0){
            break;
        } else {
            continue;
        }

        if (cur_state == Break) {
            pulse_width = get_next_pulse_width(1, samplerate, left, right, stop_index, snapshot);
            start_index = left;
            if (pulse_width >= 0.012 && pulse_width <= 1000) { // Marker After Break
                cur_state = Mab;
                _state_index.push_back(std::make_pair(std::make_pair(start_index, stop_index - start_index), std::make_pair(cur_state, 0)));
                _max_width = max(_max_width, stop_index - start_index);
            } else {
                continue;
            }
        }

        if (cur_state == Mab) {
            get_next_data(true, valid, cur_state, samplerate, left, right, snapshot);
        }

        while(valid)
            get_next_data(false, valid, cur_state, samplerate, left, right, snapshot);
    }

}

double dsDmx512::get_next_pulse_width(bool level, uint64_t samplerate, uint64_t &left, uint64_t right, uint64_t &end,
                                     const boost::shared_ptr<data::LogicSnapshot> &snapshot)
{
    double pulse_width = 0;
    uint64_t flag_index1;
    uint64_t flag_index2;
    bool edge1;
    bool edge2;

    if (snapshot->get_first_edge(flag_index1, edge1, left, right, _dmx_index, level, _dmx_index, -1) == SR_OK) {
        left = flag_index1;
        if (snapshot->get_first_edge(flag_index2, edge2, left, right, _dmx_index, !level, _dmx_index, -1) == SR_OK) {
            end = flag_index2;
            pulse_width = (flag_index2 - flag_index1) * 1000.0f / samplerate;
        }
    }

    return pulse_width;
}

uint8_t dsDmx512::get_next_data(bool code, bool &valid, uint8_t &cur_state,
                      uint64_t samplerate, uint64_t &left, uint64_t right,
                      const boost::shared_ptr<data::LogicSnapshot> &snapshot)
{
    uint8_t data = 0;
    double pulse_width;
    uint64_t org_left = left;
    //int i;
    uint64_t samplesPerBit = ceil(samplerate * 0.001 * 0.004);
    const uint64_t dmx_mask = 1ULL << _dmx_index;
    bool edge;
    uint64_t start = left;
    uint64_t end = left;
    const uint8_t *src_ptr;
    const int unit_size = snapshot->get_unit_size();

    valid = false;
    pulse_width = get_next_pulse_width(0, samplerate, left, right, end, snapshot);
    if (pulse_width >= 0.00392 && pulse_width <= 0.00408 * 9) { // Start bit
        org_left = left;
        start = left;
        end = start + samplesPerBit;
        cur_state = Start;
        _state_index.push_back(std::make_pair(std::make_pair(start, end - start), std::make_pair(cur_state, 0)));
        _max_width = max(_max_width, end - start);
    } else {
        left = org_left;
        return data;
    }

    if (cur_state == Start) {
        src_ptr = (uint8_t*)snapshot->get_data();
        start = end;
        data = data + (((*(uint64_t*)(src_ptr + (int)(start + samplesPerBit * 0.5) * unit_size) & dmx_mask) != 0) << 0);
        data = data + (((*(uint64_t*)(src_ptr + (int)(start + samplesPerBit * 1.5) * unit_size) & dmx_mask) != 0) << 1);
        data = data + (((*(uint64_t*)(src_ptr + (int)(start + samplesPerBit * 2.5) * unit_size) & dmx_mask) != 0) << 2);
        data = data + (((*(uint64_t*)(src_ptr + (int)(start + samplesPerBit * 3.5) * unit_size) & dmx_mask) != 0) << 3);
        data = data + (((*(uint64_t*)(src_ptr + (int)(start + samplesPerBit * 4.5) * unit_size) & dmx_mask) != 0) << 4);
        data = data + (((*(uint64_t*)(src_ptr + (int)(start + samplesPerBit * 5.5) * unit_size) & dmx_mask) != 0) << 5);
        data = data + (((*(uint64_t*)(src_ptr + (int)(start + samplesPerBit * 6.5) * unit_size) & dmx_mask) != 0) << 6);
        data = data + (((*(uint64_t*)(src_ptr + (int)(start + samplesPerBit * 7.5) * unit_size) & dmx_mask) != 0) << 7);
        end = start + samplesPerBit * 8;
        cur_state = code ? Scode : Slot;
        _state_index.push_back(std::make_pair(std::make_pair(start, end - start), std::make_pair(cur_state, data)));
        _max_width = max(_max_width, end - start);
    }

    org_left = end;
    left = start + samplesPerBit * 8.5;
    if ((*(uint64_t*)(src_ptr + left * unit_size) & dmx_mask) != 0) {
        if (snapshot->get_first_edge(end, edge, left, right, _dmx_index, 0, _dmx_index, -1) == SR_OK) {
            pulse_width = (end - org_left) * 1000.0f / samplerate;
            if (pulse_width >= 0.008 && pulse_width <= 1000) {
                start = org_left;
                end = start + samplesPerBit * 2;
                cur_state = Stop;
                _state_index.push_back(std::make_pair(std::make_pair(start, end - start), std::make_pair(cur_state, 0)));
                _max_width = max(_max_width, end - start);
            }
        } else {
            left = org_left;
            return data;
        }
    } else {
        return data;
    }

    valid = true;
    return data;
}

void dsDmx512::fill_color_table(std::vector <QColor>& _color_table)
{
    int i;
    for(i = 0; i < TableSize; i++)
        _color_table.push_back(ColorTable[i]);
}

void dsDmx512::fill_state_table(std::vector <QString>& _state_table)
{
    int i;
    for(i = 0; i < TableSize; i++)
        _state_table.push_back(StateTable[i]);
}

void dsDmx512::get_subsampled_states(std::vector<struct ds_view_state> &states,
                           uint64_t start, uint64_t end,
                           float min_length)
{
    ds_view_state view_state;

    const deque< boost::shared_ptr<pv::data::LogicSnapshot> > &snapshots =
        _data->get_snapshots();
    if (snapshots.empty())
        return;
    const boost::shared_ptr<pv::data::LogicSnapshot> &snapshot =
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
            view_state.type = (_state_index.at(i).second.first == Slot) ? DEC_CNT :
                              (_state_index.at(i).second.first == Scode) ? DEC_DATA : DEC_CMD;
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
