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

#include "../view/trace.h"
#include "../sigsession.h"

namespace pv {
namespace widgets {

ViewStatus::ViewStatus(SigSession &session, QWidget *parent) :
    QWidget(parent),
    _session(session)
{
}

void ViewStatus::paintEvent(QPaintEvent *)
{
    QStyleOption opt;
    opt.init(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);

    p.setPen(pv::view::Trace::DARK_FORE);
    p.drawText(this->rect(), Qt::AlignLeft | Qt::AlignVCenter, _rle_depth);
    p.drawText(this->rect(), Qt::AlignRight | Qt::AlignVCenter, _trig_time);

    p.setPen(Qt::NoPen);
    p.setBrush(pv::view::Trace::dsLightBlue);
    p.drawRect(this->rect().left(), this->rect().bottom() - 3,
               _session.get_repeat_hold() * this->rect().width() / 100, 3);

    p.setPen(pv::view::Trace::dsLightBlue);
    p.drawText(this->rect(), Qt::AlignCenter | Qt::AlignVCenter, _capture_status);
}

void ViewStatus::clear()
{
    _trig_time.clear();
    _rle_depth.clear();
    _capture_status.clear();
    update();
}

void ViewStatus::repeat_unshow()
{
    _capture_status.clear();
    update();
}

void ViewStatus::set_trig_time(QDateTime time)
{
    _trig_time = tr("Trigger Time: ") + time.toString("yyyy-MM-dd hh:mm:ss ddd");
}

void ViewStatus::set_rle_depth(uint64_t depth)
{
    _rle_depth = QString::number(depth) + tr(" Samples Captured!");
}

void ViewStatus::set_capture_status(bool triggered, int progess)
{
    if (triggered)
        _capture_status = tr("Triggered! ") + QString::number(progess) + tr("% Captured");
    else
        _capture_status = tr("Waiting for Trigger! ") + QString::number(progess) + tr("% Captured");
}

} // namespace widgets
} // namespace pv
