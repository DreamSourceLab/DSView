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


#ifndef DSVIEW_PV_LOGICSIGNAL_H
#define DSVIEW_PV_LOGICSIGNAL_H

#include "signal.h"

#include <vector>

#include <boost/shared_ptr.hpp>

namespace pv {

namespace data {
class Logic;
class Analog;
}

namespace view {

class LogicSignal : public Signal
{
private:
	static const float Oversampling;

	static const QColor EdgeColour;
	static const QColor HighColour;
	static const QColor LowColour;

    static const QColor DEFAULT_COLOR;

    static const int StateHeight;
    static const int StateRound;

    enum LogicSetRegions{
        NONTRIG = 0,
        POSTRIG,
        HIGTRIG,
        NEGTRIG,
        LOWTRIG,
        EDGTRIG,
    };

public:
    LogicSignal(boost::shared_ptr<pv::device::DevInst> dev_inst,
                boost::shared_ptr<pv::data::Logic> data,
                sr_channel *probe);

    LogicSignal(boost::shared_ptr<view::LogicSignal> s,
                boost::shared_ptr<pv::data::Logic> data,
                sr_channel *probe);

	virtual ~LogicSignal();

    const sr_channel* probe() const;

    boost::shared_ptr<pv::data::SignalData> data() const;

    boost::shared_ptr<pv::data::Logic> logic_data() const;

    /**
     *
     */
    LogicSetRegions get_trig() const;
    void set_trig(int trig);
    void commit_trig();

	/**
	 * Paints the signal with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal.
	 * @param right the x-coordinate of the right edge of the signal.
	 **/
    void paint_mid(QPainter &p, int left, int right);

    const std::vector< std::pair<uint64_t, bool> > cur_edges() const;

    bool measure(const QPointF &p, uint64_t &index0, uint64_t &index1, uint64_t &index2) const;

    bool edges(const QPointF &p, uint64_t start, uint64_t &rising, uint64_t &falling) const;

    bool mouse_press(int right, const QPoint pt);

    QRectF get_rect(LogicSetRegions type, int y, int right);

protected:
    void paint_type_options(QPainter &p, int right, const QPoint pt);

private:

	void paint_caps(QPainter &p, QLineF *const lines,
        std::vector< std::pair<uint64_t, bool> > &edges,
		bool level, double samples_per_pixel, double pixels_offset,
		float x_offset, float y_offset);

private:
	boost::shared_ptr<pv::data::Logic> _data;
    std::vector< std::pair<uint64_t, bool> > _cur_edges;
    LogicSetRegions _trig;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_LOGICSIGNAL_H
