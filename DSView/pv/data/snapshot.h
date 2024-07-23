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

#ifndef DSVIEW_PV_DATA_SNAPSHOT_H
#define DSVIEW_PV_DATA_SNAPSHOT_H

#include <mutex>
#include <vector>

namespace pv {
namespace data {

class Snapshot
{
public:
    Snapshot(int unit_size, uint64_t total_sample_count, unsigned int channel_num);

	virtual ~Snapshot();

    virtual void clear() = 0;
    virtual void init() = 0;

	uint64_t get_sample_count();
    uint64_t get_total_sample_count();
    uint64_t get_ring_sample_count();
    uint64_t get_ring_start();
    uint64_t get_ring_end();

    inline int unit_size(){
        return _unit_size;
    }

    inline uint8_t get_unit_bytes(){
        return _unit_bytes;
    }

    inline bool memory_failed(){
        return _memory_failed;
    }

    bool empty();

    inline bool last_ended(){
        return _last_ended;
    }

    inline unsigned int get_channel_num(){
        return _channel_num;
    }

    inline bool have_data(){
        return !empty();
    }

    inline double samplerate(){
        return _samplerate; 
    }

    void set_samplerate(double samplerate);

    virtual void capture_ended();
    virtual bool has_data(int index) = 0;
    virtual int get_block_num() = 0;
    virtual uint64_t get_block_size(int block_index) = 0;
     

protected:
    virtual void free_data();

    inline uint64_t sample_count(){
        return _sample_count;
    }

    uint64_t ring_start();
    uint64_t ring_end();

protected:
    mutable std::mutex  _mutex;  
    mutable std::vector<uint16_t> _ch_index;

    uint64_t    _capacity;
    unsigned int _channel_num;
	volatile uint64_t    _sample_count;
    volatile uint64_t    _total_sample_count;
    volatile uint64_t    _ring_sample_count;
	int         _unit_size;
    uint8_t     _unit_bytes;
    uint16_t    _unit_pitch;
    bool        _memory_failed;
    bool        _last_ended;
    double      _samplerate;
};

} // namespace data
} // namespace pv

#endif // DSVIEW_PV_DATA_SNAPSHOT_H
