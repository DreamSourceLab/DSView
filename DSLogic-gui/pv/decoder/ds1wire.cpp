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


#include "ds1wire.h"

#include <math.h>

using namespace boost;
using namespace std;

namespace pv {
namespace decoder {

const QColor ds1Wire::ColorTable[TableSize] = {
    QColor(255, 255, 255, 150),
    QColor(0, 255, 0, 150),
    QColor(255, 0, 0, 150),
    QColor(0, 255, 0, 150),
    QColor(255, 0, 0, 150),
    QColor(0, 0, 255, 150),
    QColor(0, 0, 255, 150),
    QColor(0, 255, 255, 150),
};

const QString ds1Wire::StateTable[TableSize] = {
    "UNKNOWN",
    "RESET",
    "PRESENCE",
    "COMMAND",
    "FAMILY CODE",
    "SERIAL NUMBER",
    "CRC",
    "DATA"
};

ds1Wire::ds1Wire(boost::shared_ptr<data::Logic> data, std::list <int > _sel_probes, QMap<QString, QVariant> &_options, QMap<QString, int> _options_index) :
    Decoder(data, _sel_probes, _options_index)
{
    (void)_options;

    assert(_sel_probes.size() == 1);

    _wire_index = _sel_probes.front();
}

ds1Wire::~ds1Wire()
{
}

QString ds1Wire::get_decode_name()
{
    return "1-Wire";
}

void ds1Wire::recode(std::list <int > _sel_probes, QMap <QString, QVariant>& _options, QMap<QString, int> _options_index)
{
    (void)_options;

    assert(_sel_probes.size() == 1);
    _wire_index = _sel_probes.front();

    this->_sel_probes = _sel_probes;
    this->_options_index = _options_index;

    decode();
}

void ds1Wire::decode()
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

    uint64_t flag_index1;
    uint64_t flag_index2;
    uint64_t flag_index3;
    uint64_t flag_index4;
    //uint64_t start_index;
    //uint64_t stop_index;
    //bool edge1;
    //bool edge2;
    uint64_t left = 0;
    uint64_t right = snapshot->get_sample_count() - 1;
    const uint64_t samplerate = _data->get_samplerate();
    double pulse_width1;
    double pulse_width2;
    double pulse_width3;
    double pulse_width4;

    uint8_t data;
    bool valid = false;

    if (!_state_index.empty())
        _state_index.clear();

