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


#ifndef DSVIEW_PV_VIEW_CURSOR_H
#define DSVIEW_PV_VIEW_CURSOR_H

#include "timemarker.h"

#include <QSizeF>

class QPainter;

namespace pv {
namespace view {

class Cursor : public TimeMarker
{
	Q_OBJECT

public:
	static const QColor LineColour;
	static const QColor FillColour;
	static const QColor HighlightColour;
	static const QColor TextColour;

	static const int Offset;

	static const int ArrowSize;
    static const int CloseSize;

public:
	/**
	 * Constructor.
	 * @param view A reference to the view that owns this cursor pair.
	 * @param time The time to set the flag to.
	 * @param other A reference to the other cursor.
	 */
    Cursor(View &view, QColor color, uint64_t index);


public:
	/**
	 * Gets the marker label rectangle.
	 * @param rect The rectangle of the ruler client area.
	 * @return Returns the label rectangle.
	 */
	QRectF get_label_rect(const QRect &rect) const;

    QRectF get_close_rect(const QRect &rect) const;

	/**
	 * Paints the cursor's label to the ruler.
	 * @param p The painter to draw with.
	 * @param rect The rectangle of the ruler client area.
	 * @param prefix The index of the SI prefix to use.
	 */
	void paint_label(QPainter &p, const QRect &rect,
        unsigned int prefix, int index);

    void paint_fix_label(QPainter &p, const QRect &rect,
        unsigned int prefix, QChar label, QColor color);

private:
	void compute_text_size(QPainter &p, unsigned int prefix);

private:
	const Cursor &_other;

	QSizeF _text_size;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_VIEW_CURSOR_H
