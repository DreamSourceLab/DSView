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


#include "snapshot.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

using namespace boost;

namespace pv {
namespace data {

Snapshot::Snapshot(int unit_size, uint64_t total_sample_count, unsigned int channel_num) :
    _data(NULL),
    _channel_num(channel_num),
    _sample_count(0),
    _total_sample_count(total_sample_count * channel_num),
    _ring_sample_count(0),
    _unit_size(unit_size)
{
	lock_guard<recursive_mutex> lock(_mutex);
	assert(_unit_size > 0);
}

Snapshot::~Snapshot()
{
	lock_guard<recursive_mutex> lock(_mutex);
    if (_data != NULL)
        free(_data);
    _data = NULL;
}

int Snapshot::init(uint64_t _total_sample_len)
{
    _data = malloc(_total_sample_len * _unit_size +
        sizeof(uint64_t));

    if (_data == NULL)
        return SR_ERR_MALLOC;
    else
        return SR_OK;
}

bool Snapshot::buf_null() const
{
    if (_data == NULL)
        return true;
    else
        return false;
}

uint64_t Snapshot::get_sample_count() const
{
	lock_guard<recursive_mutex> lock(_mutex);
    return _sample_count / _channel_num;
}

void* Snapshot::get_data() const
{
    lock_guard<recursive_mutex> lock(_mutex);
    return _data;
}

int Snapshot::get_unit_size() const
{
    lock_guard<recursive_mutex> lock(_mutex);
    return _unit_size;
}

unsigned int Snapshot::get_channel_num() const
{
    lock_guard<recursive_mutex> lock(_mutex);
    return _channel_num;
}

void Snapshot::append_data(void *data, uint64_t samples)
{
//	lock_guard<recursive_mutex> lock(_mutex);
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

} // namespace data
} // namespace pv
