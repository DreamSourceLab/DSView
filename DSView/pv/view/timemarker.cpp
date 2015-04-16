/*
 * This file is part of the DSLogic-gui project.
 * DSLogic-gui is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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


#include "timemarker.h"

#include "view.h"

#include <QPainter>

namespace pv {
namespace view {

TimeMarker::TimeMarker(View &view, QColor &colour,
	double time) :
	_view(view),
	_time(time),
        _grabbed(false),
	_colour(colour)
{
}

TimeMarker::TimeMarker(const TimeMarker &s) :
	QObject(),
	_view(s._view),
	_time(s._time),
	_colour(s._colour)
{
}

bool TimeMarker::grabbed() const
{
    return _grabbed;
}
void TimeMarker::set_grabbed(bool grabbed)
{
    _grabbed = grabbed;
}

double TimeMarker::time() const
{
	return _time;
}

void TimeMarker::set_time(double time)
{
	_time = time;
	time_changed();
}

void TimeMarker::paint(QPainter &p, const QRect &rect, const bool highlight)
{
	const float x = (_time - _view.offset()) / _view.scale();
    p.setPen((_grabbed | highlight) ? QPen(_colour.lighter(), 2, Qt::DashLine) : QPen(_colour, 1, Qt::DashLine));
    p.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
}

} // namespace view
} // namespace pv
