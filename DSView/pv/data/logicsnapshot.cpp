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

#include <extdef.h>

#include <QDebug>

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <boost/foreach.hpp>

#include "logicsnapshot.h"

using namespace boost;
using namespace std;

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
    Snapshot(1, 0, 0),
    _block_num(0)
{
}

LogicSnapshot::~LogicSnapshot()
{
}

void LogicSnapshot::free_data()
{
    Snapshot::free_data();
    for(auto& iter:_ch_data) {
        for(auto& iter_rn:iter) {
            for (unsigned int k = 0; k < Scale; k++)
                if (iter_rn.lbp[k] != NULL)
                    free(iter_rn.lbp[k]);
        }
        std::vector<struct RootNode> void_vector;
        iter.swap(void_vector);
    }
    _ch_data.clear();
    _sample_count = 0;
}

void LogicSnapshot::init()
{
    boost::lock_guard<boost::recursive_mutex> lock(_mutex);
    _sample_count = 0;
    _ring_sample_count = 0;
    _block_num = 0;
    _byte_fraction = 0;
    _ch_fraction = 0;
    _src_ptr = NULL;
    _dest_ptr = NULL;
    _data = NULL;
    _memory_failed = false;
    _last_ended = true;
}

void LogicSnapshot::clear()
{
    boost::lock_guard<boost::recursive_mutex> lock(_mutex);
    free_data();
    init();
}

void LogicSnapshot::capture_ended()
{
    Snapshot::capture_ended();

    //assert(_ch_fraction == 0);
    //assert(_byte_fraction == 0);
    uint64_t block_index = _ring_sample_count / LeafBlockSamples;
    uint64_t block_offset = (_ring_sample_count % LeafBlockSamples) / Scale;
    if (block_offset != 0) {
        uint64_t index0 = block_index / RootScale;
        uint64_t index1 = block_index % RootScale;
        int order = 0;
        for(auto& iter:_ch_data) {
            const uint64_t *end_ptr = (uint64_t *)iter[index0].lbp[index1] + (LeafBlockSamples / Scale);
            uint64_t *ptr = (uint64_t *)iter[index0].lbp[index1] + block_offset;
            while (ptr < end_ptr)
                *ptr++ = 0;

            // calc mipmap of current block
            calc_mipmap(order, index0, index1, block_offset * Scale);

            // calc root of current block
            if (*((uint64_t *)iter[index0].lbp[index1]) != 0)
                iter[index0].value += 1ULL << index1;
            if (*((uint64_t *)iter[index0].lbp[index1] + LeafBlockSpace / sizeof(uint64_t) - 1) != 0) {
                iter[index0].tog += 1ULL << index1;
            } else {
               // trim leaf to free space
               free(iter[index0].lbp[index1]);
               iter[index0].lbp[index1] = NULL;
            }

            order++;
        }
    }
    _sample_count = _ring_sample_count;
}

void LogicSnapshot::first_payload(const sr_datafeed_logic &logic, uint64_t total_sample_count, GSList *channels)
{
    bool channel_changed = false;
    uint16_t channel_num = 0;
    for (const GSList *l = channels; l; l = l->next) {
        sr_channel *const probe = (sr_channel*)l->data;
        assert(probe);
        if (probe->type == SR_CHANNEL_LOGIC) {
            channel_num += probe->enabled;
            if (!channel_changed && probe->enabled) {
                channel_changed = !has_data(probe->index);
            }
        }
    }

    if (total_sample_count != _total_sample_count ||
        channel_num != _channel_num ||
        channel_changed) {
        free_data();
        _total_sample_count = total_sample_count;
        _channel_num = channel_num;
        uint64_t rootnode_size = (_total_sample_count + RootNodeSamples - 1) / RootNodeSamples;
        for (const GSList *l = channels; l; l = l->next) {
            sr_channel *const probe = (sr_channel*)l->data;
            if (probe->type == SR_CHANNEL_LOGIC && probe->enabled) {
                std::vector<struct RootNode> root_vector;
                for (uint64_t j = 0; j < rootnode_size; j++) {
                    struct RootNode rn;
                    rn.tog = 0;
                    rn.value = 0;
                    memset(rn.lbp, 0, sizeof(rn.lbp));
                    root_vector.push_back(rn);
                }
                _ch_data.push_back(root_vector);
                _ch_index.push_back(probe->index);
            }
        }
    } else {
        for(auto& iter:_ch_data) {
            for(auto& iter_rn:iter) {
                iter_rn.tog = 0;
                iter_rn.value = 0;
            }
        }
    }

    _sample_count = 0;
    _last_sample.clear();
    _sample_cnt.clear();
    _block_cnt.clear();
    _ring_sample_cnt.clear();
    for (unsigned int i = 0; i < _channel_num; i++) {
        _last_sample.push_back(0);
        _sample_cnt.push_back(0);
        _block_cnt.push_back(0);
        _ring_sample_cnt.push_back(0);
    }

    append_payload(logic);
    _last_ended = false;
}

