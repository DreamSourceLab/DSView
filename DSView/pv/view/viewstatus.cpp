/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
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

#include "viewstatus.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QStyleOption>
#include <QMouseEvent>
#include <QBitmap>
#include <QJsonObject>
#include <QJsonArray>

#include "../view/trace.h"
#include "../sigsession.h"
#include "../view/view.h"
#include "../view/trace.h"
#include "../dialogs/dsomeasure.h"

#include "../ui/langresource.h"
#include "../log.h"
#include "../config/appconfig.h"
#include "../appcontrol.h"
#include "../ui/fn.h"
#include "../utility/formatting.h"

using namespace std;

namespace pv {
namespace view {

ViewStatus::ViewStatus(SigSession *session, View &parent) :
    QWidget(&parent),
    _session(session),
    _view(parent),
    _hit_rect(-1),
    _last_sig_index(-1)
{
}

void ViewStatus::paintEvent(QPaintEvent *)
{
    QStyleOption opt;
 #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
      opt.initFrom(this);
 #else
      opt.init(this);
 #endif

    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    QColor fore(QWidget::palette().color(QWidget::foregroundRole()));

    QFont font = p.font();
    float fSize = AppConfig::Instance().appOptions.fontSize;
    if (fSize > 10)
        fSize = 10;
    font.setPointSizeF(fSize);
    p.setFont(font);

    int mode = _session->get_device()->get_work_mode();

    if (mode == LOGIC) {
        fore.setAlpha(View::ForeAlpha);
        p.setPen(fore);
        p.drawText(this->rect(), Qt::AlignLeft | Qt::AlignVCenter, _rle_depth);
        p.drawText(this->rect(), Qt::AlignRight | Qt::AlignVCenter, _trig_time);

        p.setPen(Qt::NoPen);
        p.setBrush(View::Blue);
        p.drawRect(this->rect().left(), this->rect().bottom() - 3,
                   _session->get_repeat_hold() * this->rect().width() / 100, 3);

        p.setPen(View::Blue);
        p.drawText(this->rect(), Qt::AlignCenter | Qt::AlignVCenter, _capture_status);
    } 
    else if (mode == DSO) {
        fore.setAlpha(View::BackAlpha);

        for(size_t i = 0; i < _mrects.size(); i++) {
            int sig_index = std::get<1>(_mrects[i]);
            view::DsoSignal *dsoSig = NULL;

            for(auto s : _session->get_signals()) {
                if (s->signal_type() == SR_CHANNEL_DSO && s->enabled()) {
                    dsoSig = (view::DsoSignal*)s;
                    if (sig_index == dsoSig->get_index())
                        break;
                    else
                        dsoSig = NULL;
                }
            }

            bool active = dsoSig && dsoSig->enabled();
            const QRect rect = std::get<0>(_mrects[i]);
            p.setPen(Qt::NoPen);
            p.setBrush(active ? dsoSig->get_colour() : fore);
            p.drawRect(QRect(rect.topLeft(), QSize(10, rect.height())));

            QPixmap msPix(pv::dialogs::DsoMeasure::get_ms_icon(std::get<2>(_mrects[i])));
            QBitmap msMask = msPix.createMaskFromColor(QColor("black"), Qt::MaskOutColor);
            msPix.fill(active ? dsoSig->get_colour() : fore);
            msPix.setMask(msMask);
            p.drawPixmap(QRect(rect.left()+10, rect.top(), rect.height(), rect.height()),
                         msPix);

            p.setPen(((int)i == _hit_rect) ? View::Blue :
                     active ? dsoSig->get_colour() : fore);
            p.setBrush(Qt::NoBrush);
            p.drawRect(rect);

            enum DSO_MEASURE_TYPE mtype = std::get<2>(_mrects[i]);

            if (active && (mtype != DSO_MS_BEGIN)) {
                QString title = pv::dialogs::DsoMeasure::get_ms_text(std::get<2>(_mrects[i])) + ":";
                title += dsoSig->get_measure(mtype);
                int width = p.boundingRect(rect, title).width();
                p.drawText(QRect(rect.left()+10+rect.height(), rect.top(), width, rect.height()),
                           Qt::AlignLeft | Qt::AlignVCenter, title);
            }
            else {
                p.drawText(rect, Qt::AlignCenter | Qt::AlignVCenter, L_S(STR_PAGE_DLG, S_ID(IDS_DLG_MEASURE), "Measure") + QString::number(i));
            }
        }
    }
}

void ViewStatus::clear()
{
    _trig_time.clear();
    _rle_depth.clear();
    _capture_status.clear();
    update();
}

void ViewStatus::reload()
{
    const int COLUMN = 5;
    const int ROW = 2;
    const int MARGIN = 3;

    if (_session->get_device()->get_work_mode() == DSO)
    {
        const double width = _view.get_view_width() * 1.0 / COLUMN;
        const int height = (this->height() - 2*MARGIN) / ROW;

        for (size_t i  = 0; i < COLUMN*ROW; i++) {
            QRect rect(this->rect().left() + (i%COLUMN)*width,
                       this->rect().top() + (i/COLUMN+1)*MARGIN + (i/COLUMN)*height,
                       width-MARGIN, height);

            if (_mrects.size() <= i) {
                std::tuple<QRect, int, enum DSO_MEASURE_TYPE> rect_tuple;
                std::get<0>(rect_tuple) = rect;
                std::get<1>(rect_tuple) = -1;
                std::get<2>(rect_tuple) = DSO_MS_BEGIN;
                _mrects.push_back(rect_tuple);
            }
            else {
                std::get<0>(_mrects[i]) = rect;
            }
        }
    }
    update();
}

void ViewStatus::repeat_unshow()
{
    _capture_status.clear();
    update();
}

void ViewStatus::set_trig_time(QDateTime time)
{   
    QString dateTimeString = Formatting::DateTimeToString(time, TimeStrigFormatType::TIME_STR_FORMAT_ALL);
    _trig_time = L_S(STR_PAGE_DLG, S_ID(IDS_DLG_TRIGGER_TIME), "Trigger Time: ") + dateTimeString;
}

void ViewStatus::set_rle_depth(uint64_t depth)
{
    _rle_depth = QString::number(depth) + L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SAMPLES_CAPTURED), "Samples Captured!");
}

