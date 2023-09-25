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

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <algorithm>
 
#include "dsosnapshot.h"
#include "../dsvdef.h"
#include "../log.h"

using namespace std;

namespace pv {
namespace data {

const int DsoSnapshot::EnvelopeScalePower = 8;
const int DsoSnapshot::EnvelopeScaleFactor = 1 << EnvelopeScalePower;
const float DsoSnapshot::LogEnvelopeScaleFactor =
	logf(EnvelopeScaleFactor);
const uint64_t DsoSnapshot::EnvelopeDataUnit = 4*1024;	// bytes

const int DsoSnapshot::VrmsScaleFactor = 1 << 8;

DsoSnapshot::DsoSnapshot() :
    Snapshot(sizeof(uint16_t), 1, 1)
{   
    _envelope_en = false;
    _envelope_done = false;
    _instant = false;
    _threshold = 0;
    _measure_voltage_factor1 = 0;
    _measure_voltage_factor2 = 0;
    _data_scale1 = 0;
    _data_scale2 = 0;  
    _is_file = false;

	memset(_envelope_levels, 0, sizeof(_envelope_levels));
}

DsoSnapshot::~DsoSnapshot()
{
    free_envelop();
}

void DsoSnapshot::free_envelop()
{
    for (unsigned int i = 0; i < _channel_num; i++) {
        for(auto &e : _envelope_levels[i]) {
            if (e.samples)
                free(e.samples);
        }
    }
    memset(_envelope_levels, 0, sizeof(_envelope_levels));
}

void DsoSnapshot::init()
{
    std::lock_guard<std::mutex> lock(_mutex);
    init_all();    
}

void DsoSnapshot::init_all()
{
    _sample_count = 0;
    _ring_sample_count = 0;
    _memory_failed = false;
    _last_ended = true;
    _envelope_done = false;   
    _is_file = false; 

    for (unsigned int i = 0; i < _channel_num; i++) {
        for (unsigned int level = 0; level < ScaleStepCount; level++) {
            _envelope_levels[i][level].length = 0;
            _envelope_levels[i][level].data_length = 0;
        }
    }
}

void DsoSnapshot::clear()
{
    std::lock_guard<std::mutex> lock(_mutex);
    free_data();
    free_envelop();
    init_all();
    _envelope_en = false;
}

void DsoSnapshot::free_data()
{
    Snapshot::free_data();

    for (int i=0; i<(int)_ch_data.size(); i++)
    {
        void *p = _ch_data[i];
        free(p);
    }

    _ch_data.clear();
}

void DsoSnapshot::first_payload(const sr_datafeed_dso &dso, uint64_t total_sample_count,
                                GSList *channels, bool instant, bool isFile)
{
    assert(channels);  

    bool channel_changed = false;
    uint16_t channel_num = 0;
    _is_file = isFile;

    for (const GSList *l = channels; l; l = l->next) {
        sr_channel *const probe = (sr_channel*)l->data;

        if (probe->type == SR_CHANNEL_DSO) {
            if (probe->enabled || isFile){
                channel_num++;
                if (!channel_changed){
                    channel_changed = !has_data(probe->index);
                }
            } 
        }
    }

    assert(channel_num != 0);

    _instant = instant;
    bool isOk = true;

    if (total_sample_count != _total_sample_count
        || channel_num != _channel_num
        || channel_changed
        || isFile){
        
        std::lock_guard<std::mutex> lock(_mutex);

        free_data();

        _ch_index.clear();
        _total_sample_count = total_sample_count;
        _channel_num = channel_num; 

         for (const GSList *l = channels; l; l = l->next) {
            sr_channel *const probe = (sr_channel*)l->data;

            if (probe->type == SR_CHANNEL_DSO && (probe->enabled || isFile)) {
                
                uint8_t *chan_buffer = (uint8_t*)malloc(total_sample_count + 1);
                if (chan_buffer == NULL){
                    isOk = false;
                    dsv_err("DsoSnapshot::first_payload, Malloc memory failed!");
                    break;
                }
                _ch_data.push_back(chan_buffer);
                _ch_index.push_back(probe->index);
            }
        }
        
        if (isOk) {
            free_envelop();

            for (unsigned int i = 0; i < _channel_num; i++) {
                uint64_t envelop_count = _total_sample_count / EnvelopeScaleFactor;

                for (unsigned int level = 0; level < ScaleStepCount; level++) {
                    
                    envelop_count = ((envelop_count + EnvelopeDataUnit - 1) / EnvelopeDataUnit) 
                                        * EnvelopeDataUnit;

                    uint64_t buffer_len = envelop_count * sizeof(EnvelopeSample);
                    _envelope_levels[i][level].samples = (EnvelopeSample*)malloc(buffer_len);
                    
                    if (_envelope_levels[i][level].samples == NULL) {
                        dsv_err("DsoSnapshot::first_payload, malloc failed!");
                        isOk = false;
                        break;
                    }
                    
                    envelop_count = envelop_count / EnvelopeScaleFactor;
                }
                if (!isOk)
                    break;
            }
        }
    }

    if (isOk) {
        _memory_failed = false;
        append_payload(dso);
        _last_ended = false;
    }
    else {
        std::lock_guard<std::mutex> lock(_mutex);
        free_data();
        free_envelop();
        _memory_failed = true;
    }
}

void DsoSnapshot::append_payload(const sr_datafeed_dso &dso)
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_channel_num > 0 && dso.num_samples > 0) {       
        append_data(dso.data, dso.num_samples, _instant);

        // Generate the first mip-map from the data
        if (_envelope_en)
            append_payload_to_envelope_levels(dso.samplerate_tog);
    }
}