void LogicSnapshot::append_payload(
	const sr_datafeed_logic &logic)
{
    boost::lock_guard<boost::recursive_mutex> lock(_mutex);

    if (logic.format == LA_CROSS_DATA)
        append_cross_payload(logic);
    else if (logic.format == LA_SPLIT_DATA)
        append_split_payload(logic);
}

void LogicSnapshot::append_cross_payload(
    const sr_datafeed_logic &logic)
{
    assert(logic.format == LA_CROSS_DATA);
    assert(logic.length >= ScaleSize * _channel_num);

    if (_sample_count >= _total_sample_count)
        return;

    _src_ptr = logic.data;
    uint64_t len = logic.length;
    uint64_t samples = ceil(logic.length * 8.0 / _channel_num);
    if (_sample_count + samples < _total_sample_count) {
        _sample_count += samples;
    } else {
        len = ceil((_total_sample_count - _sample_count) * _channel_num / 8.0);
        _sample_count = _total_sample_count;
    }

    while (_sample_count > _block_num * LeafBlockSamples) {
        uint8_t index0 = _block_num / RootScale;
        uint8_t index1 = _block_num % RootScale;
        for(auto& iter:_ch_data) {
            if (iter[index0].lbp[index1] == NULL)
                iter[index0].lbp[index1] = malloc(LeafBlockSpace);
            if (iter[index0].lbp[index1] == NULL) {
                _memory_failed = true;
                return;
            }
            uint64_t *mipmap_ptr = (uint64_t *)iter[index0].lbp[index1] +
                                   (LeafBlockSamples / Scale);
            memset(mipmap_ptr, 0, LeafBlockSpace - (LeafBlockSamples / 8));
        }
        _block_num++;
    }

    // bit align
    while (((_ch_fraction != 0) || (_byte_fraction != 0)) && (len != 0)) {
        uint8_t *dp_tmp = (uint8_t *)_dest_ptr;
        uint8_t *sp_tmp = (uint8_t *)_src_ptr;
        do {
            //*(uint8_t *)_dest_ptr++ = *(uint8_t *)_src_ptr++;
            *dp_tmp++ = *sp_tmp++;
            _byte_fraction = (_byte_fraction + 1) % ScaleSize;
            len--;
        } while ((_byte_fraction != 0) && (len != 0));
        _dest_ptr = dp_tmp;
        _src_ptr = sp_tmp;
        if (_byte_fraction == 0) {
            const uint64_t index0 = _ring_sample_count / RootNodeSamples;
            const uint64_t index1 = (_ring_sample_count >> LeafBlockPower) % RootScale;
            const uint64_t offset = (_ring_sample_count % LeafBlockSamples) / Scale;

//            _dest_ptr = (uint64_t *)_ch_data[i][index0].lbp[index1] + offset;
//            uint64_t mipmap_index = offset / 8 / Scale;
//            uint64_t mipmap_offset = (offset / 8) % Scale;
//            uint64_t *l1_mipmap = (uint64_t *)_ch_data[i][index0].lbp[index1] +
//                                  (LeafBlockSamples / Scale) + mipmap_index;
//            *l1_mipmap += ((_last_sample[i] ^ *(uint64_t *)_dest_ptr) != 0 ? 1ULL : 0ULL) << mipmap_offset;
//            _last_sample[i] = *(uint64_t *)_dest_ptr & (1ULL << (Scale - 1)) ? ~0ULL : 0ULL;

            _ch_fraction = (_ch_fraction + 1) % _channel_num;
            if (_ch_fraction == 0)
                _ring_sample_count += Scale;
            _dest_ptr = (uint8_t *)_ch_data[_ch_fraction][index0].lbp[index1] + (offset * ScaleSize);
        }
    }

    // align data append
    {
        assert(_ch_fraction == 0);
        assert(_byte_fraction == 0);
        assert(_ring_sample_count % Scale == 0);
        uint64_t pre_index0 = _ring_sample_count / RootNodeSamples;
        uint64_t pre_index1 = (_ring_sample_count >> LeafBlockPower) % RootScale;
        uint64_t pre_offset = (_ring_sample_count % LeafBlockSamples) / Scale;
        uint64_t *src_ptr = NULL;
        uint64_t *dest_ptr;
        int order = 0;
        const uint64_t align_size = len / ScaleSize / _channel_num;
        _ring_sample_count += align_size * Scale;

//        uint64_t mipmap_index = pre_offset / Scale;
//        uint64_t mipmap_offset = pre_offset % Scale;
//        uint64_t *l1_mipmap;
        for(auto& iter:_ch_data) {
            uint64_t index0 = pre_index0;
            uint64_t index1 = pre_index1;
            src_ptr = (uint64_t *)_src_ptr + order;
            _dest_ptr = iter[index0].lbp[index1];
            dest_ptr = (uint64_t *)_dest_ptr + pre_offset;
//            l1_mipmap = (uint64_t *)_dest_ptr + (LeafBlockSamples / Scale) + mipmap_index;
            while (src_ptr < (uint64_t *)_src_ptr + (align_size * _channel_num)) {
                const uint64_t tmp_u64 = *src_ptr;
                *dest_ptr++ = tmp_u64;
//                *l1_mipmap += ((_last_sample[i] ^ tmp_u64) != 0 ? 1ULL : 0ULL) << mipmap_offset;
//                mipmap_offset = (mipmap_offset + 1) % Scale;
//                l1_mipmap += (mipmap_offset == 0);
//                _last_sample[i] = tmp_u64 & (1ULL << (Scale - 1)) ? ~0ULL : 0ULL;
                src_ptr += _channel_num;
                //mipmap
                if (dest_ptr == (uint64_t *)_dest_ptr + (LeafBlockSamples / Scale)) {
                    // calc mipmap of current block
                    calc_mipmap(order, index0, index1, LeafBlockSamples);

                    // calc root of current block
                    if (*((uint64_t *)iter[index0].lbp[index1]) != 0)
                        iter[index0].value +=  1ULL<< index1;
                    if (*((uint64_t *)iter[index0].lbp[index1] + LeafBlockSpace / sizeof(uint64_t) - 1) != 0) {
                        iter[index0].tog += 1ULL << index1;
                    } else {
                        // trim leaf to free space
                        free(iter[index0].lbp[index1]);
                        iter[index0].lbp[index1] = NULL;
                    }

                    index1++;
                    if (index1 == RootScale) {
                        index0++;
                        index1 = 0;
                    }
                    _dest_ptr = iter[index0].lbp[index1];
                    dest_ptr = (uint64_t *)_dest_ptr;
                }
            }
            order++;
        }
        len -= align_size * _channel_num * ScaleSize;
        _src_ptr = src_ptr - _channel_num + 1;
    }

    // fraction data append
    {
        uint64_t index0 = _ring_sample_count / RootNodeSamples;
        uint64_t index1 = (_ring_sample_count >> LeafBlockPower) % RootScale;
        uint64_t offset = (_ring_sample_count % LeafBlockSamples) / 8;
        _dest_ptr = (uint8_t *)_ch_data[_ch_fraction][index0].lbp[index1] + offset;

        uint8_t *dp_tmp = (uint8_t *)_dest_ptr;
        uint8_t *sp_tmp = (uint8_t *)_src_ptr;
        while(len-- != 0) {
            //*(uint8_t *)_dest_ptr++ = *(uint8_t *)_src_ptr++;
            *dp_tmp++ = *sp_tmp++;
            if (++_byte_fraction == ScaleSize) {
                _ch_fraction = (_ch_fraction + 1) % _channel_num;
                _byte_fraction = 0;
                //_dest_ptr = (uint8_t *)_ch_data[_ch_fraction][index0].lbp[index1] + offset;
                dp_tmp = (uint8_t *)_ch_data[_ch_fraction][index0].lbp[index1] + offset;
            }
        }
        //_dest_ptr = (uint8_t *)_dest_ptr + _byte_fraction;
        _dest_ptr = dp_tmp + _byte_fraction;
    }
}

