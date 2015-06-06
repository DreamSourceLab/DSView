/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2013 DreamSourceLab <dreamsourcelab@dreamsourcelab.com>
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

#include "dsosignal.h"
#include "pv/data/dso.h"
#include "pv/data/dsosnapshot.h"
#include "view.h"
#include "../sigsession.h"
#include "../device/devinst.h"

#include <boost/foreach.hpp>

#include <QDebug>

using namespace boost;
using namespace std;

namespace pv {
namespace view {

const uint64_t DsoSignal::vDialValue[DsoSignal::vDialValueCount] = {
    10,
    20,
    50,
    100,
    200,
    500,
    1000,
    2000,
};
const QString DsoSignal::vDialUnit[DsoSignal::vDialUnitCount] = {
    "mv",
    "v",
};

const uint64_t DsoSignal::hDialValue[DsoSignal::hDialValueCount] = {
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
    2000000,
    5000000,
    10000000,
    20000000,
    50000000,
    100000000,
    200000000,
    500000000,
    1000000000,
    2000000000,
    5000000000,
    10000000000,
};

const QString DsoSignal::hDialUnit[DsoSignal::hDialUnitCount] = {
    "ns",
    "us",
    "ms",
    "s",
};

const QColor DsoSignal::SignalColours[4] = {
    QColor(238, 178, 17, 200),  // dsYellow
    QColor(0, 153, 37, 200),    // dsGreen
    QColor(213, 15, 37, 200),   // dsRed
    QColor(17, 133, 209, 200)  // dsBlue

};

const float DsoSignal::EnvelopeThreshold = 256.0f;
const double DsoSignal::TrigMargin = 0.02;

const int DsoSignal::UpMargin = 30;
const int DsoSignal::DownMargin = 30;
const int DsoSignal::RightMargin = 30;

DsoSignal::DsoSignal(boost::shared_ptr<pv::device::DevInst> dev_inst,
                     boost::shared_ptr<data::Dso> data,
                     const sr_channel * const probe):
    Signal(dev_inst, probe, SR_CHANNEL_DSO),
    _data(data),
    _scale(0),
    _vDialActive(false),
    _hDialActive(false),
    //_trig_vpos(probe->index * 0.5 + 0.25),
    //_zeroPos(probe->index * 0.5 + 0.25)
    _trig_vpos(0.5),
    _zeroPos(0.5),
    _zero_off(255/2.0),
    _autoV(false),
    _autoH(false),
    _hover_en(false),
    _hover_index(0),
    _hover_point(QPointF(0, 0)),
    _hover_value(0)
{
    QVector<uint64_t> vValue;
    QVector<QString> vUnit;
    QVector<uint64_t> hValue;
    QVector<QString> hUnit;
    for(uint64_t i = 0; i < vDialValueCount; i++)
        vValue.append(vDialValue[i]);
    for(uint64_t i = 0; i < vDialUnitCount; i++)
        vUnit.append(vDialUnit[i]);

    for(uint64_t i = 0; i < hDialValueCount; i++)
        hValue.append(hDialValue[i]);
    for(uint64_t i = 0; i < hDialUnitCount; i++)
        hUnit.append(hDialUnit[i]);

    _vDial = new dslDial(vDialValueCount, vDialValueStep, vValue, vUnit);
    _hDial = new dslDial(hDialValueCount, hDialValueStep, hValue, hUnit);

    _colour = SignalColours[probe->index % countof(SignalColours)];

//    GVariant* gvar;

//    gvar = dev_inst->get_config(probe, NULL, SR_CONF_VDIV);
//    if (gvar != NULL) {
//        _vDial->set_value(g_variant_get_uint64(gvar));
//        g_variant_unref(gvar);
//    } else {
//        qDebug() << "ERROR: config_get SR_CONF_VDIV failed.";
//    }

//    gvar = dev_inst->get_config(NULL, NULL, SR_CONF_TIMEBASE);
//    if (gvar != NULL) {
//        _hDial->set_value(g_variant_get_uint64(gvar));
//        g_variant_unref(gvar);
//    } else {
//        qDebug() << "ERROR: config_get SR_CONF_TIMEBASE failed.";
//    }

//    gvar = dev_inst->get_config(probe, NULL, SR_CONF_COUPLING);
//    if (gvar != NULL) {
//        _acCoupling = g_variant_get_byte(gvar);
//        g_variant_unref(gvar);
//    } else {
//        qDebug() << "ERROR: config_get SR_CONF_COUPLING failed.";
//    }
    update_vDial();
    update_hDial();
    update_acCoupling();
}

DsoSignal::~DsoSignal()
{
}

boost::shared_ptr<pv::data::SignalData> DsoSignal::data() const
{
    return _data;
}

void DsoSignal::set_view(pv::view::View *view)
{
    Trace::set_view(view);
    update_zeroPos();
}

void DsoSignal::set_scale(float scale)
{
	_scale = scale;
}

void DsoSignal::set_enable(bool enable)
{
    if ((strcmp(_dev_inst->dev_inst()->driver->name, "DSLogic") == 0) &&
         get_index() == 0)
        return;
    _view->session().refresh();
    set_vDialActive(false);
    _dev_inst->set_config(_probe, NULL, SR_CONF_EN_CH,
                          g_variant_new_boolean(enable));
    int ch_num = _view->session().get_ch_num(SR_CHANNEL_DSO);

    GVariant* gvar;
    uint64_t max_sample_rate;
    uint64_t max_sample_limit;
    gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_MAX_DSO_SAMPLERATE);
    if (gvar != NULL) {
        max_sample_rate = g_variant_get_uint64(gvar);
        g_variant_unref(gvar);
    } else {
        qDebug() << "ERROR: config_get SR_CONF_MAX_DSO_SAMPLERATE failed.";
        return;
    }
    gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_MAX_DSO_SAMPLELIMITS);
    if (gvar != NULL) {
        max_sample_limit = g_variant_get_uint64(gvar);
        g_variant_unref(gvar);
    } else {
        qDebug() << "ERROR: config_get SR_CONF_MAX_DSO_SAMPLELIMITS failed.";
        return;
    }

