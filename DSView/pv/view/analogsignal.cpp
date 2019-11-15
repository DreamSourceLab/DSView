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
    _rects(NULL),
    _hover_en(false),
    _hover_index(0),
    _hover_point(QPointF(-1, -1)),
    _hover_value(0)
{
    _typeWidth = 5;
    _colour = SignalColours[probe->index % countof(SignalColours)];

    GVariant *gvar;
    // channel bits
    gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_UNIT_BITS);
    if (gvar != NULL) {
        _bits = g_variant_get_byte(gvar);
        g_variant_unref(gvar);
    } else {
        _bits = DefaultBits;
        qDebug("Warning: config_get SR_CONF_UNIT_BITS failed, set to %d(default).", DefaultBits);
    }
    gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_REF_MIN);
    if (gvar != NULL) {
        _ref_min = g_variant_get_uint32(gvar);
        g_variant_unref(gvar);
    } else {
        _ref_min = 1;
    }
    gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_REF_MAX);
    if (gvar != NULL) {
        _ref_max = g_variant_get_uint32(gvar);
        g_variant_unref(gvar);
    } else {
        _ref_max = ((1 << _bits) - 1);
    }

    // -- vpos
    gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_PROBE_OFFSET);
    if (gvar != NULL) {
        _zero_offset = g_variant_get_uint16(gvar);
        g_variant_unref(gvar);
    } else {
        qDebug() << "ERROR: config_get SR_CONF_PROBE_OFFSET failed.";
    }
}

AnalogSignal::AnalogSignal(boost::shared_ptr<view::AnalogSignal> s,
                         boost::shared_ptr<pv::data::Analog> data,
                         sr_channel *probe) :
    Signal(*s.get(), probe),
    _data(data),
    _rects(NULL),
    _hover_en(false),
    _hover_index(0),
    _hover_point(QPointF(-1, -1)),
    _hover_value(0)
{
    _typeWidth = 5;
    _bits = s->get_bits();
    _ref_min = s->get_ref_min();
    _ref_max = s->get_ref_max();
    _zero_offset = s->get_zero_offset();

    _scale = s->get_scale();
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

void AnalogSignal::set_scale(int height)
{
    _scale = height / (_ref_max - _ref_min);
}

float AnalogSignal::get_scale() const
{
    return _scale;
}

int AnalogSignal::get_bits() const
{
    return _bits;
}

double AnalogSignal::get_ref_min() const
{
    return _ref_min;
}

double AnalogSignal::get_ref_max() const
{
    return _ref_max;
}

int AnalogSignal::get_hw_offset() const
{
    int hw_offset = 0;
    GVariant *gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_PROBE_HW_OFFSET);
    if (gvar != NULL) {
        hw_offset = g_variant_get_uint16(gvar);
        g_variant_unref(gvar);
    }
    return hw_offset;
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

    // -- offset
    ret = _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_OFFSET,
                                g_variant_new_uint16(_probe->offset));

    // -- trig_value
    _dev_inst->set_config(_probe, NULL, SR_CONF_TRIGGER_VALUE,
                          g_variant_new_byte(_probe->trig_value));

    return ret;
}

bool AnalogSignal::measure(const QPointF &p)
{
    _hover_en = false;
    if (!enabled())
        return false;

    if (_view->session().get_capture_state() != SigSession::Stopped)
        return false;

    const QRectF window = get_view_rect();
    if (!window.contains(p))
        return false;

    const deque< boost::shared_ptr<pv::data::AnalogSnapshot> > &snapshots =
        _data->get_snapshots();
    if (snapshots.empty())
        return false;

    const boost::shared_ptr<pv::data::AnalogSnapshot> &snapshot =
        snapshots.front();
    if (snapshot->empty())
        return false;

    const double scale = _view->scale();
    assert(scale > 0);
    const int64_t pixels_offset = _view->offset();
    const double samplerate = _view->session().cur_snap_samplerate();
    const double samples_per_pixel = samplerate * scale;

    _hover_index = floor((p.x() + pixels_offset) * samples_per_pixel+0.5);
    if (_hover_index >= snapshot->get_sample_count())
        return false;

    _hover_point = get_point(_hover_index, _hover_value);
    _hover_en = true;
    return true;
}

bool AnalogSignal::get_hover(uint64_t &index, QPointF &p, double &value)
{
    if (_hover_en) {
        index = _hover_index;
        p = _hover_point;
        value = _hover_value;
        return true;
    }
    return false;
}