void LogicSnapshot::append_split_payload(
    const sr_datafeed_logic &logic)
{
    assert(logic.format == LA_SPLIT_DATA);

    uint64_t samples = logic.length * 8;
    uint16_t order = logic.order;
    assert(order < _ch_data.size());

    if (_sample_cnt[order] >= _total_sample_count)
        return;

    if (_sample_cnt[order] + samples < _total_sample_count) {
        _sample_cnt[order] += samples;
    } else {
        samples = _total_sample_count - _sample_cnt[order];
        _sample_cnt[order] = _total_sample_count;
    }

    while (_sample_cnt[order] > _block_cnt[order] * LeafBlockSamples) {
        uint8_t index0 = _block_cnt[order] / RootScale;
        uint8_t index1 = _block_cnt[order] % RootScale;
        if (_ch_data[order][index0].lbp[index1] == NULL)
            _ch_data[order][index0].lbp[index1] = malloc(LeafBlockSpace);
        if (_ch_data[order][index0].lbp[index1] == NULL) {
            _memory_failed = true;
            return;
        }
        memset(_ch_data[order][index0].lbp[index1], 0, LeafBlockSpace);
        _block_cnt[order]++;
    }

    while(samples > 0) {
        const uint64_t index0 = _ring_sample_cnt[order] / RootNodeSamples;
        const uint64_t index1 = (_ring_sample_cnt[order] >> LeafBlockPower) % RootScale;
        const uint64_t offset = (_ring_sample_cnt[order] % LeafBlockSamples) / 8;
        _dest_ptr = (uint8_t *)_ch_data[order][index0].lbp[index1] + offset;

        uint64_t bblank = (LeafBlockSamples - (_ring_sample_cnt[order] & LeafMask));
        if (samples >= bblank) {
            memcpy((uint8_t*)_dest_ptr, (uint8_t *)logic.data, bblank/8);
            _ring_sample_cnt[order] += bblank;
            samples -= bblank;

            // calc mipmap of current block
            calc_mipmap(order, index0, index1, LeafBlockSamples);

            // calc root of current block
            if (*((uint64_t *)_ch_data[order][index0].lbp[index1]) != 0)
                _ch_data[order][index0].value +=  1ULL<< index1;
            if (*((uint64_t *)_ch_data[order][index0].lbp[index1] + LeafBlockSpace / sizeof(uint64_t) - 1) != 0) {
                _ch_data[order][index0].tog += 1ULL << index1;
            } else {
                // trim leaf to free space
                free(_ch_data[order][index0].lbp[index1]);
                _ch_data[order][index0].lbp[index1] = NULL;
            }
        } else {
            memcpy((uint8_t*)_dest_ptr, (uint8_t *)logic.data, samples/8);
            _ring_sample_cnt[order] += samples;
            samples = 0;
        }
    }

    _sample_count = *min_element(_sample_cnt.begin(), _sample_cnt.end());
    _ring_sample_count = *min_element(_ring_sample_cnt.begin(), _ring_sample_cnt.end());
}

