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
#include "../dstimer.h"
  
namespace pv {
namespace data {
class DsoSnapshot;
}

namespace view {

//when device is oscilloscope model,to draw trace
//created by SigSession
class DsoSignal : public Signal
{
    Q_OBJECT

public:
    static const int UpMargin = 30;
    static const int DownMargin = 0;
    static const int RightMargin = 30;
    static const float EnvelopeThreshold;
    static const int HoverPointSize = 2;
    static const int RefreshShort = 200;

private:
	static const QColor SignalColours[4];
    static const int HitCursorMargin = 3;
    static const uint64_t vDialValueStep = 1000;
    static const uint64_t vDialUnitCount = 2;
    static const QString vDialUnit[vDialUnitCount];

    static const uint8_t DefaultBits = 8;
    static const int TrigMargin = 16;
    static const int RefreshLong = 800;
    static const int AutoTime = 10000;
    static const int AutoLock = 3;

    static const int TrigHRng = 2;

public:
    enum DsoSetRegions {
        DSO_NONE = -1,
        DSO_VDIAL,
        DSO_CHEN,
        DSO_ACDC,
        DSO_AUTO,
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
    DsoSignal(pv::data::DsoSnapshot *data,
              sr_channel *probe);

    virtual ~DsoSignal();

    inline data::DsoSnapshot* data(){
        return _data;
    }

    void set_data(data::DsoSnapshot *data);

    void set_scale(int height);

    inline float get_scale(){
        return _scale;
    }

    inline uint8_t get_bits(){
        return _bits;
    }

    inline double get_ref_min(){
        return _ref_min;
    }

    inline double get_ref_max(){
        return _ref_max;
    }

    inline int get_name_width(){
        return 0;
    }

    void set_enable(bool enable);

    inline bool get_vDialActive(){
        return _vDialActive;
    }

    void set_vDialActive(bool active);
    bool go_vDialPre(bool manul);
    bool go_vDialNext(bool manul); 

    inline dslDial *get_vDial(){
        return _vDial;
    }

    uint64_t get_vDialValue();
    uint16_t get_vDialSel();

    inline uint8_t get_acCoupling(){
        return _acCoupling;
    }

    void set_acCoupling(uint8_t coupling);

    void set_trig_vpos(int pos, bool delta_change = true);
    void set_trig_ratio(double ratio, bool delta_change = true);
    double get_trig_vrate();

    void set_factor(uint64_t factor);
    uint64_t get_factor();

    inline void set_show(bool show){
        _show = show;
    }

    inline bool show(){
        return _show;
    }

    inline void set_mValid(bool valid){
        _mValid = valid;
    }

    bool load_settings();
    int commit_settings();

    /**
      *
      */
    bool measure(const QPointF &p);
    bool get_hover(uint64_t &index, QPointF &p, double &value);
    QPointF get_point(uint64_t index, float &value);

    /**
      * auto set the vertical and Horizontal scale
      */
    void auto_start();
    void autoV_end();
    void autoH_end();
    void auto_end();

    /**
     * Gets the mid-Y position of this signal.
     */
    int get_zero_vpos();
    double get_zero_ratio();
    int get_hw_offset();
    /**
     * Sets the mid-Y position of this signal.
     */
    void set_zero_vpos(int pos);
    void set_zero_ratio(double ratio);
    double get_voltage(uint64_t index);
    QString get_voltage(double v, int p, bool scaled = false);
    QString get_time(double t);

    /**
     *
     */
    int ratio2value(double ratio);
    int ratio2pos(double ratio);
    double value2ratio(int value);
    double pos2ratio(int pos);

    /**
     * paint prepare
     **/
    void paint_prepare();

    /**
     * Paints the background layer of the trace with a QPainter
     * @param p the QPainter to paint into.
     * @param left the x-coordinate of the left edge of the signal
     * @param right the x-coordinate of the right edge of the signal
     **/
    void paint_back(QPainter &p, int left, int right, QColor fore, QColor back);

	/**
	 * Paints the signal with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal.
	 * @param right the x-coordinate of the right edge of the signal.
	 **/
    void paint_mid(QPainter &p, int left, int right, QColor fore, QColor back);

    /**
     * Paints the signal with a QPainter
     * @param p the QPainter to paint into.
     * @param left the x-coordinate of the left edge of the signal.
     * @param right the x-coordinate of the right edge of the signal.
     **/
    void paint_fore(QPainter &p, int left, int right, QColor fore, QColor back);

    QRect get_view_rect();

    QRectF get_trig_rect(int left, int right);

    QString get_measure(enum DSO_MEASURE_TYPE type);

    QRectF get_rect(DsoSetRegions type, int y, int right);

    bool mouse_press(int right, const QPoint pt);

    bool mouse_wheel(int right, const QPoint pt, const int shift);

    inline void set_stop_scale(float v){
        _stop_scale = v;
    }

protected:
    void paint_type_options(QPainter &p, int right, const QPoint pt, QColor fore);

private:
    void paint_trace(QPainter &p,
        const pv::data::DsoSnapshot* snapshot,
        int zeroY, int left, const int64_t start, const int64_t end, int hw_offset,
        const double pixels_offset, const double samples_per_pixel,
        uint64_t num_channels);

    void paint_envelope(QPainter &p,
        const pv::data::DsoSnapshot *snapshot,
        int zeroY, int left, const int64_t start, const int64_t end, int hw_offset,
        const double pixels_offset, const double samples_per_pixel,
        uint64_t num_channels);

    void paint_hover_measure(QPainter &p, QColor fore, QColor back);
    void auto_set();

    void call_auto_end();

private:
    pv::data::DsoSnapshot *_data;
	float _scale;
    float _stop_scale = 1;
    bool _en_lock;
    bool _show;

    dslDial *_vDial;
    bool _vDialActive;
    uint8_t _acCoupling;
    uint8_t _bits;
    double _ref_min;
    double _ref_max;

    int _trig_value;
    double _trig_delta;
    int _zero_offset;

    bool _mValid;
    uint8_t _max;
    uint8_t _min;
    double _period;
    bool _level_valid;
    uint8_t _high;
    uint8_t _low;
    double _rms;
    double _mean;
    double _rise_time;
    double _fall_time;
    double _high_time;
    double _burst_time;
    uint32_t _pcount;

    bool _autoV;
    bool _autoH;
    bool _autoV_over;
    uint16_t _auto_cnt;

    bool _hover_en;
    uint64_t _hover_index;
    QPointF _hover_point;
    float _hover_value;
    DsTimer _end_timer;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_DSOSIGNAL_H
