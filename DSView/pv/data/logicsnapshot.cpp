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
 
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
 
#include "logicsnapshot.h"
#include "../dsvdef.h"
#include "../log.h"
#include "../utility/array.h"
#include "../log.h"

using namespace std;

int _free_count = 0;

namespace pv {
namespace data {

const uint64_t LogicSnapshot::LevelMask[LogicSnapshot::ScaleLevel] = {
    ~(~0ULL << ScalePower) << 0 * ScalePower,
    ~(~0ULL << ScalePower) << 1 * ScalePower,
    ~(~0ULL << ScalePower) << 2 * ScalePower,
    ~(~0ULL << ScalePower) << 3 * ScalePower,
};
const uint64_t LogicSnapshot::LevelOffset[LogicSnapshot::ScaleLevel] = {
    0,
    (uint64_t)pow(Scale, 3),
    (uint64_t)pow(Scale, 3) + (uint64_t)pow(Scale, 2),
    (uint64_t)pow(Scale, 3) + (uint64_t)pow(Scale, 2) + (uint64_t)pow(Scale, 1),
};

LogicSnapshot::LogicSnapshot() :
    Snapshot(1, 0, 0)
{
    _channel_num = 0;
    _total_sample_count = 0;
    _is_loop = false;
    _loop_offset = 0;
    _able_free = true;
    _is_search_stop = false;
}

LogicSnapshot::~LogicSnapshot()
{
}

void LogicSnapshot::free_data()
{
    Snapshot::free_data();

    for(auto& iter : _ch_data) {
        for(auto& iter_rn : iter) {
            for (unsigned int k = 0; k < Scale; k++){
                if (iter_rn.lbp[k] != NULL)
                    free(iter_rn.lbp[k]);
            }
        }
        std::vector<struct RootNode> void_vector;
        iter.swap(void_vector);
    }
    _ch_data.clear();
    _sample_count = 0;

    for(void *p : _free_block_list){
        free(p);
    }
    _free_block_list.clear();
}

void LogicSnapshot::init()
{
    std::lock_guard<std::mutex> lock(_mutex);
    init_all(); 
}

void LogicSnapshot::init_all()
{
    _sample_count = 0;
    _ring_sample_count = 0;
    _byte_fraction = 0;
    _ch_fraction = 0;
    _dest_ptr = NULL;
    _memory_failed = false;
    _last_ended = true;
    _loop_offset = 0;
    _able_free = true;
}

void LogicSnapshot::clear()
{
    std::lock_guard<std::mutex> lock(_mutex);
    free_data();
    init_all();

    _free_count = 0;
}

void LogicSnapshot::first_payload(const sr_datafeed_logic &logic, uint64_t total_sample_count, GSList *channels, bool able_free)
{
    bool channel_changed = false;
    uint16_t channel_num = 0;
    _able_free = able_free;
    _lst_free_block_index = 0;

    for(void *p : _free_block_list){
        free(p);
    }
    _free_block_list.clear();

    for (const GSList *l = channels; l; l = l->next) {
        sr_channel *const probe = (sr_channel*)l->data;
        if (probe->type == SR_CHANNEL_LOGIC && probe->enabled) {
            channel_num++;
            if (!channel_changed){
                channel_changed = !has_data(probe->index);
            }
        }
    }

    if (total_sample_count != _total_sample_count
        || channel_num != _channel_num
        || channel_changed
        || _is_loop) {

        free_data();
        _ch_index.clear();

        _total_sample_count = total_sample_count;
        _channel_num = channel_num;
        uint64_t rootnode_size = (_total_sample_count + RootNodeSamples - 1) / RootNodeSamples;

        if (_is_loop){
            rootnode_size += 2;
        }

        for (const GSList *l = channels; l; l = l->next) {
            sr_channel *const probe = (sr_channel*)l->data;

            if (probe->type == SR_CHANNEL_LOGIC && probe->enabled) {
                std::vector<struct RootNode> root_vector;

                for (uint64_t j = 0; j < rootnode_size; j++) {
                    struct RootNode rn;
                    rn.tog = 0;
                    rn.first = 0;
                    rn.last = 0;
                    memset(rn.lbp, 0, sizeof(rn.lbp));
                    root_vector.push_back(rn);
                }
               
                _ch_data.push_back(root_vector);
                _ch_index.push_back(probe->index);
            }
        }

        if (_ch_index.size() == 0){
            dsv_info("ERROR: all channels disalbed");
            assert(0);
        }
    }
    else {
        for(auto& iter : _ch_data) {
            for(auto& iter_rn : iter) {
                iter_rn.tog = 0;
                iter_rn.first = 0;
                iter_rn.last = 0;

                for (int j=0; j<64; j++){
                    if (iter_rn.lbp[j] != NULL)
                        memset(iter_rn.lbp[j], 0, LeafBlockSpace);
                }
            }
        }
    }

    assert(_channel_num < CHANNEL_MAX_COUNT);

    _sample_count = 0;
    _ring_sample_count = 0;

    for (unsigned int i = 0; i < _channel_num; i++) {
        _last_sample[i] = 0;
        _last_calc_count[i] = 0;
        _cur_ref_block_indexs[i].root_index = 0;
        _cur_ref_block_indexs[i].lbp_index = 0;
    }

    append_payload(logic);
    _last_ended = false;
}

void LogicSnapshot::append_payload(const sr_datafeed_logic &logic)
{
    std::lock_guard<std::mutex> lock(_mutex);

    append_cross_payload(logic);
}

void LogicSnapshot::append_cross_payload(const sr_datafeed_logic &logic)
{
    assert(logic.format == LA_CROSS_DATA);
    assert(logic.length >= ScaleSize * _channel_num);
    assert(logic.data);

    uint8_t *data_src_ptr = (uint8_t*)logic.data;
    uint64_t len = logic.length;
    uint64_t index0 = 0;
    uint64_t index1 = 0;
    uint64_t offset = 0;
    void *lbp = NULL;

    // samples not accurate, lead to a larger _sampole_count
    // _sample_count should be fixed in the last packet
    // so _total_sample_count must be align to LeafBlock
    uint64_t samples = ceil(logic.length * 8.0 / _channel_num);

    if (_sample_count + samples < _total_sample_count){
        _sample_count += samples;
    }
    else{
        if (_sample_count == _total_sample_count && !_is_loop)
            return;
        _sample_count = _total_sample_count;
    }

    if (_is_loop)
    {
        if (_loop_offset >= LeafBlockSamples * Scale){     
            move_first_node_to_last();
            _loop_offset -= LeafBlockSamples * Scale;
            _lst_free_block_index = 0;

       
            dsv_info("free a node %d", ++_free_count);
        }
        else{
            int free_count = _loop_offset / LeafBlockSamples;
            if (free_count > _lst_free_block_index){
                free_head_blocks(free_count);
            }
        }
    }
 
    _ring_sample_count += _loop_offset;
 
    // bit align
    while ((_ch_fraction != 0 || _byte_fraction != 0) && len > 0) 
    {
        assert(_dest_ptr);

        do{
            *_dest_ptr++ = *data_src_ptr++;
            _byte_fraction = (_byte_fraction + 1) % 8;
            len--; 
        }
        while (_byte_fraction != 0 && len > 0);

        if (_byte_fraction == 0) {
            index0 = _ring_sample_count / LeafBlockSamples / RootScale;
            index1 = (_ring_sample_count / LeafBlockSamples) % RootScale;
            offset = (_ring_sample_count % LeafBlockSamples) / 8;

            //switch to the next channel.
            _ch_fraction = (_ch_fraction + 1) % _channel_num;

            lbp = _ch_data[_ch_fraction][index0].lbp[index1];
            if (lbp == NULL){
                lbp = malloc(LeafBlockSpace);
                if (lbp == NULL){
                    dsv_err("LogicSnapshot::append_cross_payload, Malloc memory failed!");
                    return;
                }
                _ch_data[_ch_fraction][index0].lbp[index1] = lbp;
                memset(lbp, 0, LeafBlockSpace);
            }

            _dest_ptr = (uint8_t*)lbp + offset;

            // The last channel is read end, so the channel index switch to first.
            if (_ch_fraction == 0){
                _ring_sample_count += Scale;

                if (_ring_sample_count % LeafBlockSamples == 0){
                    calc_mipmap(_channel_num - 1, index0, index1, LeafBlockSamples, true);
                }                                
                break;
            }
        }
    } 

    // append data 
    assert(_ch_fraction == 0);
    assert(_byte_fraction == 0);
    assert(_ring_sample_count % Scale == 0);

    uint64_t align_sample_count = _ring_sample_count;
    uint64_t *read_ptr = (uint64_t*)data_src_ptr;
    void *end_read_ptr = (uint8_t*)data_src_ptr + len;
  
    uint64_t filled_sample = align_sample_count % LeafBlockSamples;
    uint64_t old_filled_sample = filled_sample;     
    uint64_t* chans_read_addr[CHANNEL_MAX_COUNT];
 
    for (unsigned int i = 0; i < _channel_num; i++){
        chans_read_addr[i] = (uint64_t*)data_src_ptr + i; 
    }
    
    uint16_t fill_chan_index = 0;
    uint16_t last_chan_index = 0;
    index0 =  align_sample_count / LeafBlockSamples / RootScale;
    index1 = (align_sample_count / LeafBlockSamples) % RootScale;
    offset =  align_sample_count % LeafBlockSamples;

    if (index0 >= _ch_data[0].size()){
        assert(false);
    }
    
    lbp = _ch_data[fill_chan_index][index0].lbp[index1];
    if (lbp == NULL){
        lbp = malloc(LeafBlockSpace);
        if (lbp == NULL){
            dsv_err("LogicSnapshot::append_cross_payload, Malloc memory failed!");
            return;
        }
        _ch_data[fill_chan_index][index0].lbp[index1] = lbp;
        memset(lbp, 0, LeafBlockSpace);
    }

    uint64_t *write_ptr = (uint64_t*)lbp + offset / Scale;

    while (len >= 8)
    {     
        *write_ptr++ = *read_ptr;
        read_ptr += _channel_num;
        len -= 8;
        filled_sample += Scale;

        last_chan_index++;
        if (last_chan_index == _channel_num){
            last_chan_index = 0;
        }
  
        if (filled_sample == LeafBlockSamples)
        {
            calc_mipmap(fill_chan_index, index0, index1, LeafBlockSamples, true);

            chans_read_addr[fill_chan_index] = read_ptr; //Save the current read position.
            fill_chan_index = (fill_chan_index + 1) % _channel_num; //Switch to the next channel index.
            
            //The last channel's data read ends, update the align sample count.
            if (fill_chan_index == 0){
                align_sample_count += (filled_sample - old_filled_sample);
            }

            index0 =  align_sample_count / LeafBlockSamples / RootScale;
            index1 = (align_sample_count / LeafBlockSamples) % RootScale;
            offset =  align_sample_count % LeafBlockSamples;
            filled_sample = align_sample_count % LeafBlockSamples;
            old_filled_sample = filled_sample;

            lbp = _ch_data[fill_chan_index][index0].lbp[index1];
            if (lbp == NULL){
                lbp = malloc(LeafBlockSpace);
                if (lbp == NULL){
                    dsv_err("LogicSnapshot::append_cross_payload, Malloc memory failed!");
                    return;
                }
                _ch_data[fill_chan_index][index0].lbp[index1] = lbp;
                memset(lbp, 0, LeafBlockSpace);
            }

            write_ptr = (uint64_t*)lbp + offset / Scale;
            read_ptr = chans_read_addr[fill_chan_index];
        } 
        else if (read_ptr >= end_read_ptr) 
        {  
            calc_mipmap(fill_chan_index, index0, index1, filled_sample, false);

            fill_chan_index = (fill_chan_index + 1) % _channel_num;    

            if (fill_chan_index == 0){
                align_sample_count += (filled_sample - old_filled_sample);
            }

            index0 =  align_sample_count / LeafBlockSamples / RootScale;
            index1 = (align_sample_count / LeafBlockSamples) % RootScale;
            offset =  align_sample_count % LeafBlockSamples;
            filled_sample = align_sample_count % LeafBlockSamples;
            old_filled_sample = filled_sample;

            lbp = _ch_data[fill_chan_index][index0].lbp[index1];
            if (lbp == NULL){
                lbp = malloc(LeafBlockSpace);
                if (lbp == NULL){
                    dsv_err("LogicSnapshot::append_cross_payload, Malloc memory failed!");
                    return;
                }
                _ch_data[fill_chan_index][index0].lbp[index1] = lbp;
                memset(lbp, 0, LeafBlockSpace);
            }

            write_ptr = (uint64_t*)lbp + offset / Scale;   
            read_ptr = chans_read_addr[fill_chan_index];
        }
    }

    _ring_sample_count = align_sample_count;
    _ring_sample_count -= _loop_offset;

    if (align_sample_count > _total_sample_count){        
        _loop_offset = align_sample_count - _total_sample_count; 
        _ring_sample_count = _total_sample_count; 
    }

    _ch_fraction = last_chan_index;

    lbp = _ch_data[_ch_fraction][index0].lbp[index1];
    if (lbp == NULL){
        lbp = malloc(LeafBlockSpace);
        if (lbp == NULL){
            dsv_err("LogicSnapshot::append_cross_payload, Malloc memory failed!");
            return;
        }
        _ch_data[_ch_fraction][index0].lbp[index1] = lbp;
        memset(lbp, 0, LeafBlockSpace);
    }

    _dest_ptr = (uint8_t*)lbp + offset / 8;  
 
    if (len > 0){
        uint8_t *src_ptr = (uint8_t*)end_read_ptr - len;
        _byte_fraction += len;

        while (len > 0){
            *_dest_ptr++ = *src_ptr++;
            len--;
        }
    }   
}

void LogicSnapshot::capture_ended()
{
    std::lock_guard<std::mutex> lock(_mutex);

    Snapshot::capture_ended();  

    _sample_count = _ring_sample_count;
    _ring_sample_count += _loop_offset;
    
    uint64_t index0 = _ring_sample_count / LeafBlockSamples / RootScale;
    uint64_t index1 = (_ring_sample_count / LeafBlockSamples) % RootScale;
    uint64_t offset = (_ring_sample_count % LeafBlockSamples) / 8;

    _ring_sample_count -= _loop_offset;

    if (offset > 0)
    {
        for (unsigned int chan=0; chan<_channel_num; chan++)
        { 
            if (_ch_data[chan][index0].lbp[index1] == NULL){
                dsv_err("ERROR:LogicSnapshot::capture_ended(),buffer is null.");
                assert(false);
            }
            const uint64_t *end_ptr = (uint64_t*)_ch_data[chan][index0].lbp[index1] + (LeafBlockSamples / Scale);
            uint64_t *ptr = (uint64_t*)((uint8_t*)_ch_data[chan][index0].lbp[index1] + offset);

            while (ptr < end_ptr){
                *ptr++ = 0;
            }

            calc_mipmap(chan, index0, index1, offset * 8, true);
        }  
    }
}

void LogicSnapshot::calc_mipmap(unsigned int order, uint8_t index0, uint8_t index1, uint64_t samples, bool isEnd)
{
    void *lbp = _ch_data[order][index0].lbp[index1];
    void *level1_ptr = (uint8_t*)lbp + LeafBlockSamples / 8;
    void *level2_ptr = (uint8_t*)level1_ptr + LeafBlockSamples / Scale / 8;
    void *level3_ptr = (uint8_t*)level2_ptr + LeafBlockSamples / Scale / Scale / 8;

    // level 1
    uint64_t *src_ptr  = (uint64_t*)lbp;
    uint64_t *dest_ptr = (uint64_t*)level1_ptr;
    uint8_t offset = 0;
    uint64_t i = 0;
    uint64_t last_count  = _last_calc_count[order];

    if (last_count > 0){
        i        =  last_count / Scale;
        offset   =  i % Scale;
        src_ptr  += i;
        dest_ptr += i / Scale;
    }

    if (i == 0) {
        _last_sample[order] = (*src_ptr & LSB) ? ~0ULL : 0ULL;
    }

    for(; i < samples / Scale; i++)
    {
        if (_last_sample[order] ^ *src_ptr)
            *dest_ptr |= (1ULL << offset);

        _last_sample[order] = *src_ptr & MSB ? ~0ULL : 0ULL;
        src_ptr++;
        offset++;

        if (offset == Scale){
            offset = 0;
            dest_ptr++; 
        }
    }

    // level 2
    src_ptr  = (uint64_t*)level1_ptr;
    dest_ptr = (uint64_t*)level2_ptr;  
    offset = 0;
    i = 0;

    if (last_count > 0){
        i        =  last_count / Scale / Scale;
        offset   =  i % Scale;
        src_ptr  += i;
        dest_ptr += i / Scale;
    }

    for(; i < LeafBlockSamples / Scale / Scale; i++) 
    {
        if (*src_ptr)
            *dest_ptr |= (1ULL << offset);

        src_ptr++;
        offset++;

        if (offset == Scale){
            offset = 0;
            dest_ptr++; 
        }
    }

    // level 3
    src_ptr = (uint64_t*)level2_ptr;
    dest_ptr = (uint64_t*)level3_ptr; 

    for (i=0; i < Scale; i++)
    {
        if (*src_ptr)
            *dest_ptr |= (1ULL << i);
        src_ptr++;
    }  

    if ((*((uint64_t*)lbp) & LSB) != 0)
        _ch_data[order][index0].first |= 1ULL << index1;

    if ((*((uint64_t*)lbp + LeafBlockSamples / Scale - 1) & MSB) != 0)
        _ch_data[order][index0].last |= 1ULL << index1;

    if (*((uint64_t*)level3_ptr) != 0){
        _ch_data[order][index0].tog |= 1ULL << index1;
    }
    else if (isEnd){
        uint64_t ref_root = _cur_ref_block_indexs[order].root_index;
        uint64_t ref_lbp  = _cur_ref_block_indexs[order].lbp_index;

        if (_able_free || index0 > ref_root || (index0 == ref_root && index1 > ref_lbp))
            free(_ch_data[order][index0].lbp[index1]);
        else
            _free_block_list.push_back(_ch_data[order][index0].lbp[index1]);

        _ch_data[order][index0].lbp[index1] = NULL;
    }

    if (isEnd)
        _last_calc_count[order] = 0;
    else
        _last_calc_count[order] = samples;
} 

const uint8_t *LogicSnapshot::get_samples(uint64_t start_sample, uint64_t &end_sample, int sig_index, void **lbp)
{  
    std::lock_guard<std::mutex> lock(_mutex);

    uint64_t sample_count = _ring_sample_count;
    assert(start_sample < sample_count);

    uint64_t logic_sample_index = start_sample + _loop_offset;

    int order = get_ch_order(sig_index);
   // uint64_t index0 =  logic_sample_index >> (LeafBlockPower + RootScalePower);
   // uint64_t index1 = (logic_sample_index & RootMask) >> LeafBlockPower;
   // uint64_t offset = (start_sample & LeafMask) / 8;

    uint64_t index0 = logic_sample_index / LeafBlockSamples / RootScale;
    uint64_t index1 = (logic_sample_index / LeafBlockSamples) % RootScale;
    uint64_t offset = (start_sample % LeafBlockSamples) / 8;

    uint8_t *block_buffer = (uint8_t*)_ch_data[order][index0].lbp[index1];

     int block_num = get_block_num_unlock();

    if (_is_loop && _loop_offset > 0)
    { 
        uint64_t block0_sample = get_block_size_unlock(0) * 8;
        bool flag = false;

        if (start_sample < block0_sample){            
            block_buffer = get_block_buf_unlock(0, sig_index, flag);
            end_sample = block0_sample;
            offset = start_sample / 8;
        }
        else{
            uint64_t last_block_sample = get_block_size_unlock(block_num - 1) * 8;
            offset = ((start_sample - block0_sample) % LeafBlockSamples) / 8;

            if (start_sample >= sample_count - last_block_sample){
                end_sample = sample_count;               
            }
            else{
                int mid_block_num = (start_sample - block0_sample) / LeafBlockSamples + 1;
                end_sample = mid_block_num * LeafBlockSamples + block0_sample;
            }
        }
    }
    else{
        end_sample = (index0 << (LeafBlockPower + RootScalePower)) +
                 (index1 << LeafBlockPower) +
                 ~(~0ULL << LeafBlockPower);

        end_sample = min(end_sample + 1, sample_count);
    }

    if (order == -1 || block_buffer == NULL){
        return NULL;
    }
    else{
        if (lbp != NULL){
            *lbp = _ch_data[order][index0].lbp[index1];
        }

        _cur_ref_block_indexs[order].root_index = index0;
        _cur_ref_block_indexs[order].lbp_index  = index1;
        
        return block_buffer + offset;
    }
}

bool LogicSnapshot::get_sample(uint64_t index, int sig_index)
{
    std::lock_guard<std::mutex> lock(_mutex);
    return get_sample_unlock(index, sig_index);
}

bool LogicSnapshot::get_sample_unlock(uint64_t index, int sig_index)
{
    index += _loop_offset;
    _ring_sample_count += _loop_offset;

    bool flag = get_sample_self(index, sig_index);

    _ring_sample_count -= _loop_offset;
    return flag;
}

bool LogicSnapshot::get_sample_self(uint64_t index, int sig_index)
{
    int order = get_ch_order(sig_index);
    assert(order != -1);
    assert(_ch_data[order].size() != 0);

    if (index < _ring_sample_count) {
        uint64_t index_mask = 1ULL << (index & LevelMask[0]);
        uint64_t index0 = index >> (LeafBlockPower + RootScalePower);
        uint64_t index1 = (index & RootMask) >> LeafBlockPower;
        uint64_t root_pos_mask = 1ULL << index1;

        if ((_ch_data[order][index0].tog & root_pos_mask) == 0) {
            return (_ch_data[order][index0].first & root_pos_mask) != 0;
        }
        else {
            uint64_t *lbp = (uint64_t*)_ch_data[order][index0].lbp[index1];
            return *(lbp + ((index & LeafMask) >> ScalePower)) & index_mask;
        }
    }
    
    return false;
}

bool LogicSnapshot::get_display_edges(std::vector<std::pair<bool, bool> > &edges,
    std::vector<std::pair<uint16_t, bool> > &togs,
    uint64_t start, uint64_t end, uint16_t width, uint16_t max_togs,
    double pixels_offset, double min_length, uint16_t sig_index)
{
    if (!edges.empty())
        edges.clear();
    if (!togs.empty())
        togs.clear();

    std::lock_guard<std::mutex> lock(_mutex);

    if (_ring_sample_count == 0)
        return false;

    assert(end < _ring_sample_count);
    assert(start <= end);
    assert(min_length > 0);

    uint64_t index = start;
    bool last_sample;
    bool start_sample;

    // Get the initial state
    start_sample = last_sample = get_sample_unlock(index++, sig_index);
    togs.push_back(pair<uint16_t, bool>(0, last_sample));

    while(edges.size() < width) {
        // search next edge
        bool has_edge = get_nxt_edge_unlock(index, last_sample, end, 0, sig_index);

        // calc the edge position
        int64_t gap = (index / min_length) - pixels_offset;
        index = max((uint64_t)ceil((floor(index/min_length) + 1) * min_length), index + 1);

        while(gap > (int64_t)edges.size() && edges.size() < width){
            edges.push_back(pair<bool, bool>(false, last_sample));
        }

        if (index > end)
            last_sample = get_sample_unlock(end, sig_index);
        else
            last_sample = get_sample_unlock(index - 1, sig_index);

        if (has_edge) {
            edges.push_back(pair<bool, bool>(true, last_sample));
            if (togs.size() < max_togs)
                togs.push_back(pair<uint16_t, bool>(edges.size() - 1, last_sample));
        }

        while(index > end && edges.size() < width)
            edges.push_back(pair<bool, bool>(false, last_sample));
    }

    if (togs.size() < max_togs) {
        last_sample = get_sample_unlock(end, sig_index);
        togs.push_back(pair<uint16_t, bool>(edges.size() - 1, last_sample));
    }

    return start_sample;
}

bool LogicSnapshot::get_nxt_edge(uint64_t &index, bool last_sample, uint64_t end,
                      double min_length, int sig_index)
{
    std::lock_guard<std::mutex> lock(_mutex);
    return get_nxt_edge_unlock(index, last_sample, end, min_length, sig_index);
}

bool LogicSnapshot::get_nxt_edge_unlock(uint64_t &index, bool last_sample, uint64_t end,
                      double min_length, int sig_index)
{
    index += _loop_offset;
    end += _loop_offset;
    _ring_sample_count += _loop_offset;

    bool flag = get_nxt_edge_self(index, last_sample, end, min_length, sig_index);

    index -= _loop_offset;
    _ring_sample_count -= _loop_offset;

    return flag;
}

bool LogicSnapshot::get_nxt_edge_self(uint64_t &index, bool last_sample, uint64_t end, double min_length, int sig_index)
{
    if (index > end)
        return false;

    int order = get_ch_order(sig_index);
    if (order == -1)
        return false;

    //const unsigned int min_level = max((int)floorf(logf(min_length) / logf(Scale)) - 1, 0);
    const unsigned int min_level = max((int)(log2f(min_length) - 1) / (int)ScalePower, 0);
    uint64_t root_index = index >> (LeafBlockPower + RootScalePower);
    uint8_t root_pos = (index & RootMask) >> LeafBlockPower;
    bool root_last = (root_index != 0) ? _ch_data[order][root_index-1].last & MSB :
                                         _ch_data[order][0].first & LSB;
    bool edge_hit = false;

    // linear search for the next transition on the root level
    for (uint64_t i = root_index; !edge_hit && (index <= end) && i < (uint64_t)_ch_data[order].size(); i++) 
    {
        uint64_t cur_mask = (~0ULL << root_pos);

        do {
            uint64_t inner_tog = _ch_data[order][i].tog & cur_mask;
            uint64_t lbp_tog = (((_ch_data[order][i].last << 1) + root_last) & cur_mask) ^ (_ch_data[order][i].first & cur_mask);
            uint8_t inner_tog_pos = bsf_folded(inner_tog);
            uint8_t lbp_tog_pos = bsf_folded(lbp_tog);

            if (inner_tog != 0)
            {
                if (lbp_tog != 0) {
                    // lbp tog before inner tog
                    edge_hit = lbp_nxt_edge(index, i, lbp_tog, lbp_tog_pos, true, inner_tog_pos, last_sample, sig_index);
                }

                if (!edge_hit) {
                    uint64_t *lbp = (uint64_t*)_ch_data[order][i].lbp[inner_tog_pos];
                    uint64_t blk_start = (i << (LeafBlockPower + RootScalePower)) + (inner_tog_pos << LeafBlockPower);
                    index = max(blk_start, index);

                    if (min_level < ScaleLevel) {
                        uint64_t block_end = min(index | LeafMask, end);
                        edge_hit = block_nxt_edge(lbp, index, block_end, last_sample, min_level);
                    }
                    else {
                        edge_hit = true;
                    }

                    if (inner_tog_pos == RootScale - 1)
                        break;
                    cur_mask = (~0ULL << (inner_tog_pos + 1));
                }
            }
            else if (lbp_tog != 0) {
                // lbp tog
                edge_hit = lbp_nxt_edge(index, i, lbp_tog, lbp_tog_pos, false, Scale - 1, last_sample, sig_index);
            }
            else {
                //index = (index + (1 << (LeafBlockPower + RootScalePower))) &
                //        (~0ULL << (LeafBlockPower + RootScalePower));
                index = (((i + 1) << (LeafBlockPower + RootScalePower)) - 1);
                break;
            }
        }
        //while (!edge_hit && index < end);
        while (!edge_hit && index < (((i + 1) << (LeafBlockPower + RootScalePower)) - 1));

        root_pos = 0;
        root_last = _ch_data[order][i].last & MSB;
    }

    if (index > end) {
        // skip edges over right
        edge_hit = false;
    }

    return edge_hit;
}

bool LogicSnapshot::get_pre_edge(uint64_t &index, bool last_sample,
                      double min_length, int sig_index)
{
    std::lock_guard<std::mutex> lock(_mutex);

    index += _loop_offset;
    _ring_sample_count += _loop_offset;

    bool flag = get_pre_edge_self(index, last_sample, min_length, sig_index);

    index = (index < _loop_offset) ? 0 : index - _loop_offset;
    _ring_sample_count -= _loop_offset;
    return flag;
}

bool LogicSnapshot::get_pre_edge_self(uint64_t &index, bool last_sample,
    double min_length, int sig_index)
{
    assert(index < _ring_sample_count);

    int order = get_ch_order(sig_index);
    if (order == -1)
        return false;

    //const unsigned int min_level = max((int)floorf(logf(min_length) / logf(Scale)) - 1, 1);
    const unsigned int min_level = max((int)(log2f(min_length) - 1) / (int)ScalePower, 0);
    int root_index = index >> (LeafBlockPower + RootScalePower);
    uint8_t root_pos = (index & RootMask) >> LeafBlockPower;
    bool root_first = _ch_data[order][root_index].last & MSB;
    bool edge_hit = false;

    // linear search for the previous transition on the root level
    for (int64_t i = root_index; !edge_hit && i >= 0; i--)
    {
        uint64_t cur_mask = (~0ULL >> (RootScale - root_pos - 1));

        do {
            uint64_t inner_tog = _ch_data[order][i].tog & cur_mask;
            uint64_t lbp_tog = (_ch_data[order][i].last & cur_mask) ^ ((((uint64_t)root_first << (RootScale - 1)) + (_ch_data[order][i].first >> 1)) & cur_mask);
            uint8_t inner_tog_pos = bsr64(inner_tog);
            uint8_t lbp_tog_pos = bsr64(lbp_tog);

            if (inner_tog != 0)
            {
                if (lbp_tog != 0) {
                    // lbp tog before inner tog
                    edge_hit = lbp_pre_edge(index, i, lbp_tog, lbp_tog_pos, true, inner_tog_pos, last_sample, sig_index);
                }

                if (!edge_hit) {
                    uint64_t *lbp = (uint64_t*)_ch_data[order][i].lbp[inner_tog_pos];
                    uint64_t blk_end = ((i << (LeafBlockPower + RootScalePower)) +
                                    (inner_tog_pos << LeafBlockPower)) | LeafMask;
                    index = min(blk_end, index);
                    if (min_level < ScaleLevel) {
                        edge_hit = block_pre_edge(lbp, index, last_sample, min_level, sig_index);
                    } else {
                        edge_hit = true;
                    }
                    if (inner_tog_pos == 0)
                        break;
                    cur_mask = (~0ULL >> (RootScale - inner_tog_pos));
                }
            }
            else if (lbp_tog != 0) {
                // lbp tog
                edge_hit = lbp_pre_edge(index, i, lbp_tog, lbp_tog_pos, false, 0, last_sample, sig_index);
                if (lbp_tog_pos == 0)
                    break;
            }
            else {
                break;
            }
        }
        while (!edge_hit);

        root_pos = RootScale - 1;
        root_first = _ch_data[order][i].first & LSB;
    }

    return edge_hit;
}

bool LogicSnapshot::lbp_nxt_edge(uint64_t &index, uint64_t root_index, uint64_t lbp_tog, uint8_t lbp_tog_pos,
                  bool aft_tog, uint8_t aft_pos, bool last_sample, int sig_index)
{
    assert(lbp_tog != 0);

    // check last_sample with current index
    bool sample = get_sample_self(index, sig_index);
    if (sample ^ last_sample)
    {
        return true;
    }

    // find edge between lbp
    bool edge_hit = false;
    uint64_t aft_lbp_start = (root_index << (LeafBlockPower + RootScalePower)) + (aft_pos << LeafBlockPower);

    while(lbp_tog_pos <= aft_pos)
    {
        uint64_t lbp_tog_index = (root_index << (LeafBlockPower + RootScalePower)) + (lbp_tog_pos << LeafBlockPower);
        if (lbp_tog_index > aft_lbp_start)
        {
            edge_hit = false;
            break;
        }
        else if (lbp_tog_index > index)
        {
            index = lbp_tog_index;
            edge_hit = true;
            break;
        }

        lbp_tog_pos++;
        lbp_tog &= (~0ULL << lbp_tog_pos);
        if ((lbp_tog_pos < Scale) && (lbp_tog != 0))
        {
            lbp_tog_pos = bsf_folded(lbp_tog);
        }
        else
        {
            break;
        }
    }

    uint64_t lbp_edge_index = aft_tog ? aft_lbp_start : aft_lbp_start + (1ULL << LeafBlockPower) - 1;
    if (!edge_hit && lbp_edge_index > index)
    {
        index = lbp_edge_index;
    }

    return edge_hit;
}

bool LogicSnapshot::block_nxt_edge(uint64_t *lbp, uint64_t &index, uint64_t block_end, bool last_sample,
                                   unsigned int min_level)
{
    unsigned int level = min_level;
    bool fast_forward = true;
    const uint64_t last = last_sample ? ~0ULL : 0ULL;

    //----- Search Next Edge Within Current LeafBlock -----//
    if (level == 0)
    {
        // Search individual samples up to the beginning of
        // the next first level mip map block
        const uint64_t offset = (index & ~(~0ULL << LeafBlockPower)) >> ScalePower;
        const uint64_t mask = last_sample ? ~(~0ULL << (index & LevelMask[0])) : ~0ULL << (index & LevelMask[0]);
        uint64_t sample = last_sample ? *(lbp + offset) | mask : *(lbp + offset) & mask;
        if (sample ^ last) {
            index = (index & ~LevelMask[0]) + bsf_folded(last_sample ? ~sample : sample);
            fast_forward = false;
        } else {
            index = ((index >> ScalePower) + 1) << ScalePower;
        }
    } else {
        index = ((index >> level*ScalePower) + 1) << level*ScalePower;
    }

    if (fast_forward) {

        // Fast forward: This involves zooming out to higher
        // levels of the mip map searching for changes, then
        // zooming in on them to find the point where the edge
        // begins.

        // Zoom out at the beginnings of mip-map
        // blocks until we encounter a change
        while (index <= block_end) {
            // continue only within current block
            if (level == 0)
                level++;
            const int level_scale_power =
                (level + 1) * ScalePower;
            const uint64_t offset =
                (index & ~(~0ULL << LeafBlockPower)) >> level_scale_power;
            const uint64_t mask = ~0ULL << ((index & LevelMask[level]) >> (level*ScalePower));
            uint64_t sample = *(lbp + LevelOffset[level] + offset) & mask;

            // Check if there was a change in this block
            if (sample) {
                index = (index & (~0ULL << (level + 1)*ScalePower)) + (bsf_folded(sample) << level*ScalePower);
                break;
            } else {
                index = ((index >> (level + 1)*ScalePower) + 1) << (level + 1)*ScalePower;
                ++level;
            }
        }

        // Zoom in until we encounter a change,
        // and repeat until we reach min_level
        while ((index <= block_end) && (level > min_level)) {
            // continue only within current block
            level--;
            const int level_scale_power =
                (level + 1) * ScalePower;
            const uint64_t offset =
                (index & ~(~0ULL << LeafBlockPower)) >> level_scale_power;
            const uint64_t mask = (level == 0 && last_sample) ?
                        ~(~0ULL << ((index & LevelMask[level]) >> (level*ScalePower))) :
                        ~0ULL << ((index & LevelMask[level]) >> (level*ScalePower));
            uint64_t sample = (level == 0 && last_sample) ?
                        *(lbp + LevelOffset[level] + offset) | mask :
                        *(lbp + LevelOffset[level] + offset) & mask;

            // Update the low level position of the change in this block
            if (level == 0 ? sample ^ last : sample) {
                index = (index & (~0ULL << (level + 1)*ScalePower)) + (bsf_folded(level == 0 ? sample ^ last : sample) << level*ScalePower);
                if (level == min_level)
                    break;
            }
        }
    }

    return (index <= block_end);
}

bool LogicSnapshot::lbp_pre_edge(uint64_t &index, uint64_t root_index, uint64_t lbp_tog, uint8_t &lbp_tog_pos,
                  bool pre_tog, uint8_t pre_pos, bool last_sample, int sig_index)
{
    assert(lbp_tog != 0);

    // check last_sample with current index
    bool sample = get_sample_self(index, sig_index);
    if (sample ^ last_sample)
    {
        index++;
        return true;
    }

    // find edge between lbp
    bool edge_hit = false;
    uint64_t pre_lbp_end = (root_index << (LeafBlockPower + RootScalePower)) + (pre_pos << LeafBlockPower) + (1ULL << LeafBlockPower) - 1;

    do
    {
        uint64_t lbp_tog_index = (root_index << (LeafBlockPower + RootScalePower)) + (lbp_tog_pos << LeafBlockPower)  + (1ULL << LeafBlockPower) - 1;
        if (lbp_tog_index < pre_lbp_end)
        {
            edge_hit = false;
            break;
        }
        else if (lbp_tog_index < index)
        {
            index = lbp_tog_index + 1;
            edge_hit = true;
            break;
        }

        if (lbp_tog_pos > 0)
        {
            lbp_tog_pos--;
            lbp_tog &= (~0ULL >> (Scale - lbp_tog_pos - 1));
            lbp_tog_pos = (lbp_tog != 0) ? bsr64(lbp_tog) : 0;
        }
        else
        {
            lbp_tog = 0;
        }
    }
    while (lbp_tog != 0 && lbp_tog_pos >= pre_pos);

    uint64_t lbp_edge_index = pre_tog ? pre_lbp_end : pre_lbp_end + 1 - (1ULL << LeafBlockPower);
    if (!edge_hit && lbp_edge_index < index)
    {
        index = lbp_edge_index;
    }

    return edge_hit;
}

bool LogicSnapshot::block_pre_edge(uint64_t *lbp, uint64_t &index, bool last_sample,
                                   unsigned int min_level, int sig_index)
{
    assert(min_level == 0);

    unsigned int level = min_level;
    bool fast_forward = true;
    const uint64_t last = last_sample ? ~0ULL : 0ULL;
    uint64_t block_start = index & ~LeafMask;

    assert(lbp);

    //----- Search Next Edge Within Current LeafBlock -----//
    if (level == 0)
    {
        // Search individual samples down to the beginning of
        // the previous first level mip map block
        const uint64_t offset = (index & ~(~0ULL << LeafBlockPower)) >> ScalePower;
        const uint64_t mask = last_sample ? ~(~0ULL >> (Scale - (index & LevelMask[0]) - 1)) : ~0ULL >> (Scale - (index & LevelMask[0]) - 1);
        uint64_t sample = last_sample ? *(lbp + offset) | mask : *(lbp + offset) & mask;
        if (sample ^ last) {
            index = (index & ~LevelMask[0]) + bsr64(last_sample ? ~sample : sample) + 1;
            return true;
        } else {
            index &= ~LevelMask[0];
            if (index == 0)
                return false;
            else
                index--;

            // using get_sample_self() to avoid out of block case
            bool sample = get_sample_self(index, sig_index);
            if (sample ^ last_sample) {
                index++;
                return true;
            } else if (index < block_start) {
                return false;
            }
        }
    }

    if (fast_forward) {

        // Fast forward: This involves zooming out to higher
        // levels of the mip map searching for changes, then
        // zooming in on them to find the point where the edge
        // begins.

        // Zoom out at the beginnings of mip-map
        // blocks until we encounter a change
        while (index > block_start) {
            // continue only within current block
            if (level == 0)
                level++;
            const int level_scale_power =
                (level + 1) * ScalePower;
            const uint64_t offset =
                (index & ~(~0ULL << LeafBlockPower)) >> level_scale_power;
            const uint64_t mask = ~0ULL >> (Scale - ((index & LevelMask[level]) >> (level*ScalePower)) - 1);
            uint64_t sample = *(lbp + LevelOffset[level] + offset) & mask;

            // Check if there was a change in this block
            if (sample) {
                index = (index & (~0ULL << (level + 1)*ScalePower)) +
                        (bsr64(sample) << level*ScalePower) +
                        ~(~0ULL << level*ScalePower);
                break;
            } else {
                index = (index >> (level + 1)*ScalePower) << (level + 1)*ScalePower;
                if (index == 0)
                    return false;
                else
                    index--;
            }
        }

        // Zoom in until we encounter a change,
        // and repeat until we reach min_level
        while ((index >= block_start) && (level > min_level)) {
            // continue only within current block
            level--;
            const int level_scale_power =
                (level + 1) * ScalePower;
            const uint64_t offset =
                (index & ~(~0ULL << LeafBlockPower)) >> level_scale_power;
            const uint64_t mask = (level == 0 && last_sample) ?
                        ~(~0ULL >> (Scale - ((index & LevelMask[level]) >> (level*ScalePower)) - 1)) :
                        ~0ULL >> (Scale - ((index & LevelMask[level]) >> (level*ScalePower)) - 1);
            uint64_t sample = (level == 0 && last_sample) ?
                        *(lbp + LevelOffset[level] + offset) | mask :
                        *(lbp + LevelOffset[level] + offset) & mask;

            // Update the low level position of the change in this block
            if (level == 0 ? sample ^ last : sample) {
                index = (index & (~0ULL << (level + 1)*ScalePower)) +
                        (bsr64(level == 0 ? sample ^ last : sample) << level*ScalePower) +
                        ~(~0ULL << level*ScalePower);
                if (level == min_level) {
                    index++;
                    break;
                }
            } else {
                index = (index & (~0ULL << (level + 1)*ScalePower));
            }
        }
    }

    return (index >= block_start) && (index != 0);
}

bool LogicSnapshot::pattern_search(int64_t start, int64_t end, int64_t& index,
                        std::map<uint16_t, QString> &pattern, bool isNext)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    _is_search_stop = false;
    start += _loop_offset;
    end += _loop_offset;
    index += _loop_offset;
    _ring_sample_count += _loop_offset;

