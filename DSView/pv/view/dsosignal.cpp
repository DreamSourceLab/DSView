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
    "mv",
    "v",
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
    _vDialActive(false),
    _autoV(false),
    _autoH(false),
    _hover_en(false),
    _hover_index(0),
    _hover_point(QPointF(0, 0)),
    _hover_value(0),
    _ms_gear_hover(false),
    _ms_show_hover(false)
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
            while(NULL != (gvar = g_variant_iter_next_value(&iter)))
                vValue.push_back(g_variant_get_uint64(gvar));
            g_variant_unref(gvar);
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

void DsoSignal::set_viewport(pv::view::Viewport *viewport)
{
    Trace::set_viewport(viewport);

    const double ms_left = get_view_rect().right() - (MS_RectWidth + MS_RectMargin) * (get_index() + 1);
    const double ms_top = get_view_rect().top() + 5;
    for (int i = DSO_MS_BEGIN; i < DSO_MS_END; i++)
        _ms_rect[i] = QRect(ms_left, ms_top + MS_RectHeight * i, MS_RectWidth, MS_RectHeight);
    _ms_gear_rect = QRect(ms_left+MS_RectRad, ms_top+MS_RectRad, MS_IconSize, MS_IconSize);
    _ms_show_rect = QRect(ms_left+MS_RectWidth-MS_RectRad-MS_IconSize, ms_top+MS_RectRad, MS_IconSize, MS_IconSize);

}

void DsoSignal::set_scale(int height)
{
    _scale = height * 1.0f / (1 << _bits);
}

float DsoSignal::get_scale()
{
    return _scale;
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

bool DsoSignal::go_vDialPre()
{
    if (enabled() && !_vDial->isMin()) {
        if (_view->session().get_capture_state() == SigSession::Running)
            _view->session().refresh(RefreshShort);
        const double pre_vdiv = _vDial->get_value();
        _vDial->set_sel(_vDial->get_sel() - 1);
        _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_VDIV,
                              g_variant_new_uint64(_vDial->get_value()));
        if (_view->session().get_capture_state() == SigSession::Stopped)
            _scale *= pre_vdiv/_vDial->get_value();
        update_vpos();
        _view->update_calibration();
        _view->set_update(_viewport, true);
        _view->update();
        return true;
    } else {
        if (_autoV)
            autoV_end();
        return false;
    }
}

bool DsoSignal::go_vDialNext()
{
    if (enabled() && !_vDial->isMax()) {
        if (_view->session().get_capture_state() == SigSession::Running)
            _view->session().refresh(RefreshShort);
        const double pre_vdiv = _vDial->get_value();
        _vDial->set_sel(_vDial->get_sel() + 1);
        _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_VDIV,
                              g_variant_new_uint64(_vDial->get_value()));
        if (_view->session().get_capture_state() == SigSession::Stopped)
            _scale *= pre_vdiv/_vDial->get_value();
        update_vpos();
		_view->update_calibration();
        _view->set_update(_viewport, true);
        _view->update();
        return true;
    } else {
        if (_autoV)
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
        if (strncmp(_dev_inst->name().toLocal8Bit(), "virtual", 7))
            return false;
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
    double vpos;
    gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_PROBE_VPOS);
    if (gvar != NULL) {
        vpos = g_variant_get_double(gvar);
        g_variant_unref(gvar);
    } else {
        qDebug() << "ERROR: config_get SR_CONF_PROBE_VPOS failed.";
        return false;
    }
    _zero_vrate = min(max((0.5 - vpos / (_vDial->get_value() * DS_CONF_DSO_VDIVS)), 0.0), 1.0);
    if (_dev_inst->name().contains("virtual"))
        _hw_offset = _zero_vrate * ((1 << _bits) - 1);

    // -- trig_value
    gvar = _dev_inst->get_config(_probe, NULL, SR_CONF_TRIGGER_VALUE);
    if (gvar != NULL) {
        _trig_value = g_variant_get_byte(gvar);
        _trig_delta = get_trig_vrate() - _zero_vrate;
        g_variant_unref(gvar);
    } else {
        qDebug() << "ERROR: config_get SR_CONF_TRIGGER_VALUE failed.";
        if (strncmp(_dev_inst->name().toLocal8Bit(), "virtual", 7))
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

    // -- vpos
    double vpos_off = (0.5 - (get_zero_vpos() - UpMargin) * 1.0/get_view_rect().height()) * _vDial->get_value() * DS_CONF_DSO_VDIVS;
    ret = _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_VPOS,
                                g_variant_new_double(vpos_off));

    // -- trig_value
    _dev_inst->set_config(_probe, NULL, SR_CONF_TRIGGER_VALUE,
                          g_variant_new_byte(_trig_value));

    return ret;
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

