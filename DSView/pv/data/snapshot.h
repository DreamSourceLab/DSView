/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
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


#ifndef DSVIEW_PV_DATA_SNAPSHOT_H
#define DSVIEW_PV_DATA_SNAPSHOT_H

#include <libsigrok4DSL/libsigrok.h>

#include <boost/thread.hpp>

namespace pv {
namespace data {

class Snapshot
{
public:
    Snapshot(int unit_size, uint64_t total_sample_count, unsigned int channel_num);

	virtual ~Snapshot();

    int init(uint64_t _total_sample_len);

	uint64_t get_sample_count() const;

    void * get_data() const;

    int unit_size() const;

    bool buf_null() const;

    unsigned int get_channel_num() const;

    uint64_t get_sample(uint64_t index) const;

protected:
	void append_data(void *data, uint64_t samples);
    void refill_data(void *data, uint64_t samples, bool instant);

protected:
	mutable boost::recursive_mutex _mutex;
	void *_data;
    unsigned int _channel_num;
	uint64_t _sample_count;
    uint64_t _total_sample_count;
    uint64_t _ring_sample_count;
	int _unit_size;
};

} // namespace data
} // namespace pv

#endif // DSVIEW_PV_DATA_SNAPSHOT_H