void DsoSnapshot::append_data(void *data, uint64_t samples, bool instant)
{
    uint64_t old_sample_count = _sample_count;

    if (instant) { 
        if(_sample_count + samples > _total_sample_count)
            samples = _total_sample_count - _sample_count;
        _sample_count += samples;
    }
    else {
        _sample_count = samples;
    }

    assert(_sample_count <= _total_sample_count);
     
    for (unsigned int ch = 0; ch < _channel_num; ch++)
    {
        uint8_t *src = (uint8_t*)data + ch;
        uint8_t *dest = _ch_data[ch];

        if (instant){
            dest += old_sample_count;
        }

        for (uint64_t i = 0; i < samples; i++)
        {
            *dest++ = *src;
            src += _channel_num;
        }  
    }
}

void DsoSnapshot::enable_envelope(bool enable)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_envelope_done && enable)
        append_payload_to_envelope_levels(true);
    _envelope_en = enable;
}

const uint8_t *DsoSnapshot::get_samples(int64_t start_sample, int64_t end_sample, uint16_t ch_index)
{
    std::lock_guard<std::mutex> lock(_mutex);

	assert(start_sample >= 0);
    assert(start_sample < (int64_t)_sample_count);
	assert(end_sample >= 0);
    assert(end_sample < (int64_t)_sample_count);
	assert(start_sample <= end_sample);

    int order = get_ch_order(ch_index);

    if (order == -1){
        dsv_err("The channel index is not exist:%d", ch_index);
        assert(false);
    } 

    return (uint8_t*)_ch_data[order] + start_sample;
}

void DsoSnapshot::get_envelope_section(EnvelopeSection &s,
    uint64_t start, uint64_t end, float min_length, int probe_index)
{
	assert(end <= get_sample_count());
	assert(start <= end);
	assert(min_length > 0);

    if (!_envelope_done) {
        s.length = 0;
        return;
    }

	const unsigned int min_level = max((int)floorf(logf(min_length) /
		LogEnvelopeScaleFactor) - 1, 0);
	const unsigned int scale_power = (min_level + 1) *
		EnvelopeScalePower;
	start >>= scale_power;
	end >>= scale_power;

	s.start = start << scale_power;
	s.scale = 1 << scale_power;
    if (_envelope_levels[probe_index][min_level].length == 0)
        s.length = 0;
    else
        s.length = end - start;

    s.samples = _envelope_levels[probe_index][min_level].samples + start;
}

void DsoSnapshot::reallocate_envelope(Envelope &e)
{
	const uint64_t new_data_length = ((e.length + EnvelopeDataUnit - 1) /
		EnvelopeDataUnit) * EnvelopeDataUnit;
    if (new_data_length > e.data_length)
	{
		e.data_length = new_data_length;
	}
}

