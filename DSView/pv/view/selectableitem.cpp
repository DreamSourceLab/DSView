/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2013 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include "selectableitem.h"

#include <QApplication>
#include <QMenu>
#include <QPalette>

namespace pv {
namespace view {

const int SelectableItem::HighlightRadius = 6;

SelectableItem::SelectableItem() :
    _selected(false)
{
}

bool SelectableItem::selected() const
{
	return _selected;
}

void SelectableItem::select(bool select)
{
	_selected = select;
}

QPen SelectableItem::highlight_pen()
{
	return QPen(QApplication::palette().brush(
		QPalette::Highlight), HighlightRadius,
		Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
}

} // namespace view
} // namespace pv
