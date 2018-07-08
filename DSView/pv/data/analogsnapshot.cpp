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

#include <extdef.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <algorithm>

#include <boost/foreach.hpp>

#include "analogsnapshot.h"

using namespace boost;
using namespace std;

namespace pv {
namespace data {

const int AnalogSnapshot::EnvelopeScalePower = 4;
const int AnalogSnapshot::EnvelopeScaleFactor = 1 << EnvelopeScalePower;
const float AnalogSnapshot::LogEnvelopeScaleFactor =
	logf(EnvelopeScaleFactor);
const uint64_t AnalogSnapshot::EnvelopeDataUnit = 64*1024;	// bytes

AnalogSnapshot::AnalogSnapshot() :
    Snapshot(sizeof(uint16_t), 1, 1)
{
	memset(_envelope_levels, 0, sizeof(_envelope_levels));
    _unit_pitch = 0;
}

AnalogSnapshot::~AnalogSnapshot()
{
    free_envelop();
}

void AnalogSnapshot::free_envelop()
{
    for (unsigned int i = 0; i < _channel_num; i++) {
        BOOST_FOREACH(Envelope &e, _envelope_levels[i]) {
            if (e.samples)
                free(e.samples);
        }
    }
    memset(_envelope_levels, 0, sizeof(_envelope_levels));
}

void AnalogSnapshot::init()
{
    boost::lock_guard<boost::recursive_mutex> lock(_mutex);
    _sample_count = 0;
    _ring_sample_count = 0;
    _memory_failed = false;
    _last_ended = true;
    for (unsigned int i = 0; i < _channel_num; i++) {
        for (unsigned int level = 0; level < ScaleStepCount; level++) {
            _envelope_levels[i][level].length = 0;
            _envelope_levels[i][level].ring_length = 0;
            // fix hang issue, count should not be clear
            //_envelope_levels[i][level].count = 0;
            _envelope_levels[i][level].data_length = 0;
        }
    }
}

void AnalogSnapshot::clear()
{
    boost::lock_guard<boost::recursive_mutex> lock(_mutex);
    free_data();
    free_envelop();
    init();
}

void AnalogSnapshot::first_payload(const sr_datafeed_analog &analog, uint64_t total_sample_count, GSList *channels)
{
    _total_sample_count = total_sample_count;
    _unit_bytes = (analog.unit_bits + 7) / 8;
    assert(_unit_bytes > 0);
    assert(_unit_bytes <= sizeof(uint64_t));
    _channel_num = 0;
    for (const GSList *l = channels; l; l = l->next) {
        sr_channel *const probe = (sr_channel*)l->data;
        assert(probe);
        // TODO: data of disabled channels should not be captured.
//        if (probe->type == SR_CHANNEL_ANALOG && probe->enabled) {
//            _channel_num ++;
//        }
        if (probe->type == SR_CHANNEL_ANALOG) {
            _channel_num ++;
        }
    }

    bool isOk = true;
    uint64_t size = _total_sample_count * _channel_num * _unit_bytes + sizeof(uint64_t);
    if (size != _capacity) {
        free_data();
        _data = malloc(size);
        if (_data) {
            free_envelop();
            for (unsigned int i = 0; i < _channel_num; i++) {
                uint64_t envelop_count = _total_sample_count / EnvelopeScaleFactor;
                for (unsigned int level = 0; level < ScaleStepCount; level++) {
                    envelop_count = ((envelop_count + EnvelopeDataUnit - 1) /
                            EnvelopeDataUnit) * EnvelopeDataUnit;
                    _envelope_levels[i][level].samples = (EnvelopeSample*)malloc(envelop_count * sizeof(EnvelopeSample));
                    _envelope_levels[i][level].max = (uint8_t *)malloc(envelop_count * _unit_bytes);
                    _envelope_levels[i][level].min = (uint8_t *)malloc(envelop_count * _unit_bytes);
                    if (!_envelope_levels[i][level].samples ||
                        !_envelope_levels[i][level].max ||
                        !_envelope_levels[i][level].min) {
                        isOk = false;
                        break;
                    }
                    _envelope_levels[i][level].count = envelop_count;
                    envelop_count = envelop_count / EnvelopeScaleFactor;
                }
                if (!isOk)
                    break;
            }
        } else {
            isOk = true;
        }
    }

    if (isOk) {
        for (const GSList *l = channels; l; l = l->next) {
            sr_channel *const probe = (sr_channel*)l->data;
            assert(probe);
            // TODO: data of disabled channels should not be captured.
            //if (probe->type == SR_CHANNEL_ANALOG && probe->enabled) {
            if (probe->type == SR_CHANNEL_ANALOG) {
                _ch_index.push_back(probe->index);
            }
        }
        _capacity = size;
        _memory_failed = false;
        append_payload(analog);
        _last_ended = false;
    } else {
        free_data();
        free_envelop();
        _memory_failed = true;
    }
}

void AnalogSnapshot::append_payload(
	const sr_datafeed_analog &analog)
{
    boost::lock_guard<boost::recursive_mutex> lock(_mutex);
    append_data(analog.data, analog.num_samples, analog.unit_pitch);

	// Generate the first mip-map from the data
	append_payload_to_envelope_levels();
}

void AnalogSnapshot::append_data(void *data, uint64_t samples, uint16_t pitch)
{
    int bytes_per_sample = _unit_bytes * _channel_num;
    if (pitch <= 1) {
        if (_sample_count + samples < _total_sample_count)
            _sample_count += samples;
        else
            _sample_count = _total_sample_count;

        if (_ring_sample_count + samples >= _total_sample_count) {
            memcpy((uint8_t*)_data + _ring_sample_count * bytes_per_sample,
                data, (_total_sample_count - _ring_sample_count) * bytes_per_sample);
            data = (uint8_t*)data + (_total_sample_count - _ring_sample_count) * bytes_per_sample;
            _ring_sample_count = (samples + _ring_sample_count - _total_sample_count) % _total_sample_count;
            memcpy((uint8_t*)_data,
                data, _ring_sample_count * bytes_per_sample);
        } else {
            memcpy((uint8_t*)_data + _ring_sample_count * bytes_per_sample,
                data, samples * bytes_per_sample);
            _ring_sample_count += samples;
        }
    } else {
        while(samples--) {
            if (_unit_pitch == 0) {
                if (_sample_count < _total_sample_count)
                    _sample_count++;
                memcpy((uint8_t*)_data + _ring_sample_count * bytes_per_sample,
                    data, bytes_per_sample);
                data = (uint8_t*)data + bytes_per_sample;
                _ring_sample_count = (_ring_sample_count + 1) % _total_sample_count;
                _unit_pitch = pitch;
            }
            _unit_pitch--;
        }
    }
}

const uint8_t* AnalogSnapshot::get_samples(int64_t start_sample) const
{
	assert(start_sample >= 0);
    assert(start_sample < (int64_t)get_sample_count());

//    uint16_t *const data = new uint16_t[end_sample - start_sample];
//    memcpy(data, (uint16_t*)_data + start_sample, sizeof(uint16_t) *
//		(end_sample - start_sample));
//	return data;
    return (uint8_t*)_data + start_sample * _unit_bytes * _channel_num;
}

void AnalogSnapshot::get_envelope_section(EnvelopeSection &s,
    uint64_t start, int64_t count, float min_length, int probe_index) const
{
    assert(count >= 0);
	assert(min_length > 0);

    const unsigned int min_level = max((int)floorf(logf(min_length) /
            LogEnvelopeScaleFactor) - 1, 0);
    const unsigned int scale_power = (min_level + 1) * EnvelopeScalePower;
	start >>= scale_power;

    s.start = start;
    s.scale = (1 << scale_power);
    s.length = (count >> scale_power);
    s.samples_num = _envelope_levels[probe_index][min_level].length;
//	s.samples = new EnvelopeSample[s.length];
//	memcpy(s.samples, _envelope_levels[min_level].samples + start,
//		s.length * sizeof(EnvelopeSample));
    s.samples = _envelope_levels[probe_index][min_level].samples;
}

void AnalogSnapshot::reallocate_envelope(Envelope &e)
{
	const uint64_t new_data_length = ((e.length + EnvelopeDataUnit - 1) /
		EnvelopeDataUnit) * EnvelopeDataUnit;
    if (new_data_length > e.data_length)
	{
		e.data_length = new_data_length;
//		e.samples = (EnvelopeSample*)realloc(e.samples,
//			new_data_length * sizeof(EnvelopeSample));
	}
}

void AnalogSnapshot::append_payload_to_envelope_levels()
{
    int i;
    for (i = 0; i < (int)_channel_num; i++) {
        Envelope &e0 = _envelope_levels[i][0];
        uint64_t prev_length;
        EnvelopeSample *dest_ptr;

        // Expand the data buffer to fit the new samples
        e0.length = _sample_count / EnvelopeScaleFactor;
        prev_length = e0.ring_length;
        e0.ring_length = _ring_sample_count / EnvelopeScaleFactor;

        // Break off if there are no new samples to compute
    	if (e0.ring_length == prev_length)
            continue;
        if (e0.length == 0)
            continue;

        reallocate_envelope(e0);

        dest_ptr = e0.samples + prev_length;

        // Iterate through the samples to populate the first level mipmap
        const uint64_t src_size = _total_sample_count * _unit_bytes * _channel_num;
        uint64_t e0_sample_num = (e0.ring_length > prev_length) ? e0.ring_length - prev_length :
                                                                  e0.ring_length + (_total_sample_count / EnvelopeScaleFactor) - prev_length;
        uint8_t *src_ptr = (uint8_t*)_data +
                    (prev_length * EnvelopeScaleFactor * _channel_num + i) * _unit_bytes;
        for (uint64_t j = 0; j < e0_sample_num; j++) {
            const uint8_t *end_src_ptr =
                src_ptr + EnvelopeScaleFactor * _unit_bytes * _channel_num;
            if (end_src_ptr >= (uint8_t*)_data + src_size)
                end_src_ptr -= src_size;
            EnvelopeSample sub_sample;
            sub_sample.min = *src_ptr;
            sub_sample.max = *src_ptr;
            src_ptr += _channel_num * _unit_bytes;
            while(src_ptr != end_src_ptr) {
                sub_sample.min = min(sub_sample.min, *src_ptr);
                sub_sample.max = max(sub_sample.max, *src_ptr);
                src_ptr += _channel_num * _unit_bytes;
                if (src_ptr >= (uint8_t*)_data + src_size)
                    src_ptr -= src_size;
            }

            *dest_ptr++ = sub_sample;
            if (dest_ptr >= e0.samples + e0.count)
                dest_ptr = e0.samples;
        }

        // Compute higher level mipmaps
        for (unsigned int level = 1; level < ScaleStepCount; level++)
        {
            Envelope &e = _envelope_levels[i][level];
            const Envelope &el = _envelope_levels[i][level-1];

            // Expand the data buffer to fit the new samples
            e.length = el.length / EnvelopeScaleFactor;
            prev_length = e.ring_length;
            e.ring_length = el.ring_length / EnvelopeScaleFactor;

            // Break off if there are no more samples to computed
            if (e.ring_length == prev_length)
                break;

            reallocate_envelope(e);

            // Subsample the level lower level
            const EnvelopeSample *src_ptr =
                el.samples + prev_length * EnvelopeScaleFactor;
            const EnvelopeSample *const end_dest_ptr = e.samples + e.ring_length;
            dest_ptr = e.samples + prev_length;
            while(dest_ptr != end_dest_ptr) {
                const EnvelopeSample * end_src_ptr =
                    src_ptr + EnvelopeScaleFactor;
                if (end_src_ptr >= el.samples + el.count)
                    end_src_ptr -= el.count;

                EnvelopeSample sub_sample = *src_ptr++;
                while (src_ptr != end_src_ptr)
                {
                    sub_sample.min = min(sub_sample.min, src_ptr->min);
                    sub_sample.max = max(sub_sample.max, src_ptr->max);
                    src_ptr++;
                    if (src_ptr >= el.samples + el.count)
                        src_ptr = el.samples;
                }

                *dest_ptr++ = sub_sample;
                if (dest_ptr >= e.samples + e.count)
                    dest_ptr = e.samples;
            }
        }
    }
}

int AnalogSnapshot::get_ch_order(int sig_index)
{
    uint16_t order = 0;
    for (auto& iter:_ch_index) {
        if (iter == sig_index)
            break;
        order++;
    }

    if (order >= _ch_index.size())
        return -1;
    else
        return order;
}

int AnalogSnapshot::get_scale_factor() const
{
    return EnvelopeScaleFactor;
}

bool AnalogSnapshot::has_data(int index)
{
    for (auto& iter:_ch_index) {
        if (iter == index)
            return true;
    }
    return false;
}

int AnalogSnapshot::get_block_num()
{
    const uint64_t size = _sample_count * get_unit_bytes() * get_channel_num();
    return (size >> LeafBlockPower) +
           ((size & LeafMask) != 0);
}

uint64_t AnalogSnapshot::get_block_size(int block_index)
{
    assert(block_index < get_block_num());

    if (block_index < get_block_num() - 1) {
        return LeafBlockSamples;
    } else {
        const uint64_t size = _sample_count * get_unit_bytes() * get_channel_num();
        if (size % LeafBlockSamples == 0)
            return LeafBlockSamples;
        else
            return size % LeafBlockSamples;
    }
}

} // namespace data
} // namespace pv
