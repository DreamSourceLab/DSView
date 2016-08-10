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


#ifndef DSVIEW_PV_VIEW_VIEW_H
#define DSVIEW_PV_VIEW_VIEW_H

#include <stdint.h>

#include <set>
#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

#include <QScrollArea>
#include <QSizeF>
#include <QDateTime>
#include <QSplitter>

#include "../../extdef.h"
#include "../toolbars/samplingbar.h"
#include "../data/signaldata.h"
#include "../view/viewport.h"
#include "cursor.h"
#include "signal.h"
#include "../widgets/viewstatus.h"

namespace pv {

namespace toolbars {
    class SamplingBar;
}

class SigSession;

namespace view {

class Header;
class DevMode;
class Ruler;
class Trace;
class Viewport;

class View : public QScrollArea {
	Q_OBJECT

private:
	static const int LabelMarginWidth;
	static const int RulerHeight;

	static const int MaxScrollValue;
    static const int MaxHeightUnit;

public:
    //static const int SignalHeight;
	static const int SignalMargin;
	static const int SignalSnapGridSize;

	static const QColor CursorAreaColour;

	static const QSizeF LabelPadding;

    static const int WellPixelsPerSample = 10;
    static constexpr double MaxViewRate = 1.0;
    static const int MaxPixelsPerSample = 100;

    static const int StatusHeight = 20;

public:
    explicit View(SigSession &session, pv::toolbars::SamplingBar *sampling_bar, QWidget *parent = 0);

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

    double get_min_offset();
    double get_max_offset();

	void zoom(double steps);
	void zoom(double steps, int offset);

	/**
	 * Sets the scale and offset.
	 * @param scale The new view scale in seconds per pixel.
	 * @param offset The view time offset in seconds.
	 */
	void set_scale_offset(double scale, double offset);
    void set_preScale_preOffset();

    std::vector< boost::shared_ptr<Trace> > get_traces(int type);

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

    const QPoint& hover_point() const;

	void normalize_layout();

    void show_trig_cursor(bool show = true);

    void show_search_cursor(bool show = true);

    /*
     * cursorList
     */
    std::list<Cursor*>& get_cursorList();
    void add_cursor(QColor color, uint64_t index);
    void del_cursor(Cursor* cursor);
    void set_cursor_middle(int index);

    Cursor* get_trig_cursor();
    Cursor* get_search_cursor();

    void set_search_pos(uint64_t search_pos);

    uint64_t get_search_pos();

    /*
     *
     */
    double get_minscale() const;
    double get_maxscale() const;

    void set_update(Viewport *viewport, bool need_update);
    void set_all_update(bool need_update);

    uint64_t get_cursor_samples(int index);
    QString get_cm_time(int index);
    QString get_cm_delta(int index1, int index2);

    void on_cursor_moved();

    void on_state_changed(bool stop);

    QRect get_view_rect();
    int get_view_width();
    int get_view_height();

    void update_sample(bool instant);

    void set_sample_rate(uint64_t sample_rate, bool force = false);

    void set_sample_limit(uint64_t sample_limit, bool force = false);

    QString get_measure(QString option);

    void viewport_update();

signals:
	void hover_point_changed();

    void traces_moved();

    void cursor_update();

    void cursor_moved();

    void measure_updated();

private:
	void get_scroll_layout(double &length, double &offset) const;
	
	void update_scroll();

    void update_margins();

    static bool compare_trace_v_offsets(
        const boost::shared_ptr<pv::view::Trace> &a,
        const boost::shared_ptr<pv::view::Trace> &b);

private:
	bool eventFilter(QObject *object, QEvent *event);

	bool viewportEvent(QEvent *e);

	void resizeEvent(QResizeEvent *e);

public slots:
    void reload();
    void set_measure_en(int enable);
    void signals_changed();
    void data_updated();
    void update_scale_offset();
    void show_region(uint64_t start, uint64_t end);
    // -- calibration
    void update_calibration();
    void hide_calibration();
    void status_clear();

private slots:

	void h_scroll_value_changed(int value);
	void v_scroll_value_changed(int value);

	void marker_time_changed();

    void on_traces_moved();

    void header_updated();

    void receive_header();

    void receive_trigger(quint64 trig_pos);
    void set_trig_pos(int percent);

    void receive_end();

    void frame_began();

    // calibration for oscilloscope
    void show_calibration();
    void on_measure_updated();

    void splitterMoved(int pos, int index);

private:

	SigSession &_session;
    pv::toolbars::SamplingBar *_sampling_bar;

    QWidget *_viewcenter;
    widgets::ViewStatus *_viewbottom;
    QSplitter *_vsplitter;
    Viewport * _time_viewport;
    Viewport * _fft_viewport;
    Viewport *_active_viewport;
    std::list<Viewport *> _viewport_list;
    std::map<int, int> _trace_view_map;
	Ruler *_ruler;
	Header *_header;
    DevMode *_devmode;

	/// The view time scale in seconds per pixel.
	double _scale;
        double _preScale;
        double _maxscale;
        double _minscale;

	/// The view time offset in seconds.
        double _offset;
        double _preOffset;

        int _spanY;
        int _signalHeight;

        bool _updating_scroll;

	bool _show_cursors;

        std::list<Cursor*> _cursorList;

        Cursor *_trig_cursor;
        bool _show_trig_cursor;
        Cursor *_search_cursor;
        bool _show_search_cursor;
        uint64_t _search_pos;

        QPoint _hover_point;
    	dialogs::Calibration *_cali;
        bool _dso_auto;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_VIEW_VIEW_H