    uint64_t sample_limit = (uint64_t)(max_sample_limit / (ch_num ? ch_num : 1));
    uint64_t sample_rate = min((uint64_t)(sample_limit * std::pow(10.0, 9.0) / (_hDial->get_value() * DS_CONF_DSO_HDIVS)),
                               (uint64_t)(max_sample_rate / (ch_num ? ch_num : 1)));

    _view->set_sample_rate(sample_rate, true);
    _view->set_sample_limit(sample_limit, true);
    _view->set_need_update(true);
    _view->update();
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

bool DsoSignal::go_vDialPre()
{
    if (enabled() && !_vDial->isMin()) {
        const double pre_vdiv = _vDial->get_value();
        _vDial->set_sel(_vDial->get_sel() - 1);
        _dev_inst->set_config(_probe, NULL, SR_CONF_VDIV,
                              g_variant_new_uint64(_vDial->get_value()));
        if (_view->session().get_capture_state() == SigSession::Stopped)
            _scale *= pre_vdiv/_vDial->get_value();
        update_zeroPos();
        _view->set_need_update(true);
        _view->update();
        return true;
    } else {
        _autoV = false;
        return false;
    }
}

bool DsoSignal::go_vDialNext()
{
    if (enabled() && !_vDial->isMax()) {
        const double pre_vdiv = _vDial->get_value();
        _vDial->set_sel(_vDial->get_sel() + 1);
        _dev_inst->set_config(_probe, NULL, SR_CONF_VDIV,
                              g_variant_new_uint64(_vDial->get_value()));
        if (_view->session().get_capture_state() == SigSession::Stopped)
            _scale *= pre_vdiv/_vDial->get_value();
        update_zeroPos();
        _view->set_need_update(true);
        _view->update();
        return true;
    } else {
        _autoV = false;
        return false;
    }
}

bool DsoSignal::get_hDialActive() const
{
    return _hDialActive;
}

void DsoSignal::set_hDialActive(bool active)
{
    _hDialActive = active;
}

bool DsoSignal::go_hDialPre(bool setted)
{
    if (!_hDial->isMin()) {
        uint64_t sample_rate = _view->session().get_device()->get_sample_rate();
        const uint64_t min_div = std::pow(10.0, 9.0) / sample_rate;
        if (_view->session().get_capture_state() != SigSession::Running &&
            !_data->get_snapshots().empty()) {
            if (_hDial->get_value() > min_div)
                _hDial->set_sel(_hDial->get_sel() - 1);
        } else if ((_view->session().get_capture_state() == SigSession::Running ||
                    _data->get_snapshots().empty()) &&
                   !_view->session().get_instant()) {
            _view->session().refresh();
            _hDial->set_sel(_hDial->get_sel() - 1);

            if (!setted) {
                int ch_num = _view->session().get_ch_num(SR_CHANNEL_DSO);
                uint64_t sample_limit = _view->session().get_device()->get_sample_limit();
                GVariant* gvar;
                uint64_t max_sample_rate;
                gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_MAX_DSO_SAMPLERATE);
                if (gvar != NULL) {
                    max_sample_rate = g_variant_get_uint64(gvar);
                    g_variant_unref(gvar);
                } else {
                    qDebug() << "ERROR: config_get SR_CONF_MAX_DSO_SAMPLERATE failed.";
                    return false;
                }

                sample_rate = min((uint64_t)(sample_limit * std::pow(10.0, 9.0) / (_hDial->get_value() * DS_CONF_DSO_HDIVS)),
                                           (uint64_t)(max_sample_rate / (ch_num ? ch_num : 1)));
                _view->set_sample_rate(sample_rate, true);
            }
        } else {
            return true;
        }
        if (!setted) {
            const double scale = _hDial->get_value() * std::pow(10.0, -9.0) * DS_CONF_DSO_HDIVS / get_view_rect().width();
            _view->set_scale_offset(scale, _view->offset());
            _dev_inst->set_config(_probe, NULL, SR_CONF_TIMEBASE,
                                  g_variant_new_uint64(_hDial->get_value()));
        }
        return true;
    } else {
        _autoH = false;
        return false;
    }
}

