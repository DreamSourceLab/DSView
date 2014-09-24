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


#ifndef DSLOGIC_PV_DATA_LOGICSNAPSHOT_H
#define DSLOGIC_PV_DATA_LOGICSNAPSHOT_H

#include "snapshot.h"

#include <utility>
#include <vector>

namespace LogicSnapshotTest {
class Pow2;
class Basic;
class LargeData;
class Pulses;
class LongPulses;
}

namespace pv {
namespace data {

class LogicSnapshot : public Snapshot
{
private:
	struct MipMapLevel
	{
		uint64_t length;
		uint64_t data_length;
		void *data;
	};

private:
	static const unsigned int ScaleStepCount = 10;
	static const int MipMapScalePower;
	static const int MipMapScaleFactor;
	static const float LogMipMapScaleFactor;
	static const uint64_t MipMapDataUnit;

public:
    typedef std::pair<uint64_t, bool> EdgePair;

public:
    LogicSnapshot(const sr_datafeed_logic &logic, uint64_t _total_sample_len, unsigned int channel_num);

	virtual ~LogicSnapshot();

	void append_payload(const sr_datafeed_logic &logic);

    void get_samples(uint8_t *const data,
        int64_t start_sample, int64_t end_sample) const;

private:
	void reallocate_mipmap_level(MipMapLevel &m);

	void append_payload_to_mipmap();

public:
	/**
	 * Parses a logic data snapshot to generate a list of transitions
	 * in a time interval to a given level of detail.
	 * @param[out] edges The vector to place the edges into.
	 * @param[in] start The start sample index.
	 * @param[in] end The end sample index.
	 * @param[in] min_length The minimum number of samples that
	 * can be resolved at this level of detail.
	 * @param[in] sig_index The index of the signal.
	 **/
	void get_subsampled_edges(std::vector<EdgePair> &edges,
		uint64_t start, uint64_t end,
		float min_length, int sig_index);

    int get_first_edge(uint64_t &edge_index, bool &edge,
                       uint64_t start, uint64_t end,
                       int sig_index, int edge_type,
                       int flag_index, int flag);

    void get_edges(std::vector<EdgePair> &edges,
        uint64_t start, uint64_t end, int sig_index, int edge_type);

    uint64_t get_min_pulse(uint64_t start, uint64_t end, int sig_index);

private:
	uint64_t get_subsample(int level, uint64_t offset) const;

	static uint64_t pow2_ceil(uint64_t x, unsigned int power);

private:
	struct MipMapLevel _mip_map[ScaleStepCount];
	uint64_t _last_append_sample;

	friend class LogicSnapshotTest::Pow2;
	friend class LogicSnapshotTest::Basic;
	friend class LogicSnapshotTest::LargeData;
	friend class LogicSnapshotTest::Pulses;
	friend class LogicSnapshotTest::LongPulses;
};

} // namespace data
} // namespace pv

#endif // DSLOGIC_PV_DATA_LOGICSNAPSHOT_H
