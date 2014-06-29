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


#ifndef DSLOGIC_PV_VIEW_VIEW_H
#define DSLOGIC_PV_VIEW_VIEW_H

#include <stdint.h>

#include <QAbstractScrollArea>
#include <QSizeF>

#include "cursor.h"
#include "signal.h"

namespace pv {

class SigSession;

namespace view {

class Header;
class Ruler;
class Viewport;

class View : public QAbstractScrollArea {
	Q_OBJECT

private:
	static const int LabelMarginWidth;
	static const int RulerHeight;

	static const int MaxScrollValue;

public:
    //static const int SignalHeight;
	static const int SignalMargin;
	static const int SignalSnapGridSize;

	static const QColor CursorAreaColour;

	static const QSizeF LabelPadding;

    static const int WellPixelsPerSample;

    static const double MaxViewRate;

public:
	explicit View(SigSession &session, QWidget *parent = 0);

	SigSession& session();

	/**
	 * Returns the view time scale in seconds per pixel.
	 */
	double scale() const;

	/**
	 * Returns the time offset of the left edge of the view in
	 * seconds.
	 */
	double offset() const;
	int v_offset() const;

	void zoom(double steps);
	void zoom(double steps, int offset);

	/**
	 * Sets the scale and offset.
	 * @param scale The new view scale in seconds per pixel.
	 * @param offset The view time offset in seconds.
	 */
	void set_scale_offset(double scale, double offset);
    void set_preScale_preOffset();

	/**
	 * Returns true if cursors are displayed. false otherwise.
	 */
	bool cursors_shown() const;
    bool trig_cursor_shown() const;
    bool search_cursor_shown() const;

    int get_spanY();

    int get_signalHeight();

    int headerWidth();

    Ruler* get_ruler();

	/**
	 * Shows or hides the cursors.
	 */
	void show_cursors(bool show = true);

    const QPointF& hover_point() const;

	void normalize_layout();

    void show_trig_cursor(bool show = true);

    void show_search_cursor(bool show = true);

    /*
     * cursorList
     */
    std::list<Cursor*>& get_cursorList();
    void add_cursor(QColor color, double time);
    void del_cursor(Cursor* cursor);
    void set_cursor_middle(int index);

    Cursor* get_trig_cursor();
    Cursor* get_search_cursor();

    //void set_trig_pos(uint64_t trig_pos);
    void set_search_pos(uint64_t search_pos);

    uint64_t get_trig_pos();
    uint64_t get_search_pos();

    /*
     *
     */
    double get_minscale() const;
    double get_maxscale() const;

    void set_need_update(bool need_update);
    bool need_update() const;

    QString get_mm_width();
    QString get_mm_period();
    QString get_mm_freq();
    QString get_cm_time(int index);
    QString get_cm_delta(int index1, int index2);
    QString get_cm_delta_cnt(int index1, int index2);

    void on_mouse_moved();
    void on_cursor_moved();

    void on_state_changed(bool stop);

signals:
	void hover_point_changed();

	void signals_moved();

    void cursor_update();

    void mouse_moved();
    void cursor_moved();

private:
	void get_scroll_layout(double &length, double &offset) const;
	
	void update_scroll();

	void reset_signal_layout();

    void update_margins();

    double get_cursor_time(int index);

private:
	bool eventFilter(QObject *object, QEvent *event);

	bool viewportEvent(QEvent *e);

	void resizeEvent(QResizeEvent *e);

public slots:
    void set_measure_en(int enable);
    void hDial_changed(quint16 channel);

private slots:

	void h_scroll_value_changed(int value);
	void v_scroll_value_changed(int value);

	void signals_changed();
	void data_updated();

	void marker_time_changed();

	void on_signals_moved();

    void header_updated();

    void sample_rate_changed(quint64 sample_rate);

    void receive_data(quint64 length);

    void set_trig_pos(quint64 trig_pos);

    void vDial_changed(quint16 channel);
    void acdc_changed(quint16 channel);
    void ch_changed(quint16 channel);

private:

	SigSession &_session;

	Viewport *_viewport;
	Ruler *_ruler;
	Header *_header;

	uint64_t _data_length;

	/// The view time scale in seconds per pixel.
	double _scale;
        double _preScale;
        double _maxscale;
        double _minscale;

	/// The view time offset in seconds.
        double _offset;
        double _preOffset;

        int _spanY;
        int SignalHeight;

	int _v_offset;
	bool _updating_scroll;

        bool _need_update;

	bool _show_cursors;

        std::list<Cursor*> _cursorList;

        Cursor *_trig_cursor;
        bool _show_trig_cursor;
        uint64_t _trig_pos;
        Cursor *_search_cursor;
        bool _show_search_cursor;
        uint64_t _search_pos;

        QPointF _hover_point;
};

} // namespace view
} // namespace pv

#endif // DSLOGIC_PV_VIEW_VIEW_H