void LogicSnapshot::calc_mipmap(unsigned int order, uint8_t index0, uint8_t index1, uint64_t samples)
{
    uint8_t offset;
    uint64_t *src_ptr;
    uint64_t *dest_ptr;
    unsigned int i;

    // level 1
    src_ptr = (uint64_t *)_ch_data[order][index0].lbp[index1];
    dest_ptr = src_ptr + (LeafBlockSamples / Scale) - 1;
    const uint64_t mask =  1ULL << (Scale - 1);
    for(i = 0; i < samples / Scale; i++) {
        offset = i % Scale;
        if (offset == 0)
            dest_ptr++;
        *dest_ptr += ((_last_sample[order] ^ *src_ptr) != 0 ? 1ULL : 0ULL) << offset;
        _last_sample[order] = *src_ptr & mask ? ~0ULL : 0ULL;
        src_ptr++;
    }

    // level 2/3
    src_ptr = (uint64_t *)_ch_data[order][index0].lbp[index1] + (LeafBlockSamples / Scale);
    dest_ptr = src_ptr + (LeafBlockSamples / Scale / Scale) - 1;
    for(i = LeafBlockSamples / Scale; i < LeafBlockSpace / sizeof(uint64_t) - 1; i++) {
        offset = i % Scale;
        if (offset == 0)
            dest_ptr++;
        *dest_ptr += (*src_ptr != 0 ? 1ULL : 0ULL) << offset;
        src_ptr++;
    }
}

