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

#include <extdef.h>
#include <math.h>

#include "../../extdef.h"
#include "dsosignal.h"
#include "pv/data/dso.h"
#include "pv/data/dsosnapshot.h"
#include "view.h"
#include "../sigsession.h"
#include "../device/devinst.h"

#include <boost/foreach.hpp>

#include <QDebug>
#include <QTimer>

using namespace boost;
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

DsoSignal::DsoSignal(boost::shared_ptr<pv::device::DevInst> dev_inst,
                     boost::shared_ptr<data::Dso> data,
                     sr_channel *probe):
    Signal(dev_inst, probe),
    _data(data),
    _scale(0),
    _en_lock(false),
    _show(true),
    _vDialActive(false),
    _mValid(false),
    _level_valid(false),
    _autoV(false),
    _autoH(false),
    _autoV_over(false),
    _auto_cnt(0),
    _hover_en(false),
    _hover_index(0),
    _hover_point(QPointF(-1, -1)),
    _hover_value(0)
{
    QVector<uint64_t> vValue;
    QVector<QString> vUnit;
    for(uint64_t i = 0; i < vDialUnitCount; i++)
        vUnit.append(vDialUnit[i]);

    GVariant *gvar_list, *gvar_list_vdivs;
    if (sr_config_list(dev_inst->dev_inst()->driver, dev_inst->dev_inst(),
                       NULL, SR_CONF_PROBE_VDIV, &gvar_list) == SR_OK) {
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
}

boost::shared_ptr<pv::data::SignalData> DsoSignal::data() const
{
    return _data;
}

boost::shared_ptr<pv::data::Dso> DsoSignal::dso_data() const
{
    return _data;
}

void DsoSignal::set_scale(int height)
{
    _scale = height / (_ref_max - _ref_min) * _view->session().stop_scale();
}

float DsoSignal::get_scale()
{
    return _scale;
}

uint8_t DsoSignal::get_bits()
{
    return _bits;
}

double DsoSignal::get_ref_min() const
{
    return _ref_min;
}

double DsoSignal::get_ref_max() const
{
    return _ref_max;
}

int DsoSignal::get_name_width() const
{
    return 0;
}

void DsoSignal::set_enable(bool enable)
{
    if (_dev_inst->name() == "DSLogic" &&
         get_index() == 0)
        return;

    _en_lock = true;
    GVariant* gvar;
    bool cur_enable;
    gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_PROBE_EN);
    if (gvar != NULL) {
        cur_enable = g_variant_get_boolean(gvar);
        g_variant_unref(gvar);
    } else {
        qDebug() << "ERROR: config_get SR_CONF_PROBE_EN failed.";
        _en_lock = false;
        return;
    }
    if (cur_enable == enable) {
        _en_lock = false;
        return;
    }

    bool running =  false;
    if (_view->session().get_capture_state() == SigSession::Running) {
        running = true;
        _view->session().stop_capture();
    }
    while(_view->session().get_capture_state() == SigSession::Running)
        QCoreApplication::processEvents();

    set_vDialActive(false);
    _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_EN,
                          g_variant_new_boolean(enable));

    _view->update_hori_res();
    if (running) {
        _view->session().repeat_resume();
    }

    _view->set_update(_viewport, true);
    _view->update();
    _en_lock = false;
}

bool DsoSignal::get_vDialActive() const
{
    return _vDialActive;
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

    if (enabled() && !_vDial->isMin()) {
        if (_view->session().get_capture_state() == SigSession::Running)
            _view->session().refresh(RefreshShort);
        const double pre_vdiv = _vDial->get_value();
        _vDial->set_sel(_vDial->get_sel() - 1);
        _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_VDIV,
                              g_variant_new_uint64(_vDial->get_value()));
        if (_view->session().get_capture_state() == SigSession::Stopped) {
            _view->session().set_stop_scale(_view->session().stop_scale() * (pre_vdiv/_vDial->get_value()));
            set_scale(get_view_rect().height());
        }
        _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_OFFSET,
                              g_variant_new_uint16(_zero_offset));

        _view->vDial_updated();
        _view->set_update(_viewport, true);
        _view->update();
        return true;
    } else {
        if (_autoV && !_autoV_over)
            autoV_end();
        return false;
    }
}

