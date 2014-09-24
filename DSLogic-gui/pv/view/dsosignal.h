/*
 * This file is part of the DSLogic-gui project.
 * DSLogic-gui is based on PulseView.
 *
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


#ifndef DSLOGIC_PV_DSOSIGNAL_H
#define DSLOGIC_PV_DSOSIGNAL_H

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
    static const uint64_t vDialValueCount = 10;
    static const uint64_t vDialValueStep = 1000;
    static const uint64_t vDialUnitCount = 2;
    static const uint64_t hDialValueCount = 22;
    static const uint64_t hDialValueStep = 1000;
    static const uint64_t hDialUnitCount = 4;
    static const uint64_t vDialValue[vDialValueCount];
    static const QString vDialUnit[vDialUnitCount];

    static const uint64_t hDialValue[hDialValueCount];
    static const QString hDialUnit[hDialUnitCount];

    static const int UpMargin;
    static const int DownMargin;
    static const int RightMargin;

public:
    DsoSignal(boost::shared_ptr<pv::device::DevInst> dev_inst,
              boost::shared_ptr<pv::data::Dso> data,
              const sr_channel * const probe);

    virtual ~DsoSignal();

    boost::shared_ptr<pv::data::SignalData> data() const;

	void set_scale(float scale);

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
    bool go_hDialPre();
    bool go_hDialNext();
    uint64_t get_vDialValue() const;
    uint64_t get_hDialValue() const;
    uint16_t get_vDialSel() const;
    uint16_t get_hDialSel() const;
    bool get_acCoupling() const;
    void set_acCoupling(bool coupling);
    void set_trig_vpos(int pos);
    int get_trig_vpos() const;

    /**
     * Gets the mid-Y position of this signal.
     */
    int get_zeroPos();

    /**
     * Sets the mid-Y position of this signal.
     */
    void set_zeroPos(int pos);

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

    QRectF get_view_rect() const;

    QRectF get_trig_rect(int left, int right) const;

protected:
    void paint_type_options(QPainter &p, int right, bool hover, int action);

private:
	void paint_trace(QPainter &p,
        const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot,
		int y, int left, const int64_t start, const int64_t end,
        const double pixels_offset, const double samples_per_pixel,
        uint64_t num_channels);

	void paint_envelope(QPainter &p,
        const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot,
		int y, int left, const int64_t start, const int64_t end,
		const double pixels_offset, const double samples_per_pixel);

private:
    boost::shared_ptr<pv::data::Dso> _data;
	float _scale;

    dslDial *_vDial;
    dslDial *_hDial;
    bool _vDialActive;
    bool _hDialActive;
    bool _acCoupling;

    double _trig_vpos;
    double _zeroPos;
};

} // namespace view
} // namespace pv

#endif // DSLOGIC_PV_DSOSIGNAL_H
