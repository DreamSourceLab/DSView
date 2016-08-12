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


#ifndef DSVIEW_PV_DATA_ANALOGSNAPSHOT_H
#define DSVIEW_PV_DATA_ANALOGSNAPSHOT_H

#include "snapshot.h"

#include <utility>
#include <vector>

namespace AnalogSnapshotTest {
class Basic;
}

namespace pv {
namespace data {

class AnalogSnapshot : public Snapshot
{
public:
	struct EnvelopeSample
	{
        uint16_t min;
        uint16_t max;
	};

	struct EnvelopeSection
	{
		uint64_t start;
		unsigned int scale;
		uint64_t length;
		EnvelopeSample *samples;
	};

private:
	struct Envelope
	{
		uint64_t length;
		uint64_t data_length;
		EnvelopeSample *samples;
	};

private:
	static const unsigned int ScaleStepCount = 10;
	static const int EnvelopeScalePower;
	static const int EnvelopeScaleFactor;
	static const float LogEnvelopeScaleFactor;
	static const uint64_t EnvelopeDataUnit;

public:
    AnalogSnapshot();

	virtual ~AnalogSnapshot();

    void clear();
    void init();

    void first_payload(const sr_datafeed_analog &analog, uint64_t total_sample_count, unsigned int channel_num);

	void append_payload(const sr_datafeed_analog &analog);

    const uint16_t* get_samples(int64_t start_sample,
		int64_t end_sample) const;

	void get_envelope_section(EnvelopeSection &s,
        uint64_t start, uint64_t end, float min_length, int probe_index) const;

private:
    void free_envelop();
	void reallocate_envelope(Envelope &l);
	void append_payload_to_envelope_levels();

private:
    struct Envelope _envelope_levels[2*DS_MAX_ANALOG_PROBES_NUM][ScaleStepCount];

	friend class AnalogSnapshotTest::Basic;
};

} // namespace data
} // namespace pv

#endif // DSVIEW_PV_DATA_ANALOGSNAPSHOT_H