QPointF AnalogSignal::get_point(uint64_t index, float &value)
{
    QPointF pt = QPointF(-1, -1);

    if (!enabled())
        return pt;

    const deque< boost::shared_ptr<pv::data::AnalogSnapshot> > &snapshots =
        _data->get_snapshots();
    if (snapshots.empty())
        return pt;

    const boost::shared_ptr<pv::data::AnalogSnapshot> &snapshot =
        snapshots.front();
    if (snapshot->empty())
        return pt;

    const int order = snapshot->get_ch_order(get_index());
    if (order == -1)
        return pt;

    const double scale = _view->scale();
    assert(scale > 0);
    const int64_t pixels_offset = _view->offset();
    const double samplerate = _view->session().cur_snap_samplerate();
    const double samples_per_pixel = samplerate * scale;

    if (index >= snapshot->get_sample_count())
        return pt;

    const uint64_t ring_index = (uint64_t)(snapshot->get_ring_start() + floor(index)) % snapshot->get_sample_count();
    value = *(snapshot->get_samples(ring_index) + order*snapshot->get_unit_bytes());

    const int height = get_totalHeight();
    const float top = get_y() - height * 0.5;
    const float bottom = get_y() + height * 0.5;
    const int hw_offset = get_hw_offset();
    const float x = (index / samples_per_pixel - pixels_offset);
    const float y = min(max(top, get_zero_vpos() + (value - hw_offset)* _scale), bottom);
    pt = QPointF(x, y);

    return pt;
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

bool AnalogSignal::get_mapDefault() const
{
    bool isDefault = true;
    GVariant* gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_PROBE_MAP_DEFAULT);
    if (gvar != NULL) {
        isDefault = g_variant_get_boolean(gvar);
        g_variant_unref(gvar);
    }
    return isDefault;
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

uint64_t AnalogSignal::get_factor() const
{
    GVariant* gvar;
    uint64_t factor;
    gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_PROBE_FACTOR);
    if (gvar != NULL) {
        factor = g_variant_get_uint64(gvar);
        g_variant_unref(gvar);
        return factor;
    } else {
        qDebug() << "ERROR: config_get SR_CONF_PROBE_FACTOR failed.";
        return 1;
    }
}

int AnalogSignal::ratio2value(double ratio) const
{
    return ratio * (_ref_max - _ref_min) + _ref_min;
}

int AnalogSignal::ratio2pos(double ratio) const
{
    const int height = get_totalHeight();
    const int top = get_y() - height * 0.5;
    return ratio * height + top;
}

double AnalogSignal::value2ratio(int value) const
{
    return max(0.0, (value - _ref_min) / (_ref_max - _ref_min));
}

double AnalogSignal::pos2ratio(int pos) const
{
    const int height = get_totalHeight();
    const int top = get_y() - height / 2;
    return min(max(pos - top, 0), height) * 1.0 / height;
}

/**
 *
 **/
void AnalogSignal::set_zero_vpos(int pos)
{
    if (enabled()) {
        set_zero_ratio(pos2ratio(pos));
    }
}

int AnalogSignal::get_zero_vpos() const
{
    return ratio2pos(get_zero_ratio());
}

void AnalogSignal::set_zero_ratio(double ratio)
{
    if (_view->session().get_capture_state() == SigSession::Running)
        return;

    _zero_offset = ratio2value(ratio);
    _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_OFFSET,
                          g_variant_new_uint16(_zero_offset));
}

double AnalogSignal::get_zero_ratio() const
{
    return value2ratio(_zero_offset);
}

