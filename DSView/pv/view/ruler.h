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


#ifndef DSVIEW_PV_VIEW_RULER_H
#define DSVIEW_PV_VIEW_RULER_H

#include <QWidget>
#include <stdint.h>

namespace pv {
namespace view {

class TimeMarker;
class View;

class Ruler : public QWidget
{
	Q_OBJECT

private:
	static const int MinorTickSubdivision;
	static const int ScaleUnits[3];
    static const int MinPeriodScale;

	static const QString SIPrefixes[9];
    static const QString FreqPrefixes[9];
	static const int FirstSIPrefixPower;
    static const int pricision;

	static const int HoverArrowSize;
    static const int CursorSelWidth;

    static const QColor dsBlue;
    static const QColor dsYellow;
    static const QColor dsRed;
    static const QColor dsGreen;
    static const QColor RULER_COLOR;

public:
    static const QColor CursorColor[8];
    static const QColor HitColor;
    static const QColor WarnColor;

public:
	Ruler(View &parent);

    static QString format_time(double t, int prefix,
        unsigned precision = pricision);
    static QString format_freq(double period, unsigned precision = pricision);
    QString format_time(double t);
    static QString format_real_time(uint64_t delta_index, uint64_t sample_rate);
    static QString format_real_freq(uint64_t delta_index, uint64_t sample_rate);

    TimeMarker* get_grabbed_cursor();
    void set_grabbed_cursor(TimeMarker* grabbed_marker);
    void rel_grabbed_cursor();

    const double get_min_period() const;

private:
	void paintEvent(QPaintEvent *event);

	void mouseMoveEvent(QMouseEvent *e);
	void mousePressEvent(QMouseEvent *e);
    void mouseReleaseEvent(QMouseEvent *event);
    void leaveEvent(QEvent *);

private:
    void draw_tick_mark(QPainter &p);
    void draw_logic_tick_mark(QPainter &p);
	/**
	 * Draw a hover arrow under the cursor position.
	 */
	void draw_hover_mark(QPainter &p);

    void draw_cursor_sel(QPainter &p);

    int in_cursor_sel_rect(QPointF pos);

    QRectF get_cursor_sel_rect(int index);

private slots:
	void hover_point_changed();

private:
	View &_view;

    bool _cursor_sel_visible;
    bool _cursor_go_visible;
    int _cursor_sel_x;

	TimeMarker *_grabbed_marker;

    double _min_period;

    unsigned int _cur_prefix;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_VIEW_RULER_H