bool DsoSignal::go_hDialCur()
{
    _view->session().refresh();

    int ch_num = _view->session().get_ch_num(SR_CHANNEL_DSO);
    uint64_t sample_limit = _view->session().get_device()->get_sample_limit();
    GVariant* gvar;
    uint64_t max_sample_rate;
    gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_MAX_DSO_SAMPLERATE);
    if (gvar != NULL) {
        max_sample_rate = g_variant_get_uint64(gvar);
        g_variant_unref(gvar);
    } else {
        qDebug() << "ERROR: config_get SR_CONF_MAX_DSO_SAMPLERATE failed.";
        return false;
    }

    uint64_t sample_rate = min((uint64_t)(sample_limit * std::pow(10.0, 9.0) / (_hDial->get_value() * DS_CONF_DSO_HDIVS)),
                               (uint64_t)(max_sample_rate / (ch_num ? ch_num : 1)));
//    _dev_inst->set_config(NULL, NULL, SR_CONF_SAMPLERATE,
//                          g_variant_new_uint64(sample_rate));
    _view->set_sample_limit(sample_limit, true);
    _view->set_sample_rate(sample_rate, true);
    _dev_inst->set_config(_probe, NULL, SR_CONF_TIMEBASE,
                          g_variant_new_uint64(_hDial->get_value()));
    return true;
}

bool DsoSignal::go_hDialNext(bool setted)
{
    if (!_hDial->isMax()) {
        if (_view->session().get_capture_state() != SigSession::Running &&
            !_data->get_snapshots().empty()) {
            _hDial->set_sel(_hDial->get_sel() + 1);
        } else if ((_view->session().get_capture_state() == SigSession::Running ||
                    _data->get_snapshots().empty()) &&
                   !_view->session().get_instant()) {
            _view->session().refresh();
            _hDial->set_sel(_hDial->get_sel() + 1);

            if (!setted) {
                int ch_num = _view->session().get_ch_num(SR_CHANNEL_DSO);

                uint64_t sample_limit = _view->session().get_device()->get_sample_limit();

                GVariant* gvar;
                uint64_t max_sample_rate;
                gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_MAX_DSO_SAMPLERATE);
                if (gvar != NULL) {
                    max_sample_rate = g_variant_get_uint64(gvar);
                    g_variant_unref(gvar);
                } else {
                    qDebug() << "ERROR: config_get SR_CONF_MAX_DSO_SAMPLERATE failed.";
		    return false;
                }

                uint64_t sample_rate = min((uint64_t)(sample_limit * std::pow(10.0, 9.0) / (_hDial->get_value() * DS_CONF_DSO_HDIVS)),
                                           (uint64_t)(max_sample_rate / (ch_num ? ch_num : 1)));
                _view->set_sample_rate(sample_rate, true);
            }
        } else {
            return true;
        }
        if (!setted) {
                const double scale = _hDial->get_value() * std::pow(10.0, -9.0) * DS_CONF_DSO_HDIVS / get_view_rect().width();
                _view->set_scale_offset(scale, _view->offset());
                _dev_inst->set_config(_probe, NULL, SR_CONF_TIMEBASE,
                                      g_variant_new_uint64(_hDial->get_value()));
        }
        return true;
    } else {
        _autoH = false;
        return false;
    }
}

bool DsoSignal::update_vDial()
{
    uint64_t vdiv;
    //uint64_t pre_vdiv = _vDial->get_value();
    GVariant* gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_VDIV);
    if (gvar != NULL) {
        vdiv = g_variant_get_uint64(gvar);
        g_variant_unref(gvar);
    } else {
        qDebug() << "ERROR: config_get SR_CONF_TIMEBASE failed.";
        return false;
    }

    _vDial->set_value(vdiv);
    _dev_inst->set_config(_probe, NULL, SR_CONF_VDIV,
                          g_variant_new_uint64(_vDial->get_value()));
    if (_view) {
        update_zeroPos();
        _view->set_need_update(true);
        _view->update();
    }
    return true;
}

bool DsoSignal::update_hDial()
{
    uint64_t hdiv;
    GVariant* gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_TIMEBASE);
    if (gvar != NULL) {
        hdiv = g_variant_get_uint64(gvar);
        g_variant_unref(gvar);
    } else {
        qDebug() << "ERROR: config_get SR_CONF_TIMEBASE failed.";
        return false;
    }

    _hDial->set_value(hdiv);
    _dev_inst->set_config(_probe, NULL, SR_CONF_TIMEBASE,
                          g_variant_new_uint64(_hDial->get_value()));
    if (_view) {
        const double scale = _hDial->get_value() * std::pow(10.0, -9.0) * DS_CONF_DSO_HDIVS / get_view_rect().width();
        _view->set_scale_offset(scale, _view->offset());
        _view->set_need_update(true);
        _view->update();
    }
    return true;
}

uint64_t DsoSignal::get_vDialValue() const
{
    return _vDial->get_value();
}

uint64_t DsoSignal::get_hDialValue() const
{
    return _hDial->get_value();
}

uint16_t DsoSignal::get_vDialSel() const
{
    return _vDial->get_sel();
}

uint16_t DsoSignal::get_hDialSel() const
{
    return _hDial->get_sel();
}

uint8_t DsoSignal::get_acCoupling() const
{
    return _acCoupling;
}

void DsoSignal::set_acCoupling(uint8_t coupling)
{
    if (enabled()) {
        _acCoupling = coupling;
        _dev_inst->set_config(_probe, NULL, SR_CONF_COUPLING,
                              g_variant_new_byte(_acCoupling));
    }
}