bool DsoSignal::go_vDialNext(bool manul)
{
    if (_autoV && manul)
        autoV_end();

    if (enabled() && !_vDial->isMax()) {
        if (_view->session().get_capture_state() == SigSession::Running)
            _view->session().refresh(RefreshShort);
        const double pre_vdiv = _vDial->get_value();
        _vDial->set_sel(_vDial->get_sel() + 1);
        _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_VDIV,
                              g_variant_new_uint64(_vDial->get_value()));
        if (_view->session().get_capture_state() == SigSession::Stopped) {
            _view->session().set_stop_scale(_view->session().stop_scale() * (pre_vdiv/_vDial->get_value()));
            set_scale(get_view_rect().height());
        }
        _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_OFFSET,
                              g_variant_new_uint16(_zero_offset));

        _view->vDial_updated();
        _view->set_update(_viewport, true);
        _view->update();
        return true;
    } else {
        if (_autoV && !_autoV_over)
            autoV_end();
        return false;
    }
}

bool DsoSignal::load_settings()
{
    GVariant* gvar;

    // -- enable
//    bool enable;
//    gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_PROBE_EN);
//    if (gvar != NULL) {
//        enable = g_variant_get_boolean(gvar);
//        g_variant_unref(gvar);
//    } else {
//        qDebug() << "ERROR: config_get SR_CONF_PROBE_EN failed.";
//        return false;
//    }

    // dso channel bits
    gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_UNIT_BITS);
    if (gvar != NULL) {
        _bits = g_variant_get_byte(gvar);
        g_variant_unref(gvar);
    } else {
        _bits = DefaultBits;
        qDebug("Warning: config_get SR_CONF_UNIT_BITS failed, set to %d(default).", DefaultBits);
        if (strncmp(_dev_inst->name().toUtf8().data(), "virtual", 7))
            return false;
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

    // -- vdiv
    uint64_t vdiv;
    uint64_t vfactor;
    gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_PROBE_VDIV);
    if (gvar != NULL) {
        vdiv = g_variant_get_uint64(gvar);
        g_variant_unref(gvar);
    } else {
        qDebug() << "ERROR: config_get SR_CONF_PROBE_VDIV failed.";
        return false;
    }
    gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_PROBE_FACTOR);
    if (gvar != NULL) {
        vfactor = g_variant_get_uint64(gvar);
        g_variant_unref(gvar);
    } else {
        qDebug() << "ERROR: config_get SR_CONF_PROBE_FACTOR failed.";
        return false;
    }

    _vDial->set_value(vdiv);
    _vDial->set_factor(vfactor);
//    _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_VDIV,
//                          g_variant_new_uint64(_vDial->get_value()));

    // -- coupling
    gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_PROBE_COUPLING);
    if (gvar != NULL) {
        _acCoupling = g_variant_get_byte(gvar);
        g_variant_unref(gvar);
    } else {
        qDebug() << "ERROR: config_get SR_CONF_PROBE_COUPLING failed.";
        return false;
    }

//    _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_COUPLING,
//                          g_variant_new_byte(_acCoupling));

    // -- vpos
    gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_PROBE_OFFSET);
    if (gvar != NULL) {
        _zero_offset = g_variant_get_uint16(gvar);
        g_variant_unref(gvar);
    } else {
        qDebug() << "ERROR: config_get SR_CONF_PROBE_OFFSET failed.";
        return false;
    }

    // -- trig_value
    gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_TRIGGER_VALUE);
    if (gvar != NULL) {
        _trig_value = g_variant_get_byte(gvar);
        _trig_delta = get_trig_vrate() - get_zero_ratio();
        g_variant_unref(gvar);
    } else {
        qDebug() << "ERROR: config_get SR_CONF_TRIGGER_VALUE failed.";
        if (strncmp(_dev_inst->name().toUtf8().data(), "virtual", 7))
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
    ret = _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_EN,
                                g_variant_new_boolean(enabled()));

    // -- vdiv
    ret = _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_VDIV,
                                g_variant_new_uint64(_vDial->get_value()));
    ret = _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_FACTOR,
                                g_variant_new_uint64(_vDial->get_factor()));

    // -- coupling
    ret = _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_COUPLING,
                                g_variant_new_byte(_acCoupling));

    // -- offset
    ret = _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_OFFSET,
                                g_variant_new_uint16(_zero_offset));

    // -- trig_value
    _dev_inst->set_config(_probe, NULL, SR_CONF_TRIGGER_VALUE,
                          g_variant_new_byte(_trig_value));

    return ret;
}

