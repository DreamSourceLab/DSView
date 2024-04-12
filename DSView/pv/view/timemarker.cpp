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

#include "timemarker.h"
#include <QPainter>
#include "view.h"
#include "ruler.h"


namespace pv {
namespace view {

TimeMarker::TimeMarker(View &view,
    uint64_t index) :
	_view(view),
    _index(index),
    _grabbed(false)
{
    _colour = Qt::blue;
    _order = -1;
}

TimeMarker::TimeMarker(const TimeMarker &s) :
	QObject(),
	_view(s._view),
    _index(s._index),
	_colour(s._colour)
{

}

void TimeMarker::set_colour(QColor color)
{
    _colour = color;
}

QColor TimeMarker::get_color()
{   
    if (_order > 0){
        return Ruler::GetColorByCursorOrder(_order);
    }
    return _colour;    
}

bool TimeMarker::grabbed()
{
    return _grabbed;
}

void TimeMarker::set_grabbed(bool grabbed)
{
    _grabbed = grabbed;
}

uint64_t TimeMarker::index()
{
    return _index;
}

void TimeMarker::set_index(int64_t index)
{
    if (index < 0){
        index = 0;
    }
   _index = index;
    time_changed();
}

void TimeMarker::paint(QPainter &p, const QRect &rect, const bool highlight, bool trig_hoff)
{
    const int64_t x = _view.index2pixel(_index, trig_hoff);
    if (x <= rect.right()) {
        QColor color = (_order < 1) ? _colour : Ruler::GetColorByCursorOrder(_order);
        p.setPen((_grabbed | highlight) ? QPen(color.lighter(), 2, Qt::DashLine) : QPen(color, 1, Qt::DashLine));
        p.drawLine(QPoint(x, 0), QPoint(x, rect.bottom()));
    }
}

} // namespace view
} // namespace pv