bool DsoSignal::update_acCoupling()
{
    GVariant* gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_COUPLING);
    if (gvar != NULL) {
        _acCoupling = g_variant_get_byte(gvar);
        g_variant_unref(gvar);
    } else {
        qDebug() << "ERROR: config_get SR_CONF_COUPLING failed.";
        return false;
    }

    _dev_inst->set_config(_probe, NULL, SR_CONF_COUPLING,
                          g_variant_new_byte(_acCoupling));
    return true;
}

int DsoSignal::get_trig_vpos() const
{
    return _trig_vpos * get_view_rect().height() + UpMargin;
}

double DsoSignal::get_trigRate() const
{
    return _trig_vpos;
}

void DsoSignal::set_trig_vpos(int pos)
{
    assert(_view);
    int trig_value;
    if (enabled()) {
        double delta = min((double)max(pos - UpMargin, 0), get_view_rect().height()) * 1.0 / get_view_rect().height();
        bool isDSCope = (strcmp(_dev_inst->dev_inst()->driver->name, "DSCope") == 0);
        if (isDSCope) {
            _trig_vpos = min(max(delta, 0+TrigMargin), 1-TrigMargin);
            trig_value = delta * 255;
        } else {
            delta = delta - _zeroPos;
            delta = min(delta, 0.5);
            delta = max(delta, -0.5);
            _trig_vpos = min(max(_zeroPos + delta, 0+TrigMargin), 1-TrigMargin);
            trig_value = (delta * 255.0f + 0x80);
        }
        _dev_inst->set_config(_probe, NULL, SR_CONF_TRIGGER_VALUE,
                              g_variant_new_byte(trig_value));
    }
}

void DsoSignal::set_trigRate(double rate)
{
    int trig_value;
    double delta = rate;
    bool isDSCope = (strcmp(_dev_inst->dev_inst()->driver->name, "DSCope") == 0);
    if (isDSCope) {
        _trig_vpos = min(max(delta, 0+TrigMargin), 1-TrigMargin);
        trig_value = delta * 255;
    } else {
        delta = delta - _zeroPos;
        delta = min(delta, 0.5);
        delta = max(delta, -0.5);
        _trig_vpos = min(max(_zeroPos + delta, 0+TrigMargin), 1-TrigMargin);
        trig_value = (delta * 255.0f + 0x80);
    }
    _dev_inst->set_config(_probe, NULL, SR_CONF_TRIGGER_VALUE,
                          g_variant_new_byte(trig_value));
}

int DsoSignal::get_zeroPos()
{
    return _zeroPos * get_view_rect().height() + UpMargin;
}

double DsoSignal::get_zeroRate()
{
    return _zeroPos;
}

void DsoSignal::set_zeroPos(int pos)
{
    if (enabled()) {
        double delta = _trig_vpos - _zeroPos;
        set_trig_vpos(get_trig_vpos() + pos - get_zeroPos());
        _zeroPos = min((double)max(pos - UpMargin, 0), get_view_rect().height()) * 1.0 / get_view_rect().height();
        _trig_vpos = min(max(_zeroPos + delta, 0+TrigMargin), 1-TrigMargin);

        update_zeroPos();
    }
}

void DsoSignal::set_zeroRate(double rate)
{
    _zeroPos = rate;
    update_zeroPos();
}

void DsoSignal::set_factor(uint64_t factor)
{
    if (enabled()) {
        GVariant* gvar;
        uint64_t prefactor = 0;
        gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_FACTOR);
        if (gvar != NULL) {
            prefactor = g_variant_get_uint64(gvar);
            g_variant_unref(gvar);
        } else {
            qDebug() << "ERROR: config_get SR_CONF_FACTOR failed.";
            return;
        }
        if (prefactor != factor) {
            _dev_inst->set_config(_probe, NULL, SR_CONF_FACTOR,
                                  g_variant_new_uint64(factor));
            _vDial->set_factor(factor);
            _view->set_need_update(true);
            _view->update();
        }
    }
}

uint64_t DsoSignal::get_factor()
{
    GVariant* gvar;
    uint64_t factor;
    gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_FACTOR);
    if (gvar != NULL) {
        factor = g_variant_get_uint64(gvar);
        g_variant_unref(gvar);
        return factor;
    } else {
        qDebug() << "ERROR: config_get SR_CONF_FACTOR failed.";
        return 1;
    }
}

void DsoSignal::update_zeroPos()
{
    if (strcmp(_dev_inst->dev_inst()->driver->name, "DSCope") == 0) {
        //double vpos_off = (0.5 - _zeroPos) * _vDial->get_value() * DS_CONF_DSO_VDIVS;
        double vpos_off = (0.5 - (get_zeroPos() - UpMargin) * 1.0/get_view_rect().height()) * _vDial->get_value() * DS_CONF_DSO_VDIVS;
        _dev_inst->set_config(_probe, NULL, SR_CONF_VPOS,
                              g_variant_new_double(vpos_off));
    }
}

QRectF DsoSignal::get_view_rect() const
{
    assert(_view);
    return QRectF(0, UpMargin,
                  _view->viewport()->width() - RightMargin,
                  _view->viewport()->height() - UpMargin - DownMargin);
}

