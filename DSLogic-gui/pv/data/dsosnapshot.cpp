/*
 * This file is part of the DSLogic-gui project.
 * DSLogic-gui is based on PulseView.
 *
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


#include <extdef.h>

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <algorithm>

#include <boost/foreach.hpp>

#include "dsosnapshot.h"

using namespace boost;
using namespace std;

namespace pv {
namespace data {

const int DsoSnapshot::EnvelopeScalePower = 4;
const int DsoSnapshot::EnvelopeScaleFactor = 1 << EnvelopeScalePower;
const float DsoSnapshot::LogEnvelopeScaleFactor =
	logf(EnvelopeScaleFactor);
const uint64_t DsoSnapshot::EnvelopeDataUnit = 64*1024;	// bytes

DsoSnapshot::DsoSnapshot(const sr_datafeed_dso &dso, uint64_t _total_sample_len, unsigned int channel_num) :
    Snapshot(sizeof(uint16_t), _total_sample_len, channel_num)
{
	boost::lock_guard<boost::recursive_mutex> lock(_mutex);
	memset(_envelope_levels, 0, sizeof(_envelope_levels));
    init(_total_sample_len * channel_num);
    append_payload(dso);
}

DsoSnapshot::~DsoSnapshot()
{
	boost::lock_guard<boost::recursive_mutex> lock(_mutex);
    BOOST_FOREACH(Envelope &e, _envelope_levels[0])
		free(e.samples);
}

void DsoSnapshot::append_payload(const sr_datafeed_dso &dso)
{
	boost::lock_guard<boost::recursive_mutex> lock(_mutex);
    append_data(dso.data, dso.num_samples);

	// Generate the first mip-map from the data
	append_payload_to_envelope_levels();
}

const uint8_t *DsoSnapshot::get_samples(
    int64_t start_sample, int64_t end_sample, uint16_t index) const
{
	assert(start_sample >= 0);
    assert(start_sample < (int64_t)get_sample_count());
	assert(end_sample >= 0);
    assert(end_sample < (int64_t)get_sample_count());
	assert(start_sample <= end_sample);

	boost::lock_guard<boost::recursive_mutex> lock(_mutex);

//    uint16_t *const data = new uint16_t[end_sample - start_sample];
//    memcpy(data, (uint16_t*)_data + start_sample, sizeof(uint16_t) *
//		(end_sample - start_sample));
//	return data;
    return (uint8_t*)_data + start_sample * _channel_num + index;
}

void DsoSnapshot::get_envelope_section(EnvelopeSection &s,
    uint64_t start, uint64_t end, float min_length, int probe_index) const
{
	assert(end <= get_sample_count());
	assert(start <= end);
	assert(min_length > 0);

	boost::lock_guard<boost::recursive_mutex> lock(_mutex);

	const unsigned int min_level = max((int)floorf(logf(min_length) /
		LogEnvelopeScaleFactor) - 1, 0);
	const unsigned int scale_power = (min_level + 1) *
		EnvelopeScalePower;
	start >>= scale_power;
	end >>= scale_power;

	s.start = start << scale_power;
	s.scale = 1 << scale_power;
	s.length = end - start;
//	s.samples = new EnvelopeSample[s.length];
//	memcpy(s.samples, _envelope_levels[min_level].samples + start,
//		s.length * sizeof(EnvelopeSample));
    s.samples = _envelope_levels[probe_index][min_level].samples + start;
}

void DsoSnapshot::reallocate_envelope(Envelope &e)
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

void DsoSnapshot::append_payload_to_envelope_levels()
{
    unsigned int i;
    for (i = 0; i < _channel_num; i++) {
        Envelope &e0 = _envelope_levels[i][0];
        uint64_t prev_length;
        EnvelopeSample *dest_ptr;

        // Expand the data buffer to fit the new samples
        prev_length = e0.length;
        e0.length = get_sample_count() / EnvelopeScaleFactor;

        // Break off if there are no new samples to compute
    //	if (e0.length == prev_length)
    //		return;
        if (e0.length == 0)
            return;
        if (e0.length == prev_length)
            prev_length = 0;

        reallocate_envelope(e0);

        dest_ptr = e0.samples + prev_length;

        // Iterate through the samples to populate the first level mipmap
        const uint8_t *const stop_src_ptr = (uint8_t*)_data +
            e0.length * EnvelopeScaleFactor * _channel_num;
//        for (const uint16_t *src_ptr = (uint16_t*)_data +
//            prev_length * EnvelopeScaleFactor;
//            src_ptr < end_src_ptr; src_ptr += EnvelopeScaleFactor)
//        {
//            const EnvelopeSample sub_sample = {
//                *min_element(src_ptr, src_ptr + EnvelopeScaleFactor),
//                *max_element(src_ptr, src_ptr + EnvelopeScaleFactor),
//            };

//            *dest_ptr++ = sub_sample;
//        }
        for (const uint8_t *src_ptr = (uint8_t*)_data +
            prev_length * EnvelopeScaleFactor * _channel_num + i;
            src_ptr < stop_src_ptr; src_ptr += EnvelopeScaleFactor * _channel_num)
        {
            const uint8_t * begin_src_ptr =
                src_ptr;
            const uint8_t *const end_src_ptr =
                src_ptr + EnvelopeScaleFactor * _channel_num;

            EnvelopeSample sub_sample;
            sub_sample.min = *begin_src_ptr;
            sub_sample.max = *begin_src_ptr;
            begin_src_ptr += _channel_num;
            while (begin_src_ptr < end_src_ptr)
            {
                sub_sample.min = min(sub_sample.min, *begin_src_ptr);
                sub_sample.max = max(sub_sample.max, *begin_src_ptr);
                begin_src_ptr += _channel_num;
            }

            *dest_ptr++ = sub_sample;
        }

        // Compute higher level mipmaps
        for (unsigned int level = 1; level < ScaleStepCount; level++)
        {
            Envelope &e = _envelope_levels[i][level];
            const Envelope &el = _envelope_levels[i][level-1];

            // Expand the data buffer to fit the new samples
            prev_length = e.length;
            e.length = el.length / EnvelopeScaleFactor;

            // Break off if there are no more samples to computed
    //		if (e.length == prev_length)
    //			break;
            if (e.length == prev_length)
                prev_length = 0;

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
}

} // namespace data
} // namespace pv