void ViewStatus::set_capture_status(bool triggered, int progess)
{
    if (triggered) {
        _capture_status = L_S(STR_PAGE_DLG, S_ID(IDS_DLG_TRIGGERED), "Triggered! ") + QString::number(progess) 
                        + L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CAPTURED), "% Captured");
    } else {
        _capture_status = L_S(STR_PAGE_DLG, S_ID(IDS_DLG_WAITING_FOR_TRIGGER), "Waiting for Trigger! ") + QString::number(progess) 
                        + L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CAPTURED), "% Captured");
    }
}

void ViewStatus::mousePressEvent(QMouseEvent *event)
{
    assert(event);

    if (_session->get_device()->get_work_mode() != DSO)
        return;

    if (event->button() == Qt::LeftButton) { 
        for(size_t i = 0; i < _mrects.size(); i++) {
            const QRect rect = std::get<0>(_mrects[i]);
            if (rect.contains(event->pos())) {
                _hit_rect = (int)i;
                pv::dialogs::DsoMeasure dsoMeasureDialog(_session, _view, i, _last_sig_index);
                dsoMeasureDialog.exec();
                break;
            }
        }
        update();
    }
}

void ViewStatus::set_measure(unsigned int index, bool canceled,
                             int sig_index, enum DSO_MEASURE_TYPE ms_type)
{
    _hit_rect = -1;
    if (!canceled && index < _mrects.size()) {
        _last_sig_index = sig_index;
        std::get<1>(_mrects[index]) = sig_index;
        std::get<2>(_mrects[index]) = ms_type;
    }
    update();
}

QJsonArray ViewStatus::get_session()
{
    QJsonArray measureVar;
    for(int i = 0; i < (int)_mrects.size(); i++) {
        const int index = std::get<1>(_mrects[i]);
        if (index != -1) {
            QJsonObject m_obj;
            m_obj["site"] = i;
            m_obj["index"] = index;
            m_obj["type"] = (int)std::get<2>(_mrects[i]);
            measureVar.append(m_obj);
        }
    }

    return measureVar;
}

void ViewStatus::load_session(QJsonArray measure_array, int version)
{
    if (_session->get_device()->get_work_mode() != DSO){
        return;
    }

    for(int i = 0; i < (int)_mrects.size(); i++) 
    {
        std::get<1>(_mrects[i]) = -1;
        std::get<2>(_mrects[i]) = DSO_MS_BEGIN;
    }
 
    for (const QJsonValue &measure_value : measure_array) 
    { 
        QJsonObject m_obj = measure_value.toObject();
        int index = m_obj["site"].toInt();
        int sig_index = m_obj["index"].toInt();
       
        if (version >= 3){
            Signal *trace = NULL;

            for(auto s : _session->get_signals()){
                if (s->get_name().toInt() == sig_index){
                    trace = s;
                    break;
                }
            }

            if (trace == NULL)
                continue;
            sig_index = trace->get_index();
        }

        enum DSO_MEASURE_TYPE ms_type = DSO_MEASURE_TYPE(m_obj["type"].toInt());
        set_measure(index, false, sig_index, ms_type);
    }
}

} // namespace view
} // namespace pv