void DsoSignal::paint_back(QPainter &p, int left, int right)
{
    assert(_view);

    int i, j;
    const int height = get_view_rect().height();
    const int width = right - left;

    p.setPen(Qt::NoPen);
    p.setBrush(Trace::dsBack);
    p.drawRect(left, UpMargin, width, height);

    p.setPen(Trace::dsLightBlue);
    p.drawLine(left, UpMargin/2, left + width, UpMargin/2);
    const uint64_t sample_len = _dev_inst->get_sample_limit();
    const double samplerate = _dev_inst->get_sample_rate();
    const double samples_per_pixel = samplerate * _view->scale();
    const double shown_rate = min(samples_per_pixel * width * 1.0 / sample_len, 1.0);
    const double start_time = _data->get_start_time();
    const double start = samplerate * (_view->offset() - start_time);
    const double shown_offset = min(start / sample_len, 1.0) * width;
    const double shown_len = shown_rate * width;
    const QPointF left_edge[] =  {QPoint(shown_offset + 3, UpMargin/2 - 6),
                                  QPoint(shown_offset, UpMargin/2 - 6),
                                  QPoint(shown_offset, UpMargin/2 + 6),
                                  QPoint(shown_offset + 3, UpMargin/2 + 6)};
    const QPointF right_edge[] = {QPoint(shown_offset + shown_len - 3, UpMargin/2 - 6),
                                  QPoint(shown_offset + shown_len , UpMargin/2 - 6),
                                  QPoint(shown_offset + shown_len , UpMargin/2 + 6),
                                  QPoint(shown_offset + shown_len - 3, UpMargin/2 + 6)};
    p.drawPolyline(left_edge, countof(left_edge));
    p.drawPolyline(right_edge, countof(right_edge));
    p.setBrush(Trace::dsBlue);
    p.drawRect(shown_offset, UpMargin/2 - 3, shown_len, 6);

    QPen pen(Signal::dsFore);
    pen.setStyle(Qt::DotLine);
    p.setPen(pen);
    const double spanY =height * 1.0 / DS_CONF_DSO_VDIVS;
    for (i = 1; i <= DS_CONF_DSO_VDIVS; i++) {
        const double posY = spanY * i + UpMargin;
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
        p.drawLine(posX, UpMargin,
                   posX, height + UpMargin);
        const double miniSpanX = spanX / 5;
        for (j = 1; j < 5; j++) {
            p.drawLine(posX - miniSpanX * j, height / 2.0f + UpMargin - 5,
                       posX - miniSpanX * j, height / 2.0f + UpMargin + 5);
        }
    }
}

void DsoSignal::paint_mid(QPainter &p, int left, int right)
{
    assert(_data);
    assert(_view);
    assert(right >= left);

    if (enabled()) {
        const int height = get_view_rect().height();
        const int width = right - left;

        const int y = get_zeroPos() + height * 0.5;
        const double scale = _view->scale();
        assert(scale > 0);
        const double offset = _view->offset();

        const deque< boost::shared_ptr<pv::data::DsoSnapshot> > &snapshots =
            _data->get_snapshots();
        if (snapshots.empty())
            return;

        if (_view->session().get_capture_state() == SigSession::Running)
            _scale = height * 1.0f / 256;
        const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot =
            snapshots.front();

        const uint16_t number_channels = snapshot->get_channel_num();
        if ((strcmp(_dev_inst->dev_inst()->driver->name, "DSLogic") == 0) &&
            (unsigned int)get_index() >= number_channels)
            return;

        const double pixels_offset = offset / scale;
        //const double samplerate = _data->samplerate();
        const double samplerate = _dev_inst->get_sample_rate();
        const double start_time = _data->get_start_time();
        const int64_t last_sample = max((int64_t)(snapshot->get_sample_count() - 1), (int64_t)0);
        const double samples_per_pixel = samplerate * scale;
        const double start = samplerate * (offset - start_time);
        const double end = start + samples_per_pixel * width;

        const int64_t start_sample = min(max((int64_t)floor(start),
            (int64_t)0), last_sample);
        const int64_t end_sample = min(max((int64_t)ceil(end) + 1,
            (int64_t)0), last_sample);

        if (samples_per_pixel < EnvelopeThreshold) {
            snapshot->enable_envelope(false);
            paint_trace(p, snapshot, y, left,
                start_sample, end_sample,
                pixels_offset, samples_per_pixel, number_channels);
        } else {
            snapshot->enable_envelope(true);
            paint_envelope(p, snapshot, y, left,
                start_sample, end_sample,
                pixels_offset, samples_per_pixel, number_channels);
        }

        paint_measure(p);
    }
}