const uint8_t *LogicSnapshot::get_samples(uint64_t start_sample, uint64_t &end_sample,
                                     int sig_index)
{
    //assert(data);
    assert(start_sample < get_sample_count());
    assert(end_sample < get_sample_count());
    assert(start_sample <= end_sample);

    int order = get_ch_order(sig_index);
    uint64_t root_index = start_sample >> (LeafBlockPower + RootScalePower);
    uint8_t root_pos = (start_sample & RootMask) >> LeafBlockPower;
    uint64_t block_offset = (start_sample & LeafMask) / 8;
    end_sample = (root_index << (LeafBlockPower + RootScalePower)) +
                 (root_pos << LeafBlockPower) +
                 ~(~0ULL << LeafBlockPower);
    end_sample = min(end_sample, get_sample_count() - 1);

    if (order == -1 ||
        _ch_data[order][root_index].lbp[root_pos] == NULL)
        return NULL;
    else
        return (uint8_t *)_ch_data[order][root_index].lbp[root_pos] + block_offset;
}

bool LogicSnapshot::get_sample(uint64_t index, int sig_index)
{
    int order = get_ch_order(sig_index);
    assert(order != -1);
    assert(_ch_data[order].size() != 0);
    //assert(index < get_sample_count());

    if (index < get_sample_count()) {
        uint64_t index_mask = 1ULL << (index & LevelMask[0]);
        uint64_t root_index = index >> (LeafBlockPower + RootScalePower);
        uint8_t root_pos = (index & RootMask) >> LeafBlockPower;
        uint64_t root_pos_mask = 1ULL << root_pos;

        if ((_ch_data[order][root_index].tog & root_pos_mask) == 0) {
            return (_ch_data[order][root_index].value & root_pos_mask) != 0;
        } else {
            uint64_t *lbp = (uint64_t *)_ch_data[order][root_index].lbp[root_pos];
            return *(lbp + ((index & LeafMask) >> ScalePower)) & index_mask;
        }
    } else {
        return false;
    }
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

    if (get_sample_count() == 0)
        return false;

    assert(end < get_sample_count());
    assert(start <= end);
    assert(min_length > 0);

    uint64_t index = start;
    bool last_sample;
    bool start_sample;

    // Get the initial state
    start_sample = last_sample = get_sample(index++, sig_index);
    togs.push_back(pair<uint16_t, bool>(0, last_sample));
    while(edges.size() < width) {
        // search next edge
        bool has_edge = get_nxt_edge(index, last_sample, end, 0, sig_index);

        // calc the edge position
        int64_t gap = (index / min_length) - pixels_offset;
        index = max((uint64_t)ceil((floor(index/min_length) + 1) * min_length), index + 1);
        while(gap > (int64_t)edges.size() && edges.size() < width)
            edges.push_back(pair<bool, bool>(false, last_sample));

        if (index > end)
            last_sample = get_sample(end, sig_index);
        else
            last_sample = get_sample(index - 1, sig_index);

        if (has_edge) {
            edges.push_back(pair<bool, bool>(true, last_sample));
            if (togs.size() < max_togs)
                togs.push_back(pair<uint16_t, bool>(edges.size() - 1, last_sample));
        }

        while(index > end && edges.size() < width)
            edges.push_back(pair<bool, bool>(false, last_sample));
    }

    if (togs.size() < max_togs) {
        last_sample = get_sample(end, sig_index);
        togs.push_back(pair<uint16_t, bool>(edges.size() - 1, last_sample));
    }

    return start_sample;
}

