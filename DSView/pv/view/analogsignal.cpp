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

#include <math.h>

#include "../view/analogsignal.h"
#include "../data/analog.h"
#include "../data/analogsnapshot.h"
#include "../view/view.h"
#include "../device/devinst.h"

using namespace boost;
using namespace std;

#define byte(x) uint##x##_t

namespace pv {
namespace view {

const QColor AnalogSignal::SignalColours[4] = {
    QColor(238, 178, 17, 255),  // dsYellow
    QColor(0, 153, 37, 255),    // dsGreen
    QColor(213, 15, 37, 255),   // dsRed
    QColor(17, 133, 209, 255)  // dsBlue
};

const float AnalogSignal::EnvelopeThreshold = 16.0f;

AnalogSignal::AnalogSignal(boost::shared_ptr<pv::device::DevInst> dev_inst,
                           boost::shared_ptr<data::Analog> data,
                           sr_channel *probe) :
    Signal(dev_inst, probe),
    _data(data),
    _rects(NULL)
{
    _typeWidth = 3;
    _colour = SignalColours[probe->index % countof(SignalColours)];
    _bits = -1;
    _zero_vrate = 0.5;
}

AnalogSignal::AnalogSignal(boost::shared_ptr<view::AnalogSignal> s,
                         boost::shared_ptr<pv::data::Analog> data,
                         sr_channel *probe) :
    Signal(*s.get(), probe),
    _data(data),
    _rects(NULL)
{
    _typeWidth = 3;
    _bits = s->get_bits();
    _zero_vrate = s->get_zero_vrate();

    _scale = s->get_scale();
    _hw_offset = s->get_hw_offset();
}

AnalogSignal::~AnalogSignal()
{
    if (_rects) {
        delete[] _rects;
        _rects = NULL;
    }
}

boost::shared_ptr<pv::data::SignalData> AnalogSignal::data() const
{
    return _data;
}

void AnalogSignal::set_scale(float scale)
{
	_scale = scale;
}

float AnalogSignal::get_scale() const
{
    return _scale;
}

int AnalogSignal::get_bits() const
{
    return _bits;
}

int AnalogSignal::get_hw_offset() const
{
    return _hw_offset;
}

int AnalogSignal::commit_settings()
{
    int ret;

    // -- enable
    ret = _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_EN,
                                g_variant_new_boolean(enabled()));

    // -- vdiv
    ret = _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_VDIV,
                                g_variant_new_uint64(_probe->vdiv));

    // -- coupling
    ret = _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_COUPLING,
                                g_variant_new_byte(_probe->coupling));

    // -- vpos
    ret = _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_VPOS,
                                g_variant_new_double(_probe->vpos));

    // -- trig_value
    _dev_inst->set_config(_probe, NULL, SR_CONF_TRIGGER_VALUE,
                          g_variant_new_byte(_probe->trig_value));

    return ret;
}

/**
 * Probe options
 **/
uint64_t AnalogSignal::get_vdiv() const
{
    uint64_t vdiv = 0;
    GVariant* gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_PROBE_VDIV);
    if (gvar != NULL) {
        vdiv = g_variant_get_uint64(gvar);
        g_variant_unref(gvar);
    }
    return vdiv;
}

uint8_t AnalogSignal::get_acCoupling() const
{
    uint64_t coupling = 0;
    GVariant* gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_PROBE_COUPLING);
    if (gvar != NULL) {
        coupling = g_variant_get_byte(gvar);
        g_variant_unref(gvar);
    }
    return coupling;
}

QString AnalogSignal::get_mapUnit() const
{
    QString unit;
    GVariant* gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_PROBE_MAP_UNIT);
    if (gvar != NULL) {
        unit = g_variant_get_string(gvar, NULL);
        g_variant_unref(gvar);
    }
    return unit;
}

double AnalogSignal::get_mapMin() const
{
    double min = -1;
    GVariant* gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_PROBE_MAP_MIN);
    if (gvar != NULL) {
        min = g_variant_get_double(gvar);
        g_variant_unref(gvar);
    }
    return min;
}

