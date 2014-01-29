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


#include "dsserial.h"

#include <math.h>

using namespace boost;
using namespace std;

namespace pv {
namespace decoder {

const QColor dsSerial::ColorTable[TableSize] = {
    QColor(255, 255, 255, 150),
    QColor(0, 255, 0, 150),
    QColor(255, 0, 0, 150),
    QColor(0, 255, 0, 150),
    QColor(255, 0, 0, 150),
    QColor(0, 0, 255, 150),
    QColor(0, 0, 255, 150),
    QColor(0, 255, 255, 150),
};

const QString dsSerial::StateTable[TableSize] = {
    "UNKNOWN",
    "START",
    "STOP",
    "START ERROR",
    "STOP ERROR",
    "PARITY",
    "PARITY ERROR",
    "DATA"
};

dsSerial::dsSerial(shared_ptr<data::Logic> data, std::list <int > _sel_probes, QMap<QString, QVariant> &_options, QMap<QString, int> _options_index) :
    Decoder(data, _sel_probes, _options_index)
{
    assert(_sel_probes.size() == 1);

    _serial_index = _sel_probes.front();

    _baudrate = _options.value("baudrate").toULongLong();
    _stopbits = _options.value("stopbits").toFloat();
    _parity = _options.value("parity").toInt();
    _order = _options.value("order").toBool();
    _bits = _options.value("bits").toUInt();
    _idle_level = _options.value("idle").toBool();
}

dsSerial::~dsSerial()
{
}

QString dsSerial::get_decode_name()
{
    return "Async Serial";
}

void dsSerial::recode(std::list <int > _sel_probes, QMap <QString, QVariant>& _options, QMap<QString, int> _options_index)
{
    assert(_sel_probes.size() == 1);

    _serial_index = _sel_probes.front();

    _baudrate = _options.value("baudrate").toULongLong();
    _stopbits = _options.value("stopbits").toFloat();
    _parity = _options.value("parity").toInt();
    _order = _options.value("order").toBool();
    _bits = _options.value("bits").toUInt();
    _idle_level = _options.value("idle").toBool();


    this->_sel_probes = _sel_probes;
    this->_options_index = _options_index;

    decode();
}

void dsSerial::decode()
{
    assert(_data);
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
    float samplesPerBit;
    //int sampleOffset;
    int i;
    _max_width = 0;
    _min_width = right;

    int start_flag = _idle_level ? 0 : 1;
    if (_baudrate != 0)
        samplesPerBit = _data->get_samplerate() * 1.0f / _baudrate;
    else
        samplesPerBit = snapshot->get_min_pulse(left, right, _serial_index);
    //sampleOffset = samplesPerBit / 2;

    if (!_state_index.empty())
        _state_index.clear();

    while(1)
    {
        // search start flag
        bool stop_err = false;
        if (snapshot->get_first_edge(flag_index, edge, left, right, _serial_index, start_flag, _serial_index, -1) == SR_OK) {
            left = flag_index + floor((_bits + (_parity != -1) + _stopbits + 1) * samplesPerBit);

            cur_state = Start;
            _state_index.push_back(std::make_pair(std::make_pair(flag_index, samplesPerBit), std::make_pair(cur_state, 0)));
            _max_width = max(_max_width, (uint64_t)samplesPerBit);
            _min_width = min(_min_width, (uint64_t)samplesPerBit);

            start_index = flag_index + samplesPerBit * 1.5;
            stop_index = flag_index + ceil((_bits + (_parity != -1) + 1) * samplesPerBit);

            if (left < right) {
                for (i = 0; i < _stopbits * samplesPerBit; i++) {
                    if (_idle_level != ((snapshot->get_sample(stop_index + i) & 1ULL << _serial_index) != 0)) {
                        stop_err = true;
                        break;
                    }
                }
                if (stop_err) {
                    cur_state = StopErr;
                    _state_index.push_back(std::make_pair(std::make_pair(stop_index, samplesPerBit), std::make_pair(cur_state, 0)));
                } else {
                    data_decode(snapshot, start_index, stop_index, samplesPerBit);
                    cur_state = Stop;
                    _state_index.push_back(std::make_pair(std::make_pair(stop_index, samplesPerBit), std::make_pair(cur_state, 0)));
                }
            } else {
                _cur_edges.clear();
                break;
            }
        } else {
            _cur_edges.clear();
            break;
        }
    }

}

void dsSerial::data_decode(const shared_ptr<data::LogicSnapshot> &snapshot, uint64_t start, uint64_t stop, float samplesPerBit)
{
    (void)stop;

    uint8_t cur_state;
    const uint8_t *src_ptr;
    const int unit_size = snapshot->get_unit_size();

    const uint64_t serial_mask = 1ULL << _serial_index;

    uint32_t data = 0;
    uint32_t parity;
    int i;

    src_ptr = (uint8_t*)snapshot->get_data();
    if (_order) {
        for (i = 0; i < _bits; i++) {
            data = data + (((*(uint64_t*)(src_ptr + (int)(start + samplesPerBit * i) * unit_size) & serial_mask) != 0) << i);
        }
    } else {
        for (i = 0; i < _bits; i++) {
            data = (data << 1) + ((*(uint64_t*)(src_ptr + (int)(start + samplesPerBit * i) * unit_size) & serial_mask) != 0);
        }
    }

    cur_state = Data;
    _state_index.push_back(std::make_pair(std::make_pair(start - samplesPerBit * 0.5, samplesPerBit * _bits),
                                          std::make_pair(cur_state, data)));
    if (_parity != -1) {
        parity = ((*(uint64_t*)(src_ptr + (int)(start + samplesPerBit * _bits) * unit_size) & serial_mask) != 0);
        parity = parity ^ data;
        parity = (parity & 0x55555555) + ((parity >> 1) & 0x55555555);
        parity = (parity & 0x33333333) + ((parity >> 2) & 0x33333333);
        parity = (parity & 0x0f0f0f0f) + ((parity >> 4) & 0x0f0f0f0f);
        parity = (parity & 0x00ff00ff) + ((parity >> 8) & 0x00ff00ff);
        parity = (parity & 0x0000ffff) + ((parity >> 16) & 0x0000ffff);
        parity = (parity & 0x00000001) ^ _parity;
        cur_state = parity ? ParityErr : Parity;
        _state_index.push_back(std::make_pair(std::make_pair(start + samplesPerBit * (_bits - 0.5), samplesPerBit),
                                              std::make_pair(cur_state, 0)));
    }
    _max_width = max(_max_width, (uint64_t)(samplesPerBit * _bits));
}

void dsSerial::fill_color_table(std::vector <QColor>& _color_table)
{
    int i;
    for(i = 0; i < TableSize; i++)
        _color_table.push_back(ColorTable[i]);
}

void dsSerial::fill_state_table(std::vector <QString>& _state_table)
{
    int i;
    for(i = 0; i < TableSize; i++)
        _state_table.push_back(StateTable[i]);
}

void dsSerial::get_subsampled_states(std::vector<struct ds_view_state> &states,
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
