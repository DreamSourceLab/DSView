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

#ifndef DSVIEW_PV_VIEW_TRACE_H
#define DSVIEW_PV_VIEW_TRACE_H

#include <QColor>
#include <QPainter>
#include <QPen>
#include <QRect>
#include <QString>

#include <stdint.h>

#include <libsigrok4DSL/libsigrok.h>

#include "selectableitem.h"
#include "dsldial.h"

class QFormLayout;

namespace pv {
namespace view {

class View;
class Viewport;

class Trace : public SelectableItem
{
	Q_OBJECT

protected:
    static const int Margin = 3;
    static const int SquareNum = 5;
	static const QPen AxisPen;
	static const int LabelHitPadding;

public:
    static const int SquareWidth = 20;
    static const int COLOR = 1;
    static const int NAME = 2;
    static const int LABEL = 8;

    static const QColor dsBlue;
    static const QColor dsYellow;
    static const QColor dsRed;
    static const QColor dsGreen;
    static const QColor dsGray;
    static const QColor dsFore;
    static const QColor dsBack;
    static const QColor dsDisable;
    static const QColor dsActive;
    static const QColor dsLightBlue;
    static const QColor dsLightRed;
    static const QPen SignalAxisPen;

    static const QColor DARK_BACK;
    static const QColor DARK_FORE;
    static const QColor DARK_HIGHLIGHT;
    static const QColor DARK_BLUE;

    static const QColor PROBE_COLORS[8];

protected:
    Trace(QString name, uint16_t index, int type);
    Trace(QString name, std::list<int> index_list, int type, int sec_index);

    /**
     * Copy constructor
     */
    Trace(const Trace &t);

public:
	/**
	 * Gets the name of this signal.
	 */
	QString get_name() const;

	/**
	 * Sets the name of the signal.
	 */
	virtual void set_name(QString name);

	/**
	 * Get the colour of the signal.
	 */
	QColor get_colour() const;

	/**
	 * Set the colour of the signal.
	 */
	void set_colour(QColor colour);

	/**
	 * Gets the vertical layout offset of this signal.
	 */
	int get_v_offset() const;

	/**
	 * Sets the vertical layout offset of this signal.
	 */
	void set_v_offset(int v_offset);

    /**
     * Gets trace type
     */
    int get_type() const;

    /**
     * Index process
     */
    int get_index() const;
    std::list<int> &get_index_list();
    void set_index_list(std::list<int> index_list);
    int get_sec_index() const;
    void set_sec_index(int sec_index);

    /**
     * Gets the height of this signal.
     */
    int get_totalHeight() const;

    /**
     * Sets the height of this signal.
     */
    void set_totalHeight(int height);

    /**
     * Geom
     */
    int get_leftWidth() const;
    int get_rightWidth() const;
    int get_headerHeight() const;

    /**
     * Gets the old vertical layout offset of this signal.
     */
    int get_old_v_offset() const;

    /**
     * Sets the old vertical layout offset of this signal.
     */
    void set_old_v_offset(int v_offset);

    virtual int get_zero_vpos();

	/**
	 * Returns true if the trace is visible and enabled.
	 */
	virtual bool enabled() const = 0;

	virtual void set_view(pv::view::View *view);
    pv::view::View* get_view() const;
    virtual void set_viewport(pv::view::Viewport *viewport);
    pv::view::Viewport* get_viewport() const;

	/**
	 * Paints the background layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal
	 * @param right the x-coordinate of the right edge of the signal
	 **/
	virtual void paint_back(QPainter &p, int left, int right);

	/**
	 * Paints the mid-layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal
	 * @param right the x-coordinate of the right edge of the signal
	 **/
	virtual void paint_mid(QPainter &p, int left, int right);

	/**
	 * Paints the foreground layer of the trace with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal
	 * @param right the x-coordinate of the right edge of the signal
	 **/
	virtual void paint_fore(QPainter &p, int left, int right);

	/**
     * Paints the trace label.
	 * @param p the QPainter to paint into.
	 * @param right the x-coordinate of the right edge of the header
	 * 	area.
     * @param point the mouse point.
	 */
    virtual void paint_label(QPainter &p, int right, const QPoint pt);

	/**
	 * Gets the y-offset of the axis.
	 */
	int get_y() const;

    /**
     * Determines if a point is in the header rect.
     * 1 - in color rect
     * 2 - in name rect
     * 3 - in posTrig rect
     * 4 - in higTrig rect
     * 5 - in negTrig rect
     * 6 - in lowTrig rect
     * 7 - in label rect
     * 0 - not
     * @param y the y-coordinate of the signal.
     * @param right the x-coordinate of the right edge of the header
     * 	area.
     * @param point the point to test.
     */
    int pt_in_rect(int y, int right,
        const QPoint &point);

    /**
     * Computes the outline rectangle of a label.
     * @param p the QPainter to lay out text with.
     * @param y the y-coordinate of the signal.
     * @param right the x-coordinate of the right edge of the header
     * 	area.
     * @return Returns the rectangle of the signal label.
     */
    QRectF get_rect(const char *s, int y, int right) const;

    virtual int rows_size();

    virtual QRect get_view_rect() const;

    virtual bool mouse_double_click(int right, const QPoint pt);

    virtual bool mouse_press(int right, const QPoint pt);

    virtual bool mouse_wheel(int right, const QPoint pt, const int shift);

protected:

	/**
	 * Gets the text colour.
	 * @remarks This colour is computed by comparing the lightness
	 * of the trace colour against a threshold to determine whether
	 * white or black would be more visible.
	 */
	QColor get_text_colour() const;

	/**
	 * Paints a zero axis across the viewport.
	 * @param p the QPainter to paint into.
	 * @param y the y-offset of the axis.
	 * @param left the x-coordinate of the left edge of the view.
	 * @param right the x-coordinate of the right edge of the view.
	 */
	void paint_axis(QPainter &p, int y, int left, int right);

    /**
     * Paints optoins for different trace type.
     * @param p the QPainter to paint into.
     * @param right the x-coordinate of the right edge of the header
     * 	area.
     * @param point the mouse point.
     */
    virtual void paint_type_options(QPainter &p, int right, const QPoint pt);

private:

    /**
     * Computes an caches the size of the label text.
     */
    void compute_text_size(QPainter &p);

private slots:
	void on_text_changed(const QString &text);

	void on_colour_changed(const QColor &colour);

signals:
	void visibility_changed();
	void text_changed();	
	void colour_changed();

protected:
	pv::view::View *_view;
    pv::view::Viewport *_viewport;

	QString _name;
	QColor _colour;
	int _v_offset;
    int _type;
    std::list<int> _index_list;
    int _sec_index;
    int _old_v_offset;
    int _totalHeight;
    int _typeWidth;

    QSizeF _text_size;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_VIEW_TRACE_H