    while(left < right && pulse_width1 != 0)    // Regular Speed
    {
        // search reset flag
        pulse_width1 = get_next_pulse_width(0, samplerate, left, right, snapshot);
        flag_index1 = left;
        if (pulse_width1 >= 0.48 && pulse_width1 <= 0.96) { // tRSTL
            pulse_width2 = get_next_pulse_width(1, samplerate, left, right, snapshot);
            flag_index2 = left;
            if (pulse_width2 >= 0.015 && pulse_width2 <= 0.06) { // tPDH
                pulse_width3 = get_next_pulse_width(0, samplerate, left, right, snapshot);
                flag_index3 = left;
                if (pulse_width3 >= 0.06 && pulse_width3 <= 0.24) { // tPDL
                    pulse_width4 = get_next_pulse_width(1, samplerate, left, right, snapshot);
                    flag_index4 = left;
                    if (pulse_width2 + pulse_width3 + pulse_width4 >= 0.48) {
                        cur_state = Reset;
                        _state_index.push_back(std::make_pair(std::make_pair(flag_index1, flag_index2 - flag_index1), std::make_pair(cur_state, 0)));
                        cur_state = Presence;
                        _state_index.push_back(std::make_pair(std::make_pair(flag_index3, flag_index4 - flag_index3), std::make_pair(cur_state, 0)));
                    } else {
                        continue;
                    }
                } else {
                    continue;
                }
            } else {
                continue;
            }
        } else {
            continue;
        }

        uint64_t start;
        uint64_t end;
        int i;
        if (cur_state == Presence) {
            data = get_next_data(false, valid, start, end, samplerate, left, right, snapshot);
            if (valid) {
                cur_state = Command;
                _state_index.push_back(std::make_pair(std::make_pair(start, end - start), std::make_pair(cur_state, data)));
            }else {
                continue;
            }
        }

        if (cur_state == Command) {
            data = get_next_data(false, valid, start, end, samplerate, left, right, snapshot);
            if (valid) {
                cur_state = Family;
                _state_index.push_back(std::make_pair(std::make_pair(start, end - start), std::make_pair(cur_state, data)));
            } else {
                continue;
            }
        }

        if (cur_state == Family) {
            for (i = 0; i < 6; i++) {
                data = get_next_data(false, valid, start, end, samplerate, left, right, snapshot);
                if (valid) {
                    cur_state = Serial;
                    _state_index.push_back(std::make_pair(std::make_pair(start, end - start), std::make_pair(cur_state, data)));
                } else {
                    break;
                }
            }
            if (i != 6)
                continue;
        }

        if (cur_state == Serial) {
            data = get_next_data(false, valid, start, end, samplerate, left, right, snapshot);
            if (valid) {
                cur_state = Crc;
                _state_index.push_back(std::make_pair(std::make_pair(start, end - start), std::make_pair(cur_state, data)));
            } else {
                continue;
            }
        }

        if (cur_state == Crc) {
            while (1) {
                data = get_next_data(false, valid, start, end, samplerate, left, right, snapshot);
                if (valid) {
                    cur_state = Data;
                    _state_index.push_back(std::make_pair(std::make_pair(start, end - start), std::make_pair(cur_state, data)));
                } else {
                    break;
                }
            }
        }
    }

//    if (cur_state == Unknown) {
//        while(1) { // Overdrive Speed

//        }
//    }
}

double ds1Wire::get_next_pulse_width(bool level, uint64_t samplerate, uint64_t &left, uint64_t right,
                                     const boost::shared_ptr<data::LogicSnapshot> &snapshot)
{
    double pulse_width = 0;
    uint64_t flag_index1;
    uint64_t flag_index2;
    bool edge1;
    bool edge2;

    if (snapshot->get_first_edge(flag_index1, edge1, left, right, _wire_index, level, _wire_index, -1) == SR_OK) {
        left = flag_index1;
        if (snapshot->get_first_edge(flag_index2, edge2, left, right, _wire_index, !level, _wire_index, -1) == SR_OK) {
            pulse_width = (flag_index2 - flag_index1) * 1000.0f / samplerate;
        }
    }

    return pulse_width;
}

uint8_t ds1Wire::get_next_data(bool speed, bool &valid, uint64_t &start, uint64_t &end,
                      uint64_t samplerate, uint64_t &left, uint64_t right,
                      const boost::shared_ptr<data::LogicSnapshot> &snapshot)
{
    uint8_t data = 0;
    double pulse_width1;
    uint64_t org_left = left;
    int i;

    valid = true;
    if (speed == false) {
        for (i = 0; i < 8; i++) {
            pulse_width1 = get_next_pulse_width(0, samplerate, left, right, snapshot);
            start = (i == 0) ? left : start;
            end = (i == 7) ? left + samplerate * 0.001 * 0.06 : end;
            if (pulse_width1 >= 0.001 && pulse_width1 < 0.015) {
                pulse_width1 = get_next_pulse_width(1, samplerate, left, right, snapshot);
                if (pulse_width1 >= 0.046) {
                    data = data + (1 << i);
                    org_left = left;
                } else {
                    left = org_left;
                    valid = false;
                    break;
                }
            } else if (pulse_width1 >= 0.06 && pulse_width1 < 0.12) {
                pulse_width1 = get_next_pulse_width(1, samplerate, left, right, snapshot);
                if (pulse_width1 >= 0.001) {
                    org_left = left;
                    //data = data << 1;
                } else {
                    left = org_left;
                    valid = false;
                    break;
                }
            } else {
                left = org_left;
                valid = false;
                break;
            }
        }
    } else {
        for (i = 0; i < 8; i++) {
            pulse_width1 = get_next_pulse_width(0, samplerate, left, right, snapshot);
            start = (i == 0) ? left : start;
            end = (i == 7) ? left + samplerate * 0.001 * 0.006 : end;
            if (pulse_width1 >= 0.001 && pulse_width1 < 0.002) {
                pulse_width1 = get_next_pulse_width(1, samplerate, left, right, snapshot);
                if (pulse_width1 >= 0.006) {
                    org_left = left;
                    data = data + (1 << i);
                } else {
                    left = org_left;
                    valid = false;
                    break;
                }
            } else if (pulse_width1 >= 0.006 && pulse_width1 < 0.016) {
                pulse_width1 = get_next_pulse_width(1, samplerate, left, right, snapshot);
                if (pulse_width1 >= 0.001) {
                    org_left = left;
                    //data = data << 1;
                } else {
                    left = org_left;
                    valid = false;
                    break;
                }
            } else {
                left = org_left;
                valid = false;
                break;
            }
        }
    }

    return data;
}

void ds1Wire::fill_color_table(std::vector <QColor>& _color_table)
{
    int i;
    for(i = 0; i < TableSize; i++)
        _color_table.push_back(ColorTable[i]);
}

void ds1Wire::fill_state_table(std::vector <QString>& _state_table)
{
    int i;
    for(i = 0; i < TableSize; i++)
        _state_table.push_back(StateTable[i]);
}

void ds1Wire::get_subsampled_states(std::vector<struct ds_view_state> &states,
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
            view_state.type = (_state_index.at(i).second.first == Reset ||
                               _state_index.at(i).second.first == Presence) ? DEC_CMD : DEC_DATA;
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
