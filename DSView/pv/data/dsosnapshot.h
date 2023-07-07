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


#ifndef DSVIEW_PV_DATA_DSOSNAPSHOT_H
#define DSVIEW_PV_DATA_DSOSNAPSHOT_H

#include <utility>
#include <vector>

#include <libsigrok.h> 
#include "snapshot.h"
 
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

    static const uint64_t LeafBlockPower = 21;
    static const uint64_t LeafBlockSamples = 1 << LeafBlockPower;
    static const uint64_t LeafMask = ~(~0ULL << LeafBlockPower);

    static const int VrmsScaleFactor;

private:
    void init_all();

public:
    DsoSnapshot();

    virtual ~DsoSnapshot();

    void clear();
    void init();

    void first_payload(const sr_datafeed_dso &dso, uint64_t total_sample_count,
                       GSList *channels, bool instant, bool isFile);

    void append_payload(const sr_datafeed_dso &dso);
    const uint8_t* get_samples(int64_t start_sample, int64_t end_sample, uint16_t ch_index);

	void get_envelope_section(EnvelopeSection &s,
        uint64_t start, uint64_t end, float min_length, int probe_index);

    void enable_envelope(bool enable);
    double cal_vrms(double zero_off, int index);
    double cal_vmean(int index);
    bool has_data(int sig_index);
    int get_block_num();
    uint64_t get_block_size(int block_index);

    bool get_max_min_value(uint8_t &maxv, uint8_t &minv, int chan_index);

    inline void set_threshold(float threshold){
        _threshold = threshold;
    }

    inline float get_threshold(){
        return _threshold;
    }

    inline void set_measure_voltage_factor(uint64_t v, int index){   
        index == 0 ? _measure_voltage_factor1 = v : _measure_voltage_factor2 = v;
    }

    inline uint64_t get_measure_voltage_factor(int index){
        return index == 0 ? _measure_voltage_factor1 : _measure_voltage_factor2;
    }

    inline void set_data_scale(float scale, int index){
        index == 0 ? _data_scale1 = scale : _data_scale2 = scale;
    }

    inline float get_data_scale(int index){
        return index == 0 ? _data_scale1 : _data_scale2;
    }

    inline bool is_file(){
        return _is_file;
    }

private:
    void append_data(void *data, uint64_t samples, bool instant);
    void free_envelop();
	void reallocate_envelope(Envelope &l);
    void append_payload_to_envelope_levels(bool header);
    void free_data();   
    int  get_ch_order(int sig_index);

private:
    struct Envelope _envelope_levels[2*DS_MAX_DSO_PROBES_NUM][ScaleStepCount];
    bool    _envelope_en;
    bool    _envelope_done;
    bool    _instant; 
    std::vector<uint8_t*>   _ch_data;
    float   _threshold;
    uint64_t _measure_voltage_factor1;
    uint64_t _measure_voltage_factor2;
    float _data_scale1 = 0;
    float _data_scale2 = 0;
    bool    _is_file;
 
    friend class DsoSnapshotTest::Basic;
};

} // namespace data
} // namespace pv

#endif // DSVIEW_PV_DATA_DSOSNAPSHOT_H
