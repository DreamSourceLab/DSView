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


#ifndef DSVIEW_PV_ANALOGSIGNAL_H
#define DSVIEW_PV_ANALOGSIGNAL_H

#include "signal.h"

namespace pv {

namespace data {
class AnalogSnapshot;
}

namespace view {

//when device is data acquisition model, to draw signal trace
//created by SigSession
class AnalogSignal : public Signal
{
    Q_OBJECT

private:
	static const QColor SignalColours[4];
	static const float EnvelopeThreshold;
    static const int NumSpanY = 5;
    static const int NumMiniSpanY = 5;
    static const int NumSpanX = 10;
    static const int HoverPointSize = 2;
    static const uint8_t DefaultBits = 8;

public:
    AnalogSignal(data::AnalogSnapshot *data,
                 sr_channel *probe);

    AnalogSignal(view::AnalogSignal* s, data::AnalogSnapshot *data,  sr_channel *probe);

	virtual ~AnalogSignal();

    inline void set_scale(int height){
        _scale = height / (_ref_max - _ref_min);
    }

    inline float get_scale(){
        return _scale;
    }

    inline int get_bits(){
        return _bits;
    }

    inline double get_ref_min(){
        return _ref_min;
    }

    inline double get_ref_max(){
        return _ref_max;
    }

    inline data::AnalogSnapshot* data(){
        return _data;
    }

    void set_data(data::AnalogSnapshot *data);

    int get_hw_offset();
    int commit_settings();

    bool measure(const QPointF &p);
    bool get_hover(uint64_t &index, QPointF &p, double &value);
    QPointF get_point(uint64_t index, float &value);
    QString get_voltage(double v, int p, bool scaled = false);

    /**
     * Probe options
     **/
    uint64_t get_vdiv();
    uint8_t get_acCoupling();
    bool get_mapDefault();
    QString get_mapUnit();
    double get_mapMin();
    double get_mapMax();
    uint64_t get_factor();
    
    void set_zero_vpos(int pos);
    int get_zero_vpos();
    void set_zero_ratio(double ratio);
    double get_zero_ratio();

    inline int get_zero_offset(){
        return _zero_offset;
    }
    
    int ratio2value(double ratio);
    int ratio2pos(double ratio);
    double value2ratio(int value);
    double pos2ratio(int pos);

    /**
     * Event
     **/
    void resize();

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

private:
    void paint_trace(QPainter &p,
                     const pv::data::AnalogSnapshot *snapshot,
                     int zeroY, const int start_pixel,
                     const uint64_t start_index, const int64_t sample_count,
                     const double samples_per_pixel, const int order,
                     const float top, const float bottom, const int width);

    void paint_envelope(QPainter &p,
                        const pv::data::AnalogSnapshot *snapshot,
                        int zeroY, const int start_pixel,
                        const uint64_t start_index, const int64_t sample_count,
                        const double samples_per_pixel, const int order,
                        const float top, const float bottom, const int width);

    void paint_hover_measure(QPainter &p, QColor fore, QColor back);

private:
	pv::data::AnalogSnapshot *_data;

    QRectF *_rects;

	float _scale;
    double _zero_vrate;
    int _zero_offset;
    int _bits;
    double _ref_min;
    double _ref_max;

    bool _hover_en;
    uint64_t _hover_index;
    QPointF _hover_point;
    float _hover_value;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_ANALOGSIGNAL_H
