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

#include <QDebug>

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <boost/foreach.hpp>

#include "logicsnapshot.h"

using namespace boost;
using namespace std;

namespace pv {
namespace data {

const int LogicSnapshot::MipMapScalePower = 4;
const int LogicSnapshot::MipMapScaleFactor = 1 << MipMapScalePower;
const float LogicSnapshot::LogMipMapScaleFactor = logf(MipMapScaleFactor);
const uint64_t LogicSnapshot::MipMapDataUnit = 64*1024;	// bytes

LogicSnapshot::LogicSnapshot() :
    Snapshot(1, 1, 1),
    _last_append_sample(0)
{
    memset(_mip_map, 0, sizeof(_mip_map));
}

LogicSnapshot::~LogicSnapshot()
{
    free_mipmap();
}

void LogicSnapshot::free_mipmap()
{
    BOOST_FOREACH(MipMapLevel &l, _mip_map) {
        if (l.data)
            free(l.data);
    }
    memset(_mip_map, 0, sizeof(_mip_map));
}

void LogicSnapshot::init()
{
    boost::lock_guard<boost::recursive_mutex> lock(_mutex);
    _sample_count = 0;
    _ring_sample_count = 0;
    _memory_failed = false;
    _last_ended = true;
    for (unsigned int level = 0; level < ScaleStepCount; level++) {
        _mip_map[level].length = 0;
        _mip_map[level].data_length = 0;
    }
}

void LogicSnapshot::clear()
{
    boost::lock_guard<boost::recursive_mutex> lock(_mutex);
    free_data();
    free_mipmap();
    init();
}

void LogicSnapshot::first_payload(const sr_datafeed_logic &logic, uint64_t total_sample_count, unsigned int channel_num)
{
    _total_sample_count = total_sample_count;
    _channel_num = channel_num;
    _unit_size = logic.unitsize;

    bool isOk = true;
    uint64_t size = _total_sample_count * _unit_size + sizeof(uint64_t);
    if (size != _capacity) {
        free_data();
        _data = malloc(size);
        if (_data) {
            free_mipmap();
            uint64_t mipmap_count = _total_sample_count /  MipMapScaleFactor;
            for (unsigned int level = 0; level < ScaleStepCount; level++) {
                mipmap_count = ((mipmap_count + MipMapDataUnit - 1) /
                        MipMapDataUnit) * MipMapDataUnit;
                _mip_map[level].data = malloc(mipmap_count * _unit_size + sizeof(uint64_t));
                if (!_mip_map[level].data) {
                    isOk = false;
                    break;
                }
                mipmap_count = mipmap_count /  MipMapScaleFactor;
            }
        } else {
            isOk = false;
        }
    }

    if (isOk) {
        _capacity = size;
        _memory_failed = false;
        append_payload(logic);
        _last_ended = false;
    } else {
        free_data();
        free_mipmap();
        _capacity = 0;
        _memory_failed = true;
    }
}

void LogicSnapshot::append_payload(
	const sr_datafeed_logic &logic)
{
	assert(_unit_size == logic.unitsize);
	assert((logic.length % _unit_size) == 0);

    boost::lock_guard<boost::recursive_mutex> lock(_mutex);

	append_data(logic.data, logic.length / _unit_size);

	// Generate the first mip-map from the data
    append_payload_to_mipmap();
}

uint8_t * LogicSnapshot::get_samples(int64_t start_sample, int64_t end_sample) const
{
    //assert(data);
    assert(start_sample >= 0);
    assert(start_sample <= (int64_t)get_sample_count());
    assert(end_sample >= 0);
    assert(end_sample <= (int64_t)get_sample_count());
    assert(start_sample <= end_sample);

    (void)end_sample;

    //const size_t size = (end_sample - start_sample) * _unit_size;
    //memcpy(data, (const uint8_t*)_data + start_sample * _unit_size, size);
    return (uint8_t*)_data + start_sample * _unit_size;
}

void LogicSnapshot::reallocate_mipmap_level(MipMapLevel &m)
{
	const uint64_t new_data_length = ((m.length + MipMapDataUnit - 1) /
		MipMapDataUnit) * MipMapDataUnit;
	if (new_data_length > m.data_length)
	{
		m.data_length = new_data_length;

		// Padding is added to allow for the uint64_t write word
//		m.data = realloc(m.data, new_data_length * _unit_size +
//			sizeof(uint64_t));
	}
}

void LogicSnapshot::append_payload_to_mipmap()
{
	MipMapLevel &m0 = _mip_map[0];
	uint64_t prev_length;
	const uint8_t *src_ptr;
	uint8_t *dest_ptr;
	uint64_t accumulator;
	unsigned int diff_counter;

	// Expand the data buffer to fit the new samples
	prev_length = m0.length;
	m0.length = _sample_count / MipMapScaleFactor;

	// Break off if there are no new samples to compute
	if (m0.length == prev_length)
		return;

	reallocate_mipmap_level(m0);

	dest_ptr = (uint8_t*)m0.data + prev_length * _unit_size;

	// Iterate through the samples to populate the first level mipmap
    const uint8_t *const end_src_ptr = (uint8_t*)_data +
		m0.length * _unit_size * MipMapScaleFactor;
    for (src_ptr = (uint8_t*)_data +
		prev_length * _unit_size * MipMapScaleFactor;
		src_ptr < end_src_ptr;)
	{
		// Accumulate transitions which have occurred in this sample
		accumulator = 0;
		diff_counter = MipMapScaleFactor;
		while (diff_counter-- > 0)
		{
			const uint64_t sample = *(uint64_t*)src_ptr;
			accumulator |= _last_append_sample ^ sample;
			_last_append_sample = sample;
			src_ptr += _unit_size;
		}

		*(uint64_t*)dest_ptr = accumulator;
		dest_ptr += _unit_size;
	}

	// Compute higher level mipmaps
	for (unsigned int level = 1; level < ScaleStepCount; level++)
	{
		MipMapLevel &m = _mip_map[level];
		const MipMapLevel &ml = _mip_map[level-1];

		// Expand the data buffer to fit the new samples
		prev_length = m.length;
		m.length = ml.length / MipMapScaleFactor;

		// Break off if there are no more samples to computed
		if (m.length == prev_length)
			break;

		reallocate_mipmap_level(m);

		// Subsample the level lower level
		src_ptr = (uint8_t*)ml.data +
			_unit_size * prev_length * MipMapScaleFactor;
		const uint8_t *const end_dest_ptr =
			(uint8_t*)m.data + _unit_size * m.length;
		for (dest_ptr = (uint8_t*)m.data +
			_unit_size * prev_length;
			dest_ptr < end_dest_ptr;
			dest_ptr += _unit_size)
		{
			accumulator = 0;
			diff_counter = MipMapScaleFactor;
			while (diff_counter-- > 0)
			{
				accumulator |= *(uint64_t*)src_ptr;
				src_ptr += _unit_size;
			}

			*(uint64_t*)dest_ptr = accumulator;
		}
	}
}

void LogicSnapshot::get_subsampled_edges(
	std::vector<EdgePair> &edges,
	uint64_t start, uint64_t end,
	float min_length, int sig_index)
{
    if (!edges.empty())
        edges.clear();

    if (get_sample_count() == 0)
        return;

    assert(end < get_sample_count());
	assert(start <= end);
	assert(min_length > 0);
	assert(sig_index >= 0);
	assert(sig_index < 64);

    uint64_t index = start;
    bool last_sample;
	const uint64_t block_length = (uint64_t)max(min_length, 1.0f);
	const uint64_t sig_mask = 1ULL << sig_index;


	// Store the initial state
	last_sample = (get_sample(start) & sig_mask) != 0;
	edges.push_back(pair<int64_t, bool>(index++, last_sample));

    while (index + block_length <= end)
	{
        // search next edge
        get_nxt_edge(index, last_sample, end, min_length, sig_index);

		//----- Store the edge -----//

		// Take the last sample of the quanization block
		const int64_t final_index = index + block_length;
        if (index + block_length > end)
			break;

		// Store the final state
		const bool final_sample =
			(get_sample(final_index - 1) & sig_mask) != 0;
		edges.push_back(pair<int64_t, bool>(index, final_sample));

		index = final_index;
		last_sample = final_sample;
	}

    // Add the final state
    const bool end_sample = ((get_sample(end) & sig_mask) != 0);
    if ((end != get_sample_count() - 1) ||
            ((end == get_sample_count() - 1) && end_sample != last_sample))
        edges.push_back(pair<int64_t, bool>(end, end_sample));

    if (end == get_sample_count() - 1)
        edges.push_back(pair<int64_t, bool>(end + 1, ~last_sample));
}

bool LogicSnapshot::get_nxt_edge(
    uint64_t &index, bool last_sample, uint64_t end,
    float min_length, int sig_index)
{
    unsigned int level;
    bool fast_forward;

    //assert(index > 0);

    const unsigned int min_level = max((int)floorf(logf(min_length) /
        LogMipMapScaleFactor) - 1, 0);
    const uint64_t sig_mask = 1ULL << sig_index;

    if (index >= end)
        return false;

    //----- Continue to search -----//
    level = min_level;

    // We cannot fast-forward if there is no mip-map data at
    // at the minimum level.
    fast_forward = (_mip_map[level].data != NULL);

    if (min_length < MipMapScaleFactor)
    {
        // Search individual samples up to the beginning of
        // the next first level mip map block
        const uint64_t final_index = min(end,
            pow2_ceil(index, MipMapScalePower));

        for (; index < final_index &&
            (index & ~(~0 << MipMapScalePower)) != 0;
            index++)
        {
            const bool sample =
                (get_sample(index) & sig_mask) != 0;

            // If there was a change we cannot fast forward
            if (sample != last_sample) {
                fast_forward = false;
                break;
            }
        }
    }
    else
    {
        // If resolution is less than a mip map block,
        // round up to the beginning of the mip-map block
        // for this level of detail
        const int min_level_scale_power =
            (level + 1) * MipMapScalePower;
        index = pow2_ceil(index, min_level_scale_power);
        if (index >= end)
            return false;

        // We can fast forward only if there was no change
        const bool sample =
            (get_sample(index) & sig_mask) != 0;
        if (last_sample != sample)
            fast_forward = false;
    }

    if (fast_forward) {

        // Fast forward: This involves zooming out to higher
        // levels of the mip map searching for changes, then
        // zooming in on them to find the point where the edge
        // begins.

        // Slide right and zoom out at the beginnings of mip-map
        // blocks until we encounter a change
        while (1) {
            const int level_scale_power =
                (level + 1) * MipMapScalePower;
            const uint64_t offset =
                index >> level_scale_power;

            // Check if we reached the last block at this
            // level, or if there was a change in this block
            if (offset >= _mip_map[level].length ||
                (get_subsample(level, offset) &
                    sig_mask))
                break;

            if ((offset & ~(~0 << MipMapScalePower)) == 0) {
                // If we are now at the beginning of a
                // higher level mip-map block ascend one
                // level
                if (level + 1 >= ScaleStepCount ||
                    !_mip_map[level + 1].data)
                    break;

                level++;
            } else {
                // Slide right to the beginning of the
                // next mip map block
                index = pow2_ceil(index + 1,
                    level_scale_power);
            }
        }

        // Zoom in, and slide right until we encounter a change,
        // and repeat until we reach min_level
        while (1) {
            assert(_mip_map[level].data);

            const int level_scale_power =
                (level + 1) * MipMapScalePower;
            const uint64_t offset =
                index >> level_scale_power;

            // Check if we reached the last block at this
            // level, or if there was a change in this block
            if (offset >= _mip_map[level].length ||
                (get_subsample(level, offset) &
                    sig_mask)) {
                // Zoom in unless we reached the minimum
                // zoom
                if (level == min_level)
                    break;

                level--;
            } else {
                // Slide right to the beginning of the
                // next mip map block
                index = pow2_ceil(index + 1,
                    level_scale_power);
            }
        }

        // If individual samples within the limit of resolution,
        // do a linear search for the next transition within the
        // block
        if (min_length < MipMapScaleFactor) {
            for (; index < end; index++) {
                const bool sample = (get_sample(index) &
                    sig_mask) != 0;
                if (sample != last_sample)
                    break;
            }
        }
    }

    if (index >= end)
        return false;
    else
        return true;
}

bool LogicSnapshot::get_pre_edge(
    uint64_t &index, bool last_sample,
    float min_length, int sig_index)
{
    unsigned int level;
    bool fast_forward;

    assert(index < get_sample_count());

    const unsigned int min_level = max((int)floorf(logf(min_length) /
        LogMipMapScaleFactor) - 1, 0);
    const uint64_t sig_mask = 1ULL << sig_index;

    //----- Continue to search -----//
    level = min_level;

    // We cannot fast-forward if there is no mip-map data at
    // at the minimum level.
    fast_forward = (_mip_map[level].data != NULL);

    if (min_length < MipMapScaleFactor)
    {
        // Search individual samples down to the ending of
        // the previous first level mip map block
        uint64_t final_index;
        if (index < (1 << MipMapScalePower))
            final_index = 0;
        else
            final_index = pow2_ceil(index + 1, MipMapScalePower) - (1 << MipMapScalePower) - 1;

        for (; index >= final_index; index--)
        {
            const bool sample =
                (get_sample(index) & sig_mask) != 0;

            // If there was a change we cannot fast forward
            if (sample != last_sample) {
                fast_forward = false;
                index++;
                return true;
            }

            if (index == 0)
                return false;
        }
    }
    else
    {
        // If resolution is less than a mip map block,
        // round up to the beginning of the mip-map block
        // for this level of detail
        const unsigned int min_level_scale_power =
            (level + 1) * MipMapScalePower;
        if (index < (uint64_t)(1 << min_level_scale_power))
            index = 0;
        else
            index = pow2_ceil(index, min_level_scale_power) - (1 << min_level_scale_power) - 1;

        // We can fast forward only if there was no change
        const bool sample =
            (get_sample(index) & sig_mask) != 0;
        if (last_sample != sample) {
            fast_forward = false;
            index++;
            return true;
        }

        if (index == 0)
            return false;
    }

    if (fast_forward) {

        // Fast forward: This involves zooming out to higher
        // levels of the mip map searching for changes, then
        // zooming in on them to find the point where the edge
        // begins.

        // Slide left and zoom out at the endings of mip-map
        // blocks until we encounter a change
        while (1) {
            const int level_scale_power =
                (level + 1) * MipMapScalePower;
            const uint64_t offset =
                index >> level_scale_power;

            // Check if we reached the first block at this
            // level, or if there was a change in this block
            if (offset == 0 ||
                (get_subsample(level, offset) &
                    sig_mask))
                break;

            if (((offset+1) & ~(~0 << MipMapScalePower)) == 0) {
                // If we are now at the ending of a
                // higher level mip-map block ascend one
                // level
                if (level + 1 >= ScaleStepCount ||
                    !_mip_map[level + 1].data)
                    break;

                level++;
            } else {
                // Slide left to the beginning of the
                // previous mip map block
                index = pow2_ceil(index + 1,
                    level_scale_power) - (1 << level_scale_power) - 1;
            }
        }

        // Zoom in, and slide left until we encounter a change,
        // and repeat until we reach min_level
        while (1) {
            assert(_mip_map[level].data);

            const int level_scale_power =
                (level + 1) * MipMapScalePower;
            const uint64_t offset =
                index >> level_scale_power;

            // Check if we reached the first block at this
            // level, or if there was a change in this block
            if (offset == 0 ||
                (get_subsample(level, offset) &
                    sig_mask)) {
                // Zoom in unless we reached the minimum
                // zoom
                if (level == min_level)
                    break;

                level--;
            } else {
                // Slide left to the ending of the
                // previous mip map block
                index = pow2_ceil(index + 1,
                    level_scale_power) - (1 << level_scale_power) - 1;
            }
        }

        // If individual samples within the limit of resolution,
        // do a linear search for the next transition within the
        // block
        if (min_length < MipMapScaleFactor) {
            for (; index > 0; index--) {
                const bool sample = (get_sample(index) &
                    sig_mask) != 0;
                if (sample != last_sample) {
                    index++;
                    return true;
                }
            }
        }
    }
    return false;
}

uint64_t LogicSnapshot::get_subsample(int level, uint64_t offset) const
{
	assert(level >= 0);
	assert(_mip_map[level].data);
    return *(uint64_t*)((uint8_t*)_mip_map[level].data +
		_unit_size * offset);
}

uint64_t LogicSnapshot::pow2_ceil(uint64_t x, unsigned int power)
{
	const uint64_t p = 1 << power;
	return (x + p - 1) / p * p;
}

} // namespace data
} // namespace pv
