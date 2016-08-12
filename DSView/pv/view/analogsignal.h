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

	virtual ~AnalogSignal();

    boost::shared_ptr<pv::data::SignalData> data() const;

	void set_scale(float scale);

	/**
	 * Paints the signal with a QPainter
	 * @param p the QPainter to paint into.
	 * @param left the x-coordinate of the left edge of the signal.
	 * @param right the x-coordinate of the right edge of the signal.
	 **/
    void paint_mid(QPainter &p, int left, int right);

    const std::vector< std::pair<uint64_t, bool> > cur_edges() const;

private:
	void paint_trace(QPainter &p,
		const boost::shared_ptr<pv::data::AnalogSnapshot> &snapshot,
		int y, int left, const int64_t start, const int64_t end,
		const double pixels_offset, const double samples_per_pixel);

	void paint_envelope(QPainter &p,
		const boost::shared_ptr<pv::data::AnalogSnapshot> &snapshot,
		int y, int left, const int64_t start, const int64_t end,
		const double pixels_offset, const double samples_per_pixel);

private:
	boost::shared_ptr<pv::data::Analog> _data;
	float _scale;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_ANALOGSIGNAL_H
