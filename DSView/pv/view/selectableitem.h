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

#ifndef DSVIEW_PV_SELECTABLEITEM_H
#define DSVIEW_PV_SELECTABLEITEM_H

#include <list>

#include <QPen>

class QAction;
class QMenu;
class QWidget;

namespace pv {

namespace view {

class SelectableItem : public QObject
{
	Q_OBJECT

private:
	static const int HighlightRadius;

public:
	SelectableItem();

public:
	/**
	 * Returns true if the signal has been selected by the user.
	 */
	bool selected() const;

	/**
	 * Selects or deselects the signal.
	 */
	void select(bool select = true);

protected:
	static QPen highlight_pen();

private:
	bool _selected;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_SELECTABLEITEM_H
