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
 

#include <QColor>
#include <QPainter>
#include <QPen>
#include <QRect>
#include <QString>

#include <stdint.h>
#include <list>

#include <libsigrok.h> 
#include "trace.h"

namespace pv {

namespace data {
class SignalData;
}

class SigSession;

namespace view {

//draw signal trace base class
class Signal : public Trace
{
    Q_OBJECT

signals:
    void sig_released(void *o);

protected:
    Signal(sr_channel * const probe);

    /**
     * Copy constructor
     */
    Signal(const Signal &s, sr_channel * const probe);

public: 
    /**
     * Returns true if the trace is visible and enabled.
     */
    bool enabled();

    /**
     * Sets the name of the signal.
     */
    void set_name(QString name);


protected: 
    sr_channel *const _probe;
    SigSession      *session;    
};

} // namespace view
} // namespace pv

#endif // DSVIEW_PV_SIGNAL_H