bool LogicSnapshot::get_nxt_edge(
    uint64_t &index, bool last_sample, uint64_t end,
    double min_length, int sig_index)
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
    bool edge_hit = false;

    // linear search for the next transition on the root level
    for (int64_t i = root_index; !edge_hit && (index <= end) && i < (int64_t)_ch_data[order].size(); i++) {
        uint64_t cur_mask = (~0ULL << root_pos);
        do {
            uint64_t cur_tog = _ch_data[order][i].tog & cur_mask;
            if (cur_tog != 0) {
                uint64_t first_edge_pos = bsf_folded(cur_tog);
                uint64_t *lbp = (uint64_t *)_ch_data[order][i].lbp[first_edge_pos];
                uint64_t blk_start = (i << (LeafBlockPower + RootScalePower)) + (first_edge_pos << LeafBlockPower);
                index = max(blk_start, index);
                if (min_level < ScaleLevel) {
                    uint64_t block_end = min(index | LeafMask, end);
                    edge_hit = block_nxt_edge(lbp, index, block_end, last_sample, min_level);
                } else {
                    edge_hit = true;
                }
                if (first_edge_pos == RootScale - 1)
                    break;
                cur_mask = (~0ULL << (first_edge_pos + 1));
            } else {
                index = (index + (1 << (LeafBlockPower + RootScalePower))) &
                        (~0ULL << (LeafBlockPower + RootScalePower));
                break;
            }
        } while (!edge_hit && index < end);
        root_pos = 0;
    }
    return edge_hit;
}