    bool flag = pattern_search_self(start, end, index, pattern, isNext);

    index -= _loop_offset;
    _ring_sample_count -= _loop_offset;
    return flag;
}

bool LogicSnapshot::pattern_search_self(int64_t start, int64_t end, int64_t &index,
                    std::map<uint16_t, QString> &pattern, bool isNext)
{
    if (pattern.empty()) {
        return true;
    }
  
    char flagList[CHANNEL_MAX_COUNT];
    char lstValues[CHANNEL_MAX_COUNT];
    int  chanIndexs[CHANNEL_MAX_COUNT];
    int  count = 0;  
    bool bEdgeFlag = false;

    const int64_t to = isNext ? end + 1 : start - 1;
    const int64_t step = isNext ? 1 : -1;

    for (auto it = pattern.begin(); it != pattern.end(); it++){
         char flag = *(it->second.toStdString().c_str());
         int channel = it->first;

         if (flag != 'X' && has_data(channel)){
             flagList[count]  = flag;
             chanIndexs[count] = channel;
             count++;

             if (flag == 'R' || flag == 'F' || flag == 'C'){
                 bEdgeFlag = true;
             }
         }
    }
    if (count == 0){
        return true;
    }  

    //find
    bool ret = false;
    char val = 0;
    int macthed = 0;  

    //get first edge values
    if (bEdgeFlag){
        for (int i=0; i < count; i++){
            lstValues[i] =  (char)get_sample_self(index, chanIndexs[i]);
        }
        index += step;
    }

    if (index < start){
        index = start;
    }
    if (index > end){
        index = end;
    }

    while (index != to && !_is_search_stop)
    {
        macthed = 0;

        for (int i = 0; i < count; i++)
        {
            if (_is_search_stop){
                break;
            }

            val = (char)get_sample_self(index, chanIndexs[i]);

            if (flagList[i] == '0')
            {
                macthed += !val;
            }
            else if (flagList[i] == '1')
            {
                macthed += val;
            } 
            else if (flagList[i] == 'R')
            {
                if (isNext)
                    macthed += (lstValues[i] == 0 && val == 1);
                else
                    macthed += (lstValues[i] == 1 && val == 0);
            }
            else if (flagList[i] == 'F')
            {
                if (isNext)
                    macthed += (lstValues[i] == 1 && val == 0);
                else
                    macthed += (lstValues[i] == 0 && val == 1);
            }
            else if (flagList[i] == 'C')
            {   
                if (isNext)
                    macthed += (lstValues[i] == 0 && val == 1) || (lstValues[i] == 1 && val == 0);
                else
                    macthed += (lstValues[i] == 1 && val == 0) || (lstValues[i] == 0 && val == 1);
            }
            lstValues[i] = val;
        }

        // matched all
        if (macthed == count)
        {
            ret = true;
            if (!isNext){
                index++; //move to prev position
            }
            break;
        }

        index += step;
    }

    return ret;
}

bool LogicSnapshot::has_data(int sig_index)
{
    return get_ch_order(sig_index) != -1;
}

int LogicSnapshot::get_block_num()
{
    std::lock_guard<std::mutex> lock(_mutex);
    return get_block_num_unlock();
}

int LogicSnapshot::get_block_num_unlock()
{  
    auto align_sample_count = _ring_sample_count;
    int block = align_sample_count / LeafBlockSamples;

    if (align_sample_count % LeafBlockSamples != 0){
        block++;
    }

    if (_loop_offset > 0){
        uint64_t diff1 = align_sample_count % LeafBlockSamples;
        uint64_t diff2 = _loop_offset % LeafBlockSamples;

        if ((diff1 == 0 && diff2 != 0) ||
                (diff1 != 0 && diff1 + diff2 > LeafBlockSamples)){
            block++;
        }
    }

    return block;
}

uint64_t LogicSnapshot::get_block_size(int block_index)
{
    std::lock_guard<std::mutex> lock(_mutex);
    return get_block_size_unlock(block_index);
}

uint64_t LogicSnapshot::get_block_size_unlock(int block_index)
{
    int block_num = get_block_num_unlock();
    assert(block_index < block_num);

    auto align_sample_count = _ring_sample_count;

    if (_loop_offset > 0)
    {
        if (block_index > 0 && block_index < block_num - 1) {
            return LeafBlockSamples / 8;
        }
        else if (block_index == 0){
            if (align_sample_count + _loop_offset <= LeafBlockSamples){
                return align_sample_count / 8;
            }
            else{ 
                return (LeafBlockSamples - _loop_offset % LeafBlockSamples) / 8;
            }
        }
        else{
            uint64_t sum_sample_count = align_sample_count + _loop_offset;

            if (sum_sample_count % LeafBlockSamples == 0)
                return LeafBlockSamples / 8;
            else
                return (sum_sample_count % LeafBlockSamples) / 8;
        }
    }
    else{
        if (block_index < block_num - 1) {
            return LeafBlockSamples / 8;
        }
        else { // The data length of last block.
            if (align_sample_count % LeafBlockSamples == 0)
                return LeafBlockSamples / 8;
            else
                return (align_sample_count % LeafBlockSamples) / 8;
        }
    }    
}

uint8_t *LogicSnapshot::get_block_buf(int block_index, int sig_index, bool &sample)
{
    std::lock_guard<std::mutex> lock(_mutex);
    return get_block_buf_unlock(block_index, sig_index, sample);
}

uint8_t *LogicSnapshot::get_block_buf_unlock(int block_index, int sig_index, bool &sample)
{
    int block_num = get_block_num_unlock();
    assert(block_index < block_num);

    int order = get_ch_order(sig_index);
    if (order == -1) {
        sample = 0;
        return NULL;
    }

    int block_index0 = block_index;
    block_index += _loop_offset / LeafBlockSamples;

    uint64_t index = block_index / RootScale;
    uint8_t pos = block_index % RootScale;
    uint8_t *lbp = (uint8_t*)_ch_data[order][index].lbp[pos];

    if (lbp == NULL){
        sample = (_ch_data[order][index].first & 1ULL << pos) != 0;
    }

    if (lbp != NULL && _loop_offset > 0 && block_index0 == 0)
    {
        lbp += (_loop_offset % LeafBlockSamples) / 8;
    }

    return lbp;
}

int LogicSnapshot::get_ch_order(int sig_index)
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

void LogicSnapshot::move_first_node_to_last()
{
    for (unsigned int i=0; i<_channel_num; i++)
    {
        struct RootNode rn = _ch_data[i][0];
        _ch_data[i].erase(_ch_data[i].begin());

        for (int x=0; x<(int)Scale; x++)
        {
            if (rn.lbp[x] != NULL){
                free(rn.lbp[x]);
                rn.lbp[x] = NULL;
            }
        }

        rn.tog = 0;
        rn.first = 0;
        rn.last = 0;

        _ch_data[i].push_back(rn);                        
    }
}

void LogicSnapshot::decode_end()
{
   std::lock_guard<std::mutex> lock(_mutex);

   for(void *p : _free_block_list){
        free(p);
    }
    _free_block_list.clear();
}

void LogicSnapshot::free_decode_lpb(void *lbp)
{
    assert(lbp);

    std::lock_guard<std::mutex> lock(_mutex);

    for (auto it = _free_block_list.begin(); it != _free_block_list.end(); it++)
    {
        if ((*it) == lbp){
            _free_block_list.erase(it);
            free(lbp);
            break;
        }
    }
}

void LogicSnapshot::free_head_blocks(int count)
{
    assert(count < (int)Scale);
    assert(count > 0);

    for (int i = 0; i < (int)_channel_num; i++)
    {
        for (int j=_lst_free_block_index; j<count; j++){
            if (_ch_data[i][0].lbp[j] != NULL){
                free(_ch_data[i][0].lbp[j]);
                _ch_data[i][0].lbp[j] = NULL;
            }

            _ch_data[i][0].tog = (_ch_data[i][0].tog >> count) << count;
            _ch_data[i][0].first = (_ch_data[i][0].first >> count) << count;
            _ch_data[i][0].last = (_ch_data[i][0].last >> count) << count;
        }
    }
    _lst_free_block_index = count;
}

int LogicSnapshot::get_block_index_with_sample(uint64_t sample_index, uint64_t *out_offset)
{
    std::lock_guard<std::mutex> lock(_mutex);

    int block_index = 0;
    uint64_t offset = 0;

    if (_is_loop && _loop_offset > 0){ 
        uint64_t block_sample0 = get_block_size_unlock(0) * 8;

        if (sample_index < block_sample0){
            block_index = 0;
            offset = sample_index;
        }
        else{
            block_index = (sample_index - block_sample0) / LeafBlockSamples + 1;
            offset = (sample_index - block_sample0) % LeafBlockSamples;            
        }
    }
    else{
        block_index = sample_index / LeafBlockSamples;
        offset = sample_index % LeafBlockSamples;
    }

    if (out_offset != NULL){
        *out_offset = offset;
    }

    return block_index;
}

} // namespace data
} // namespace pv