void DsoSignal::paint_fore(QPainter &p, int left, int right)
{
    assert(_view);

    QPen pen(Signal::dsGray);
    pen.setStyle(Qt::DotLine);
    p.setPen(pen);
    p.drawLine(left, get_zeroPos(), right, get_zeroPos());

    if(enabled()) {
        const QPointF mouse_point = _view->hover_point();
        const QRectF label_rect = get_trig_rect(left, right);
        const bool hover = label_rect.contains(mouse_point);

        // Paint the trig line
        const QPointF points[] = {
            QPointF(right, get_trig_vpos()),
            label_rect.topLeft(),
            label_rect.topRight(),
            label_rect.bottomRight(),
            label_rect.bottomLeft()
        };

        p.setPen(Qt::transparent);
        p.setBrush(hover ? _colour.dark() : _colour);
        p.drawPolygon(points, countof(points));

        // paint the trig voltage
        int trigp = get_trig_vpos();
        float t_vol = (_zeroPos - _trig_vpos) * _vDial->get_value() * _vDial->get_factor() * DS_CONF_DSO_VDIVS;
        QString t_vol_s = (_vDial->get_value() >= 500) ? QString::number(t_vol/1000.0f, 'f', 2)+"V" : QString::number(t_vol, 'f', 2)+"mV";
        int vol_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
                                       Qt::AlignLeft | Qt::AlignTop, t_vol_s).width();
        const QRectF t_vol_rect = QRectF(right-vol_width, trigp-10, vol_width, 20);
        p.setPen(Qt::white);
        p.drawText(t_vol_rect, Qt::AlignRight | Qt::AlignVCenter, t_vol_s);

        // paint the _trig_vpos line
        p.setPen(QPen(_colour, 1, Qt::DotLine));
        p.drawLine(left, trigp, right - p.boundingRect(t_vol_rect, Qt::AlignLeft, t_vol_s).width(), trigp);

        // Paint the text
        p.setPen(Qt::white);
        p.drawText(label_rect, Qt::AlignCenter | Qt::AlignVCenter, "T");
    }
}

QRectF DsoSignal::get_trig_rect(int left, int right) const
{
    (void)left;

    return QRectF(right + SquareWidth / 2,
                  get_trig_vpos() - SquareWidth / 2,
                  SquareWidth, SquareWidth);
}

void DsoSignal::paint_trace(QPainter &p,
    const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot,
    int y, int left, const int64_t start, const int64_t end,
    const double pixels_offset, const double samples_per_pixel, uint64_t num_channels)
{
    (void)y;

    const int64_t sample_count = end - start + 1;

    if (sample_count > 0) {
        const uint8_t *const samples = snapshot->get_samples(start, end, get_index());
        assert(samples);

        p.setPen(_colour);
        //p.setPen(QPen(_colour, 3, Qt::SolidLine));

        QPointF *points = new QPointF[sample_count];
        QPointF *point = points;

        float top = get_view_rect().top();
        float bottom = get_view_rect().bottom();
        float zeroP = _zeroPos * get_view_rect().height() + top;;
        if (strcmp(_dev_inst->dev_inst()->driver->name, "DSCope") == 0 &&
            _view->session().get_capture_state() == SigSession::Running)
            _zero_off = _zeroPos * 255;
        float x = (start / samples_per_pixel - pixels_offset) + left;
        double  pixels_per_sample = 1.0/samples_per_pixel;
        uint8_t offset;
        int64_t sample_end = sample_count*num_channels;
        for (int64_t sample = 0; sample < sample_end; sample+=num_channels) {
            //const float x = (sample / samples_per_pixel - pixels_offset) + left;
            //uint8_t offset = samples[(sample - start)*num_channels];

            //offset = samples[(sample - start)*num_channels];
            offset = samples[sample];
            const float y = min(max(top, zeroP + (offset - _zero_off) * _scale), bottom);
            *point++ = QPointF(x, y);
            x += pixels_per_sample;
            //*point++ = QPointF(x, top + offset);
        }

        p.drawPolyline(points, point - points);

        //delete[] samples;
        delete[] points;
    }
}

