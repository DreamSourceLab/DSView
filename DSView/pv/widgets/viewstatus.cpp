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

namespace pv {
namespace widgets {

ViewStatus::ViewStatus(QWidget *parent) : QWidget(parent)
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
}

void ViewStatus::clear()
{
    _trig_time.clear();
    _rle_depth.clear();
}

void ViewStatus::set_trig_time(QDateTime time)
{
    _trig_time = tr("Trigger Time: ") + time.toString("yyyy-MM-dd hh:mm:ss ddd");
}

void ViewStatus::set_rle_depth(uint64_t depth)
{
    _rle_depth = tr("RLE FULL: ") + QString::number(depth) + tr(" Samples Captured!");
}

} // namespace widgets
} // namespace pv