double AnalogSignal::get_mapMax() const
{
    double max = 1;
    GVariant* gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_PROBE_MAP_MAX);
    if (gvar != NULL) {
        max = g_variant_get_double(gvar);
        g_variant_unref(gvar);
    }
    return max;
}

/**
 *
 **/
void AnalogSignal::set_zero_vpos(int pos)
{
    if (enabled()) {
        const int height = get_totalHeight();
        const int top = get_y() - height / 2;
        set_zero_vrate(min(max(pos - top, 0), height) * 1.0 / height, false);
    }
}

int AnalogSignal::get_zero_vpos() const
{
    return (_zero_vrate - 0.5) * get_totalHeight() + get_y();
}

void AnalogSignal::set_zero_vrate(double rate, bool force_update)
{
    if (_view->session().get_capture_state() == SigSession::Running)
        return;

    _zero_vrate = rate;
    update_vpos();

    if (force_update)
        update_offset();
}

double AnalogSignal::get_zero_vrate() const
{
    return _zero_vrate;
}

void AnalogSignal::update_vpos()
{
    double vpos_off = (0.5 - _zero_vrate) * get_vdiv() * DS_CONF_DSO_VDIVS;
    _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_VPOS,
                          g_variant_new_double(vpos_off));
}

void AnalogSignal::update_offset()
{
    if (_dev_inst->name().contains("virtual") ||
        _dev_inst->name() == "DSLogic")
        _hw_offset = (1 << _bits) * 0.5;
    else
        _hw_offset = _zero_vrate * ((1 << _bits) - 1);
}
/**
 * Event
 **/
void AnalogSignal::resize()
{
    if (_rects) {
        delete[] _rects;
        _rects = NULL;
    }
}

/**
 * Paint
 **/
void AnalogSignal::paint_back(QPainter &p, int left, int right)
{
    assert(_view);

    int i, j;
    const double height = get_totalHeight();
    const int DIVS = DS_CONF_DSO_VDIVS;
    const int minDIVS = 5;
    const double STEPS = height / (DIVS * minDIVS);
    const double mapSteps = (get_mapMax() - get_mapMin()) / DIVS;
    const QString mapUnit = get_mapUnit();

    QPen solidPen(Signal::dsFore);
    solidPen.setStyle(Qt::SolidLine);
    p.setPen(solidPen);
    p.setBrush(Trace::dsBack);

    // paint rule
    double y = get_y() - height * 0.5;
    double mapValue = get_mapMax() + (_zero_vrate - 0.5) * (get_mapMax() - get_mapMin());
    for (i = 0; i < DIVS; i++) {
        p.drawLine(left, y, left+10, y);
        if (i == 0 || i == DIVS/2)
            p.drawText(QRectF(left+15, y-10, 100, 20),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       QString::number(mapValue,'f',2)+mapUnit);
        p.drawLine(right, y, right-10, y);
        if (i == 0 || i == DIVS/2)
            p.drawText(QRectF(right-115, y-10, 100, 20),
                       Qt::AlignRight | Qt::AlignVCenter,
                       QString::number(mapValue,'f',2)+mapUnit);
        for (j = 0; j < minDIVS - 1; j++) {
            y += STEPS;
            p.drawLine(left, y, left+5, y);
            p.drawLine(right, y, right-5, y);
        }
        y += STEPS;
        mapValue -= mapSteps;
    }
    p.drawLine(left, y, left+10, y);
    p.drawText(QRectF(left+15, y-10, 100, 20),
               Qt::AlignLeft | Qt::AlignVCenter,
               QString::number(mapValue,'f',2)+mapUnit);
    p.drawLine(right, y, right-10, y);
    p.drawText(QRectF(right-115, y-10, 100, 20),
               Qt::AlignRight | Qt::AlignVCenter,
               QString::number(mapValue,'f',2)+mapUnit);
}

