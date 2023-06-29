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

/*
#include "../dsvdef.h"
#include "groupsignal.h" 
#include "view.h"
#include <cmath>
 
using namespace std;

namespace pv {
namespace view {

const QColor GroupSignal::SignalColours[4] = {
	QColor(0xC4, 0xA0, 0x00),	// Yellow
	QColor(0x87, 0x20, 0x7A),	// Magenta
	QColor(0x20, 0x4A, 0x87),	// Blue
	QColor(0x4E, 0x9A, 0x06)	// Green
};

const float GroupSignal::EnvelopeThreshold = 256.0f;

GroupSignal::GroupSignal(QString name,
                         std::list<int> probe_index_list, int group_index) :
    Trace(name, probe_index_list, SR_CHANNEL_GROUP, group_index)
{
    _colour = SignalColours[probe_index_list.front() % countof(SignalColours)];
    _scale = _totalHeight * 1.0f / std::pow(2.0, static_cast<double>(probe_index_list.size()));
}

GroupSignal::~GroupSignal()
{
}

bool GroupSignal::enabled()
{
    return true;
}

pv::data::SignalData* GroupSignal::data()
{
    return NULL;
}

void GroupSignal::set_scale(float scale)
{
	_scale = scale;
}

void GroupSignal::paint_mid(QPainter &p, int left, int right, QColor fore, QColor back)
{
   
}

void GroupSignal::paint_trace(QPainter &p,
	int y, int left, const int64_t start, const int64_t end,
	const double pixels_offset, const double samples_per_pixel)
{
	 
}

void GroupSignal::paint_envelope(QPainter &p,
	int y, int left, const int64_t start, const int64_t end,
	const double pixels_offset, const double samples_per_pixel)
{ 

 
}

void GroupSignal::paint_type_options(QPainter &p, int right, const QPoint pt, QColor fore)
{ 
}

QRectF GroupSignal::get_rect(GroupSetRegions type, int y, int right)
{ 
}

} // namespace view
} // namespace pv

*/
