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

#include "dsosignal.h"
#include <QTimer>
#include <functional>
#include <QApplication>
#include <math.h>

#include "view.h"
#include "../dsvdef.h"
#include "../data/dsosnapshot.h"
#include "../sigsession.h" 
#include "../log.h"
#include "../appcontrol.h"
 
using namespace std;

namespace pv {
namespace view {

const QString DsoSignal::vDialUnit[DsoSignal::vDialUnitCount] = {
    "mV",
    "V",
};

const QColor DsoSignal::SignalColours[4] = {
    QColor(238, 178, 17, 255),  // dsYellow
    QColor(0, 153, 37, 255),    // dsGreen
    QColor(213, 15, 37, 255),   // dsRed
    QColor(17, 133, 209, 255)  // dsBlue

};

const float DsoSignal::EnvelopeThreshold = 256.0f;

DsoSignal::DsoSignal(data::DsoSnapshot *data,
                     sr_channel *probe):
    Signal(probe),
    _data(data), 
    _hover_point(QPointF(-1, -1))
{
    QVector<uint64_t> vValue;
    QVector<QString> vUnit;

    for(uint64_t i = 0; i < vDialUnitCount; i++){
        vUnit.append(vDialUnit[i]);
    }
    
    _vDial = NULL;
    _period = 0;
    _pcount = 0;
    _scale = 0;
    _en_lock = false;
    _show = true;
    _vDialActive = false;
    _mValid = false;
    _level_valid = false;
    _autoV = false;
    _autoH = false;
    _autoV_over = false;
    _auto_cnt = 0;
    _hover_en = false;
    _hover_index = 0;
    _hover_value = 0;

    GVariant *gvar_list, *gvar_list_vdivs;

    gvar_list = session->get_device()->get_config_list(NULL, SR_CONF_PROBE_VDIV);
    
    if (gvar_list != NULL)
    {
        assert(gvar_list);
        if ((gvar_list_vdivs = g_variant_lookup_value(gvar_list,
                "vdivs", G_VARIANT_TYPE("at")))) {
            GVariant *gvar;
            GVariantIter iter;
            g_variant_iter_init(&iter, gvar_list_vdivs);

            while(NULL != (gvar = g_variant_iter_next_value(&iter))) {
                vValue.push_back(g_variant_get_uint64(gvar));
                g_variant_unref(gvar);
            }
            
            g_variant_unref(gvar_list_vdivs);
            g_variant_unref(gvar_list);
        }
    }
    _vDial = new dslDial(vValue.count(), vDialValueStep, vValue, vUnit);
    _colour = SignalColours[probe->index % countof(SignalColours)];

    load_settings();
}

DsoSignal::~DsoSignal()
{
    DESTROY_OBJECT(_vDial);  
}

void DsoSignal::set_scale(int height)
{
    _scale = height / (_ref_max - _ref_min) * _stop_scale;
}

void DsoSignal::set_enable(bool enable)
{  
    if (session->get_device()->is_hardware_logic() 
        && get_index() == 0){
        return;
    }

    _en_lock = true;
    bool cur_enable;
    bool ret;
    ret = session->get_device()->get_config_bool(SR_CONF_PROBE_EN, cur_enable, _probe, NULL);

    if (!ret) { 
        dsv_err("ERROR: config_get SR_CONF_PROBE_EN failed.");
        _en_lock = false;
        return;
    }
    if (cur_enable == enable) {
        _en_lock = false;
        return;
    }

    bool running =  false;

    if (session->is_running_status()) {
        running = true;
        session->stop_capture();
    }

    while(session->is_running_status())
        QCoreApplication::processEvents();

    set_vDialActive(false);
    session->get_device()->set_config_bool( SR_CONF_PROBE_EN,
                          enable, _probe, NULL);

    _view->update_hori_res();
    
    if (running) {
       session->stop_capture();
       session->start_capture(false);
    }

    _view->set_update(_viewport, true);
    _view->update();
    _en_lock = false;
}

void DsoSignal::set_vDialActive(bool active)
{
    if (enabled())
        _vDialActive = active;
}

bool DsoSignal::go_vDialPre(bool manul)
{  
    if (_autoV && manul)
        autoV_end(); 

    if (enabled() && !_vDial->isMin()) 
    {
        if (session->is_running_status())
            session->refresh(RefreshShort);

        const double pre_vdiv = _vDial->get_value();
        _vDial->set_sel(_vDial->get_sel() - 1);

        session->get_device()->set_config_uint64(SR_CONF_PROBE_VDIV,
                              _vDial->get_value(), _probe, NULL);

        if (session->is_stopped_status()) {
            set_stop_scale(_stop_scale * (pre_vdiv/_vDial->get_value()));
            set_scale(get_view_rect().height());
        }
        session->get_device()->set_config_uint16(SR_CONF_PROBE_OFFSET,
                              _zero_offset, _probe, NULL);

        _view->vDial_updated();
        _view->set_update(_viewport, true);
        _view->update();
        return true;
    }
    else {
        if (_autoV && !_autoV_over)
            autoV_end();
        return false;
    }
}

bool DsoSignal::go_vDialNext(bool manul)
{
    if (_autoV && manul)
        autoV_end(); 

    if (enabled() && !_vDial->isMax())
    {
        if (session->is_running_status())
            session->refresh(RefreshShort);

        const double pre_vdiv = _vDial->get_value();
        _vDial->set_sel(_vDial->get_sel() + 1);

        session->get_device()->set_config_uint64(SR_CONF_PROBE_VDIV,
                              _vDial->get_value(), _probe, NULL);

        if (session->is_stopped_status()) {
            set_stop_scale(_stop_scale * (pre_vdiv/_vDial->get_value()));
            set_scale(get_view_rect().height());
        }
        session->get_device()->set_config_uint16(SR_CONF_PROBE_OFFSET,
                              _zero_offset, _probe, NULL);

        _view->vDial_updated();
        _view->set_update(_viewport, true);
        _view->update();
        return true;
    } 
    else {
        if (_autoV && !_autoV_over)
            autoV_end();
        return false;
    }
}

bool DsoSignal::load_settings()
{
    int v;
    uint32_t ui32;
    bool ret;

    // dso channel bits
    ret = session->get_device()->get_config_byte(SR_CONF_UNIT_BITS, v);
    if (ret) {
        _bits = (uint8_t)v;
    } 
    else {
        _bits = DefaultBits; 
        dsv_warn("%s%d", "Warning: config_get SR_CONF_UNIT_BITS failed, set to %d(default).", DefaultBits);

        if (session->get_device()->is_hardware())
            return false;
    }

    ret = session->get_device()->get_config_uint32(SR_CONF_REF_MIN, ui32);
    if (ret) 
        _ref_min = (double)ui32;
    else
        _ref_min = 1;
    
    ret = session->get_device()->get_config_uint32(SR_CONF_REF_MAX, ui32);
    if (ret)
        _ref_max = (double)ui32;
    else
        _ref_max = ((1 << _bits) - 1);

    // -- vdiv
    uint64_t vdiv;
    uint64_t vfactor;
    ret = session->get_device()->get_config_uint64(SR_CONF_PROBE_VDIV, vdiv, _probe, NULL);
    if (!ret) {
        dsv_err("ERROR: config_get SR_CONF_PROBE_VDIV failed.");
        return false;
    }

    ret = session->get_device()->get_config_uint64(SR_CONF_PROBE_FACTOR, vfactor, _probe, NULL);
    if (!ret) {
        dsv_err("ERROR: config_get SR_CONF_PROBE_FACTOR failed.");
        return false;
    }

    _vDial->set_value(vdiv);
    _vDial->set_factor(vfactor);

    // -- coupling
    ret = session->get_device()->get_config_byte(SR_CONF_PROBE_COUPLING, v, _probe, NULL);
    if (ret) {
        _acCoupling = uint8_t(v);
    }
    else { 
        dsv_err("ERROR: config_get SR_CONF_PROBE_COUPLING failed.");
        return false;
    }
 
    // -- vpos
    ret = session->get_device()->get_config_uint16(SR_CONF_PROBE_OFFSET, _zero_offset, _probe, NULL);
    if (!ret) {
        dsv_err("ERROR: config_get SR_CONF_PROBE_OFFSET failed.");
        return false;
    }

    // -- trig_value
    ret = session->get_device()->get_config_byte(SR_CONF_TRIGGER_VALUE, _trig_value, _probe, NULL);
    if (ret) { 
        _trig_delta = get_trig_vrate() - get_zero_ratio();
    }
    else {
        dsv_err("ERROR: config_get SR_CONF_TRIGGER_VALUE failed.");

        if (session->get_device()->is_hardware())
            return false;
    }

    if (_view) {
        _view->set_update(_viewport, true);
        _view->update();
    }
    return true;
}

int DsoSignal::commit_settings()
{
    int ret;

    // -- enable
    ret = session->get_device()->set_config_bool(SR_CONF_PROBE_EN,
                                enabled(), _probe, NULL);

    // -- vdiv
    ret = session->get_device()->set_config_uint64(SR_CONF_PROBE_VDIV,
                                _vDial->get_value(), _probe, NULL);
    ret = session->get_device()->set_config_uint64(SR_CONF_PROBE_FACTOR,
                                _vDial->get_factor(), _probe, NULL);

    // -- coupling
    ret = session->get_device()->set_config_byte(SR_CONF_PROBE_COUPLING,
                                _acCoupling, _probe, NULL);

    // -- offset
    ret = session->get_device()->set_config_uint16(SR_CONF_PROBE_OFFSET,
                                _zero_offset, _probe, NULL);

    // -- trig_value
    session->get_device()->set_config_byte(SR_CONF_TRIGGER_VALUE,
                          _trig_value, _probe, NULL);

    return ret;
}

uint64_t DsoSignal::get_vDialValue()
{
    return _vDial->get_value();
}

uint16_t DsoSignal::get_vDialSel()
{
    return _vDial->get_sel();
}

void DsoSignal::set_acCoupling(uint8_t coupling)
{
    if (enabled()) {
        _acCoupling = coupling; 
        session->get_device()->set_config_byte(SR_CONF_PROBE_COUPLING,
                              _acCoupling, _probe, NULL);
    }
}

int DsoSignal::ratio2value(double ratio)
{
    return ratio * (_ref_max - _ref_min) + _ref_min;
}

int DsoSignal::ratio2pos(double ratio)
{
    return ratio * get_view_rect().height() + get_view_rect().top();
}

double DsoSignal::value2ratio(int value)
{
    return max(0.0, (value - _ref_min) / (_ref_max - _ref_min));
}

double DsoSignal::pos2ratio(int pos)
{
    return min(max(pos - get_view_rect().top(), 0), get_view_rect().height()) * 1.0 / get_view_rect().height();
}

double DsoSignal::get_trig_vrate()
{ 
    if (session->get_device()->is_hardware_logic())
        return value2ratio(_trig_value - ratio2value(0.5)) + get_zero_ratio();
    else
        return value2ratio(_trig_value);
}

void DsoSignal::set_trig_vpos(int pos, bool delta_change)
{
    assert(_view);
    if (enabled()) {
        set_trig_ratio(pos2ratio(pos), delta_change);
    }
}

void DsoSignal::set_trig_ratio(double ratio, bool delta_change)
{
    double delta = ratio; 

    if (session->get_device()->is_hardware_logic()) {
        delta = delta - get_zero_ratio();
        delta = min(delta, 0.5);
        delta = max(delta, -0.5);
        _trig_value = ratio2value(delta + 0.5);
    }
    else {
        if (delta < 0.06f)
            delta = 0.06f;
        if (delta > 0.945f)
            delta = 0.945f;

        _trig_value = ratio2value(delta);
    }
 
    if (delta_change)
        _trig_delta = get_trig_vrate() - get_zero_ratio();
    session->get_device()->set_config_byte(SR_CONF_TRIGGER_VALUE,
                          _trig_value, _probe, NULL);
}

int DsoSignal::get_zero_vpos()
{
    return ratio2pos(get_zero_ratio());
}

double DsoSignal::get_zero_ratio()
{
    return value2ratio(_zero_offset);
}

int DsoSignal::get_hw_offset()
{
    int hw_offset = 0;
    session->get_device()->get_config_uint16(SR_CONF_PROBE_HW_OFFSET, hw_offset, _probe, NULL);
    return hw_offset;
}

void DsoSignal::set_zero_vpos(int pos)
{
    if (enabled()) {
        set_zero_ratio(pos2ratio(pos));
        set_trig_ratio(_trig_delta + get_zero_ratio(), false);
    }
}

void DsoSignal::set_zero_ratio(double ratio)
{
    _zero_offset = ratio2value(ratio); 
    session->get_device()->set_config_uint16(SR_CONF_PROBE_OFFSET,
                          _zero_offset, _probe, NULL);
}

void DsoSignal::set_factor(uint64_t factor)
{
    if (enabled()) {
        uint64_t prefactor = 0; 
        bool ret;

        ret = session->get_device()->get_config_uint64(SR_CONF_PROBE_FACTOR, prefactor, _probe, NULL);
        if (!ret) { 
            dsv_err("ERROR: config_get SR_CONF_PROBE_FACTOR failed.");
            return;
        }

        if (prefactor != factor) {
            session->get_device()->set_config_uint64(SR_CONF_PROBE_FACTOR,
                                  factor, _probe, NULL);
            _vDial->set_factor(factor);
            _view->set_update(_viewport, true);
            _view->update();
        }
    }
}

uint64_t DsoSignal::get_factor()
{ 
    uint64_t factor; 

    bool ret = session->get_device()->get_config_uint64(SR_CONF_PROBE_FACTOR, factor, _probe, NULL);
    if (ret) {
        return factor;
    } 
    else { 
        dsv_err("ERROR: config_get SR_CONF_PROBE_FACTOR failed.");
        return 1;
    }
}

QString DsoSignal::get_measure(enum DSO_MEASURE_TYPE type)
{
    const QString mNone = "--";
    QString mString;

    if (_data->empty()){
        return mNone;
    }

    if (_mValid) {
        const int hw_offset = get_hw_offset();

        switch(type) {
        case DSO_MS_AMPT:
            if (_level_valid)
                mString = get_voltage(_high - _low, 2);
            else
                mString = mNone;
            break;
        case DSO_MS_VHIG:
            if (_level_valid)
                mString = get_voltage(hw_offset - _low, 2);
            else
                mString = mNone;
            break;
        case DSO_MS_VLOW:
            if (_level_valid)
                mString = get_voltage(hw_offset - _high, 2);
            else
                mString = mNone;
            break;
        case DSO_MS_VP2P:
            mString = get_voltage(_max - _min, 2);
            break;
        case DSO_MS_VMAX:
            mString = get_voltage(hw_offset - _min, 2);
            break;
        case DSO_MS_VMIN:
            mString = get_voltage(hw_offset - _max, 2);
            break;
        case DSO_MS_PERD:
            mString = get_time(_period);
            break;
        case DSO_MS_FREQ: 
            if (_period == 0)
                mString = mNone;
            else if (abs(_period) > 1000000)
                mString = QString::number(1000000000/_period, 'f', 2) + "Hz";
            else if (abs(_period) > 1000)
                mString = QString::number(1000000/_period, 'f', 2) + "kHz";
            else
                mString = QString::number(1000/_period, 'f', 2) + "MHz";
            break;
        case DSO_MS_VRMS:
            mString = get_voltage(_rms, 2);
            break;
        case DSO_MS_VMEA:
            mString = get_voltage(_mean, 2);
            break;
        case DSO_MS_NOVR:
            if (_level_valid && (_high - _low != 0) )
                mString = QString::number((_max - _high) * 100.0 / (_high - _low), 'f', 2) + "%";
            else
                mString = mNone;
            break;
        case DSO_MS_POVR:
            if (_level_valid && (_high - _low != 0) )
                mString = QString::number((_low - _min) * 100.0 / (_high - _low), 'f', 2) + "%";
            else
                mString = mNone;
            break;
        case DSO_MS_PDUT:
            if (_level_valid && _period != 0)
                mString = QString::number(_high_time / _period * 100, 'f', 2)+"%";
            else
                mString = mNone;
            break;
        case DSO_MS_NDUT:
            if (_level_valid && _period != 0)
                mString = QString::number(100 - _high_time / _period * 100, 'f', 2)+"%";
            else
                mString = mNone;
            break;
        case DSO_MS_PWDT:
            if (_level_valid)
                mString = get_time(_high_time);
            else
                mString = mNone;
            break;
        case DSO_MS_NWDT:
            if (_level_valid)
                mString = get_time(_period - _high_time);
            else
                mString = mNone;
            break;
        case DSO_MS_RISE:
            if (_level_valid)
                mString = get_time(_rise_time);
            else
                mString = mNone;
            break;
        case DSO_MS_FALL:
            if (_level_valid)
                mString = get_time(_fall_time);
            else
                mString = mNone;
            break;
        case DSO_MS_BRST:
            if (_level_valid)
                mString = get_time(_burst_time);
            else
                mString = mNone;
            break;
        case DSO_MS_PCNT:
            if (_level_valid)
                mString = (_pcount > 1000000 ? QString::number(_pcount/1000000, 'f', 6) + "M" :
                           _pcount > 1000 ? QString::number(_pcount/1000, 'f', 3) + "K" : QString::number(_pcount, 'f', 0));
            else
                mString = mNone;
            break;
        default:
            mString = "Error";
            break;
        }
    } else {
        mString = mNone;
    }
    return mString;
}

QRect DsoSignal::get_view_rect()
{
    assert(_viewport);
    return QRect(0, UpMargin,
                  _viewport->width() - RightMargin,
                  _viewport->height() - UpMargin - DownMargin);
}

void DsoSignal::paint_prepare()
{
    assert(_view);

    if (_data->empty() || !_data->has_data(get_index()))
        return; 

    if (session->trigd()) {
        if (get_index() == session->trigd_ch()) {
            uint8_t slope = DSO_TRIGGER_RISING;
            int v;
            bool ret;

            ret = session->get_device()->get_config_byte(SR_CONF_TRIGGER_SLOPE, v);
            if (ret) {
                slope = (uint8_t)v;
            }

            int64_t trig_index = _view->get_trig_cursor()->index();
            if (trig_index >= (int64_t)_data->get_sample_count())
                return;

            const uint8_t *const trig_samples = _data->get_samples(0, 0, get_index());
            for (uint16_t i = 0; i < TrigHRng; i++) {
                const int64_t i0 = trig_index - i - 1;
                const int64_t i1 = trig_index - i;
                if (i1 < 0)
                    break;
                const uint8_t t0 = trig_samples[i0];
                const uint8_t t1 = trig_samples[i1];
                if((slope == DSO_TRIGGER_RISING && t0 >= _trig_value && t1 <= _trig_value) ||
                   (slope == DSO_TRIGGER_FALLING && t0 <= _trig_value && t1 >= _trig_value)) {
                    const double xoff = (t1 == t0) ? 0 : (_trig_value - t0) * 1.0 / (t1 - t0);
                    _view->set_trig_hoff(i + 1 - xoff);
                    break;
                }
            }
        }
    } else {
        _view->set_trig_hoff(0);
    }
}

void DsoSignal::paint_back(QPainter &p, int left, int right, QColor fore, QColor back)
{
    assert(_view);

    if (!_show)
        return;

    int i, j;
    const int height = get_view_rect().height();
    const int width = right - left; 

    fore.setAlpha(View::BackAlpha);

    QPen solidPen(fore);
    solidPen.setStyle(Qt::SolidLine);
    p.setPen(solidPen);
    p.setBrush(back.black() > 0x80 ? back.darker() : back.lighter());
    p.drawRect(left, UpMargin, width, height);

    // draw zoom region
    fore.setAlpha(View::ForeAlpha);
    p.setPen(fore);

    const uint64_t sample_len = session->cur_samplelimits();
    const double samplerate = session->cur_snap_samplerate();
    const double samples_per_pixel = samplerate * _view->scale();
    const double shown_rate = min(samples_per_pixel * width * 1.0 / sample_len, 1.0);
    const double start = _view->offset() * samples_per_pixel;
    const double shown_offset = min(start / sample_len, 1.0) * width;
    const double shown_len = max(shown_rate * width, 6.0);
    const QPointF left_edge[] =  {QPoint(shown_offset + 3, UpMargin/2 - 6),
                                  QPoint(shown_offset, UpMargin/2 - 6),
                                  QPoint(shown_offset, UpMargin/2 + 6),
                                  QPoint(shown_offset + 3, UpMargin/2 + 6)};
    const QPointF right_edge[] = {QPoint(shown_offset + shown_len - 3, UpMargin/2 - 6),
                                  QPoint(shown_offset + shown_len , UpMargin/2 - 6),
                                  QPoint(shown_offset + shown_len , UpMargin/2 + 6),
                                  QPoint(shown_offset + shown_len - 3, UpMargin/2 + 6)};
    p.drawLine(left, UpMargin/2, shown_offset, UpMargin/2);
    p.drawLine(shown_offset + shown_len, UpMargin/2, left + width, UpMargin/2);
    p.drawPolyline(left_edge, countof(left_edge));
    p.drawPolyline(right_edge, countof(right_edge));
    p.setBrush(fore);
    p.drawRect(shown_offset, UpMargin/2 - 3, shown_len, 6);

    // draw divider
    fore.setAlpha(View::BackAlpha);
    QPen dashPen(fore);
    dashPen.setStyle(Qt::DashLine);
    p.setPen(dashPen);
    const double spanY =height * 1.0 / DS_CONF_DSO_VDIVS;
    for (i = 1; i <= DS_CONF_DSO_VDIVS; i++) {
        const double posY = spanY * i + UpMargin;
        if (i != DS_CONF_DSO_VDIVS)
            p.drawLine(left, posY, right, posY);
        const double miniSpanY = spanY / 5;
        for (j = 1; j < 5; j++) {
            p.drawLine(width / 2.0f - 5, posY - miniSpanY * j,
                       width / 2.0f + 5, posY - miniSpanY * j);
        }
    }
    const double spanX = width * 1.0 / DS_CONF_DSO_HDIVS;
    for (i = 1; i <= DS_CONF_DSO_HDIVS; i++) {
        const double posX = spanX * i;
        if (i != DS_CONF_DSO_HDIVS)
            p.drawLine(posX, UpMargin,posX, height + UpMargin);
        const double miniSpanX = spanX / 5;
        for (j = 1; j < 5; j++) {
            p.drawLine(posX - miniSpanX * j, height / 2.0f + UpMargin - 5,
                       posX - miniSpanX * j, height / 2.0f + UpMargin + 5);
        }
    }
    _view->set_back(true);
}

void DsoSignal::paint_mid(QPainter &p, int left, int right, QColor fore, QColor back)
{
    (void)fore;
    (void)back;

    if (!_show || right <= left){
        return;
    }

    assert(_data);
    assert(_view); 

    if (enabled()) {
        const int index = get_index();
        const int width = right - left;
        const float zeroY = get_zero_vpos();

        const double scale = _view->scale();
        assert(scale > 0);
        const int64_t offset = _view->offset();

        if (_data->empty() || !_data->has_data(index))
            return;

        const uint16_t enabled_channels = _data->get_channel_num();
        const double pixels_offset = offset;
        const double samplerate = _data->samplerate();

        assert(samplerate > 0);
       
        const int64_t last_sample = max((int64_t)(_data->get_sample_count() - 1), (int64_t)0);
        const double samples_per_pixel = samplerate * scale;
        const double start = offset * samples_per_pixel - _view->trig_hoff();
        const double end = start + samples_per_pixel * width;

        const int64_t start_sample = min(max((int64_t)floor(start),
            (int64_t)0), last_sample);
        const int64_t end_sample = min(max((int64_t)ceil(end) + 1,
            (int64_t)0), last_sample);
        const int hw_offset = get_hw_offset();

        if (samples_per_pixel < EnvelopeThreshold) {
            _data->enable_envelope(false);
            paint_trace(p, _data, zeroY, left,
                start_sample, end_sample, hw_offset,
                pixels_offset, samples_per_pixel, enabled_channels);
        } else {
            _data->enable_envelope(true);
            paint_envelope(p, _data, zeroY, left,
                start_sample, end_sample, hw_offset,
                pixels_offset, samples_per_pixel, enabled_channels);
        }

        sr_status status;
        
        if (session->dso_status_is_valid()) {
            _mValid = true;
            status = session->get_dso_status();

            if (status.measure_valid) {
                _min = (index == 0) ? status.ch0_min : status.ch1_min;
                _max = (index == 0) ? status.ch0_max : status.ch1_max;

                _level_valid = (index == 0) ? status.ch0_level_valid : status.ch1_level_valid;
                _low = (index == 0) ? status.ch0_low_level : status.ch1_low_level;
                _high = (index == 0) ? status.ch0_high_level : status.ch1_high_level;

                const uint32_t count  = (index == 0) ? status.ch0_cyc_cnt : status.ch1_cyc_cnt;
                const bool plevel = (index == 0) ? status.ch0_plevel : status.ch1_plevel;
                const bool startXORend = (index == 0) ? (status.ch0_cyc_llen == 0) : (status.ch1_cyc_llen == 0);
                uint16_t total_channels = g_slist_length(session->get_device()->get_channels());

                if (total_channels == 1 && _data->is_file()){
                    total_channels++;
                }
                
                const double tfactor = (total_channels / enabled_channels) * SR_GHZ(1) * 1.0 / samplerate;
 
                double samples = (index == 0) ? status.ch0_cyc_tlen : status.ch1_cyc_tlen;
                _period = ((count == 0) ? 0 : samples / count) * tfactor;

                samples = (index == 0) ? status.ch0_cyc_flen : status.ch1_cyc_flen;
                _rise_time = ((count == 0) ? 0 : samples / ((plevel && startXORend) ? count : count + 1)) * tfactor;
                samples = (index == 0) ? status.ch0_cyc_rlen : status.ch1_cyc_rlen;
                _fall_time = ((count == 0) ? 0 : samples / ((!plevel && startXORend) ? count : count + 1)) * tfactor;

                samples = (index == 0) ? (status.ch0_plevel ? status.ch0_cyc_plen - status.ch0_cyc_llen :
                                                              status.ch0_cyc_tlen - status.ch0_cyc_plen + status.ch0_cyc_llen) :
                                         (status.ch1_plevel ? status.ch1_cyc_plen - status.ch1_cyc_llen :
                                                              status.ch1_cyc_tlen - status.ch1_cyc_plen + status.ch1_cyc_llen);
                _high_time = ((count == 0) ? 0 : samples / count) * tfactor;

                samples = (index == 0) ? status.ch0_cyc_tlen + status.ch0_cyc_llen : status.ch1_cyc_flen + status.ch1_cyc_llen;
                _burst_time = samples * tfactor;

                _pcount = count + (plevel & !startXORend);
                _rms = (index == 0) ? status.ch0_acc_square : status.ch1_acc_square;
                _rms = sqrt(_rms / _data->get_sample_count());
                _mean = (index == 0) ? status.ch0_acc_mean : status.ch1_acc_mean;
                _mean = hw_offset - _mean / _data->get_sample_count();
            }
        }
    }
}

void DsoSignal::paint_fore(QPainter &p, int left, int right, QColor fore, QColor back)
{
    if (!_show)
        return;

    assert(_view); 

    fore.setAlpha(View::BackAlpha);
    QPen pen(fore);
    pen.setStyle(Qt::DotLine);
    p.setPen(pen);
    p.drawLine(left, get_zero_vpos(), right, get_zero_vpos());

    fore.setAlpha(View::ForeAlpha);
    if(enabled()) {
        const QPointF mouse_point = _view->hover_point();
        const QRectF label_rect = get_trig_rect(left, right);
        const bool hover = label_rect.contains(mouse_point);

        // Paint the trig line
        const QPointF points[] = {
            QPointF(right, ratio2pos(get_trig_vrate())),
            label_rect.topLeft(),
            label_rect.topRight(),
            label_rect.bottomRight(),
            label_rect.bottomLeft()
        };

        p.setPen(Qt::transparent);
        p.setBrush(_colour);
        p.drawPolygon(points, countof(points));

        p.setPen(fore);
        const QPointF arrow_points[] = {
            QPoint(label_rect.left(), label_rect.center().y()),
            QPoint(label_rect.left(), label_rect.center().y()-1),
            QPoint(label_rect.left(), label_rect.center().y()+1),
            QPoint(label_rect.left(), label_rect.center().y()-2),
            QPoint(label_rect.left(), label_rect.center().y()+2),
            QPoint(label_rect.left(), label_rect.center().y()-3),
            QPoint(label_rect.left(), label_rect.center().y()+3),
            QPoint(label_rect.left(), label_rect.center().y()-4),
            QPoint(label_rect.left(), label_rect.center().y()+4),
            QPoint(label_rect.left()-1, label_rect.center().y()-3),
            QPoint(label_rect.left()-1, label_rect.center().y()+3),
            QPoint(label_rect.left()+1, label_rect.center().y()-3),
            QPoint(label_rect.left()+1, label_rect.center().y()+3),
            QPoint(label_rect.left()-1, label_rect.center().y()-2),
            QPoint(label_rect.left()-1, label_rect.center().y()+2),
            QPoint(label_rect.left()+1, label_rect.center().y()-2),
            QPoint(label_rect.left()+1, label_rect.center().y()+2),
            QPoint(label_rect.left()-2, label_rect.center().y()-2),
            QPoint(label_rect.left()-2, label_rect.center().y()+2),
            QPoint(label_rect.left()+2, label_rect.center().y()-2),
            QPoint(label_rect.left()+2, label_rect.center().y()+2),
        };
        if (hover || selected())
            p.drawPoints(arrow_points, countof(arrow_points));

        // paint the trig voltage
        int trigp = ratio2pos(get_trig_vrate());
        QString t_vol_s = get_voltage(get_zero_vpos() - trigp, 2, true);
        int vol_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
                                       Qt::AlignLeft | Qt::AlignTop, t_vol_s).width();
        const QRectF t_vol_rect = QRectF(right-vol_width, trigp-10, vol_width, 20);
        p.setPen(fore);
        p.drawText(t_vol_rect, Qt::AlignRight | Qt::AlignVCenter, t_vol_s);

        // paint the _trig_vpos line
        if (_view->get_dso_trig_moved()) {
            p.setPen(QPen(_colour, 1, Qt::DotLine));
            p.drawLine(left, trigp, right - p.boundingRect(t_vol_rect, Qt::AlignLeft, t_vol_s).width(), trigp);
        }

        // Paint the text
        p.setPen(fore);
        p.drawText(label_rect, Qt::AlignCenter | Qt::AlignVCenter, "T");

        // Paint measure
        if (session->is_stopped_status())
            paint_hover_measure(p, fore, back);

        // autoset
        auto_set();
    }
}

QRectF DsoSignal::get_trig_rect(int left, int right)
{
    (void)left;

    return QRectF(right + SquareWidth / 2,
                  ratio2pos(get_trig_vrate()) - SquareWidth / 2,
                  SquareWidth, SquareWidth);
}

void DsoSignal::paint_trace(QPainter &p,
    const pv::data::DsoSnapshot *snapshot,
    int zeroY, int left, const int64_t start, const int64_t end, int hw_offset,
    const double pixels_offset, const double samples_per_pixel, uint64_t num_channels)
{
    (void)num_channels;

    const int64_t sample_count = end - start + 1;

    if (sample_count > 0) {
        pv::data::DsoSnapshot *pshot = const_cast<pv::data::DsoSnapshot*>(snapshot);
        const uint8_t *const samples_buffer = pshot->get_samples(start, end, get_index());;
        assert(samples_buffer);

        QColor trace_colour = _colour;
        trace_colour.setAlpha(View::ForeAlpha);
        p.setPen(trace_colour);

        QPointF *points = new QPointF[sample_count];
        QPointF *point = points;

        float top = get_view_rect().top();
        float bottom = get_view_rect().bottom();
        float right =  (float)get_view_rect().right();
        double  pixels_per_sample = 1.0/samples_per_pixel;

        uint8_t value; 
        float x = (start / samples_per_pixel - pixels_offset) + left + _view->trig_hoff()*pixels_per_sample;
        float y;
 
        for (int64_t sample = 0; sample < sample_count; sample++) {
            value = samples_buffer[sample];
            y = min(max(top, zeroY + (value - hw_offset) * _scale), bottom);
            if (x > right) {
                point--;
                const float lastY = point->y() + (y - point->y()) / (x - point->x()) * (right - point->x());
                point++;
                *point++ = QPointF(right, lastY);
                break;
            }
            *point++ = QPointF(x, y);
            x += pixels_per_sample;
        }

        p.drawPolyline(points, point - points);

        delete[] points;
    }
}

void DsoSignal::paint_envelope(QPainter &p,
    const pv::data::DsoSnapshot *snapshot,
    int zeroY, int left, const int64_t start, const int64_t end, int hw_offset,
    const double pixels_offset, const double samples_per_pixel, uint64_t num_channels)
{
	using namespace Qt;
    using pv::data::DsoSnapshot;

    data::DsoSnapshot *pshot = const_cast<data::DsoSnapshot*>(snapshot);

    DsoSnapshot::EnvelopeSection e;
    const uint16_t index = get_index() % num_channels;
    pshot->get_envelope_section(e, start, end, samples_per_pixel, index);

	if (e.length < 2)
		return;

    p.setPen(QPen(NoPen));
    //p.setPen(QPen(_colour, 2, Qt::SolidLine));
    QColor envelope_colour = _colour;
    envelope_colour.setAlpha(View::ForeAlpha);
    p.setBrush(envelope_colour);

	QRectF *const rects = new QRectF[e.length];
	QRectF *rect = rects;
    float top = get_view_rect().top();
    float bottom = get_view_rect().bottom();
    for(uint64_t sample = 0; sample < e.length-1; sample++) {
		const float x = ((e.scale * sample + e.start) /
            samples_per_pixel - pixels_offset) + left + _view->trig_hoff()/samples_per_pixel;
        const DsoSnapshot::EnvelopeSample *const s =
			e.samples + sample;

		// We overlap this sample with the next so that vertical
		// gaps do not appear during steep rising or falling edges
        const float b = min(max(top, ((max(s->max, (s+1)->min) - hw_offset) * _scale + zeroY)), bottom);
        const float t = min(max(top, ((min(s->min, (s+1)->max) - hw_offset) * _scale + zeroY)), bottom);

		float h = b - t;
		if(h >= 0.0f && h <= 1.0f)
			h = 1.0f;
		if(h <= 0.0f && h >= -1.0f)
			h = -1.0f;

		*rect++ = QRectF(x, t, 1.0f, h);
	}

	p.drawRects(rects, e.length);

	delete[] rects;
    //delete[] e.samples;
}

void DsoSignal::paint_type_options(QPainter &p, int right, const QPoint pt, QColor fore)
{ 
    p.setRenderHint(QPainter::Antialiasing, true);

    QColor foreBack = fore;
    foreBack.setAlpha(View::BackAlpha);
    int y = get_y();
    const QRectF vDial_rect = get_rect(DSO_VDIAL, y, right);
    const QRectF x1_rect = get_rect(DSO_X1, y, right);
    const QRectF x10_rect = get_rect(DSO_X10, y, right);
    const QRectF x100_rect = get_rect(DSO_X100, y, right);
    const QRectF acdc_rect = get_rect(DSO_ACDC, y, right);
    const QRectF chEn_rect = get_rect(DSO_CHEN, y, right);
    const QRectF auto_rect = get_rect(DSO_AUTO, y, right);

    QString pText;
    _vDial->paint(p, vDial_rect, _colour, pt, pText);
    QFontMetrics fm(p.font());
    const QRectF valueRect = QRectF(chEn_rect.left(), vDial_rect.top()-fm.height()-10, right, fm.height());
    p.drawText(valueRect, Qt::AlignCenter, pText);

    const char *strings[6] = {
        QT_TR_NOOP("EN"),
        QT_TR_NOOP("DIS"),
        QT_TR_NOOP("GND"),
        QT_TR_NOOP("DC"),
        QT_TR_NOOP("AC"),
        QT_TR_NOOP("AUTO"),
    };
    p.setPen(Qt::transparent);
    p.setBrush(chEn_rect.contains(pt) ? _colour.darker() : _colour);
    p.drawRect(chEn_rect);
    p.setPen(Qt::white);
    p.drawText(chEn_rect, Qt::AlignCenter | Qt::AlignVCenter, enabled() ? tr(strings[0]) : tr(strings[1]));

    p.setPen(Qt::transparent);
    p.setBrush(enabled() ? (acdc_rect.contains(pt) ? _colour.darker() : _colour) : foreBack);
    p.drawRect(acdc_rect);
    p.setPen(Qt::white);
    p.drawText(acdc_rect, Qt::AlignCenter | Qt::AlignVCenter, (_acCoupling == SR_GND_COUPLING) ? tr(strings[2]):
                                                              (_acCoupling == SR_DC_COUPLING) ? tr(strings[3]) : tr(strings[4]));

    if (session->get_device()->is_hardware()) {
        p.setPen(Qt::transparent);
        p.setBrush(enabled() ? (auto_rect.contains(pt) ? _colour.darker() : _colour) : foreBack);
        p.drawRect(auto_rect);
        p.setPen(Qt::white);
        p.drawText(auto_rect, Qt::AlignCenter | Qt::AlignVCenter, tr(strings[5]));
    }

    // paint the probe factor selector
    uint64_t factor;
    bool ret;

    ret = session->get_device()->get_config_uint64(SR_CONF_PROBE_FACTOR, factor, _probe, NULL);
    if (!ret) {
        dsv_err("ERROR: config_get SR_CONF_PROBE_FACTOR failed.");
        return;
    }

    p.setPen(Qt::transparent);
    p.setBrush((enabled() && (factor == 100)) ? (x100_rect.contains(pt) ? _colour.darker() : _colour)  : (x100_rect.contains(pt) ? _colour.darker() : foreBack));
    p.drawRect(x100_rect);
    p.setBrush((enabled() && (factor == 10)) ? (x10_rect.contains(pt) ? _colour.darker() : _colour)  : (x10_rect.contains(pt) ? _colour.darker() : foreBack));
    p.drawRect(x10_rect);
    p.setBrush((enabled() && (factor == 1)) ? (x1_rect.contains(pt) ? _colour.darker() : _colour)  : (x1_rect.contains(pt) ? _colour.darker() : foreBack));
    p.drawRect(x1_rect);

    p.setPen(Qt::white);
    p.drawText(x100_rect, Qt::AlignCenter | Qt::AlignVCenter, "x100");
    p.drawText(x10_rect, Qt::AlignCenter | Qt::AlignVCenter, "x10");
    p.drawText(x1_rect, Qt::AlignCenter | Qt::AlignVCenter, "x1");

    p.setRenderHint(QPainter::Antialiasing, false);
}

bool DsoSignal::mouse_press(int right, const QPoint pt)
{
    int y = get_y();
    const QRectF vDial_rect = get_rect(DSO_VDIAL, y, right);
    const QRectF chEn_rect = get_rect(DSO_CHEN, y, right);
    const QRectF acdc_rect = get_rect(DSO_ACDC, y, right);
    const QRectF auto_rect = get_rect(DSO_AUTO, y, right);
    const QRectF x1_rect = get_rect(DSO_X1, y, right);
    const QRectF x10_rect = get_rect(DSO_X10, y, right);
    const QRectF x100_rect = get_rect(DSO_X100, y, right);

    if (chEn_rect.contains(pt)) {
       if (session->get_device()->is_file() == false && !_en_lock) {
           set_enable(!enabled());
       }
       return true;
    }
    else if (enabled()) {
        if (vDial_rect.contains(pt) && pt.x() > vDial_rect.center().x()) {
            if (pt.y() > vDial_rect.center().y())
                go_vDialNext(true);
            else
                go_vDialPre(true);
        } 
        else if (session->get_device()->is_file() == false && acdc_rect.contains(pt)) {
           if (session->get_device()->is_hardware_logic())
               set_acCoupling((get_acCoupling()+1)%2);
           else
               set_acCoupling((get_acCoupling()+1)%2);
        }
        else if (auto_rect.contains(pt)) {
            if (session->get_device()->is_hardware())
                auto_start();
        }
        else if (x1_rect.contains(pt)) {
           set_factor(1);
        }
        else if (x10_rect.contains(pt)) {
           set_factor(10);
        }
        else if (x100_rect.contains(pt)) {
           set_factor(100);
        }
        else {
            return false;
        }

        return true;
    }
    return false;
}

bool DsoSignal::mouse_wheel(int right, const QPoint pt, const int shift)
{
    int y = get_y();
    const QRectF vDial_rect = get_rect(DSO_VDIAL, y, right);

    if (vDial_rect.contains(pt)) {
        if (shift > 0.5)
            go_vDialPre(true);
        else if (shift < -0.5)
            go_vDialNext(true);
        return true;
    } else {
        return false;
    }

    return true;
}

QRectF DsoSignal::get_rect(DsoSetRegions type, int y, int right)
{
    (void)right;

    if (type == DSO_VDIAL)
        return QRectF(
            get_leftWidth() + SquareWidth*0.5 + Margin,
            y - SquareWidth * SquareNum + SquareWidth * 3,
            SquareWidth * (SquareNum-1), SquareWidth * (SquareNum-1));
    else if (type == DSO_X1)
        return QRectF(
            get_leftWidth() + SquareWidth*0.5,
            y - SquareWidth * 2 - SquareWidth * (SquareNum-2) * 1 + SquareWidth * 3,
            SquareWidth * 1.75, SquareWidth);
    else if (type == DSO_X10)
        return QRectF(
            get_leftWidth() + SquareWidth*0.5,
            y - SquareWidth * 2 - SquareWidth * (SquareNum-2) * 0.5 + SquareWidth * 3,
            SquareWidth * 1.75, SquareWidth);
    else if (type == DSO_X100)
        return QRectF(
            get_leftWidth() + SquareWidth*0.5,
            y - SquareWidth * 2 - SquareWidth * (SquareNum-2) * 0 + SquareWidth * 3,
            SquareWidth * 1.75, SquareWidth);
    else if (type == DSO_CHEN)
        return QRectF(
            2,
            y - SquareWidth / 2 + SquareWidth * 3,
            SquareWidth * 1.75, SquareWidth);
    else if (type == DSO_ACDC)
        return QRectF(
            2+SquareWidth*1.75 + Margin,
            y - SquareWidth / 2 + SquareWidth * 3,
            SquareWidth * 1.75, SquareWidth);
    else if (type == DSO_AUTO)
        return QRectF(
            2+SquareWidth*3.5 + Margin*2,
            y - SquareWidth / 2 + SquareWidth * 3,
            SquareWidth * 1.75, SquareWidth);
    else
        return QRectF(0, 0, 0, 0);
}

void DsoSignal::paint_hover_measure(QPainter &p, QColor fore, QColor back)
{
    const int hw_offset = get_hw_offset();
    // Hover measure
    if (_hover_en && _hover_point != QPointF(-1, -1)) {
        QString hover_str = get_voltage(hw_offset - _hover_value, 2);
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

    auto &cursor_list = _view->get_cursorList();
    auto i = cursor_list.begin();

    while (i != cursor_list.end()) {
        float pt_value;

        int chan_index = (*i)->index();
        if (_data->has_data(chan_index) == false){
            i++;
            continue;
        }

        const QPointF pt = get_point(chan_index, pt_value);
        if (pt == QPointF(-1, -1)) {
            i++;
            continue;
        }
        
        QString pt_str = get_voltage(hw_offset - pt_value, 2);
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

void DsoSignal::auto_set()
{ 
    if (session->is_stopped_status()) {
        if (_autoV)
            autoV_end();
        if (_autoH)
            autoH_end();
    } 
    else {
        if (_autoH && _autoV && get_zero_ratio() != 0.5) {
            set_zero_ratio(0.5);
        }
        if (_mValid && !session->get_data_auto_lock()) {
            if (_autoH) {
                bool roll = false;
                session->get_device()->get_config_bool(SR_CONF_ROLL, roll);

                const double hori_res = _view->get_hori_res();
                if (_level_valid && ((!roll && _pcount < 3) || _period > 4*hori_res)) {
                    _view->zoom(-1);
                } else if (_level_valid && _pcount > 6 && _period < 1.5*hori_res) {
                    _view->zoom(1);
                } else if (_level_valid) {
                    autoH_end();
                }
            }
            if (_autoV) {
                const bool over_flag = _max == 0xff || _min == 0x0;
                const bool out_flag = _max >= 0xE0 || _min <= 0x20;
                const bool under_flag = _max <= 0xA0 && _min >= 0x60;
                if (over_flag) {
                    if (!_autoV_over)
                        _auto_cnt = 0;
                    _autoV_over = true;
                    go_vDialNext(false);
                } else if (out_flag) {
                    go_vDialNext(false);
                } else if (!_autoV_over && under_flag) {
                    go_vDialPre(false);
                } else if (!_autoH) {
                    autoV_end();
                }

                if (_autoV_over && under_flag) {
                    if (_auto_cnt++ > 16)
                        _autoV_over = false;
                } else {
                    _auto_cnt = 0;
                }

                if (_level_valid) {
                    _trig_value = (_min+_max)/2;
                    set_trig_vpos(ratio2pos(get_trig_vrate()));
                }
            }
            if (_autoH || _autoV)
                session->data_auto_lock(AutoLock);
        }
    }
}

void DsoSignal::autoV_end()
{
    _autoV = false;
    _autoV_over = false;
    _view->auto_trig(get_index());
    _trig_value = (_min+_max)/2;
    set_trig_vpos(ratio2pos(get_trig_vrate()));
    _view->set_update(_viewport, true);
    _view->update();
}

void DsoSignal::autoH_end()
{
    _autoH = false;
    _view->set_update(_viewport, true);
    _view->update();
}

void DsoSignal::auto_end()
{
    if (_autoV)
        autoV_end();
    if (_autoH)
        autoH_end();
}

void DsoSignal::auto_start()
{ 
    if (_autoV || _autoH)
        return;

    if (session->is_running_status()) {
        session->data_auto_lock(AutoLock);
        _autoV = true;
        _autoH = true;
        _view->auto_trig(get_index()); 
        _end_timer.TimeOut(AutoTime, std::bind(&DsoSignal::call_auto_end, this)); //start a timeout
    }
}

bool DsoSignal::measure(const QPointF &p)
{
    _hover_en = false;
    
    if (!enabled() || !show())
        return false;

    if (session->is_stopped_status() == false)
        return false;

    const QRectF window = get_view_rect();
    if (!window.contains(p))
        return false;

    if (_data->empty())
        return false;

    _hover_index = _view->pixel2index(p.x());
    if (_hover_index >= _data->get_sample_count())
        return false;

    int chan_index = get_index();
    if (_data->has_data(chan_index) == false){
        dsv_err("channel %d have no data.", chan_index);
        return false;
    }

    _hover_point = get_point(_hover_index, _hover_value);
    _hover_en = true;
    return true;
}

bool DsoSignal::get_hover(uint64_t &index, QPointF &p, double &value)
{
    if (_hover_en) {
        index = _hover_index;
        p = _hover_point;
        value = _hover_value;
        return true;
    }
    return false;
}

QPointF DsoSignal::get_point(uint64_t index, float &value)
{
    QPointF pt = QPointF(-1, -1);

    if (!enabled())
        return pt;

    if (_data->empty())
        return pt;

    if (index >= _data->get_sample_count())
        return pt;

    value = *_data->get_samples(index, index, get_index());
    const float top = get_view_rect().top();
    const float bottom = get_view_rect().bottom();
    const int hw_offset = get_hw_offset();
    const float x = _view->index2pixel(index);
    const float y = min(max(top, get_zero_vpos() + (value - hw_offset)* _scale), bottom);
    pt = QPointF(x, y);

    return pt;
}

double DsoSignal::get_voltage(uint64_t index)
{
    if (!enabled())
        return 1;

    if (_data->empty())
        return 1;

    if (index >= _data->get_sample_count())
        return 1;
    
    assert(_data);

    const double value = *_data->get_samples(index, index, get_index());
    const int hw_offset = get_hw_offset();
    uint64_t k = _data->get_measure_voltage_factor(this->get_index());
    float data_scale = _data->get_data_scale(this->get_index());

    return (hw_offset - value) * data_scale * 
             k *_vDial->get_factor() *
               DS_CONF_DSO_VDIVS / get_view_rect().height();
}

QString DsoSignal::get_voltage(double v, int p, bool scaled)
{
    if (_vDial == NULL){
        assert(false);
    }

    if (get_view_rect().height() == 0){
        assert(false);
    }

    assert(_data);

    uint64_t k = _data->get_measure_voltage_factor(this->get_index());
    float data_scale = _data->get_data_scale(this->get_index());

    if (scaled)
        v = v * k * _vDial->get_factor() * DS_CONF_DSO_VDIVS / get_view_rect().height();
    else
        v = v * data_scale * k * _vDial->get_factor() * DS_CONF_DSO_VDIVS / get_view_rect().height();
    
    return abs(v) >= 1000 ? QString::number(v/1000.0, 'f', p) + "V" : QString::number(v, 'f', p) + "mV";
}

QString DsoSignal::get_time(double t)
{
    QString str = (abs(t) > 1000000000 ? QString::number(t/1000000000, 'f', 2) + "S" :
                   abs(t) > 1000000 ? QString::number(t/1000000, 'f', 2) + "mS" :
                   abs(t) > 1000 ? QString::number(t/1000, 'f', 2) + "uS" : QString::number(t, 'f', 2) + "nS");
    return str;
}

void DsoSignal::call_auto_end(){
    session->auto_end();
}

void DsoSignal::set_data(data::DsoSnapshot *data)
{
    assert(data);
    _data = data;
}

} // namespace view
} // namespace pv