int DsoSignal::get_trig_vpos() const
{
    return get_trig_vrate() * get_view_rect().height() + UpMargin;
}

double DsoSignal::get_trig_vrate() const
{
    if (_dev_inst->name() == "DSLogic")
        return (_trig_value - (1 << (_bits - 1)))* 1.0 / ((1 << _bits) - 1.0) + _zero_vrate;
    else
        return _trig_value * 1.0 / ((1 << _bits) - 1.0);
}

void DsoSignal::set_trig_vpos(int pos, bool delta_change)
{
    assert(_view);
    if (enabled()) {
        double delta = min(max(pos - UpMargin, 0), get_view_rect().height()) * 1.0 / get_view_rect().height();
        if (_dev_inst->name() == "DSLogic") {
            delta = delta - _zero_vrate;
            delta = min(delta, 0.5);
            delta = max(delta, -0.5);
            _trig_value = delta * ((1 << _bits) -1) + (1 << (_bits - 1));
        } else {
            _trig_value = delta * ((1 << _bits) -1) + 0.5;
        }
        int margin = TrigMargin;
        _trig_value = std::min(std::max(_trig_value, margin), ((1 << _bits) - margin - 1));
        if (delta_change)
            _trig_delta = get_trig_vrate() - _zero_vrate;
        _dev_inst->set_config(_probe, NULL, SR_CONF_TRIGGER_VALUE,
                              g_variant_new_byte(_trig_value));
    }
}

void DsoSignal::set_trig_vrate(double rate)
{
    double delta = rate;
    if (_dev_inst->name() == "DSLogic") {
        delta = delta - _zero_vrate;
        delta = min(delta, 0.5);
        delta = max(delta, -0.5);
        _trig_value = delta * ((1 << _bits) - 1) + (1 << (_bits - 1));
    } else {
        _trig_value = delta * ((1 << _bits) - 1) + 0.5;
    }
    _trig_delta = get_trig_vrate() - _zero_vrate;
    _dev_inst->set_config(_probe, NULL, SR_CONF_TRIGGER_VALUE,
                          g_variant_new_byte(_trig_value));
}

int DsoSignal::get_zero_vpos() const
{
    return _zero_vrate * get_view_rect().height() + UpMargin;
}

double DsoSignal::get_zero_vrate()
{
    return _zero_vrate;
}

double DsoSignal::get_hw_offset()
{
    return _hw_offset;
}

void DsoSignal::set_zero_vpos(int pos)
{
    if (enabled()) {
        double delta = _trig_delta* get_view_rect().height();
        set_zero_vrate(min(max(pos - UpMargin, 0), get_view_rect().height()) * 1.0 / get_view_rect().height(), false);
        set_trig_vpos(get_zero_vpos() + delta, false);
    }
}

