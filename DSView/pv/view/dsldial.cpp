/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2014 DreamSourceLab <support@dreamsourcelab.com>
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

#include "dsldial.h"

#include <cassert>
#include <cmath>

namespace pv {
namespace view {

dslDial::dslDial(const uint64_t div, const uint64_t step,
                 const QVector<uint64_t> value, const QVector<QString> unit)
{
    assert(div > 0);
    assert(step > 0);
    assert((uint64_t)value.count() == div);
    assert(unit.count() > 0);

    _div = div;
    _step = step;
    _value = value;
    _unit = unit;
    _sel = 0;
    _factor = 1;
}

dslDial::~dslDial()
{
}

void dslDial::paint(QPainter &p, QRectF dialRect, QColor dialColor, bool hover, bool inc)
{
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(dialColor);
    p.setBrush(dialColor);

    int dialStartAngle = 225 * 16;
    int dialSpanAngle = -270 * 16;

    // draw dial arc
    p.drawArc(dialRect, dialStartAngle, dialSpanAngle);
    // draw ticks
    p.save();
    p.translate(dialRect.center());
    p.rotate(45);
    for (uint64_t i = 0; i < _div; i++) {
        // draw major ticks
        p.drawLine(0, dialRect.width()/2+3, 0, dialRect.width()/2+8);
        // draw minor ticks
        for (uint64_t j = 0; (j < 5) && (i < _div - 1); j++) {
            p.drawLine(0, dialRect.width()/2+3, 0, dialRect.width()/2+5);
            p.rotate(54.0/(_div-1));
        }
    }
    // draw pointer
    p.rotate(90+270.0/(_div-1)*_sel);
    p.drawEllipse(-3, -3, 6, 6);
    p.drawLine(3, 0, 0, dialRect.width()/2-3);
    p.drawLine(-3, 0, 0, dialRect.width()/2-3);
    p.restore();
    // draw value
    uint64_t displayValue = _value[_sel]*_factor;
    uint64_t displayIndex = 0;
    while(displayValue / _step >= 1) {
        displayValue = displayValue / _step;
        displayIndex++;
    }
    QString pText = QString::number(displayValue) + _unit[displayIndex] + "/div";
    QFontMetrics fm(p.font());
    const QRectF valueRect = QRectF(dialRect.left(), dialRect.bottom()-dialRect.width()*0.3+fm.height()*0.5, dialRect.width(), fm.height());
    p.drawText(valueRect, Qt::AlignCenter, pText);

    // draw +/-
    if (hover) {
        const int arcInc = 15;
        const QRectF hoverRect = QRectF(dialRect.left()-arcInc, dialRect.top()-arcInc, dialRect.width()+arcInc*2, dialRect.height()+arcInc*2);
        const double arcSpan = hoverRect.width()/(2*sqrt(2));
        p.drawArc(hoverRect, 135 * 16, -90 * 16);
        if (inc)
            p.drawLine(hoverRect.center().x()+arcSpan, hoverRect.center().y()-arcSpan,
                       hoverRect.center().x()+arcSpan-4, hoverRect.center().y()-arcSpan-10);
        else
            p.drawLine(hoverRect.center().x()-arcSpan, hoverRect.center().y()-arcSpan,
                       hoverRect.center().x()-arcSpan+4, hoverRect.center().y()-arcSpan-10);
    }
}

void dslDial::set_sel(uint64_t sel)
{
    assert(sel < _div);

    _sel = sel;
}

uint64_t dslDial::get_sel()
{
    return _sel;
}

bool dslDial::isMin()
{
    if(_sel == 0)
        return true;
    else
        return false;
}

bool dslDial::isMax()
{
    if(_sel == _div - 1)
        return true;
    else
        return false;
}

uint64_t dslDial::get_value()
{
    return _value[_sel];
}

bool dslDial::set_value(uint64_t value)
{
    assert(_value.contains(value));
    _sel = _value.indexOf(value, 0);
}

void dslDial::set_factor(uint64_t factor)
{
    if (_factor != factor) {
        _factor = factor;
    }
}

uint64_t dslDial::get_factor()
{
    return _factor;
}

} // namespace view
} // namespace pv
