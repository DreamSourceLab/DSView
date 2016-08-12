/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
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

#include "logicsnapshot.h"
#include "groupsnapshot.h"

using namespace boost;
using namespace std;

namespace pv {
namespace data {

const int GroupSnapshot::EnvelopeScalePower = 4;
const int GroupSnapshot::EnvelopeScaleFactor = 1 << EnvelopeScalePower;
const float GroupSnapshot::LogEnvelopeScaleFactor =
	logf(EnvelopeScaleFactor);
const uint64_t GroupSnapshot::EnvelopeDataUnit = 64*1024;	// bytes
const uint16_t GroupSnapshot::value_mask[16] = {0x1, 0x2, 0x4, 0x8,
                                                0x10, 0x20, 0x40, 0x80,
                                                0x100, 0x200, 0x400, 0x800,
                                                0x1000, 0x2000, 0x4000, 0x8000};

GroupSnapshot::GroupSnapshot(const boost::shared_ptr<LogicSnapshot> &_logic_snapshot, std::list<int> index_list)
{
    assert(_logic_snapshot);

    //boost::lock_guard<boost::recursive_mutex> lock(_mutex);
	memset(_envelope_levels, 0, sizeof(_envelope_levels));
    _data = _logic_snapshot->get_data();
    _sample_count = _logic_snapshot->get_sample_count();
    _unit_size = _logic_snapshot->unit_size();
    _index_list = index_list;

    _mask = 0;
    std::list<int>::iterator j = _index_list.begin();
    while(j != _index_list.end())
        _mask |= value_mask[(*j++)];

    for (int i=0; i<32; i++) {
        _bubble_start[i] = -1;
        _bubble_end[i] = -1;
    }
    uint16_t mask = _mask;
    int i = 0;
    int k = 0;
    int zero = 0;
    int zero_pre = 0;
    // max bubble count: 31
    do {
        if (mask & 0x1) {
            if (_bubble_start[k] != -1 &&
                _bubble_end[k] == -1)
                _bubble_end[k++] = i - zero_pre;
        } else {
            if (_bubble_start[k] == -1) {
                _bubble_start[k] = i - zero;
                zero_pre = zero;
            }
            zero++;
        }
        i++;
    }while(mask >>= 1);

    append_payload();
}

GroupSnapshot::~GroupSnapshot()
{
    //boost::lock_guard<boost::recursive_mutex> lock(_mutex);
	BOOST_FOREACH(Envelope &e, _envelope_levels)
		free(e.samples);
}

void GroupSnapshot::init()
{

}

void GroupSnapshot::clear()
{

}

uint64_t GroupSnapshot::get_sample_count() const
{
    //boost::lock_guard<boost::recursive_mutex> lock(_mutex);
    return _sample_count;
}

void GroupSnapshot::append_payload()
{
    //boost::lock_guard<boost::recursive_mutex> lock(_mutex);

	// Generate the first mip-map from the data
	append_payload_to_envelope_levels();
}

const uint16_t* GroupSnapshot::get_samples(
    int64_t start_sample, int64_t end_sample)
{
	assert(start_sample >= 0);
	assert(start_sample < (int64_t)_sample_count);
	assert(end_sample >= 0);
	assert(end_sample < (int64_t)_sample_count);
	assert(start_sample <= end_sample);

    int64_t i;
    uint16_t tmpl, tmpr;
    //boost::lock_guard<boost::recursive_mutex> lock(_mutex);

    uint16_t *const data = new uint16_t[end_sample - start_sample];
//    memcpy(data, (uint16_t*)_data + start_sample, sizeof(uint16_t) *
//		(end_sample - start_sample));
//    memset(data, 0, sizeof(uint16_t) * (end_sample - start_sample));
    for(i = start_sample; i < end_sample; i++) {
        if (_unit_size == 2)
            tmpl = *((uint16_t*)_data + i) & _mask;
        else
            tmpl = *((uint8_t*)_data + i) & _mask;
        for(int j=0; _bubble_start[j] != -1; j++) {
            tmpr = tmpl & (0xffff >> (16 - _bubble_start[j]));
            tmpl >>= _bubble_end[j];
            tmpl <<= _bubble_start[j];
            tmpl += tmpr;
        }
        *(data + i - start_sample) = tmpl;
    }
	return data;
}

void GroupSnapshot::get_envelope_section(EnvelopeSection &s,
	uint64_t start, uint64_t end, float min_length) const
{
    assert(end <= _sample_count);
	assert(start <= end);
	assert(min_length > 0);

    //boost::lock_guard<boost::recursive_mutex> lock(_mutex);

	const unsigned int min_level = max((int)floorf(logf(min_length) /
		LogEnvelopeScaleFactor) - 1, 0);
	const unsigned int scale_power = (min_level + 1) *
		EnvelopeScalePower;
	start >>= scale_power;
	end >>= scale_power;

	s.start = start << scale_power;
	s.scale = 1 << scale_power;
	s.length = end - start;
	s.samples = new EnvelopeSample[s.length];
	memcpy(s.samples, _envelope_levels[min_level].samples + start,
		s.length * sizeof(EnvelopeSample));
}

void GroupSnapshot::reallocate_envelope(Envelope &e)
{
	const uint64_t new_data_length = ((e.length + EnvelopeDataUnit - 1) /
		EnvelopeDataUnit) * EnvelopeDataUnit;
	if (new_data_length > e.data_length)
	{
		e.data_length = new_data_length;
		e.samples = (EnvelopeSample*)realloc(e.samples,
			new_data_length * sizeof(EnvelopeSample));
	}
}

void GroupSnapshot::append_payload_to_envelope_levels()
{
	Envelope &e0 = _envelope_levels[0];
	uint64_t prev_length;
	EnvelopeSample *dest_ptr;

	// Expand the data buffer to fit the new samples
	prev_length = e0.length;
	e0.length = _sample_count / EnvelopeScaleFactor;

	// Break off if there are no new samples to compute
	if (e0.length == prev_length)
		return;

	reallocate_envelope(e0);

	dest_ptr = e0.samples + prev_length;

	// Iterate through the samples to populate the first level mipmap
    uint16_t group_value[EnvelopeScaleFactor];
    const uint8_t *const end_src_ptr = (uint8_t*)_data +
        e0.length * EnvelopeScaleFactor * _unit_size;
    for (const uint8_t *src_ptr = (uint8_t*)_data +
        prev_length * EnvelopeScaleFactor * _unit_size;
        src_ptr < end_src_ptr; src_ptr += EnvelopeScaleFactor * _unit_size)
	{
        uint16_t tmpr;
        for(int i = 0; i < EnvelopeScaleFactor; i++) {
            if (_unit_size == 2)
                group_value[i] = *((uint16_t*)src_ptr + i) & _mask;
            else
                group_value[i] = *((uint8_t*)src_ptr + i) & _mask;
            for(int j=0; _bubble_start[j] != -1; j++) {
                tmpr = group_value[i] & (0xffff >> (16 - _bubble_start[j]));
                group_value[i] >>= _bubble_end[j];
                group_value[i] <<= _bubble_start[j];
                group_value[i] += tmpr;
            }
        }

		const EnvelopeSample sub_sample = {
            *min_element(group_value, group_value + EnvelopeScaleFactor),
            *max_element(group_value, group_value + EnvelopeScaleFactor),
		};

		*dest_ptr++ = sub_sample;
	}

	// Compute higher level mipmaps
	for (unsigned int level = 1; level < ScaleStepCount; level++)
	{
		Envelope &e = _envelope_levels[level];
		const Envelope &el = _envelope_levels[level-1];

		// Expand the data buffer to fit the new samples
		prev_length = e.length;
		e.length = el.length / EnvelopeScaleFactor;

		// Break off if there are no more samples to computed
		if (e.length == prev_length)
			break;

		reallocate_envelope(e);

		// Subsample the level lower level
		const EnvelopeSample *src_ptr =
			el.samples + prev_length * EnvelopeScaleFactor;
		const EnvelopeSample *const end_dest_ptr = e.samples + e.length;
		for (dest_ptr = e.samples + prev_length;
			dest_ptr < end_dest_ptr; dest_ptr++)
		{
			const EnvelopeSample *const end_src_ptr =
				src_ptr + EnvelopeScaleFactor;

			EnvelopeSample sub_sample = *src_ptr++;
			while (src_ptr < end_src_ptr)
			{
				sub_sample.min = min(sub_sample.min, src_ptr->min);
				sub_sample.max = max(sub_sample.max, src_ptr->max);
				src_ptr++;
			}

			*dest_ptr = sub_sample;
		}
	}
}

} // namespace data
} // namespace pv
