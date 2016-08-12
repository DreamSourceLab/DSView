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


#ifndef DSVIEW_SV_GROUPSIGNAL_H
#define DSVIEW_SV_GROUPSIGNAL_H

#include "signal.h"
#include "../data/groupsnapshot.h"

#include <list>
#include <boost/shared_ptr.hpp>

namespace pv {

namespace data {
class Logic;
class Analog;
class Group;
class GroupSnapshot;
}

namespace view {

class GroupSignal : public Trace
{
private:
	static const QColor SignalColours[4];

	static const float EnvelopeThreshold;

    enum GroupSetRegions{
        NONEREG = -1,
        CHNLREG,
    };

public:
    GroupSignal(QString name,
        boost::shared_ptr<pv::data::Group> data,
                std::list<int> probe_index_list, int group_index);

    virtual ~GroupSignal();

    /**
     * Returns true if the trace is visible and enabled.
     */
    bool enabled() const;

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

    QRectF get_rect(GroupSetRegions type, int y, int right);

protected:
    void paint_type_options(QPainter &p, int right, const QPoint pt);

private:
	void paint_trace(QPainter &p,
        const boost::shared_ptr<pv::data::GroupSnapshot> &snapshot,
		int y, int left, const int64_t start, const int64_t end,
		const double pixels_offset, const double samples_per_pixel);

	void paint_envelope(QPainter &p,
        const boost::shared_ptr<pv::data::GroupSnapshot> &snapshot,
		int y, int left, const int64_t start, const int64_t end,
		const double pixels_offset, const double samples_per_pixel);

private:
    boost::shared_ptr<pv::data::Group> _data;
	float _scale;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_GROUPSIGNAL_H
