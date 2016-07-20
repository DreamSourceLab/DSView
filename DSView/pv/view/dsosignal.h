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


#ifndef DSVIEW_PV_DSOSIGNAL_H
#define DSVIEW_PV_DSOSIGNAL_H

#include "signal.h"

#include <boost/shared_ptr.hpp>

namespace pv {

namespace data {
class Logic;
class Dso;
class Analog;
class DsoSnapshot;
}

namespace view {

class DsoSignal : public Signal
{
private:
	static const QColor SignalColours[4];
	static const float EnvelopeThreshold;

    static const int HitCursorMargin = 3;
    static const uint64_t vDialValueCount = 8;
    static const uint64_t vDialValueStep = 1000;
    static const uint64_t vDialUnitCount = 2;
    static const uint64_t hDialValueCount = 28;
    static const uint64_t hDialValueStep = 1000;
    static const uint64_t hDialUnitCount = 4;
    static const uint64_t vDialValue[vDialValueCount];
    static const QString vDialUnit[vDialUnitCount];

    static const uint64_t hDialValue[hDialValueCount];
    static const QString hDialUnit[hDialUnitCount];

    static const int UpMargin = 30;
    static const int DownMargin = 0;
    static const int RightMargin = 30;

    static const uint8_t DefaultBits = 8;
    static const int TrigMargin = 16;
    static const int RefreshShort = 200;
    static const int RefreshLong = 800;

public:
    enum DsoSetRegions {
        DSO_NONE = -1,
        DSO_VDIAL,
        DSO_HDIAL,
        DSO_VDEC,
        DSO_VINC,
        DSO_HDEC,
        DSO_HINC,
        DSO_CHEN,
        DSO_ACDC,
        DSO_X1,
        DSO_X10,
        DSO_X100,
    };

private:
    static const uint16_t MS_RectRad = 5;
    static const uint16_t MS_IconSize = 16;
    static const uint16_t MS_RectWidth = 120;
    static const uint16_t MS_RectMargin = 10;
    static const uint16_t MS_RectHeight = 25;

public:
    DsoSignal(boost::shared_ptr<pv::device::DevInst> dev_inst,
              boost::shared_ptr<pv::data::Dso> data,
              sr_channel *probe);

    virtual ~DsoSignal();

    boost::shared_ptr<pv::data::SignalData> data() const;
    boost::shared_ptr<pv::data::Dso> dso_data() const;
    void set_viewport(pv::view::Viewport *viewport);

	void set_scale(float scale);
    float get_scale();

    /**
     *
     */
    void set_enable(bool enable);
    bool get_vDialActive() const;
    void set_vDialActive(bool active);
    bool get_hDialActive() const;
    void set_hDialActive(bool active);
    bool go_vDialPre();
    bool go_vDialNext();
    bool go_hDialPre(bool setted);
    bool go_hDialNext(bool setted);
    bool go_hDialCur();
    uint64_t get_vDialValue() const;
    uint64_t get_hDialValue() const;
    uint16_t get_vDialSel() const;
    uint16_t get_hDialSel() const;
    uint8_t get_acCoupling() const;
    void set_acCoupling(uint8_t coupling);
    void set_trig_vpos(int pos, bool delta_change);
    int get_trig_vpos() const;
    void set_trig_vrate(double rate);
    double get_trig_vrate() const;
    void set_factor(uint64_t factor);
    uint64_t get_factor();

    bool load_settings();
    int commit_settings();

    /**
      *
      */
    bool measure(const QPointF &p);
    bool get_hover(uint64_t &index, QPointF &p, double &value);

    /**
      * auto set the vertical and Horizontal scale
      */
    void auto_set();

    /**
     * Gets the mid-Y position of this signal.
     */
    int get_zero_vpos();
    double get_zero_vrate();
    double get_zero_value();
    /**
     * Sets the mid-Y position of this signal.
     */
    void set_zero_vpos(int pos);
    void set_zero_vrate(double rate);
    void update_offset();

    /**
     * Paints the background layer of the trace with a QPainter
     * @param p the QPainter to paint into.
     * @param left the x-coordinate of the left edge of the signal
     * @param right the x-coordinate of the right edge of the signal
     **/
    void paint_back(QPainter &p, int left, int right);

	/**
	 * Paints the signal with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal.
	 * @param right the x-coordinate of the right edge of the signal.
	 **/
    void paint_mid(QPainter &p, int left, int right);

    /**
     * Paints the signal with a QPainter
     * @param p the QPainter to paint into.
     * @param left the x-coordinate of the left edge of the signal.
     * @param right the x-coordinate of the right edge of the signal.
     **/
    void paint_fore(QPainter &p, int left, int right);

    const std::vector< std::pair<uint64_t, bool> > cur_edges() const;

    QRect get_view_rect() const;

    QRectF get_trig_rect(int left, int right) const;

    void set_ms_show(bool show);
    bool get_ms_show() const;
    bool get_ms_show_hover() const;
    bool get_ms_gear_hover() const;
    void set_ms_en(int index, bool en);
    bool get_ms_en(int index) const;
    QString get_ms_string(int index)  const;

    QRectF get_rect(DsoSetRegions type, int y, int right);

    bool mouse_double_click(int right, const QPoint pt);

    bool mouse_press(int right, const QPoint pt);

    bool mouse_wheel(int right, const QPoint pt, const int shift);

protected:
    void paint_type_options(QPainter &p, int right, const QPoint pt);

private:
	void paint_trace(QPainter &p,
        const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot,
		int y, int left, const int64_t start, const int64_t end,
        const double pixels_offset, const double samples_per_pixel,
        uint64_t num_channels);

	void paint_envelope(QPainter &p,
        const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot,
		int y, int left, const int64_t start, const int64_t end,
        const double pixels_offset, const double samples_per_pixel,
        uint64_t num_channels);

    void paint_measure(QPainter &p);

private:
    boost::shared_ptr<pv::data::Dso> _data;
	float _scale;

    dslDial *_vDial;
    dslDial *_hDial;
    bool _vDialActive;
    bool _hDialActive;
    uint8_t _acCoupling;
    uint8_t _bits;

    int _trig_value;
    double _trig_delta;
    double _zero_vrate;
    float _zero_value;

    uint8_t _max;
    uint8_t _min;
    double _period;
    bool _autoV;
    bool _autoH;

    bool _hover_en;
    uint64_t _hover_index;
    QPointF _hover_point;
    double _hover_value;

    QRect _ms_gear_rect;
    QRect _ms_show_rect;
    QRect _ms_rect[DSO_MS_END-DSO_MS_BEGIN];
    bool _ms_gear_hover;
    bool _ms_show_hover;
    bool _ms_show;
    bool _ms_en[DSO_MS_END-DSO_MS_BEGIN];
    QString _ms_string[DSO_MS_END-DSO_MS_BEGIN];
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_DSOSIGNAL_H
