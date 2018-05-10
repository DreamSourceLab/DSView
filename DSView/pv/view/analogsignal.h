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

#include <boost/shared_ptr.hpp>

namespace pv {

namespace data {
class Logic;
class Analog;
class AnalogSnapshot;
}

namespace view {

class AnalogSignal : public Signal
{
private:
	static const QColor SignalColours[4];
	static const float EnvelopeThreshold;
    static const int NumSpanY = 5;
    static const int NumMiniSpanY = 5;
    static const int NumSpanX = 10;
public:
    AnalogSignal(boost::shared_ptr<pv::device::DevInst> dev_inst,
                 boost::shared_ptr<pv::data::Analog> data,
                 sr_channel *probe);
    AnalogSignal(boost::shared_ptr<view::AnalogSignal> s,
                 boost::shared_ptr<pv::data::Analog> data,
                 sr_channel *probe);

	virtual ~AnalogSignal();

    boost::shared_ptr<pv::data::SignalData> data() const;

	void set_scale(float scale);
    float get_scale() const;
    int get_bits() const;
    int get_hw_offset() const;
    int commit_settings();

    /**
     * Probe options
     **/
    uint64_t get_vdiv() const;
    uint8_t get_acCoupling() const;
    QString get_mapUnit() const;
    double get_mapMin() const;
    double get_mapMax() const;

    /**
     *
     **/
    void set_zero_vpos(int pos);
    int get_zero_vpos() const;
    void set_zero_vrate(double rate, bool force_update);
    double get_zero_vrate() const;
    void update_vpos();
    void update_offset();

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
    void paint_back(QPainter &p, int left, int right);

	/**
	 * Paints the signal with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal.
	 * @param right the x-coordinate of the right edge of the signal.
	 **/
    void paint_mid(QPainter &p, int left, int right);

private:
    void paint_trace(QPainter &p,
                     const boost::shared_ptr<pv::data::AnalogSnapshot> &snapshot,
                     int zeroY, const int start_pixel,
                     const uint64_t start_index, const int64_t sample_count,
                     const double samples_per_pixel, const int order,
                     const int top, const int bottom, const int width);

    void paint_envelope(QPainter &p,
                        const boost::shared_ptr<pv::data::AnalogSnapshot> &snapshot,
                        int zeroY, const int start_pixel,
                        const uint64_t start_index, const int64_t sample_count,
                        const double samples_per_pixel, const int order,
                        const int top, const int bottom, const int width);

private:
	boost::shared_ptr<pv::data::Analog> _data;

    QRectF *_rects;

	float _scale;
    double _zero_vrate;
    int _hw_offset;
    int _bits;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_ANALOGSIGNAL_H