dslDial * DsoSignal::get_vDial() const
{
    return _vDial;
}

uint64_t DsoSignal::get_vDialValue() const
{
    return _vDial->get_value();
}

uint16_t DsoSignal::get_vDialSel() const
{
    return _vDial->get_sel();
}

uint8_t DsoSignal::get_acCoupling() const
{
    return _acCoupling;
}

void DsoSignal::set_acCoupling(uint8_t coupling)
{
    if (enabled()) {
        _acCoupling = coupling;
        _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_COUPLING,
                              g_variant_new_byte(_acCoupling));
    }
}

int DsoSignal::ratio2value(double ratio) const
{
    return ratio * (_ref_max - _ref_min) + _ref_min;
}

int DsoSignal::ratio2pos(double ratio) const
{
    return ratio * get_view_rect().height() + get_view_rect().top();
}

double DsoSignal::value2ratio(int value) const
{
    return max(0.0, (value - _ref_min) / (_ref_max - _ref_min));
}

double DsoSignal::pos2ratio(int pos) const
{
    return min(max(pos - get_view_rect().top(), 0), get_view_rect().height()) * 1.0 / get_view_rect().height();
}

double DsoSignal::get_trig_vrate() const
{
    if (_dev_inst->name() == "DSLogic")
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
    if (_dev_inst->name() == "DSLogic") {
        delta = delta - get_zero_ratio();
        delta = min(delta, 0.5);
        delta = max(delta, -0.5);
        _trig_value = ratio2value(delta + 0.5);
    } else {
        _trig_value = ratio2value(delta);
    }

    int margin = TrigMargin;
    _trig_value = std::min(std::max(_trig_value, margin), (ratio2value(1) - margin));
    if (delta_change)
        _trig_delta = get_trig_vrate() - get_zero_ratio();
    _dev_inst->set_config(_probe, NULL, SR_CONF_TRIGGER_VALUE,
                          g_variant_new_byte(_trig_value));
}

int DsoSignal::get_zero_vpos() const
{
    return ratio2pos(get_zero_ratio());
}

double DsoSignal::get_zero_ratio() const
{
    return value2ratio(_zero_offset);
}

int DsoSignal::get_hw_offset() const
{
    int hw_offset = 0;
    GVariant *gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_PROBE_HW_OFFSET);
    if (gvar != NULL) {
        hw_offset = g_variant_get_uint16(gvar);
        g_variant_unref(gvar);
    }
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
    _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_OFFSET,
                          g_variant_new_uint16(_zero_offset));
}

void DsoSignal::set_factor(uint64_t factor)
{
    if (enabled()) {
        GVariant* gvar;
        uint64_t prefactor = 0;
        gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_PROBE_FACTOR);
        if (gvar != NULL) {
            prefactor = g_variant_get_uint64(gvar);
            g_variant_unref(gvar);
        } else {
            qDebug() << "ERROR: config_get SR_CONF_PROBE_FACTOR failed.";
            return;
        }
        if (prefactor != factor) {
            _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_FACTOR,
                                  g_variant_new_uint64(factor));
            _vDial->set_factor(factor);
            _view->set_update(_viewport, true);
            _view->update();
        }
    }
}

uint64_t DsoSignal::get_factor()
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

void DsoSignal::set_show(bool show)
{
    _show = show;
}

bool DsoSignal::show() const
{
    return _show;
}

void DsoSignal::set_mValid(bool valid)
{
    _mValid = valid;
}