void DsoSnapshot::append_payload_to_envelope_levels(bool header)
{
    for (unsigned int i = 0; i < _channel_num; i++) {
        Envelope &e0 = _envelope_levels[i][0];
        uint64_t prev_length;
        EnvelopeSample *dest_ptr;

        if (header)
            prev_length = 0;
        else
            prev_length = e0.length;
        e0.length = _sample_count / EnvelopeScaleFactor;

        if (e0.length == 0)
            return;
        if (e0.length == prev_length)
            prev_length = 0;

        // Expand the data buffer to fit the new samples
        reallocate_envelope(e0);

        assert(e0.samples);

        dest_ptr = e0.samples + prev_length;

        // Iterate through the samples to populate the first level mipmap
        const uint8_t *const stop_src_ptr = (uint8_t*)_ch_data[i] + e0.length * EnvelopeScaleFactor;
        const uint8_t *src_ptr = (uint8_t*)_ch_data[i] + prev_length * EnvelopeScaleFactor;

        for (; src_ptr < stop_src_ptr; src_ptr += EnvelopeScaleFactor)
        {
            const uint8_t *begin_src_ptr = src_ptr;
            const uint8_t *const end_src_ptr = src_ptr + EnvelopeScaleFactor;

            EnvelopeSample sub_sample;
            sub_sample.min = *begin_src_ptr;
            sub_sample.max = *begin_src_ptr;

            while (begin_src_ptr < end_src_ptr)
            {
                sub_sample.min = ds_min(sub_sample.min, *begin_src_ptr);
                sub_sample.max = ds_max(sub_sample.max, *begin_src_ptr);
                begin_src_ptr++;
            }
            
            *dest_ptr++ = sub_sample;
        }

        // Compute higher level mipmaps
        for (unsigned int level = 1; level < ScaleStepCount; level++)
        {
            Envelope &e = _envelope_levels[i][level];
            const Envelope &el = _envelope_levels[i][level-1];

            // Expand the data buffer to fit the new samples
            if (header)
                prev_length = 0;
            else
                prev_length = e.length;
            e.length = el.length / EnvelopeScaleFactor;

            // Break off if there are no more samples to computed
            if (e.length == 0)
                break;
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
    _envelope_done = true;
}

double DsoSnapshot::cal_vrms(double zero_off, int index)
{
    assert(index >= 0);

    // root-meam-squart value
    double vrms_pre = 0;
    double vrms = 0;
    double tmp;

    // Iterate through the samples to populate the first level mipmap
    const uint8_t *const stop_src_ptr = (uint8_t*)_ch_data[index] + _sample_count;
    const uint8_t *src_ptr = (uint8_t*)_ch_data[index];

    for (;src_ptr < stop_src_ptr; src_ptr += VrmsScaleFactor)
    {
        const uint8_t * begin_src_ptr = src_ptr;
        const uint8_t *const end_src_ptr = src_ptr + VrmsScaleFactor;

        while (begin_src_ptr < end_src_ptr)
        {
            tmp = (zero_off - *begin_src_ptr);
            vrms += tmp * tmp;
            begin_src_ptr++;
        }
        vrms = vrms_pre + vrms / _sample_count;
        vrms_pre = vrms;
    }
    vrms = pow(vrms, 0.5);

    return vrms;
}

double DsoSnapshot::cal_vmean(int index)
{
    assert(index >= 0);

    // mean value
    double vmean_pre = 0;
    double vmean = 0;

    // Iterate through the samples to populate the first level mipmap
    const uint8_t *const stop_src_ptr = (uint8_t*)_ch_data[index] + _sample_count;
    const uint8_t *src_ptr = (uint8_t*)_ch_data[index];

    for (; src_ptr < stop_src_ptr; src_ptr += VrmsScaleFactor)
    {
        const uint8_t * begin_src_ptr = src_ptr;
        const uint8_t *const end_src_ptr = src_ptr + VrmsScaleFactor;

        while (begin_src_ptr < end_src_ptr)
        {
            vmean += *begin_src_ptr;
            begin_src_ptr += _channel_num;
        }
        vmean = vmean_pre + vmean / _sample_count;
        vmean_pre = vmean;
    }

    return vmean;
}

int DsoSnapshot::get_block_num()
{
    const uint64_t size = _sample_count * get_unit_bytes() * get_channel_num();
    return (size >> LeafBlockPower) +
           ((size & LeafMask) != 0);
}

uint64_t DsoSnapshot::get_block_size(int block_index)
{
    assert(block_index < get_block_num());

    if (block_index < get_block_num() - 1) {
        return LeafBlockSamples;
    } else {
        const uint64_t size = _sample_count * get_unit_bytes() * get_channel_num();
        if (size % LeafBlockSamples == 0)
            return LeafBlockSamples;
        else
            return size % LeafBlockSamples;
    }
}

bool DsoSnapshot::get_max_min_value(uint8_t &maxv, uint8_t &minv, int chan_index)
{
    std::lock_guard<std::mutex> lock(_mutex);

    if (_sample_count == 0){
        return false;
    }

    if (chan_index < 0 || chan_index >= (int)_ch_data.size()){
        assert(false);
    }

    uint8_t *p = _ch_data[chan_index];
    maxv = *p;
    minv = *p;

    for (uint64_t i=1; i<_sample_count; i++){
        p++;
        if (*p > maxv)
            maxv = *p;
        if (*p < minv)
            minv = *p;
    }
    
    return true;
}

bool DsoSnapshot::has_data(int sig_index)
{
    return get_ch_order(sig_index) != -1;
}

int DsoSnapshot::get_ch_order(int sig_index)
{
    uint16_t order = 0;

    for (uint16_t i : _ch_index) {
        if (i == sig_index)
            return order;
        else
            order++;
    }

    return -1;
}

} // namespace data
} // namespace pv
