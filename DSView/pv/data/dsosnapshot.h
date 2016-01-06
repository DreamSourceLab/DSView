/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
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


#ifndef DSVIEW_PV_DATA_DSOSNAPSHOT_H
#define DSVIEW_PV_DATA_DSOSNAPSHOT_H

#include "snapshot.h"

#include <utility>
#include <vector>

namespace DsoSnapshotTest {
class Basic;
}

namespace pv {
namespace data {

class DsoSnapshot : public Snapshot
{
public:
	struct EnvelopeSample
	{
        uint8_t min;
        uint8_t max;
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

    static const int VrmsScaleFactor;

public:
    DsoSnapshot(const sr_datafeed_dso &dso, uint64_t _total_sample_len, unsigned int channel_num, bool instant);

    virtual ~DsoSnapshot();

    void append_payload(const sr_datafeed_dso &dso);

    const uint8_t* get_samples(int64_t start_sample,
        int64_t end_sample, uint16_t index) const;

	void get_envelope_section(EnvelopeSection &s,
        uint64_t start, uint64_t end, float min_length, int probe_index) const;

    void enable_envelope(bool enable);

    double cal_vrms(double zero_off, unsigned int index) const;
    double cal_vmean(unsigned int index) const;

private:
	void reallocate_envelope(Envelope &l);

    void append_payload_to_envelope_levels(bool header);

private:
    struct Envelope _envelope_levels[2*DS_MAX_DSO_PROBES_NUM][ScaleStepCount];
    bool _envelope_en;
    bool _envelope_done;
    bool _instant;

    friend class DsoSnapshotTest::Basic;
};

} // namespace data
} // namespace pv

#endif // DSVIEW_PV_DATA_DSOSNAPSHOT_H
