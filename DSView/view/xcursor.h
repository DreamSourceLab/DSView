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

#ifndef DSVIEW_PV_VIEW_XCURSOR_H
#define DSVIEW_PV_VIEW_XCURSOR_H

#include <QColor>
#include <QObject>
#include <QRectF>
 
#include <stdint.h>

class QPainter;
class QRect;

namespace pv {
namespace view {

class View;
class DsoSignal;

//created by View
class XCursor : public QObject
{
	Q_OBJECT
  
public:
    enum XCur_type {
        XCur_None = -2,
        XCur_All = -1,
        XCur_Y = 0,
        XCur_X0,
        XCur_X1
    };

public:
	/**
	 * Constructor.
	 * @param view A reference to the view that owns this marker.
	 * @param colour A reference to the colour of this cursor.
     * @param value0
     * @param value1
	 */
    XCursor(View &view, QColor &colour, double value0, double value1);

	/**
	 * Copy constructor
	 */
    XCursor(const XCursor &x);

public:
	/**
     * Gets/Set the value of the marker.
	 */
    double value(enum XCur_type type);
    void set_value(enum XCur_type type, double value);

    /**
     * Gets/Sets colour of the marker
     */
    QColor colour();
    void set_colour(QColor color);

    /**
     * Gets/Sets the mapping channel of the marker
     */
    DsoSignal* channel();
    void set_channel(DsoSignal *sig);

    /**
     * grab & move
     */
    enum XCur_type grabbed();
    void set_grabbed(enum XCur_type type, bool grabbed);
    void rel_grabbed();

	/**
	 * Paints the marker to the viewport.
	 * @param p The painter to draw with.
	 * @param rect The rectangle of the viewport client area.
	 */
    void paint(QPainter &p, const QRect &rect, enum XCur_type highlight, int order);

	/**
     * Gets the map label rectangle.
     * @param rect The rectangle of the xcursor area.
     * @return Returns the map label rectangle.
	 */
    QRect get_map_rect(const QRect &rect);

    /**
     * Gets the close label rectangle.
     * @param rect The rectangle of the xcursor area.
     * @return Returns the close label rectangle.
     */
    QRect get_close_rect(const QRect &rect);

	/**
     * Paints the labels to the xcursor.
	 * @param p The painter to draw with.
     * @param rect The rectangle of the xcursor area.
	 */
    void paint_label(QPainter &p, const QRect &rect);

signals:
    void value_changed();

private slots:
    void on_signal_deleted(void *o);

protected:
    View        &_view;
    DsoSignal   *_dsoSig;
    int         _sig_index;

    double _yvalue;
    double _value0;
    double _value1;

    QSizeF _v0_size;
    QSizeF _v1_size;

private:
    enum XCur_type _grabbed;
    QColor _colour;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_VIEW_XCURSOR_H
