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


#ifndef DSVIEW_PV_DATA_GROUPSNAPSHOT_H
#define DSVIEW_PV_DATA_GROUPSNAPSHOT_H

#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>

#include "../view/signal.h"

#include <utility>
#include <vector>
#include <list>

namespace GroupSnapshotTest {
class Basic;
}

namespace pv {
namespace data {

class LogicSnapshot;

class GroupSnapshot
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
    static const unsigned int ScaleStepCount = 16;
	static const int EnvelopeScalePower;
	static const int EnvelopeScaleFactor;
	static const float LogEnvelopeScaleFactor;
	static const uint64_t EnvelopeDataUnit;
    static const uint16_t value_mask[16];

public:
    GroupSnapshot(const boost::shared_ptr<LogicSnapshot> &_logic_snapshot, std::list<int> index_list);

    virtual ~GroupSnapshot();

    void clear();
    void init();

    void append_payload();

    uint64_t get_sample_count() const;

    const uint16_t* get_samples(int64_t start_sample,
        int64_t end_sample);

	void get_envelope_section(EnvelopeSection &s,
		uint64_t start, uint64_t end, float min_length) const;

private:
	void reallocate_envelope(Envelope &l);

	void append_payload_to_envelope_levels();

private:
	struct Envelope _envelope_levels[ScaleStepCount];
    //mutable boost::recursive_mutex _mutex;
    const void *_data;
    uint64_t _sample_count;
    int _unit_size;
    boost::shared_ptr<view::Signal> _signal;
    std::list<int> _index_list;
    uint16_t _mask;
    int _bubble_start[32];
    int _bubble_end[32];

    friend class GroupSnapshotTest::Basic;
};

} // namespace data
} // namespace pv

#endif // DSVIEW_PV_DATA_GROUPSNAPSHOT_H
