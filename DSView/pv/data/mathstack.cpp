/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2016 DreamSourceLab <support@dreamsourcelab.com>
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

#include "mathstack.h"

#include <boost/foreach.hpp>
#include <boost/thread/thread.hpp>

#include <pv/data/dso.h>
#include <pv/data/dsosnapshot.h>
#include <pv/sigsession.h>
#include <pv/view/dsosignal.h>

#define PI 3.1415

using namespace boost;
using namespace std;

namespace pv {
namespace data {

const int MathStack::EnvelopeScalePower = 8;
const int MathStack::EnvelopeScaleFactor = 1 << EnvelopeScalePower;
const float MathStack::LogEnvelopeScaleFactor = logf(EnvelopeScaleFactor);
const uint64_t MathStack::EnvelopeDataUnit = 4*1024;	// bytes

const uint64_t MathStack::vDialValue[MathStack::vDialValueCount] = {
    1,
    2,
    5,
    10,
    20,
    50,
    100,
    200,
    500,
    1000,
    2000,
    5000,
    10000,
    20000,
    50000,
    100000,
    200000,
    500000,
    1000000,
};
const QString MathStack::vDialAddUnit[MathStack::vDialUnitCount] = {
    "mV",
    "V",
};
const QString MathStack::vDialMulUnit[MathStack::vDialUnitCount] = {
    "mV*V",
    "V*V",
};
const QString MathStack::vDialDivUnit[MathStack::vDialUnitCount] = {
    "mV/V",
    "V/V",
};

MathStack::MathStack(pv::SigSession &session,
                     boost::shared_ptr<view::DsoSignal> dsoSig1,
                     boost::shared_ptr<view::DsoSignal> dsoSig2,
                     MathType type) :
    _session(session),
    _dsoSig1(dsoSig1),
    _dsoSig2(dsoSig2),
    _type(type),
    _sample_num(0),
    _total_sample_num(0),
    _math_state(Init),
    _envelope_en(false),
    _envelope_done(false)
{
    memset(_envelope_level, 0, sizeof(_envelope_level));
}

MathStack::~MathStack()
{
    _math.clear();
    free_envelop();
}

void MathStack::free_envelop()
{
    BOOST_FOREACH(Envelope &e, _envelope_level) {
        if (e.samples)
            free(e.samples);
    }
    memset(_envelope_level, 0, sizeof(_envelope_level));
}

void MathStack::clear()
{
    boost::lock_guard<boost::recursive_mutex> lock(_mutex);
}

void MathStack::init()
{
    boost::lock_guard<boost::recursive_mutex> lock(_mutex);

    _sample_num = 0;
    _envelope_done = false;
}

MathStack::MathType MathStack::get_type() const
{
    return _type;
}

uint64_t MathStack::get_sample_num() const
{
    return _sample_num;
}

void MathStack::realloc(uint64_t num)
{
    if (num != _total_sample_num) {
        free_envelop();
        _total_sample_num = num;

        _math.resize(_total_sample_num);
        uint64_t envelop_count = _total_sample_num / EnvelopeScaleFactor;
        for (unsigned int level = 0; level < ScaleStepCount; level++) {
            envelop_count = ((envelop_count + EnvelopeDataUnit - 1) /
                    EnvelopeDataUnit) * EnvelopeDataUnit;
            _envelope_level[level].samples = (EnvelopeSample*)malloc(envelop_count * sizeof(EnvelopeSample));
            envelop_count = envelop_count / EnvelopeScaleFactor;
        }
    }
}

void MathStack::enable_envelope(bool enable)
{
    if (!_envelope_done && enable)
        append_to_envelope_level(true);
    _envelope_en = enable;
}

uint64_t MathStack::default_vDialValue()
{
    uint64_t value = 0;
    view::dslDial *dial1 = _dsoSig1->get_vDial();
    view::dslDial *dial2 = _dsoSig1->get_vDial();
    const uint64_t dial1_value = dial1->get_value() * dial1->get_factor();
    const uint64_t dial2_value = dial2->get_value() * dial2->get_factor();

    switch(_type) {
    case MATH_ADD:
    case MATH_SUB:
        value = max(dial1_value, dial2_value);
        break;
    case MATH_MUL:
        value = dial1_value * dial2_value / 1000.0;
        break;
    case MATH_DIV:
        value = dial1_value * 1000.0 / dial2_value;
        break;
    }

    for (int i = 0; i < vDialValueCount; i++) {
        if (vDialValue[i] >= value) {
            value = vDialValue[i];
            break;
        }
    }

    return value;
}

view::dslDial * MathStack::get_vDial()
{
    QVector<uint64_t> vValue;
    QVector<QString> vUnit;
    view::dslDial *dial1 = _dsoSig1->get_vDial();
    view::dslDial *dial2 = _dsoSig2->get_vDial();
    const uint64_t dial1_min = dial1->get_value(0) * dial1->get_factor();
    const uint64_t dial1_max = dial1->get_value(dial1->get_count() - 1) * dial1->get_factor();
    const uint64_t dial2_min = dial2->get_value(0) * dial2->get_factor();
    const uint64_t dial2_max = dial2->get_value(dial2->get_count() - 1) * dial2->get_factor();

    switch(_type) {
    case MATH_ADD:
    case MATH_SUB:
        for (int i = 0; i < vDialValueCount; i++) {
            if (vDialValue[i] < min(dial1_min, dial2_min))
                continue;
            vValue.append(vDialValue[i]);
            if (vDialValue[i] > max(dial1_max, dial2_max))
                break;
        }
        for(int i = 0; i < vDialUnitCount; i++)
            vUnit.append(vDialAddUnit[i]);
        break;
    case MATH_MUL:
        for (int i = 0; i < vDialValueCount; i++) {
            if (vDialValue[i] < dial1_min * dial2_min / 1000.0)
                continue;
            vValue.append(vDialValue[i]);
            if (vDialValue[i] > dial1_max * dial2_max / 1000.0)
                break;
        }
        for(int i = 0; i < vDialUnitCount; i++)
            vUnit.append(vDialMulUnit[i]);
        break;
    case MATH_DIV:
        for (int i = 0; i < vDialValueCount; i++) {
            if (vDialValue[i] < min(dial1_min * 1000.0 / dial2_max, dial2_min * 1000.0  / dial1_max))
                continue;
            vValue.append(vDialValue[i]);
            if (vDialValue[i] > max(dial1_max * 1000.0 / dial2_min, dial2_max * 1000.0 / dial1_min))
                break;
        }
        for(int i = 0; i < vDialUnitCount; i++)
            vUnit.append(vDialDivUnit[i]);
        break;
    }

    view::dslDial *vDial = new view::dslDial(vValue.count(), vDialValueStep, vValue, vUnit);
    return vDial;
}

QString MathStack::get_unit(int level)
{
    if (level >= vDialUnitCount)
        return tr(" ");

    QString unit;
    switch(_type) {
    case MATH_ADD:
    case MATH_SUB:
        unit = vDialAddUnit[level];
        break;
    case MATH_MUL:
        unit = vDialMulUnit[level];
        break;
    case MATH_DIV:
        unit = vDialDivUnit[level];
        break;
    }

    return unit;
}

double MathStack::get_math_scale()
{
    double scale = 0;
    switch(_type) {
    case MATH_ADD:
    case MATH_SUB:
        scale = 1.0 / DS_CONF_DSO_VDIVS;
        break;
    case MATH_MUL:
        //scale = 1.0 / (DS_CONF_DSO_VDIVS * DS_CONF_DSO_VDIVS);
        scale = 1.0 / DS_CONF_DSO_VDIVS;
        break;
    case MATH_DIV:
        scale = 1.0 / DS_CONF_DSO_VDIVS;
        break;
    }

    return scale;
}

const double* MathStack::get_math(uint64_t start) const
{
    return _math.data() + start;
}

void MathStack::get_math_envelope_section(EnvelopeSection &s,
    uint64_t start, uint64_t end, float min_length) const
{
    assert(end <= get_sample_num());
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
    if (_envelope_level[min_level].length == 0)
        s.length = 0;
    else
        s.length = end - start;

    s.samples = _envelope_level[min_level].samples + start;
}

void MathStack::calc_math()
{
    boost::lock_guard<boost::recursive_mutex> lock(_mutex);

    _math_state = Running;

    const boost::shared_ptr<pv::data::Dso> data = _dsoSig1->dso_data();
    const deque< boost::shared_ptr<pv::data::DsoSnapshot> > &snapshots =
        data->get_snapshots();
    if (snapshots.empty())
        return;

    const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot =
        snapshots.front();
    if (snapshot->empty())
        return;

    if (_math.size() < _total_sample_num)
        return;

    if (!_dsoSig1->enabled() || !_dsoSig2->enabled())
        return;

    const double scale1 = _dsoSig1->get_vDialValue() / 1000.0 * _dsoSig1->get_factor() * DS_CONF_DSO_VDIVS *
                          _dsoSig1->get_scale() / _dsoSig1->get_view_rect().height();
    const double delta1 = _dsoSig1->get_hw_offset() * scale1;

    const double scale2 = _dsoSig2->get_vDialValue() / 1000.0 * _dsoSig2->get_factor() * DS_CONF_DSO_VDIVS *
                          _dsoSig2->get_scale() / _dsoSig2->get_view_rect().height();
    const double delta2 = _dsoSig2->get_hw_offset() * scale2;

    const int index1 = _dsoSig1->get_index();
    const int index2 = _dsoSig2->get_index();

    const int num_channels = snapshot->get_channel_num();
    const uint8_t* value = snapshot->get_samples(0, 0, 0);
    _sample_num = snapshot->get_sample_count();
    assert(_sample_num <= _total_sample_num);

    double value1, value2;
    for (uint64_t sample = 0; sample < _sample_num; sample++) {
        value1 = value[sample * num_channels + index1];
        value2 = value[sample * num_channels + index2];
        switch(_type) {
        case MATH_ADD:
            _math[sample] = (delta1 - scale1 * value1) + (delta2 - scale2 * value2);
            break;
        case MATH_SUB:
            _math[sample] = (delta1 - scale1 * value1) - (delta2 - scale2 * value2);
            break;
        case MATH_MUL:
            _math[sample] = (delta1 - scale1 * value1) * (delta2 - scale2 * value2);
            break;
        case MATH_DIV:
            _math[sample] = (delta1 - scale1 * value1) / (delta2 - scale2 * value2);
            break;
        }
    }

    if (_envelope_en)
        append_to_envelope_level(true);

    // stop
    _math_state = Stopped;
}

void MathStack::reallocate_envelope(Envelope &e)
{
    const uint64_t new_data_length = ((e.length + EnvelopeDataUnit - 1) /
        EnvelopeDataUnit) * EnvelopeDataUnit;
    if (new_data_length > e.data_length)
    {
        e.data_length = new_data_length;
    }
}

void MathStack::append_to_envelope_level(bool header)
{
    Envelope &e0 = _envelope_level[0];
    uint64_t prev_length;
    EnvelopeSample *dest_ptr;

    if (header)
        prev_length = 0;
    else
        prev_length = e0.length;
    e0.length = _sample_num / EnvelopeScaleFactor;

    if (e0.length == 0)
        return;
    if (e0.length == prev_length)
        prev_length = 0;

    // Expand the data buffer to fit the new samples
    reallocate_envelope(e0);

    dest_ptr = e0.samples + prev_length;

    // Iterate through the samples to populate the first level mipmap
    const double *const stop_src_ptr = (double*)_math.data() +
        e0.length * EnvelopeScaleFactor;
    for (const double *src_ptr = (double*)_math.data() +
        prev_length * EnvelopeScaleFactor;
        src_ptr < stop_src_ptr; src_ptr += EnvelopeScaleFactor)
    {
        const double * begin_src_ptr =
            src_ptr;
        const double *const end_src_ptr =
            src_ptr + EnvelopeScaleFactor;

        EnvelopeSample sub_sample;
        sub_sample.min = *begin_src_ptr;
        sub_sample.max = *begin_src_ptr;
        //begin_src_ptr += _channel_num;
        while (begin_src_ptr < end_src_ptr)
        {
            sub_sample.min = min(sub_sample.min, *begin_src_ptr);
            sub_sample.max = max(sub_sample.max, *begin_src_ptr);
            begin_src_ptr ++;
        }
        *dest_ptr++ = sub_sample;
    }

    // Compute higher level mipmaps
    for (unsigned int level = 1; level < ScaleStepCount; level++)
    {
        Envelope &e = _envelope_level[level];
        const Envelope &el = _envelope_level[level-1];

        // Expand the data buffer to fit the new samples
        prev_length = e.length;
        e.length = el.length / EnvelopeScaleFactor;

        // Break off if there are no more samples to computed
//		if (e.length == prev_length)
//			break;
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

    _envelope_done = true;
}

} // namespace data
} // namespace pv
