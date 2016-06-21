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


#ifndef DSVIEW_PV_SIGNAL_H
#define DSVIEW_PV_SIGNAL_H

#include <boost/shared_ptr.hpp>

#include <QColor>
#include <QPainter>
#include <QPen>
#include <QRect>
#include <QString>

#include <stdint.h>
#include <list>

#include "libsigrok4DSL/libsigrok.h"
#include "trace.h"

namespace pv {

namespace data {
class SignalData;
}

namespace device {
class DevInst;
}

namespace view {

class Signal : public Trace
{
private:


protected:
    Signal(boost::shared_ptr<pv::device::DevInst> dev_inst,
           sr_channel * const probe);

    /**
     * Copy constructor
     */
    Signal(const Signal &s, sr_channel * const probe);

public:
    virtual boost::shared_ptr<pv::data::SignalData> data() const = 0;

    virtual const std::vector< std::pair<uint64_t, bool> > cur_edges() const = 0;

    /**
     * Returns true if the trace is visible and enabled.
     */
    bool enabled() const;

    /**
     * Sets the name of the signal.
     */
    void set_name(QString name);

	/**
	 * Paints the signal label into a QGLWidget.
	 * @param p the QPainter to paint into.
	 * @param right the x-coordinate of the right edge of the header
	 * 	area.
	 * @param hover true if the label is being hovered over by the mouse.
     * @param action mouse position for hover
	 */
    //virtual void paint_label(QPainter &p, int right, bool hover, int action);

    boost::shared_ptr<device::DevInst> get_device() const;

protected:

	/**
	 * Paints a zero axis across the viewport.
	 * @param p the QPainter to paint into.
	 * @param y the y-offset of the axis.
	 * @param left the x-coordinate of the left edge of the view.
	 * @param right the x-coordinate of the right edge of the view.
	 */
	void paint_axis(QPainter &p, int y, int left, int right);

protected:
    boost::shared_ptr<pv::device::DevInst> _dev_inst;
    sr_channel *const _probe;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_SIGNAL_H
