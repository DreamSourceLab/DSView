/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef DSVIEW_PV_DEVICE_SESSIONFILE_H
#define DSVIEW_PV_DEVICE_SESSIONFILE_H

#include "file.h"

namespace pv {
namespace device {

class SessionFile : public File
{
public:
    SessionFile(QString path);

	sr_dev_inst* dev_inst() const;

	virtual void use(SigSession *owner) throw(QString);

	virtual void release();

private:
	sr_dev_inst *_sdi;
};

} // device
} // pv

#endif // DSVIEW_PV_DEVICE_SESSIONFILE_H
