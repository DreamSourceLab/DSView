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


#ifndef DSVIEW_PV_LISSAJOUSTRACE_H
#define DSVIEW_PV_LISSAJOUSTRACE_H

#include "trace.h"

#include <boost/shared_ptr.hpp>

namespace pv {

namespace data {
class Logic;
class Dso;
class Analog;
class DsoSnapshot;
}

namespace view {

class LissajousTrace : public Trace
{
    Q_OBJECT

private:
    static const int DIV_NUM = 10;

public:
    LissajousTrace(bool enable,
                   boost::shared_ptr<pv::data::Dso> data,
                   int xIndex, int yIndex, int percent);

    virtual ~LissajousTrace();

    bool enabled() const;
    void set_enable(bool enable);
    int xIndex() const;
    int yIndex() const;
    int percent() const;

    boost::shared_ptr<pv::data::Dso> get_data() const;
    void set_data(boost::shared_ptr<pv::data::Dso> data);

    int rows_size();

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

    void paint_label(QPainter &p, int right, const QPoint pt, QColor fore);

private:
    boost::shared_ptr<pv::data::Dso> _data;

    bool _enable;
    int _xIndex;
    int _yIndex;
    int _percent;
    QRect _border;
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_LISSAJOUSTRACE_H
