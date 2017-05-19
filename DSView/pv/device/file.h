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

#ifndef DSVIEW_PV_DEVICE_FILE_H
#define DSVIEW_PV_DEVICE_FILE_H

#include <string>

#include <QJsonArray>
#include <QJsonDocument>

#include "devinst.h"

namespace pv {
namespace device {

class File : public DevInst
{
protected:
    File(QString path);

public:
    static File* create(QString name);

    QJsonArray get_decoders();

public:
    QString format_device_title() const;

protected:
    const QString _path;
};

} // device
} // pv

#endif // DSVIEW_PV_DEVICE_FILE_H
