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


#ifndef DSVIEW_PV_DEVICEMANAGER_H
#define DSVIEW_PV_DEVICEMANAGER_H

#include <glib.h>

#include <list>
#include <map>
#include <string>
#include <QObject>
#include <QString>

struct sr_context;
struct sr_dev_driver;
struct sr_dev_inst;
struct libusbhp_t;
struct libusbhp_device_t;

namespace pv {

class SigSession;

namespace device {
class DevInst;
}

using namespace pv::device;

class DeviceManager
{
private:
    DeviceManager(DeviceManager &o);

public:
	DeviceManager();

	~DeviceManager();

    inline const std::list<DevInst*>& devices(){
        return _devices;        
    }

    void add_device(DevInst *device);
    void del_device(DevInst *device);

    void driver_scan(
                std::list<DevInst*> &driver_devices,
	            struct sr_dev_driver *const driver=NULL, 
                GSList *const drvopts=NULL);

    void initAll(struct sr_context *sr_ctx);

    void UnInitAll();

private:
	void init_drivers();

	void release_devices();

    void scan_all_drivers();

	void release_driver(struct sr_dev_driver *const driver);

    static bool compare_devices(DevInst *a, DevInst *b);

private:
	struct sr_context*  _sr_ctx;
    std::list<DevInst*> _devices;
};

} // namespace pv

#endif // DSVIEW_PV_DEVICEMANAGER_H