void DsoSignal::set_zero_vrate(double rate, bool force_update)
{
    _zero_vrate = rate;
    update_vpos();

    if (!_dev_inst->name().contains("virtual") &&
        (force_update ||
        _view->session().get_capture_state() == SigSession::Running)) {
        if (_dev_inst->name() == "DSLogic")
            _hw_offset = 0x80;
        else
            _hw_offset = _zero_vrate * ((1 << _bits) - 1);
    }
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

void DsoSignal::set_ms_show(bool show)
{
    _probe->ms_show = show;
    _view->set_update(_viewport, true);
}

bool DsoSignal::get_ms_show() const
{
    return _probe->ms_show;
}

bool DsoSignal::get_ms_show_hover() const
{
    return _ms_show_hover;
}

bool DsoSignal::get_ms_gear_hover() const
{
    return _ms_gear_hover;
}

void DsoSignal::set_ms_en(int index, bool en)
{
    assert(index > DSO_MS_BEGIN);
    assert(index < DSO_MS_END);

    _probe->ms_en[index] = en;
}

bool DsoSignal::get_ms_en(int index) const
{
    assert(index > DSO_MS_BEGIN);
    assert(index < DSO_MS_END);

    return _probe->ms_en[index];
}

QString DsoSignal::get_ms_string(int index) const
{
    assert(index > DSO_MS_BEGIN);
    assert(index < DSO_MS_END);

    switch(index) {
        case DSO_MS_FREQ: return "Frequency";
        case DSO_MS_PERD: return "Period";
        case DSO_MS_VMAX: return "Vmax";
        case DSO_MS_VMIN: return "Vmin";
        case DSO_MS_VRMS: return "Vrms";
        case DSO_MS_VMEA: return "Vmean";
        case DSO_MS_VP2P: return "Vp-p";
        default: return "Error: Out of Bounds";
    }
}

void DsoSignal::update_vpos()
{
    double vpos_off = (0.5 - _zero_vrate) * _vDial->get_value() * DS_CONF_DSO_VDIVS;
    _dev_inst->set_config(_probe, NULL, SR_CONF_PROBE_VPOS,
                          g_variant_new_double(vpos_off));
}

QRect DsoSignal::get_view_rect() const
{
    assert(_viewport);
    return QRect(0, UpMargin,
                  _viewport->width() - RightMargin,
                  _viewport->height() - UpMargin - DownMargin);
}

void DsoSignal::paint_back(QPainter &p, int left, int right)
{
    assert(_view);

    int i, j;
    const int height = get_view_rect().height();
    const int width = right - left;

    QPen solidPen(Signal::dsFore);
    solidPen.setStyle(Qt::SolidLine);
    p.setPen(solidPen);
    p.setBrush(Trace::dsBack);
    p.drawRect(left, UpMargin, width, height);

    p.setPen(Trace::dsLightBlue);
    p.drawLine(left, UpMargin/2, left + width, UpMargin/2);
    const uint64_t sample_len = _view->session().cur_samplelimits();
    const double samplerate = _view->session().cur_samplerate();
    const double samples_per_pixel = samplerate * _view->scale();
    const double shown_rate = min(samples_per_pixel * width * 1.0 / sample_len, 1.0);
    const double start = _view->offset() * samples_per_pixel;
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

    QPen dashPen(Signal::dsFore);
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
}

void DsoSignal::paint_mid(QPainter &p, int left, int right)
{
    assert(_data);
    assert(_view);
    assert(right >= left);

    if (enabled()) {
        const float top = get_view_rect().top();
        const int height = get_view_rect().height();
        const int width = right - left;
        const float zeroY = _zero_vrate * height + top;

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

        if (!snapshot->has_data(get_index()))
            return;

        const uint16_t number_channels = snapshot->get_channel_num();
        const double pixels_offset = offset;
        //const double samplerate = _data->samplerate();
        //const double samplerate = _dev_inst->get_sample_rate();
        const double samplerate = _view->session().cur_samplerate();
        const int64_t last_sample = max((int64_t)(snapshot->get_sample_count() - 1), (int64_t)0);
        const double samples_per_pixel = samplerate * scale;
        const double start = offset * samples_per_pixel;
        const double end = start + samples_per_pixel * width;

        const int64_t start_sample = min(max((int64_t)floor(start),
            (int64_t)0), last_sample);
        const int64_t end_sample = min(max((int64_t)ceil(end) + 1,
            (int64_t)0), last_sample);

        if (samples_per_pixel < EnvelopeThreshold) {
            snapshot->enable_envelope(false);
            paint_trace(p, snapshot, zeroY, left,
                start_sample, end_sample,
                pixels_offset, samples_per_pixel, number_channels);
        } else {
            snapshot->enable_envelope(true);
            paint_envelope(p, snapshot, zeroY, left,
                start_sample, end_sample,
                pixels_offset, samples_per_pixel, number_channels);
        }
    }
}

void DsoSignal::paint_fore(QPainter &p, int left, int right)
{
    assert(_view);

    bool antialiasing = p.Antialiasing;
    p.setRenderHint(QPainter::Antialiasing, false);

    QPen pen(Signal::dsGray);
    pen.setStyle(Qt::DotLine);
    p.setPen(pen);
    p.drawLine(left, get_zero_vpos(), right, get_zero_vpos());

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
        p.setBrush(_colour);
        p.drawPolygon(points, countof(points));

        p.setPen(Qt::white);
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
        int trigp = get_trig_vpos();
        float t_vol = (_zero_vrate - get_trig_vrate()) * _vDial->get_value() * _vDial->get_factor() * DS_CONF_DSO_VDIVS;
        QString t_vol_s = (_vDial->get_value() >= 500) ? QString::number(t_vol/1000.0f, 'f', 2)+"V" : QString::number(t_vol, 'f', 2)+"mV";
        int vol_width = p.boundingRect(0, 0, INT_MAX, INT_MAX,
                                       Qt::AlignLeft | Qt::AlignTop, t_vol_s).width();
        const QRectF t_vol_rect = QRectF(right-vol_width, trigp-10, vol_width, 20);
        p.setPen(Qt::white);
        p.drawText(t_vol_rect, Qt::AlignRight | Qt::AlignVCenter, t_vol_s);

        // paint the _trig_vpos line
        if (_view->get_dso_trig_moved()) {
            p.setPen(QPen(_colour, 1, Qt::DotLine));
            p.drawLine(left, trigp, right - p.boundingRect(t_vol_rect, Qt::AlignLeft, t_vol_s).width(), trigp);
        }

        // Paint the text
        p.setPen(Qt::white);
        p.drawText(label_rect, Qt::AlignCenter | Qt::AlignVCenter, "T");

        // Paint measure
        paint_measure(p);
    }

    p.setRenderHint(QPainter::Antialiasing, antialiasing);
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
    int zeroY, int left, const int64_t start, const int64_t end,
    const double pixels_offset, const double samples_per_pixel, uint64_t num_channels)
{
    const int64_t sample_count = end - start + 1;

    if (sample_count > 0) {
        const uint8_t *const samples = snapshot->get_samples(start, end, get_index());
        assert(samples);

        QColor trace_colour = _colour;
        trace_colour.setAlpha(150);
        p.setPen(trace_colour);

        QPointF *points = new QPointF[sample_count];
        QPointF *point = points;

        float top = get_view_rect().top();
        float bottom = get_view_rect().bottom();
        float x = (start / samples_per_pixel - pixels_offset) + left;
        double  pixels_per_sample = 1.0/samples_per_pixel;
        uint8_t offset;
        int64_t sample_end = sample_count*num_channels;
        for (int64_t sample = 0; sample < sample_end; sample+=num_channels) {
            //const float x = (sample / samples_per_pixel - pixels_offset) + left;
            //uint8_t offset = samples[(sample - start)*num_channels];

            //offset = samples[(sample - start)*num_channels];
            offset = samples[sample];
            const float y = min(max(top, zeroY + (offset - _hw_offset) * _scale), bottom);
            *point++ = QPointF(x, y);
            x += pixels_per_sample;
            //*point++ = QPointF(x, top + offset);
        }

        p.drawPolyline(points, point - points);
        p.eraseRect(get_view_rect().right()+1, get_view_rect().top(),
                    _view->viewport()->width() - get_view_rect().width(), get_view_rect().height());

        //delete[] samples;
        delete[] points;
    }
}