void AnalogSignal::paint_mid(QPainter &p, int left, int right)
{
    assert(_data);
    assert(_view);
    assert(right >= left);

    const int height = get_totalHeight();
    const int top = get_y() - height * 0.5;
    const int bottom = get_y() + height * 0.5;
    const float zeroY = _zero_vrate * height + top;

    const double scale = _view->scale();
    assert(scale > 0);
    const int64_t offset = _view->offset();

    const deque< boost::shared_ptr<pv::data::AnalogSnapshot> > &snapshots =
        _data->get_snapshots();
    if (snapshots.empty())
        return;

    const boost::shared_ptr<pv::data::AnalogSnapshot> &snapshot =
        snapshots.front();
    if (snapshot->empty())
        return;

    const int order = snapshot->get_ch_order(get_index());
    if (order == -1)
        return;

    if (_bits != snapshot->get_unit_bytes()*8) {
        _bits = snapshot->get_unit_bytes()*8;
        _scale = _totalHeight * 1.0f / ((1 << _bits) - 1);
        update_offset();
    }
    const double pixels_offset = offset;
    const double samplerate = _data->samplerate();
    const int64_t cur_sample_count = snapshot->get_sample_count();
    const double samples_per_pixel = samplerate * scale;
    const uint64_t ring_start = snapshot->get_ring_start();

//    int64_t start_pixel;
//    uint64_t start_index;
//    int64_t start_skew_pixels;
    //const double first_pos = (_view->session().cur_samplelimits() - cur_sample_count) / samples_per_pixel;
    //    const double start_sample = (snapshot->get_ring_start() +
    //                    (pixels_offset + left - first_pos) * samples_per_pixel);
    //start_pixel = floor(first_pos - pixels_offset - left);
    //    if (start_sample < 0) {
    //        start_index = 0;
    //        start_skew_pixels = 0;
    //    } else {
    //        start_index = (uint64_t)(start_sample) % cur_sample_count;
    //        start_skew_pixels = (start_sample - floor(start_sample)) / samples_per_pixel;
    //    }
    //    if (start_pixel < left)
    //        start_pixel = left;
    //    start_pixel -= start_skew_pixels;
    //    int64_t show_length = ceil(samples_per_pixel*(right - start_pixel + 1));

    int64_t start_pixel;
    uint64_t start_index;
    const double over_pixel = cur_sample_count / samples_per_pixel -
                              pixels_offset - right;
    if (over_pixel <= left - right) {
        return;
    } else if (over_pixel <= 0) {
        start_index = ring_start;
        start_pixel = over_pixel + right - left;
    } else {
        const double over_sample = over_pixel * samples_per_pixel;
        start_index = (uint64_t)(ring_start + floor(over_sample)) % cur_sample_count;
        start_pixel = right + (over_sample - floor(over_sample)) / samples_per_pixel;
    }

    int64_t show_length = ceil(samples_per_pixel*(start_pixel + 1));
    if (show_length <= 0)
        return;

    if (samples_per_pixel < EnvelopeThreshold)
        paint_trace(p, snapshot, zeroY,
            start_pixel, start_index, show_length,
            samples_per_pixel, order,
            top, bottom, right-left);
    else
        paint_envelope(p, snapshot, zeroY,
            start_pixel, start_index, show_length,
            samples_per_pixel, order,
            top, bottom, right-left);
}

void AnalogSignal::paint_trace(QPainter &p,
    const boost::shared_ptr<pv::data::AnalogSnapshot> &snapshot,
    int zeroY, const int start_pixel,
    const uint64_t start_index, const int64_t sample_count,
    const double samples_per_pixel, const int order,
    const int top, const int bottom, const int width)
{
    (void)width;

    const int64_t channel_num = snapshot->get_channel_num();
    if (sample_count > 0) {
        const uint8_t unit_bytes = snapshot->get_unit_bytes();
        const uint8_t *const samples = snapshot->get_samples(0);
        assert(samples);

        p.setPen(_colour);
        //p.setPen(QPen(_colour, 2, Qt::SolidLine));

        QPointF *points = new QPointF[sample_count + 2];
        QPointF *point = points;
        uint64_t yindex = start_index;
        int x = 0;
//        const int64_t start_offset = start_pixel - (int64_t)(start_index / samples_per_pixel + 0.5);
        //for (int64_t sample = 0; x < right; sample++) {
        const int64_t start_offset = start_pixel + (int64_t)(start_index / samples_per_pixel + 0.5);
        for (int64_t sample = 0; x >= 0; sample++) {
            x = start_offset - (start_index + sample) / samples_per_pixel - 0.5;
            uint64_t index = (yindex * channel_num + order) * unit_bytes;
            double yvalue = samples[index];
            for(uint8_t i = 1; i < unit_bytes; i++)
                yvalue += (samples[++index] << i*8);
            yvalue = zeroY + ((int)yvalue - _hw_offset) * _scale;
            yvalue = min(max((int)yvalue, top), bottom);
            *point++ = QPointF(x, yvalue);
            if (sample != 0 && yindex == snapshot->get_ring_end()) {
                *point++ = QPointF(0, points[sample].y());
                break;
            }
            yindex++;
            yindex %= snapshot->get_sample_count();
        }
        p.drawPolyline(points, point - points);
        delete[] points;
    }
}

