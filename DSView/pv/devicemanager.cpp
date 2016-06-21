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


#include "devicemanager.h"
#include "device/devinst.h"
#include "device/device.h"
#include "sigsession.h"

#include <cassert>
#include <sstream>
#include <stdexcept>
#include <string>

#include <QApplication>
#include <QObject>
#include <QDebug>
#include <QDir>

#include <boost/foreach.hpp>

#include <libsigrok4DSL/libsigrok.h>

using boost::shared_ptr;
using std::list;
using std::map;
using std::ostringstream;
using std::runtime_error;
using std::string;

namespace pv {

DeviceManager::DeviceManager(struct sr_context *sr_ctx) :
	_sr_ctx(sr_ctx)
{
	init_drivers();
	scan_all_drivers();
}

DeviceManager::~DeviceManager()
{
	release_devices();
}

const std::list<boost::shared_ptr<device::DevInst> > &DeviceManager::devices() const
{
	return _devices;
}

void DeviceManager::add_device(boost::shared_ptr<pv::device::DevInst> device)
{
    assert(device);

    if (std::find(_devices.begin(), _devices.end(), device) ==
        _devices.end())
        _devices.push_front(device);
}

std::list<boost::shared_ptr<device::DevInst> > DeviceManager::driver_scan(
	struct sr_dev_driver *const driver, GSList *const drvopts)
{
    list< shared_ptr<device::DevInst> > driver_devices;

	assert(driver);

	// Remove any device instances from this driver from the device
	// list. They will not be valid after the scan.
    list< shared_ptr<device::DevInst> >::iterator i = _devices.begin();
	while (i != _devices.end()) {
        if ((*i)->dev_inst() &&
            (*i)->dev_inst()->driver == driver) {
            (*i)->release();
			i = _devices.erase(i);
        } else {
			i++;
        }
	}

    // Clear all the old device instances from this driver
    sr_dev_clear(driver);
    //release_driver(driver);

    // Check If DSL hardware driver
    if (strncmp(driver->name, "virtual", 7)) {
        QDir dir(DS_RES_PATH);
        if (!dir.exists())
            return driver_devices;
    }

	// Do the scan
	GSList *const devices = sr_driver_scan(driver, drvopts);
	for (GSList *l = devices; l; l = l->next)
        driver_devices.push_front(shared_ptr<device::DevInst>(
                                     new device::Device((sr_dev_inst*)l->data)));
	g_slist_free(devices);
    //driver_devices.sort(compare_devices);

	// Add the scanned devices to the main list
	_devices.insert(_devices.end(), driver_devices.begin(),
		driver_devices.end());
    //_devices.sort(compare_devices);

	return driver_devices;
}

void DeviceManager::init_drivers()
{
	// Initialise all libsigrok drivers
	sr_dev_driver **const drivers = sr_driver_list();
	for (sr_dev_driver **driver = drivers; *driver; driver++) {
		if (sr_driver_init(_sr_ctx, *driver) != SR_OK) {
			throw runtime_error(
				string("Failed to initialize driver ") +
				string((*driver)->name));
		}
	}
}

void DeviceManager::release_devices()
{
    // Release all the used devices
    BOOST_FOREACH(shared_ptr<device::DevInst> dev, _devices) {
        assert(dev);
        dev->release();
    }

    // Clear all the drivers
    sr_dev_driver **const drivers = sr_driver_list();
    for (sr_dev_driver **driver = drivers; *driver; driver++)
        sr_dev_clear(*driver);
}

void DeviceManager::scan_all_drivers()
{
	// Scan all drivers for all devices.
	struct sr_dev_driver **const drivers = sr_driver_list();
	for (struct sr_dev_driver **driver = drivers; *driver; driver++)
		driver_scan(*driver);
}

void DeviceManager::release_driver(struct sr_dev_driver *const driver)
{
    BOOST_FOREACH(shared_ptr<device::DevInst> dev, _devices) {
        assert(dev);
        if(dev->dev_inst()->driver == driver)
            dev->release();
    }

    // Clear all the old device instances from this driver
    sr_dev_clear(driver);
}

bool DeviceManager::compare_devices(boost::shared_ptr<device::DevInst> a,
    boost::shared_ptr<device::DevInst> b)
{
    assert(a);
    assert(b);
    return a->format_device_title().compare(b->format_device_title()) <= 0;
}

} // namespace pv