void DsoSignal::paint_envelope(QPainter &p,
    const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot,
    int zeroY, int left, const int64_t start, const int64_t end,
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
    envelope_colour.setAlpha(150);
    p.setBrush(envelope_colour);

	QRectF *const rects = new QRectF[e.length];
	QRectF *rect = rects;
    float top = get_view_rect().top();
    float bottom = get_view_rect().bottom();
    for(uint64_t sample = 0; sample < e.length-1; sample++) {
		const float x = ((e.scale * sample + e.start) /
			samples_per_pixel - pixels_offset) + left;
        const DsoSnapshot::EnvelopeSample *const s =
			e.samples + sample;

		// We overlap this sample with the next so that vertical
		// gaps do not appear during steep rising or falling edges
        const float b = min(max(top, ((max(s->max, (s+1)->min) - _hw_offset) * _scale + zeroY)), bottom);
        const float t = min(max(top, ((min(s->min, (s+1)->max) - _hw_offset) * _scale + zeroY)), bottom);

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

void DsoSignal::paint_type_options(QPainter &p, int right, const QPoint pt)
{
    int y = get_y();
    const QRectF vDial_rect = get_rect(DSO_VDIAL, y, right);
    const QRectF x1_rect = get_rect(DSO_X1, y, right);
    const QRectF x10_rect = get_rect(DSO_X10, y, right);
    const QRectF x100_rect = get_rect(DSO_X100, y, right);
    const QRectF acdc_rect = get_rect(DSO_ACDC, y, right);
    const QRectF chEn_rect = get_rect(DSO_CHEN, y, right);
    const QRectF auto_rect = get_rect(DSO_AUTO, y, right);

    _vDial->paint(p, vDial_rect, _colour, pt);

    p.setPen(Qt::transparent);
    p.setBrush(chEn_rect.contains(pt) ? _colour.darker() : _colour);
    p.drawRect(chEn_rect);
    p.setPen(Qt::white);
    p.drawText(chEn_rect, Qt::AlignCenter | Qt::AlignVCenter, enabled() ? tr("EN") : tr("DIS"));

    p.setPen(Qt::transparent);
    p.setBrush(enabled() ? (acdc_rect.contains(pt) ? _colour.darker() : _colour) : dsDisable);
    p.drawRect(acdc_rect);
    p.setPen(Qt::white);
    p.drawText(acdc_rect, Qt::AlignCenter | Qt::AlignVCenter, (_acCoupling == SR_GND_COUPLING) ? tr("GND") :
                                                              (_acCoupling == SR_DC_COUPLING) ? tr("DC") : tr("AC"));

    if (!_dev_inst->name().contains("virtual")) {
        p.setPen(Qt::transparent);
        p.setBrush(enabled() ? (auto_rect.contains(pt) ? _colour.darker() : _colour) : dsDisable);
        p.drawRect(auto_rect);
        p.setPen(Qt::white);
        p.drawText(auto_rect, Qt::AlignCenter | Qt::AlignVCenter, tr("AUTO"));
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
    p.setBrush((enabled() && (factor == 100)) ? (x100_rect.contains(pt) ? _colour.darker() : _colour)  : (x100_rect.contains(pt) ? _colour.darker() : dsDisable));
    p.drawRect(x100_rect);
    p.setBrush((enabled() && (factor == 10)) ? (x10_rect.contains(pt) ? _colour.darker() : _colour)  : (x10_rect.contains(pt) ? _colour.darker() : dsDisable));
    p.drawRect(x10_rect);
    p.setBrush((enabled() && (factor == 1)) ? (x1_rect.contains(pt) ? _colour.darker() : _colour)  : (x1_rect.contains(pt) ? _colour.darker() : dsDisable));
    p.drawRect(x1_rect);

    p.setPen(Qt::white);
    p.drawText(x100_rect, Qt::AlignCenter | Qt::AlignVCenter, "x100");
    p.drawText(x10_rect, Qt::AlignCenter | Qt::AlignVCenter, "x10");
    p.drawText(x1_rect, Qt::AlignCenter | Qt::AlignVCenter, "x1");
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
                go_vDialNext();
            else
                go_vDialPre();
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
            go_vDialPre();
        else if (shift < -0.5)
            go_vDialNext();
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

void DsoSignal::paint_measure(QPainter &p)
{
    sr_status status;
    int index = get_index();
    const int st_begin = (index == 0) ? SR_STATUS_CH0_BEGIN : SR_STATUS_CH1_BEGIN;
    const int st_end = (index == 0) ? SR_STATUS_CH0_END : SR_STATUS_CH1_END;
    if (sr_status_get(_dev_inst->dev_inst(), &status, false, st_begin, st_end) == SR_OK) {
        _max = (index == 0) ? status.ch0_max : status.ch1_max;
        _min = (index == 0) ? status.ch0_min : status.ch1_min;
        const uint64_t period = (index == 0) ? status.ch0_period : status.ch1_period;
        const uint32_t count  = (index == 0) ? status.ch0_pcnt : status.ch1_pcnt;
        double value_max = (_hw_offset - _min) * _scale * _vDial->get_value() * _vDial->get_factor() * DS_CONF_DSO_VDIVS / get_view_rect().height();
        double value_min = (_hw_offset - _max) * _scale * _vDial->get_value() * _vDial->get_factor() * DS_CONF_DSO_VDIVS / get_view_rect().height();
        double value_p2p = value_max - value_min;
        _period = (count == 0) ? 0 : period * 10.0 / count;
        const int channel_count = _view->session().get_ch_num(SR_CHANNEL_DSO);
        uint64_t sample_rate = _dev_inst->get_sample_rate();
        _period = _period * 200.0 / (channel_count * sample_rate * 1.0 / SR_MHZ(1));
        _ms_string[DSO_MS_VMAX] = tr("Vmax: ") + (abs(value_max) > 1000 ? QString::number(value_max/1000.0, 'f', 2) + "V" : QString::number(value_max, 'f', 2) + "mV");
        _ms_string[DSO_MS_VMIN] = tr("Vmin: ") + (abs(value_min) > 1000 ? QString::number(value_min/1000.0, 'f', 2) + "V" : QString::number(value_min, 'f', 2) + "mV");
        _ms_string[DSO_MS_PERD] = tr("Perd: ") + (abs(_period) > 1000000000 ? QString::number(_period/1000000000, 'f', 2) + "S" :
                                abs(_period) > 1000000 ? QString::number(_period/1000000, 'f', 2) + "mS" :
                                abs(_period) > 1000 ? QString::number(_period/1000, 'f', 2) + "uS" : QString::number(_period, 'f', 2) + "nS");
        _ms_string[DSO_MS_FREQ] = tr("Freq: ") + (abs(_period) > 1000000 ? QString::number(1000000000/_period, 'f', 2) + "Hz" :
                              abs(_period) > 1000 ? QString::number(1000000/_period, 'f', 2) + "kHz" : QString::number(1000/_period, 'f', 2) + "MHz");
        _ms_string[DSO_MS_VP2P] = tr("Vp-p: ") +  (abs(value_p2p) > 1000 ? QString::number(value_p2p/1000.0, 'f', 2) + "V" : QString::number(value_p2p, 'f', 2) + "mV");

        if (_probe->ms_show && _probe->ms_en[DSO_MS_VRMS]) {
            const deque< boost::shared_ptr<pv::data::DsoSnapshot> > &snapshots =
                _data->get_snapshots();
            if (!snapshots.empty()) {
                const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot =
                    snapshots.front();
                const double vrms = snapshot->cal_vrms(_hw_offset, get_index());
                const double value_vrms = vrms * _scale * _vDial->get_value() * _vDial->get_factor() * DS_CONF_DSO_VDIVS / get_view_rect().height();
                _ms_string[DSO_MS_VRMS] = tr("Vrms: ") +  (abs(value_vrms) > 1000 ? QString::number(value_vrms/1000.0, 'f', 2) + "V" : QString::number(value_vrms, 'f', 2) + "mV");
            }
        }

        if (_probe->ms_show && _probe->ms_en[DSO_MS_VMEA]) {
            const deque< boost::shared_ptr<pv::data::DsoSnapshot> > &snapshots =
                _data->get_snapshots();
            if (!snapshots.empty()) {
                const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot =
                    snapshots.front();
                const double vmean = snapshot->cal_vmean(get_index());
                const double value_vmean = (_hw_offset - vmean) * _scale * _vDial->get_value() * _vDial->get_factor() * DS_CONF_DSO_VDIVS / get_view_rect().height();
                _ms_string[DSO_MS_VMEA] = tr("Vmean: ") +  (abs(value_vmean) > 1000 ? QString::number(value_vmean/1000.0, 'f', 2) + "V" : QString::number(value_vmean, 'f', 2) + "mV");
            }
        }
    } else {
        _ms_string[DSO_MS_VMAX] = tr("Vmax: #####");
        _ms_string[DSO_MS_VMIN] = tr("Vmin: #####");
        _ms_string[DSO_MS_PERD] = tr("Perd: #####");
        _ms_string[DSO_MS_FREQ] = tr("Freq: #####");
        _ms_string[DSO_MS_VP2P] = tr("Vp-p: #####");
        _ms_string[DSO_MS_VRMS] = tr("Vrms: #####");
        _ms_string[DSO_MS_VMEA] = tr("Vmean: #####");
    }

    QColor measure_colour = _colour;
    measure_colour.setAlpha(180);
    QColor back_colour = Qt::white;
    back_colour.setAlpha(100);

    bool antialiasing = p.Antialiasing;
    p.setRenderHint(QPainter::Antialiasing, true);

    p.setPen(Qt::NoPen);
    p.setBrush(measure_colour);
    p.drawRoundedRect(_ms_rect[DSO_MS_BEGIN], MS_RectRad, MS_RectRad);
    const QPixmap gear_pix(":/icons/settings.png");
    const QPixmap show_pix(_probe->ms_show ? ":/icons/shown.png" : ":/icons/hidden.png");
    if (_ms_gear_hover) {
        p.setBrush(back_colour);
        p.drawRoundedRect(_ms_gear_rect, MS_RectRad, MS_RectRad);
    } else if (_ms_show_hover) {
        p.setBrush(back_colour);
        p.drawRoundedRect(_ms_show_rect, MS_RectRad, MS_RectRad);
    }
    p.drawPixmap(_ms_gear_rect, gear_pix);
    p.drawPixmap(_ms_show_rect, show_pix);
    p.setPen(Qt::white);
    p.drawText(_ms_rect[DSO_MS_BEGIN], Qt::AlignCenter | Qt::AlignVCenter, "CH"+QString::number(index));

    if (_probe->ms_show) {
        p.setBrush(back_colour);
        int j = DSO_MS_BEGIN+1;
        for (int i=DSO_MS_BEGIN+1; i<DSO_MS_END; i++) {
            if (_probe->ms_en[i]) {
                p.setPen(_colour);
                p.drawText(_ms_rect[j], Qt::AlignLeft | Qt::AlignVCenter, _ms_string[i]);
                p.setPen(Qt::NoPen);
                p.drawRoundedRect(_ms_rect[j], MS_RectRad, MS_RectRad);
                j++;
            }
        }
    }
    p.setRenderHint(QPainter::Antialiasing, antialiasing);

    if (_view->session().get_capture_state() == SigSession::Stopped) {
        if (_autoV)
            autoV_end();
        if (_autoH)
            autoH_end();
    }

    if (_autoV && !_view->session().get_data_auto_lock()) {
        set_zero_vrate(0.5, true);
        const uint8_t vscale = abs(_max - _min);
        if (_max == 0xff || _min == 0x00 || vscale > 0xCC) {
            go_vDialNext();
        } else if (vscale > 0x33) {
            autoV_end();
        } else {
            go_vDialPre();
        }
        _view->session().data_auto_lock(AutoLock);
    }

    if (_autoH && !_view->session().get_data_auto_lock()) {
        const double hori_res = _view->get_hori_res();
        if (_period <= 0) {
            _view->zoom(-1);
        } else if (_period < 1.5*hori_res) {
            _view->zoom(1);
        } else if (_period > 6*hori_res) {
            _view->zoom(-1);
        } else {
            autoH_end();
        }
        _view->session().data_auto_lock(AutoLock);
    }
}

void DsoSignal::autoV_end()
{
    _autoV = false;
    _view->auto_trig(get_index());
    _trig_value = (_min+_max)/2;
    set_trig_vpos(get_trig_vpos(), true);
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
        QTimer::singleShot(AutoTime, this, SLOT(auto_end()));
    }
}

bool DsoSignal::measure(const QPointF &p)
{
    if (_ms_gear_rect.contains(QPoint(p.x(), p.y()))) {
        _ms_gear_hover = true;
        _view->set_update(_viewport, true);
        return false;
    } else if (_ms_gear_hover) {
        _view->set_update(_viewport, true);
        _ms_gear_hover = false;
    }
    if (_ms_show_rect.contains(QPoint(p.x(), p.y()))) {
        _ms_show_hover = true;
        _view->set_update(_viewport, true);
        return false;
    } else if (_ms_show_hover){
        _view->set_update(_viewport, true);
        _ms_show_hover = false;
    }

    _hover_en = false;
    if (!enabled())
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

    const double scale = _view->scale();
    assert(scale > 0);
    const int64_t pixels_offset = _view->offset();
    const double samplerate = _view->session().cur_samplerate();
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

    _hover_value = (_hw_offset - cur_sample) * _scale * _vDial->get_value() * _vDial->get_factor() * DS_CONF_DSO_VDIVS / get_view_rect().height();

    float top = get_view_rect().top();
    float bottom = get_view_rect().bottom();
    float zeroP = _zero_vrate * get_view_rect().height() + top;
    float pre_x = (pre_index / samples_per_pixel - pixels_offset);
    const float pre_y = min(max(top, zeroP + (pre_sample - _hw_offset)* _scale), bottom);
    float x = (_hover_index / samples_per_pixel - pixels_offset);
    const float y = min(max(top, zeroP + (cur_sample - _hw_offset)* _scale), bottom);
    float nxt_x = (nxt_index / samples_per_pixel - pixels_offset);
    const float nxt_y = min(max(top, zeroP + (nxt_sample - _hw_offset)* _scale), bottom);
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