QString DsoSignal::get_measure(enum DSO_MEASURE_TYPE type)
{
    const QString mNone = "--";
    QString mString;
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
            mString = (abs(_period) > 1000000 ? QString::number(1000000000/_period, 'f', 2) + "Hz" :
                       abs(_period) > 1000 ? QString::number(1000000/_period, 'f', 2) + "kHz" : QString::number(1000/_period, 'f', 2) + "MHz");
            break;
        case DSO_MS_VRMS:
            mString = get_voltage(_rms, 2);
            break;
        case DSO_MS_VMEA:
            mString = get_voltage(_mean, 2);
            break;
        case DSO_MS_NOVR:
            if (_level_valid)
                mString = QString::number((_max - _high) * 100.0 / (_high - _low), 'f', 2) + "%";
            else
                mString = mNone;
            break;
        case DSO_MS_POVR:
            if (_level_valid)
                mString = QString::number((_low - _min) * 100.0 / (_high - _low), 'f', 2) + "%";
            else
                mString = mNone;
            break;
        case DSO_MS_PDUT:
            if (_level_valid)
                mString = QString::number(_high_time/_period*100, 'f', 2)+"%";
            else
                mString = mNone;
            break;
        case DSO_MS_NDUT:
            if (_level_valid)
                mString = QString::number(100 - _high_time/_period*100, 'f', 2)+"%";
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

QRect DsoSignal::get_view_rect() const
{
    assert(_viewport);
    return QRect(0, UpMargin,
                  _viewport->width() - RightMargin,
                  _viewport->height() - UpMargin - DownMargin);
}

void DsoSignal::paint_prepare()
{
    assert(_view);

    const deque< boost::shared_ptr<pv::data::DsoSnapshot> > &snapshots =
        _data->get_snapshots();
    if (snapshots.empty())
        return;
    const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot =
        snapshots.front();
    if (snapshot->empty())
        return;

    if (!snapshot->has_data(get_index()))
        return;

    const uint16_t enabled_channels = snapshot->get_channel_num();
    if (_view->session().trigd()) {
        if (get_index() == _view->session().trigd_ch()) {
            uint8_t slope = DSO_TRIGGER_RISING;
            GVariant *gvar = _view->session().get_device()->get_config(NULL, NULL, SR_CONF_TRIGGER_SLOPE);
            if (gvar != NULL) {
                slope = g_variant_get_byte(gvar);
                g_variant_unref(gvar);
            }

            int64_t trig_index = _view->get_trig_cursor()->index();
            if (trig_index >= (int64_t)snapshot->get_sample_count())
                return;
            const uint8_t *const trig_samples = snapshot->get_samples(0, 0, get_index());
            for (uint16_t i = 0; i < TrigHRng; i++) {
                const int64_t i0 = (trig_index - i - 1)*enabled_channels;
                const int64_t i1 = (trig_index - i)*enabled_channels;
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
        //if (_view->trig_hoff() == 0 && trig_samples[3] != _trig_value)
        //    _view->set_trig_hoff(0);
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
    const uint64_t sample_len = _view->session().cur_samplelimits();
    const double samplerate = _view->session().cur_snap_samplerate();
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

    if (!_show)
        return;

    assert(_data);
    assert(_view);
    assert(right >= left);

    if (enabled()) {
        const int index = get_index();
        const int width = right - left;
        const float zeroY = get_zero_vpos();

        const double scale = _view->scale();
        assert(scale > 0);
        const int64_t offset = _view->offset();

        const deque< boost::shared_ptr<pv::data::DsoSnapshot> > &snapshots =
            _data->get_snapshots();
        if (snapshots.empty())
            return;
        const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot =
            snapshots.front();
        if (snapshot->empty())
            return;

        if (!snapshot->has_data(index))
            return;

        const uint16_t enabled_channels = snapshot->get_channel_num();
        const double pixels_offset = offset;
        const double samplerate = _data->samplerate();
        //const double samplerate = _dev_inst->get_sample_rate();
        //const double samplerate = _view->session().cur_snap_samplerate();
        const int64_t last_sample = max((int64_t)(snapshot->get_sample_count() - 1), (int64_t)0);
        const double samples_per_pixel = samplerate * scale;
        const double start = offset * samples_per_pixel - _view->trig_hoff();
        const double end = start + samples_per_pixel * width;

        const int64_t start_sample = min(max((int64_t)floor(start),
            (int64_t)0), last_sample);
        const int64_t end_sample = min(max((int64_t)ceil(end) + 1,
            (int64_t)0), last_sample);
        const int hw_offset = get_hw_offset();

        if (samples_per_pixel < EnvelopeThreshold) {
            snapshot->enable_envelope(false);
            paint_trace(p, snapshot, zeroY, left,
                start_sample, end_sample, hw_offset,
                pixels_offset, samples_per_pixel, enabled_channels);
        } else {
            snapshot->enable_envelope(true);
            paint_envelope(p, snapshot, zeroY, left,
                start_sample, end_sample, hw_offset,
                pixels_offset, samples_per_pixel, enabled_channels);
        }

        sr_status status;
        if (sr_status_get(_dev_inst->dev_inst(), &status, false) == SR_OK) {
            _mValid = true;
            if (status.measure_valid) {
                _min = (index == 0) ? status.ch0_min : status.ch1_min;
                _max = (index == 0) ? status.ch0_max : status.ch1_max;

                _level_valid = (index == 0) ? status.ch0_level_valid : status.ch1_level_valid;
                _low = (index == 0) ? status.ch0_low_level : status.ch1_low_level;
                _high = (index == 0) ? status.ch0_high_level : status.ch1_high_level;

                const uint32_t count  = (index == 0) ? status.ch0_cyc_cnt : status.ch1_cyc_cnt;
                const bool plevel = (index == 0) ? status.ch0_plevel : status.ch1_plevel;
                const bool startXORend = (index == 0) ? (status.ch0_cyc_llen == 0) : (status.ch1_cyc_llen == 0);
                const uint16_t total_channels = g_slist_length(_dev_inst->dev_inst()->channels);
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
                _rms = sqrt(_rms / snapshot->get_sample_count());
                _mean = (index == 0) ? status.ch0_acc_mean : status.ch1_acc_mean;
                _mean = hw_offset - _mean / snapshot->get_sample_count();
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
        if (_view->session().get_capture_state() == SigSession::Stopped)
            paint_hover_measure(p, fore, back);

        // autoset
        auto_set();
    }
}

QRectF DsoSignal::get_trig_rect(int left, int right) const
{
    (void)left;

    return QRectF(right + SquareWidth / 2,
                  ratio2pos(get_trig_vrate()) - SquareWidth / 2,
                  SquareWidth, SquareWidth);
}

void DsoSignal::paint_trace(QPainter &p,
    const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot,
    int zeroY, int left, const int64_t start, const int64_t end, int hw_offset,
    const double pixels_offset, const double samples_per_pixel, uint64_t num_channels)
{
    const int64_t sample_count = end - start + 1;

    if (sample_count > 0) {
        const uint8_t *const samples = snapshot->get_samples(start, end, get_index());
        assert(samples);

        QColor trace_colour = _colour;
        trace_colour.setAlpha(View::ForeAlpha);
        p.setPen(trace_colour);

        QPointF *points = new QPointF[sample_count];
        QPointF *point = points;

        float top = get_view_rect().top();
        float bottom = get_view_rect().bottom();
        double  pixels_per_sample = 1.0/samples_per_pixel;

        uint8_t value;
        int64_t sample_end = sample_count*num_channels;
        float x = (start / samples_per_pixel - pixels_offset) + left + _view->trig_hoff()*pixels_per_sample;
        for (int64_t sample = 0; sample < sample_end; sample+=num_channels) {
            value = samples[sample];
            const float y = min(max(top, zeroY + (value - hw_offset) * _scale), bottom);
            if (x > get_view_rect().right()) {
                point--;
                const float lastY = point->y() + (y - point->y()) / (x - point->x()) * (get_view_rect().right() - point->x());
                point++;
                *point++ = QPointF(get_view_rect().right(), lastY);
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
    const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot,
    int zeroY, int left, const int64_t start, const int64_t end, int hw_offset,
    const double pixels_offset, const double samples_per_pixel, uint64_t num_channels)
{
	using namespace Qt;
    using pv::data::DsoSnapshot;

    DsoSnapshot::EnvelopeSection e;
    const uint16_t index = get_index() % num_channels;
    snapshot->get_envelope_section(e, start, end, samples_per_pixel, index);

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

    if (!_dev_inst->name().contains("virtual")) {
        p.setPen(Qt::transparent);
        p.setBrush(enabled() ? (auto_rect.contains(pt) ? _colour.darker() : _colour) : foreBack);
        p.drawRect(auto_rect);
        p.setPen(Qt::white);
        p.drawText(auto_rect, Qt::AlignCenter | Qt::AlignVCenter, tr(strings[5]));
    }

    // paint the probe factor selector
    GVariant* gvar;
    uint64_t factor;
    gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_PROBE_FACTOR);
    if (gvar != NULL) {
        factor = g_variant_get_uint64(gvar);
        g_variant_unref(gvar);
    } else {
        qDebug() << "ERROR: config_get SR_CONF_PROBE_FACTOR failed.";
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
       if (_dev_inst->name() != "virtual-session" &&
           !_en_lock) {
           set_enable(!enabled());
       }
       return true;
    } else if (enabled()) {
        if (vDial_rect.contains(pt) && pt.x() > vDial_rect.center().x()) {
            if (pt.y() > vDial_rect.center().y())
                go_vDialNext(true);
            else
                go_vDialPre(true);
        } else if (_dev_inst->name() != "virtual-session" &&
                   acdc_rect.contains(pt)) {
           if (_dev_inst->name() == "DSLogic")
               set_acCoupling((get_acCoupling()+1)%2);
           else
               set_acCoupling((get_acCoupling()+1)%2);
        } else if (auto_rect.contains(pt)) {
            if (!_dev_inst->name().contains("virtual"))
                auto_start();
        } else if (x1_rect.contains(pt)) {
           set_factor(1);
        } else if (x10_rect.contains(pt)) {
           set_factor(10);
        } else if (x100_rect.contains(pt)) {
           set_factor(100);
        } else {
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

    list<Cursor*>::iterator i = _view->get_cursorList().begin();
    while (i != _view->get_cursorList().end()) {
        float pt_value;
        const QPointF pt = get_point((*i)->index(), pt_value);
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
    if (_view->session().get_capture_state() == SigSession::Stopped) {
        if (_autoV)
            autoV_end();
        if (_autoH)
            autoH_end();
    } else {
        if (_autoH && _autoV && get_zero_ratio() != 0.5) {
            set_zero_ratio(0.5);
        }
        if (_mValid && !_view->session().get_data_auto_lock()) {
            if (_autoH) {
                bool roll = false;
                GVariant *gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_ROLL);
                if (gvar != NULL) {
                    roll = g_variant_get_boolean(gvar);
                    g_variant_unref(gvar);
                }
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
                _view->session().data_auto_lock(AutoLock);
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
    if (_view->session().get_capture_state() == SigSession::Running) {
        _view->session().data_auto_lock(AutoLock);
        _autoV = true;
        _autoH = true;
        _view->auto_trig(get_index());
        QTimer::singleShot(AutoTime, &_view->session(), SLOT(auto_end()));
    }
}

bool DsoSignal::measure(const QPointF &p)
{
    _hover_en = false;
    if (!enabled() || !show())
        return false;

    if (_view->session().get_capture_state() != SigSession::Stopped)
        return false;

    const QRectF window = get_view_rect();
    if (!window.contains(p))
        return false;

    const deque< boost::shared_ptr<pv::data::DsoSnapshot> > &snapshots =
        _data->get_snapshots();
    if (snapshots.empty())
        return false;

    const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot =
        snapshots.front();
    if (snapshot->empty())
        return false;

    _hover_index = _view->pixel2index(p.x());
    if (_hover_index >= snapshot->get_sample_count())
        return false;

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

    const deque< boost::shared_ptr<pv::data::DsoSnapshot> > &snapshots =
        _data->get_snapshots();
    if (snapshots.empty())
        return pt;

    const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot =
        snapshots.front();
    if (snapshot->empty())
        return pt;

    if (index >= snapshot->get_sample_count())
        return pt;

    value = *snapshot->get_samples(index, index, get_index());
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

    const deque< boost::shared_ptr<pv::data::DsoSnapshot> > &snapshots =
        _data->get_snapshots();
    if (snapshots.empty())
        return 1;

    const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot =
        snapshots.front();
    if (snapshot->empty())
        return 1;

    if (index >= snapshot->get_sample_count())
        return 1;

    const double value = *snapshot->get_samples(index, index, get_index());
    const int hw_offset = get_hw_offset();
    return (hw_offset - value) * _scale *
            _vDial->get_value() * _vDial->get_factor() *
            DS_CONF_DSO_VDIVS / get_view_rect().height();
}

QString DsoSignal::get_voltage(double v, int p, bool scaled)
{
    if (scaled)
        v = v * _vDial->get_value() * _vDial->get_factor() * DS_CONF_DSO_VDIVS / get_view_rect().height();
    else
        v = v * _scale * _vDial->get_value() * _vDial->get_factor() * DS_CONF_DSO_VDIVS / get_view_rect().height();
    return abs(v) >= 1000 ? QString::number(v/1000.0, 'f', p) + "V" : QString::number(v, 'f', p) + "mV";
}

QString DsoSignal::get_time(double t)
{
    QString str = (abs(t) > 1000000000 ? QString::number(t/1000000000, 'f', 2) + "S" :
                   abs(t) > 1000000 ? QString::number(t/1000000, 'f', 2) + "mS" :
                   abs(t) > 1000 ? QString::number(t/1000, 'f', 2) + "uS" : QString::number(t, 'f', 2) + "nS");
    return str;
}

} // namespace view
} // namespace pv