int AnalogSignal::get_zero_offset() const
{
    return _zero_offset;
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
void AnalogSignal::paint_back(QPainter &p, int left, int right, QColor fore, QColor back)
{
    assert(_view);

    int i, j;
    const double height = get_totalHeight();
    const int DIVS = DS_CONF_DSO_VDIVS;
    const int minDIVS = 5;
    const double STEPS = height / (DIVS * minDIVS);
    const double mapSteps = (get_mapMax() - get_mapMin()) / DIVS;
    const QString mapUnit = get_mapUnit();

    QPen solidPen(fore);
    solidPen.setStyle(Qt::SolidLine);
    p.setPen(solidPen);
    p.setBrush(back);

    // paint rule
    double y = get_y() - height * 0.5;
    double mapValue = get_mapMax() + (get_zero_ratio() - 0.5) * (get_mapMax() - get_mapMin());
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

void AnalogSignal::paint_mid(QPainter &p, int left, int right, QColor fore, QColor back)
{
    (void)fore;
    (void)back;

    assert(_data);
    assert(_view);
    assert(right >= left);

    const int height = get_totalHeight();
    const float top = get_y() - height * 0.5;
    const float bottom = get_y() + height * 0.5;
    const float zeroY = ratio2pos(get_zero_ratio());
    const int width = right - left + 1;

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

    const double pixels_offset = offset;
    const double samplerate = _data->samplerate();
    const int64_t cur_sample_count = snapshot->get_sample_count();
    const double samples_per_pixel = samplerate * scale;
    const uint64_t ring_start = snapshot->get_ring_start();

    int64_t start_pixel;
    uint64_t start_index;
    const double index_offset = pixels_offset * samples_per_pixel;
    start_index = (uint64_t)(ring_start + floor(index_offset)) % cur_sample_count;
    start_pixel = (floor(index_offset) - index_offset) / samples_per_pixel ;

    int64_t show_length = min(floor(cur_sample_count - floor(index_offset)), ceil(width*samples_per_pixel + 1));
    if (show_length <= 0)
        return;

    if (samples_per_pixel < EnvelopeThreshold)
        paint_trace(p, snapshot, zeroY,
            start_pixel, start_index, show_length,
            samples_per_pixel, order,
            top, bottom, width);
    else
        paint_envelope(p, snapshot, zeroY,
            start_pixel, start_index, show_length,
            samples_per_pixel, order,
            top, bottom, width);
}

void AnalogSignal::paint_fore(QPainter &p, int left, int right, QColor fore, QColor back)
{
    assert(_view);

    fore.setAlpha(View::BackAlpha);
    QPen pen(fore);
    pen.setStyle(Qt::DotLine);
    p.setPen(pen);
    p.drawLine(left, get_zero_vpos(), right, get_zero_vpos());

    fore.setAlpha(View::ForeAlpha);
    if(enabled()) {
        // Paint measure
        if (_view->session().get_capture_state() == SigSession::Stopped)
            paint_hover_measure(p, fore, back);
    }
}

void AnalogSignal::paint_trace(QPainter &p,
    const boost::shared_ptr<pv::data::AnalogSnapshot> &snapshot,
    int zeroY, const int start_pixel,
    const uint64_t start_index, const int64_t sample_count,
    const double samples_per_pixel, const int order,
    const float top, const float bottom, const int width)
{
    (void)width;

    const int64_t channel_num = snapshot->get_channel_num();
    if (sample_count > 0) {
        const uint8_t unit_bytes = snapshot->get_unit_bytes();
        const uint8_t *const samples = snapshot->get_samples(0);
        assert(samples);

        p.setPen(_colour);
        //p.setPen(QPen(_colour, 2, Qt::SolidLine));

        QPointF *points = new QPointF[sample_count];
        QPointF *point = points;
        uint64_t yindex = start_index;

        const int hw_offset = get_hw_offset();
        float x = start_pixel;
        double  pixels_per_sample = 1.0/samples_per_pixel;
        for (int64_t sample = 0; sample < sample_count; sample++) {
            uint64_t index = (yindex * channel_num + order) * unit_bytes;
            float yvalue = samples[index];
            for(uint8_t i = 1; i < unit_bytes; i++)
                yvalue += (samples[++index] << i*8);
            yvalue = zeroY + (yvalue - hw_offset) * _scale;
            yvalue = min(max(yvalue, top), bottom);
            *point++ = QPointF(x, yvalue);
            if (yindex == snapshot->get_ring_end())
                break;
            yindex++;
            yindex %= snapshot->get_sample_count();
            x += pixels_per_sample;
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
    const float top, const float bottom, const int width)
{
    using namespace Qt;
    using pv::data::AnalogSnapshot;

    AnalogSnapshot::EnvelopeSection e;
    snapshot->get_envelope_section(e, start_index, sample_count,
                                   samples_per_pixel, order);
    if (e.samples_num == 0)
        return;

    p.setPen(QPen(NoPen));
    p.setBrush(_colour);

    if (!_rects)
        _rects = new QRectF[width+10];
    QRectF *rect = _rects;
    int px = -1, pre_px;
    float y_min = zeroY, y_max = zeroY, pre_y_min = zeroY, pre_y_max = zeroY;
    int pcnt = 0;
    const double scale_pixels_per_samples = e.scale / samples_per_pixel;
    const uint64_t ring_end = max((int64_t)0, (int64_t)snapshot->get_ring_end() / e.scale - 1);
    const int hw_offset = get_hw_offset();

    float x = start_pixel;
    for(uint64_t sample = 0; sample < e.length; sample++) {
        const uint64_t ring_index = (e.start + sample) % (_view->session().cur_samplelimits() / e.scale);
        if (sample != 0 && ring_index == ring_end)
            break;

        const AnalogSnapshot::EnvelopeSample *const ev =
            e.samples + ((e.start + sample) % e.samples_num);

        const float b = min(max((float)(zeroY + (ev->max - hw_offset) * _scale + 0.5), top), bottom);
        const float t = min(max((float)(zeroY + (ev->min - hw_offset) * _scale + 0.5), top), bottom);

        pre_px = px;
        if(px != floor(x)) {
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
        x += scale_pixels_per_samples;
    }

    p.drawRects(_rects, pcnt);
}

void AnalogSignal::paint_hover_measure(QPainter &p, QColor fore, QColor back)
{
    const int hw_offset = get_hw_offset();
    const int height = get_totalHeight();
    const float top = get_y() - height * 0.5;
    const float bottom = get_y() + height * 0.5;

    // Hover measure
    if (_hover_en && _hover_point != QPointF(-1, -1)) {
        QString hover_str = get_voltage(hw_offset - _hover_value, 2);
        if (_hover_point.y() <= top || _hover_point.y() >= bottom)
            hover_str += "/out";
        const int hover_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
            Qt::AlignLeft | Qt::AlignTop, hover_str).width() + 10;
        const int hover_height = p.boundingRect(0, 0, INT_MAX, INT_MAX,
            Qt::AlignLeft | Qt::AlignTop, hover_str).height();
        QRectF hover_rect(_hover_point.x(), _hover_point.y()-hover_height/2, hover_width, hover_height);
        if (hover_rect.right() > get_view_rect().right())
            hover_rect.moveRight(_hover_point.x());
        if (hover_rect.top() < get_view_rect().top())
            hover_rect.moveTop(_hover_point.y());
        if (hover_rect.bottom() > get_view_rect().bottom())
            hover_rect.moveBottom(_hover_point.y());

        p.setPen(fore);
        p.setBrush(back);
        p.drawRect(_hover_point.x()-1, _hover_point.y()-1, HoverPointSize, HoverPointSize);
        p.drawText(hover_rect, Qt::AlignCenter | Qt::AlignTop | Qt::TextDontClip, hover_str);
    }

    list<Cursor*>::iterator i = _view->get_cursorList().begin();
    while (i != _view->get_cursorList().end()) {
        float pt_value;
        const QPointF pt = get_point((*i)->index(), pt_value);
        if (pt == QPointF(-1, -1)) {
            i++;
            continue;
        }
        QString pt_str = get_voltage(hw_offset - pt_value, 2);
        if (pt.y() <= top || pt.y() >= bottom)
            pt_str += "/out";
        const int pt_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
            Qt::AlignLeft | Qt::AlignTop, pt_str).width() + 10;
        const int pt_height = p.boundingRect(0, 0, INT_MAX, INT_MAX,
            Qt::AlignLeft | Qt::AlignTop, pt_str).height();
        QRectF pt_rect(pt.x(), pt.y()-pt_height/2, pt_width, pt_height);
        if (pt_rect.right() > get_view_rect().right())
            pt_rect.moveRight(pt.x());
        if (pt_rect.top() < get_view_rect().top())
            pt_rect.moveTop(pt.y());
        if (pt_rect.bottom() > get_view_rect().bottom())
            pt_rect.moveBottom(pt.y());

        p.drawRect(pt.x()-1, pt.y()-1, 2, 2);
        p.drawLine(pt.x()-2, pt.y()-2, pt.x()+2, pt.y()+2);
        p.drawLine(pt.x()+2, pt.y()-2, pt.x()-2, pt.y()+2);
        p.drawText(pt_rect, Qt::AlignCenter | Qt::AlignTop | Qt::TextDontClip, pt_str);

        i++;
    }
}

QString AnalogSignal::get_voltage(double v, int p, bool scaled)
{
    const double mapRange = (get_mapMax() - get_mapMin()) * 1000;
    const QString mapUnit = get_mapUnit();

    if (scaled)
        v = v / get_totalHeight() * mapRange;
    else
        v = v * _scale / get_totalHeight() * mapRange;
    return abs(v) >= 1000 ? QString::number(v/1000.0, 'f', p) + mapUnit : QString::number(v, 'f', p) + "m" + mapUnit;
}

} // namespace view
} // namespace pv