void AnalogSignal::paint_envelope(QPainter &p,
    const boost::shared_ptr<pv::data::AnalogSnapshot> &snapshot,
    int zeroY, const int start_pixel,
    const uint64_t start_index, const int64_t sample_count,
    const double samples_per_pixel, const int order,
    const int top, const int bottom, const int width)
{
    using namespace Qt;
    using pv::data::AnalogSnapshot;

    AnalogSnapshot::EnvelopeSection e;
    snapshot->get_envelope_section(e, start_index, sample_count,
                                   samples_per_pixel, order);
    if (e.samples_num == 0)
        return;

    p.setPen(QPen(NoPen));
    //p.setPen(QPen(_colour, 2, Qt::SolidLine));
    p.setBrush(_colour);

    if (!_rects)
        _rects = new QRectF[width+3];
    QRectF *rect = _rects;
    int px = -1, pre_px;
    int y_min = zeroY, y_max = zeroY, pre_y_min = zeroY, pre_y_max = zeroY;
    int pcnt = 0;
    const double scale_samples_pre_pixel = samples_per_pixel / e.scale;
    const uint64_t ring_end = max((int64_t)0, (int64_t)snapshot->get_ring_end() / e.scale - 1);
//    const int64_t start_offset = start_pixel -
//            (int64_t)(e.start / scale_samples_pre_pixel + 0.5);
//    for(uint64_t sample = 0; sample < e.length; sample++) {
    const int64_t start_offset = start_pixel +
            (int64_t)(e.start / scale_samples_pre_pixel + 0.5);
    for(uint64_t sample = 0; sample < e.length; sample++) {
        const uint64_t ring_index = (e.start + sample) % (_view->session().cur_samplelimits() / e.scale);
        if (sample != 0 && ring_index == ring_end)
            break;
//        const int x = start_offset +
//                (e.start + sample) / scale_samples_pre_pixel + 0.5;
        const int x = start_offset -
                (e.start + sample) / scale_samples_pre_pixel - 0.5;
        const AnalogSnapshot::EnvelopeSample *const ev =
            e.samples + ((e.start + sample) % e.samples_num);

        const int b = min(max((int)(zeroY + (ev->max - _hw_offset) * _scale + 0.5), top), bottom);
        const int t = min(max((int)(zeroY + (ev->min - _hw_offset) * _scale + 0.5), top), bottom);

        pre_px = px;
        if(px != x) {
            if (pre_px != -1) {
                // We overlap this sample with the previous so that vertical
                // gaps do not appear during steep rising or falling edges
                if (pre_y_min > y_max)
                    *rect++ = QRectF(pre_px, y_min, 1.0f, pre_y_min-y_min+1);
                else if (pre_y_max < y_min)
                    *rect++ = QRectF(pre_px, pre_y_max, 1.0f, y_max-pre_y_max+1);
                else
                    *rect++ = QRectF(pre_px, y_min, 1.0f, y_max-y_min+1);
                pre_y_min = y_min;
                pre_y_max = y_max;
                pcnt++;
            } else {
                pre_y_max = min(max(b, top), bottom);
                pre_y_min = min(max(t, top), bottom);
            }
            px = x;
            y_max = min(max(b, top), bottom);
            y_min = min(max(t, top), bottom);
        }
        if (px == pre_px) {
            y_max = max(b, y_max);
            y_min = min(t, y_min);
        }
    }

    p.drawRects(_rects, pcnt);
}

} // namespace view
} // namespace pv
