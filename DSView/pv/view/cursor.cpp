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


#include "cursor.h"

#include "ruler.h"
#include "view.h"
#include "../device/device.h"

#include <QBrush>
#include <QPainter>
#include <QPointF>
#include <QRect>
#include <QRectF>

#include <assert.h>
#include <stdio.h>

#include <extdef.h>

namespace pv {
namespace view {

const QColor Cursor::LineColour(32, 74, 135);
const QColor Cursor::FillColour(52, 101, 164);
const QColor Cursor::HighlightColour(83, 130, 186);
const QColor Cursor::TextColour(Qt::white);

const int Cursor::Offset = 1;

const int Cursor::ArrowSize = 10;

const int Cursor::CloseSize = 10;

Cursor::Cursor(View &view, QColor color, uint64_t index) :
    TimeMarker(view, color, index),
    _other(*this)
{
}

QRectF Cursor::get_label_rect(const QRect &rect) const
{
    const double samples_per_pixel = _view.session().cur_samplerate() * _view.scale();
    const double x = _index/samples_per_pixel - (_view.offset() / _view.scale());

	const QSizeF label_size(
		_text_size.width() + View::LabelPadding.width() * 2,
		_text_size.height() + View::LabelPadding.height() * 2);
	const float top = rect.height() - label_size.height() -
		Cursor::Offset - Cursor::ArrowSize - 0.5f;
	const float height = label_size.height();

    return QRectF(x - label_size.width() / 2, top, label_size.width(), height);
}

QRectF Cursor::get_close_rect(const QRect &rect) const
{
    const QRectF r(get_label_rect(rect));
    return QRectF(r.right() - CloseSize, r.top(), CloseSize, CloseSize);
}

void Cursor::paint_label(QPainter &p, const QRect &rect,
    unsigned int prefix, int index)
{
    assert(index > 0);

    using pv::view::Ruler;

	compute_text_size(p, prefix);
	const QRectF r(get_label_rect(rect));
    const QRectF close(get_close_rect(rect));

    p.setPen(Qt::transparent);
    if (close.contains(QPoint(_view.hover_point().x(), _view.hover_point().y())))
        p.setBrush(Ruler::CursorColor[(index - 1) % 8]);
    else if (r.contains(QPoint(_view.hover_point().x(), _view.hover_point().y())))
        p.setBrush(Ruler::HitColor);
    else
        p.setBrush(Ruler::CursorColor[(index - 1) % 8]);
    p.drawRect(r);

    const QPointF points[] = {
        QPointF(r.left() + r.width() / 2 - ArrowSize, r.bottom()),
        QPointF(r.left() + r.width() / 2 + ArrowSize, r.bottom()),
        QPointF(r.left() + r.width() / 2, rect.bottom()),
    };
    p.drawPolygon(points, countof(points));

    if (close.contains(QPoint(_view.hover_point().x(), _view.hover_point().y())))
        p.setBrush(Ruler::WarnColor);
    else
        p.setBrush(Ruler::HitColor);
    p.drawRect(close);
    p.setPen(Qt::black);
    p.drawLine(close.left() + 2, close.top() + 2, close.right() - 2, close.bottom() - 2);
    p.drawLine(close.left() + 2, close.bottom() - 2, close.right() - 2, close.top() + 2);

	p.drawText(r, Qt::AlignCenter | Qt::AlignVCenter,
        Ruler::format_real_time(_index, _view.session().cur_samplerate()));

    const QRectF arrowRect = QRectF(r.bottomLeft().x(), r.bottomLeft().y(), r.width(), ArrowSize);
    p.drawText(arrowRect, Qt::AlignCenter | Qt::AlignVCenter, QString::number(index));
}

void Cursor::paint_fix_label(QPainter &p, const QRect &rect,
    unsigned int prefix, QChar label, QColor color)
{
    using pv::view::Ruler;

    compute_text_size(p, prefix);
    const QRectF r(get_label_rect(rect));

    p.setPen(Qt::transparent);
    p.setBrush(color);
    p.drawRect(r);

    const QPointF points[] = {
        QPointF(r.left() + r.width() / 2 - ArrowSize, r.bottom()),
        QPointF(r.left() + r.width() / 2 + ArrowSize, r.bottom()),
        QPointF(r.left() + r.width() / 2, rect.bottom()),
    };
    p.drawPolygon(points, countof(points));

    p.setPen(Qt::white);
    p.drawText(r, Qt::AlignCenter | Qt::AlignVCenter,
        Ruler::format_real_time(_index, _view.session().cur_samplerate()));

    const QRectF arrowRect = QRectF(r.bottomLeft().x(), r.bottomLeft().y(), r.width(), ArrowSize);
    p.drawText(arrowRect, Qt::AlignCenter | Qt::AlignVCenter, label);
}

void Cursor::compute_text_size(QPainter &p, unsigned int prefix)
{
    (void)prefix;
    _text_size = p.boundingRect(QRectF(), 0,
        Ruler::format_real_time(_index, _view.session().cur_samplerate())).size();
}

} // namespace view
} // namespace pv
