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


#ifndef DSLOGIC_PV_LOGICSIGNAL_H
#define DSLOGIC_PV_LOGICSIGNAL_H

#include "signal.h"
#include "../decoder/decoder.h"

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

    static const QColor SignalColours[8];

    static const int StateHeight;
    static const int StateRound;

public:
	LogicSignal(QString name,
		boost::shared_ptr<pv::data::Logic> data,
        int probe_index, int order);

	virtual ~LogicSignal();

    void set_data(boost::shared_ptr<pv::data::Logic> _logic_data,
                  boost::shared_ptr<pv::data::Dso> _dso_data,
                  boost::shared_ptr<pv::data::Analog> _analog_data,
                  boost::shared_ptr<pv::data::Group> _group_data);
	/**
	 * Paints the signal with a QPainter
	 * @param p the QPainter to paint into.
	 * @param y the y-coordinate to draw the signal at.
	 * @param left the x-coordinate of the left edge of the signal.
	 * @param right the x-coordinate of the right edge of the signal.
	 * @param scale the scale in seconds per pixel.
	 * @param offset the time to show at the left hand edge of
	 *   the view in seconds.
	 **/
	void paint(QPainter &p, int y, int left, int right, double scale,
		double offset);

    const std::vector< std::pair<uint64_t, bool> > cur_edges() const;

    void set_decoder(pv::decoder::Decoder *decoder);

    pv::decoder::Decoder* get_decoder();

    void del_decoder();

private:

	void paint_caps(QPainter &p, QLineF *const lines,
        std::vector< std::pair<uint64_t, bool> > &edges,
		bool level, double samples_per_pixel, double pixels_offset,
		float x_offset, float y_offset);

private:
	int _probe_index;
	boost::shared_ptr<pv::data::Logic> _data;
    std::vector< std::pair<uint64_t, bool> > _cur_edges;

    bool _need_decode;
    pv::decoder::Decoder * _decoder;
    std::vector<pv::decoder::ds_view_state > _cur_states;
    std::vector< QColor > _color_table;
    std::vector< QString > _state_table;
};

} // namespace view
} // namespace pv

#endif // DSLOGIC_PV_LOGICSIGNAL_H