void DsoSignal::paint_envelope(QPainter &p,
    const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot,
    int y, int left, const int64_t start, const int64_t end,
    const double pixels_offset, const double samples_per_pixel, uint64_t num_channels)
{
	using namespace Qt;
    using pv::data::DsoSnapshot;

    (void)y;

    DsoSnapshot::EnvelopeSection e;
    const uint16_t index = get_index() % num_channels;
    snapshot->get_envelope_section(e, start, end, samples_per_pixel, index);

	if (e.length < 2)
		return;

    p.setPen(QPen(NoPen));
    //p.setPen(QPen(_colour, 2, Qt::SolidLine));
    p.setBrush(_colour);

	QRectF *const rects = new QRectF[e.length];
	QRectF *rect = rects;
    float top = get_view_rect().top();
    float bottom = get_view_rect().bottom();
    float zeroP = _zeroPos * get_view_rect().height() + top;
    if (strcmp(_dev_inst->dev_inst()->driver->name, "DSCope") == 0 &&
        _view->session().get_capture_state() == SigSession::Running)
        _zero_off = _zeroPos * 255;
    for(uint64_t sample = 0; sample < e.length-1; sample++) {
		const float x = ((e.scale * sample + e.start) /
			samples_per_pixel - pixels_offset) + left;
        const DsoSnapshot::EnvelopeSample *const s =
			e.samples + sample;

		// We overlap this sample with the next so that vertical
		// gaps do not appear during steep rising or falling edges
        const float b = min(max(top, ((max(s->max, (s+1)->min) - _zero_off) * _scale + zeroP)), bottom);
        const float t = min(max(top, ((min(s->min, (s+1)->max) - _zero_off) * _scale + zeroP)), bottom);

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

const std::vector< std::pair<uint64_t, bool> > DsoSignal::cur_edges() const
{

}

void DsoSignal::paint_type_options(QPainter &p, int right, bool hover, int action)
{
    int y = get_y();
    const QRectF vDial_rect = get_rect("vDial", y, right);
    const QRectF x1_rect = get_rect("x1", y, right);
    const QRectF x10_rect = get_rect("x10", y, right);
    const QRectF x100_rect = get_rect("x100", y, right);
    const QRectF hDial_rect = get_rect("hDial", y, right);
    const QRectF acdc_rect = get_rect("acdc", y, right);
    const QRectF chEn_rect = get_rect("chEn", y, right);

    QColor vDial_color = _vDialActive ? dsActive : dsDisable;
    QColor hDial_color = _hDialActive ? dsActive : dsDisable;
    _vDial->paint(p, vDial_rect, vDial_color);
    _hDial->paint(p, hDial_rect, hDial_color);

    p.setPen(Qt::transparent);
    p.setBrush((hover && action == CHEN) ? _colour.darker() : _colour);
    p.drawRect(chEn_rect);
    p.setPen(Qt::white);
    p.drawText(chEn_rect, Qt::AlignCenter | Qt::AlignVCenter, enabled() ? tr("EN") : tr("DIS"));

    p.setPen(Qt::transparent);
    p.setBrush(enabled() ? ((hover && action == ACDC) ? _colour.darker() : _colour) : dsDisable);
    p.drawRect(acdc_rect);
    p.setPen(Qt::white);
    p.drawText(acdc_rect, Qt::AlignCenter | Qt::AlignVCenter, (_acCoupling == SR_GND_COUPLING) ? tr("GND") :
                                                              (_acCoupling == SR_DC_COUPLING) ? tr("DC") : tr("AC"));

    // paint the probe factor selector
    GVariant* gvar;
    uint64_t factor;
    gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_FACTOR);
    if (gvar != NULL) {
        factor = g_variant_get_uint64(gvar);
        g_variant_unref(gvar);
    } else {
        qDebug() << "ERROR: config_get SR_CONF_FACTOR failed.";
        return;
    }

    p.setPen(Qt::white);
    p.setBrush((enabled() && (factor == 100)) ? ((hover && action == X100) ? _colour.darker() : _colour)  : ((hover && action == X100) ? _colour.darker() : dsDisable));
    p.drawRect(x100_rect);
    p.drawText(x100_rect, Qt::AlignLeft | Qt::AlignVCenter, "x100");

    p.setBrush((enabled() && (factor == 10)) ? ((hover && action == X10) ? _colour.darker() : _colour)  : ((hover && action == X10) ? _colour.darker() : dsDisable));
    p.drawRect(x10_rect);
    p.drawText(x10_rect, Qt::AlignLeft | Qt::AlignVCenter, "x10");

    p.setBrush((enabled() && (factor == 1)) ? ((hover && action == X1) ? _colour.darker() : _colour)  : ((hover && action == X1) ? _colour.darker() : dsDisable));
    p.drawRect(x1_rect);
    p.drawText(x1_rect, Qt::AlignLeft | Qt::AlignVCenter, "x1");
}

void DsoSignal::paint_measure(QPainter &p)
{
    sr_status status;
    int index = get_index();
    const int st_begin = (index == 0) ? SR_STATUS_CH0_BEGIN : SR_STATUS_CH1_BEGIN;
    const int st_end = (index == 0) ? SR_STATUS_CH0_END : SR_STATUS_CH1_END;
    if (sr_status_get(_dev_inst->dev_inst(), &status, st_begin, st_end) == SR_OK) {
        _max = (index == 0) ? status.ch0_max : status.ch1_max;
        _min = (index == 0) ? status.ch0_min : status.ch1_min;
        const uint64_t period = (index == 0) ? status.ch0_period : status.ch1_period;
        const uint32_t count  = (index == 0) ? status.ch0_pcnt : status.ch1_pcnt;
        double value_max = (_zero_off - _min) * _scale * _vDial->get_value() * _vDial->get_factor() * DS_CONF_DSO_VDIVS / get_view_rect().height();
        double value_min = (_zero_off - _max) * _scale * _vDial->get_value() * _vDial->get_factor() * DS_CONF_DSO_VDIVS / get_view_rect().height();
        _period = (count == 0) ? period * 10.0 : period * 10.0 / count;
        const int channel_count = _view->session().get_ch_num(SR_CHANNEL_DSO);
        uint64_t sample_rate = _dev_inst->get_sample_rate();
        _period = _period * 200.0 / (channel_count * sample_rate * 1.0 / SR_MHZ(1));
        QString max_string = abs(value_max) > 1000 ? QString::number(value_max/1000.0, 'f', 2) + "V" : QString::number(value_max, 'f', 2) + "mV";
        QString min_string = abs(value_min) > 1000 ? QString::number(value_min/1000.0, 'f', 2) + "V" : QString::number(value_min, 'f', 2) + "mV";
        QString period_string = abs(_period) > 1000000000 ? QString::number(_period/1000000000, 'f', 2) + "S" :
                                abs(_period) > 1000000 ? QString::number(_period/1000000, 'f', 2) + "mS" :
                                abs(_period) > 1000 ? QString::number(_period/1000, 'f', 2) + "uS" : QString::number(_period, 'f', 2) + "nS";
        QString freq_string = abs(_period) > 1000000 ? QString::number(1000000000/_period, 'f', 2) + "Hz" :
                              abs(_period) > 1000 ? QString::number(1000000/_period, 'f', 2) + "kHz" : QString::number(1000/_period, 'f', 2) + "MHz";
        p.setPen(_colour);
        p.drawText(QRectF(0, 100*index + UpMargin, get_view_rect().width()*0.9, 20), Qt::AlignRight | Qt::AlignVCenter, tr("Max: ")+max_string+"        ");
        p.drawText(QRectF(0, 100*index + UpMargin + 20, get_view_rect().width()*0.9, 20), Qt::AlignRight | Qt::AlignVCenter, tr("Min: ")+min_string+"        ");
        p.drawText(QRectF(0, 100*index + UpMargin + 40, get_view_rect().width()*0.9, 20), Qt::AlignRight | Qt::AlignVCenter, tr("Period: ")+period_string+"        ");
        p.drawText(QRectF(0, 100*index + UpMargin + 60, get_view_rect().width()*0.9, 20), Qt::AlignRight | Qt::AlignVCenter, tr("Frequency: ")+freq_string+"        ");

        if (_autoV) {
            const uint8_t vscale = abs(_max - _min);
            if (_max == 0xff || _min == 0x00 || vscale > 0xCC) {
                go_vDialNext();
            } else if (vscale > 0x33) {
                _autoV = false;
            } else {
                go_vDialPre();
            }
        }

        bool setted = false;
        if (_autoH) {
            const vector< boost::shared_ptr<Trace> > traces(_view->get_traces());
            const double upPeriod = get_hDialValue() * DS_CONF_DSO_HDIVS * 0.8;
            const double dnPeriod = get_hDialValue() * DS_CONF_DSO_HDIVS * 0.2;
            if (_period > upPeriod) {
                boost::shared_ptr<view::DsoSignal> dsoSig;
                BOOST_FOREACH(const boost::shared_ptr<Trace> t, traces) {
                    if (dsoSig = dynamic_pointer_cast<view::DsoSignal>(t)) {
                        dsoSig->go_hDialNext(setted);
                        setted = true;
                    }
                }
            } else if (_period > dnPeriod) {
                _autoH = false;
            } else {
                boost::shared_ptr<view::DsoSignal> dsoSig;
                BOOST_FOREACH(const boost::shared_ptr<Trace> t, traces) {
                    if (dsoSig = dynamic_pointer_cast<view::DsoSignal>(t)) {
                        dsoSig->go_hDialPre(setted);
                        setted = true;
                    }
                }
            }
        }
    }
    _view->update();
}

void DsoSignal::auto_set()
{
    if (_autoV | _autoH) {
        _autoV = false;
        _autoH = false;
    } else {
        _autoV = true;
        _autoH = true;
    }
}

bool DsoSignal::measure(const QPointF &p)
{
    _hover_en = false;
    if (!enabled())
        return false;

    const deque< boost::shared_ptr<pv::data::DsoSnapshot> > &snapshots =
        _data->get_snapshots();
    if (snapshots.empty())
        return false;

    const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot =
        snapshots.front();
    if (snapshot->buf_null())
        return false;

    const double scale = _view->scale();
    assert(scale > 0);
    const double offset = _view->offset();
    const double pixels_offset = offset / scale;
    const double samplerate = _dev_inst->get_sample_rate();
    const double samples_per_pixel = samplerate * scale;

    _hover_index = floor((p.x() + pixels_offset) * samples_per_pixel+0.5);
    if (_hover_index >= snapshot->get_sample_count())
        return false;

    uint64_t pre_index;
    uint64_t nxt_index;
    if (_hover_index > 0)
        pre_index = _hover_index - 1;
    else
        pre_index = _hover_index;
    if (_hover_index < snapshot->get_sample_count() - 1)
        nxt_index = _hover_index + 1;
    else
        nxt_index = _hover_index;
    const uint8_t pre_sample = *snapshot->get_samples(pre_index, pre_index, get_index());
    const uint8_t cur_sample = *snapshot->get_samples(_hover_index, _hover_index, get_index());
    const uint8_t nxt_sample = *snapshot->get_samples(nxt_index, nxt_index, get_index());

    _hover_value = (_zero_off - cur_sample) * _scale * _vDial->get_value() * _vDial->get_factor() * DS_CONF_DSO_VDIVS / get_view_rect().height();

    float top = get_view_rect().top();
    float bottom = get_view_rect().bottom();
    float zeroP = _zeroPos * get_view_rect().height() + top;
    float pre_x = (pre_index / samples_per_pixel - pixels_offset);
    const float pre_y = min(max(top, zeroP + (pre_sample - _zero_off)* _scale), bottom);
    float x = (_hover_index / samples_per_pixel - pixels_offset);
    const float y = min(max(top, zeroP + (cur_sample - _zero_off)* _scale), bottom);
    float nxt_x = (nxt_index / samples_per_pixel - pixels_offset);
    const float nxt_y = min(max(top, zeroP + (nxt_sample - _zero_off)* _scale), bottom);
    const QRectF slope_rect = QRectF(QPointF(pre_x - 10, pre_y - 10), QPointF(nxt_x + 10, nxt_y + 10));
    if (abs(y-p.y()) < 20 || slope_rect.contains(p)) {
        _hover_point = QPointF(x, y);
        _hover_en = true;
        return true;
    } else {
        return false;
    }
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

} // namespace view
} // namespace pv
