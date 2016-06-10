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


#include "snapshot.h"

#include <QDebug>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

using namespace boost;

namespace pv {
namespace data {

Snapshot::Snapshot(int unit_size, uint64_t total_sample_count, unsigned int channel_num) :
    _data(NULL),
    _capacity(0),
    _channel_num(channel_num),
    _sample_count(0),
    _total_sample_count(total_sample_count),
    _ring_sample_count(0),
    _unit_size(unit_size),
    _memory_failed(false),
    _last_ended(true)
{
    assert(_unit_size > 0);
}

Snapshot::~Snapshot()
{
    free_data();
}

void Snapshot::free_data()
{
    if (_data) {
        free(_data);
        _data = NULL;
        _capacity = 0;
    }
}

bool Snapshot::memory_failed() const
{
    return _memory_failed;
}

bool Snapshot::empty() const
{
    if (get_sample_count() == 0 || _memory_failed || !_data)
        return true;
    else
        return false;
}

bool Snapshot::last_ended() const
{
    return _last_ended;
}

void Snapshot::set_last_ended(bool ended)
{
    _last_ended = ended;
}

uint64_t Snapshot::get_sample_count() const
{
    boost::lock_guard<boost::recursive_mutex> lock(_mutex);
    return _sample_count;
}

const void* Snapshot::get_data() const
{
    return _data;
}

int Snapshot::unit_size() const
{
    return _unit_size;
}

unsigned int Snapshot::get_channel_num() const
{
    return _channel_num;
}

uint64_t Snapshot::get_sample(uint64_t index) const
{
    assert(_data);
    assert(index < get_sample_count());

    return *(uint64_t*)((uint8_t*)_data + index * _unit_size);
}

void Snapshot::append_data(void *data, uint64_t samples)
{
//	_data = realloc(_data, (_sample_count + samples) * _unit_size +
//		sizeof(uint64_t));
    if (_sample_count + samples < _total_sample_count)
        _sample_count += samples;
    else
        _sample_count = _total_sample_count;

    if (_ring_sample_count + samples > _total_sample_count) {
        memcpy((uint8_t*)_data + _ring_sample_count * _unit_size,
            data, (_total_sample_count - _ring_sample_count) * _unit_size);
        _ring_sample_count = (samples + _ring_sample_count - _total_sample_count) % _total_sample_count;
        memcpy((uint8_t*)_data,
            data, _ring_sample_count * _unit_size);
    } else {
        memcpy((uint8_t*)_data + _ring_sample_count * _unit_size,
            data, samples * _unit_size);
        _ring_sample_count += samples;
    }
}

void Snapshot::refill_data(void *data, uint64_t samples, bool instant)
{
    if (instant) {
        memcpy((uint8_t*)_data + _sample_count * _channel_num, data, samples*_channel_num);
        _sample_count = (_sample_count + samples) % (_total_sample_count + 1);
    } else {
        memcpy((uint8_t*)_data, data, samples*_channel_num);
        _sample_count = samples;
    }

}

} // namespace data
} // namespace pv