bool LogicSnapshot::get_pre_edge(uint64_t &index, bool last_sample,
    double min_length, int sig_index)
{
    assert(index < get_sample_count());

    int order = get_ch_order(sig_index);
    if (order == -1)
        return false;

    //const unsigned int min_level = max((int)floorf(logf(min_length) / logf(Scale)) - 1, 1);
    const unsigned int min_level = max((int)(log2f(min_length) - 1) / (int)ScalePower, 0);
    int root_index = index >> (LeafBlockPower + RootScalePower);
    uint8_t root_pos = (index & RootMask) >> LeafBlockPower;
    bool edge_hit = false;

    // linear search for the previous transition on the root level
    for (int64_t i = root_index; !edge_hit && i >= 0; i--) {
        uint64_t cur_mask = (~0ULL >> (RootScale - root_pos - 1));
        do {
            uint64_t cur_tog = _ch_data[order][i].tog & cur_mask;
            if (cur_tog != 0) {
                uint64_t first_edge_pos = bsr64(cur_tog);
                uint64_t *lbp = (uint64_t *)_ch_data[order][i].lbp[first_edge_pos];
                uint64_t blk_end = ((i << (LeafBlockPower + RootScalePower)) +
                                   (first_edge_pos << LeafBlockPower)) | LeafMask;
                index = min(blk_end, index);
                if (min_level < ScaleLevel) {
                    edge_hit = block_pre_edge(lbp, index, last_sample, min_level, sig_index);
                } else {
                    edge_hit = true;
                }
                if (first_edge_pos == 0)
                    break;
                cur_mask = (~0ULL >> (RootScale - first_edge_pos));
            } else {
                break;
            }
        } while (!edge_hit);
        root_pos = RootScale - 1;
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

bool LogicSnapshot::block_pre_edge(uint64_t *lbp, uint64_t &index, bool last_sample,
                                   unsigned int min_level, int sig_index)
{
    assert(min_level == 0);

    unsigned int level = min_level;
    bool fast_forward = true;
    const uint64_t last = last_sample ? ~0ULL : 0ULL;
    uint64_t block_start = index & ~LeafMask;

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

            // using get_sample() to avoid out of block case
            bool sample = get_sample(index, sig_index);
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

bool LogicSnapshot::pattern_search(int64_t start, int64_t end, bool nxt, int64_t &index,
                    std::map<uint16_t, QString> pattern)
{
    int start_match_pos = pattern.size() - 1;
    int end_match_pos = 0;
    if (pattern.empty()) {
        return true;
    } else {
        for (auto& iter:pattern) {
            int char_index = 0;
            for (auto& iter_char:iter.second) {
                if (iter_char != 'X') {
                   start_match_pos = min(start_match_pos, char_index);
                   end_match_pos = max(end_match_pos, char_index);
                }
                char_index++;
            }
        }
    }

    if (start_match_pos > end_match_pos)
        return true;

    std::map<uint16_t, bool> cur_sample;
    std::map<uint16_t, bool> exp_sample;
    std::map<uint16_t, bool> cur_edge;
    std::map<uint16_t, bool> exp_edge;
    int nxt_match_pos = nxt ? start_match_pos : end_match_pos;
    int cur_match_pos = nxt ? nxt_match_pos - 1 : nxt_match_pos + 1;
    std::vector<uint64_t> exp_index;
    bool find_edge;
    typedef std::map<uint16_t, QString>::iterator it_type;
    while(nxt ? index <= end : index >= start) {
        // get expacted and current pattern
        exp_index.clear();
        cur_edge.clear();
        for(it_type iterator = pattern.begin();
            iterator != pattern.end(); iterator++) {
            uint16_t ch_index = iterator->first;
            if (!has_data(ch_index))
                continue;
            exp_index.push_back(index);
            cur_sample[ch_index] = get_sample(index, ch_index);

            if (iterator->second[nxt_match_pos] == '0') {
                exp_sample[ch_index] = false;
            } else if (iterator->second[nxt_match_pos] == '1') {
                exp_sample[ch_index] = true;
            } else if (iterator->second[nxt_match_pos] == 'X') {
                exp_sample[ch_index] = cur_sample[ch_index];
            } else if (iterator->second[nxt_match_pos] == 'F') {
                exp_sample[ch_index] = false;
                exp_edge[ch_index] = true;
            } else if (iterator->second[nxt_match_pos] == 'R') {
                exp_sample[ch_index] = true;
                exp_edge[ch_index] = true;
            } else if (iterator->second[nxt_match_pos] == 'C') {
                exp_sample[ch_index] = cur_sample[ch_index];
                exp_edge[ch_index] = true;
            }

            if (exp_edge.find(ch_index) != exp_edge.end()) {
                if (index > start) {
                    bool sample = get_sample(index - 1, ch_index);
                    if (sample != cur_sample[ch_index])
                        cur_edge[ch_index] = true;
                }
            }
        }
        cur_match_pos = nxt_match_pos;

        // pattern compare
        if (cur_edge == exp_edge &&
            cur_sample == exp_sample)
            nxt_match_pos += nxt ? 1 : -1;

        if (nxt ? nxt_match_pos > end_match_pos :
                  nxt_match_pos < start_match_pos) {
            // all matched
            return true;
        } else if (nxt ? nxt_match_pos > cur_match_pos :
                         nxt_match_pos < cur_match_pos) {
            // one stage matched
            int64_t sub_index = nxt ? index + 1 : index - 1;
            while((nxt ? cur_match_pos++ < end_match_pos : cur_match_pos-- > start_match_pos) &&
                  (nxt ? sub_index <= end : sub_index >= start)) {
                // get expacted and current pattern
                exp_index.clear();
                cur_edge.clear();
                for(it_type iterator = pattern.begin();
                    iterator != pattern.end(); iterator++) {
                    uint16_t ch_index = iterator->first;
                    if (!has_data(ch_index))
                        continue;
                    exp_index.push_back(sub_index);
                    cur_sample[ch_index] = get_sample(sub_index, ch_index);

                    if (iterator->second[cur_match_pos] == '0') {
                        exp_sample[ch_index] = false;
                    } else if (iterator->second[cur_match_pos] == '1') {
                        exp_sample[ch_index] = true;
                    } else if (iterator->second[cur_match_pos] == 'X') {
                        exp_sample[ch_index] = cur_sample[ch_index];
                    } else if (iterator->second[cur_match_pos] == 'F') {
                        exp_sample[ch_index] = false;
                        exp_edge[ch_index] = true;
                    } else if (iterator->second[cur_match_pos] == 'R') {
                        exp_sample[ch_index] = true;
                        exp_edge[ch_index] = true;
                    } else if (iterator->second[cur_match_pos] == 'C') {
                        exp_sample[ch_index] = cur_sample[ch_index];
                        exp_edge[ch_index] = true;
                    }

                    if (exp_edge.find(ch_index) != exp_edge.end()) {
                        if (sub_index > start) {
                            bool sample = get_sample(sub_index - 1, ch_index);
                            if (sample != cur_sample[ch_index])
                                cur_edge[ch_index] = true;
                        }
                    }
                }

                // pattern compare
                if (cur_edge != exp_edge ||
                    cur_sample != exp_sample) {
                    cur_match_pos = nxt_match_pos - 1;
                    index += nxt ? 1 : -1;
                    break;
                } else {
                    sub_index += nxt ? 1 : -1;
                }
            }

            if (nxt ? cur_match_pos > end_match_pos :
                    cur_match_pos < start_match_pos) {
                index = nxt ? sub_index - 1 : index;
                return true;
            } else {
                return false;
            }
        } else {
            // not matched, find the next index for pattern compare
            find_edge = true;
            int seq = 0;
            for(it_type iterator = pattern.begin();
                iterator != pattern.end(); iterator++) {
                uint16_t ch_index = iterator->first;
                if (!has_data(ch_index))
                    continue;
                if (exp_edge.find(ch_index) != exp_edge.end() ||
                    cur_sample[ch_index] != exp_sample[ch_index]) {
                    do {
                        if (nxt) {
                            exp_index[seq] += 1;
                            find_edge = get_nxt_edge(exp_index[seq], cur_sample[ch_index], end, 1, ch_index);
                        } else {
                            exp_index[seq] -= 1;
                            cur_sample[ch_index] = get_sample(exp_index[seq], ch_index);
                            find_edge = get_pre_edge(exp_index[seq], cur_sample[ch_index], 1, ch_index);
                        }
                        if (find_edge) {
                            cur_sample[ch_index] = get_sample(exp_index[seq], ch_index);
                            if (iterator->second[cur_match_pos] == 'C')
                                exp_sample[ch_index] = cur_sample[ch_index];
                        } else
                            break;
                    }while(cur_sample[ch_index] != exp_sample[ch_index]);
                }
                if (find_edge)
                    seq++;
                else
                    break;
            }
            if (find_edge) {
                if (nxt)
                    index = *max_element(exp_index.begin(), exp_index.end());
                else
                    index = *min_element(exp_index.begin(), exp_index.end());
            } else {
                break;
            }
        }
    }
    return false;
}

bool LogicSnapshot::has_data(int sig_index)
{
    return get_ch_order(sig_index) != -1;
}

int LogicSnapshot::get_block_num()
{
    return (_ring_sample_count >> LeafBlockPower) +
           ((_ring_sample_count & LeafMask) != 0);
}

uint64_t LogicSnapshot::get_block_size(int block_index)
{
    assert(block_index < get_block_num());

    if (block_index < get_block_num() - 1) {
        return LeafBlockSamples / 8;
    } else {
        if (_ring_sample_count % LeafBlockSamples == 0)
            return LeafBlockSamples / 8;
        else
            return (_ring_sample_count % LeafBlockSamples) / 8;
    }
}

uint8_t *LogicSnapshot::get_block_buf(int block_index, int sig_index, bool &sample)
{
    assert(block_index < get_block_num());

    int order = get_ch_order(sig_index);
    if (order == -1) {
        sample = 0;
        return NULL;
    }
    uint64_t index = block_index / RootScale;
    uint8_t pos = block_index % RootScale;
    uint8_t *lbp = (uint8_t *)_ch_data[order][index].lbp[pos];

    if (lbp == NULL)
        sample = (_ch_data[order][index].value & 1ULL << pos) != 0;

    return lbp;
}

int LogicSnapshot::get_ch_order(int sig_index)
{
    uint16_t order = 0;
    for (auto& iter:_ch_index) {
        if (iter == sig_index)
            break;
        order++;
    }

    if (order >= _ch_index.size())
        return -1;
    else
        return order;
}

} // namespace data
} // namespace pv
