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


#include "devicemanager.h"
#include "sigsession.h"

#include <cassert>
#include <sstream>
#include <stdexcept>
#include <string>

#include <QtGui/QApplication>
#include <QObject>
#include <QDebug>
#include <QDir>

#include <libsigrok4DSLogic/libsigrok.h>

using namespace std;
char config_path[256];

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

const std::list<sr_dev_inst*>& DeviceManager::devices() const
{
	return _devices;
}

int DeviceManager::use_device(sr_dev_inst *sdi, SigSession *owner)
{
	assert(sdi);
	assert(owner);

    if (sr_dev_open(sdi) != SR_OK)
        return SR_ERR;

    _used_devices[sdi] = owner;
    return SR_OK;
}

void DeviceManager::release_device(sr_dev_inst *sdi)
{
	assert(sdi);

	// Notify the owner, and removed the device from the used device list
	_used_devices[sdi]->release_device(sdi);
	_used_devices.erase(sdi);

	sr_dev_close(sdi);
}

list<sr_dev_inst*> DeviceManager::driver_scan(
	struct sr_dev_driver *const driver, GSList *const drvopts)
{
	list<sr_dev_inst*> driver_devices;

	assert(driver);

	// Remove any device instances from this driver from the device
	// list. They will not be valid after the scan.
	list<sr_dev_inst*>::iterator i = _devices.begin();
	while (i != _devices.end()) {
		if ((*i)->driver == driver)
			i = _devices.erase(i);
		else
			i++;
	}

	// Release this driver and all it's attached devices
	release_driver(driver);

    // Check If DSLogic driver
    if (strcmp(driver->name, "DSLogic") == 0) {
        QDir dir(QApplication::applicationDirPath());
        if (!dir.cd("res"))
            return driver_devices;
        std::string str = dir.absolutePath().toStdString() + "/";
        strcpy(config_path, str.c_str());
    }

	// Do the scan
	GSList *const devices = sr_driver_scan(driver, drvopts);
	for (GSList *l = devices; l; l = l->next)
		driver_devices.push_back((sr_dev_inst*)l->data);
	g_slist_free(devices);
	driver_devices.sort(compare_devices);

	// Add the scanned devices to the main list
	_devices.insert(_devices.end(), driver_devices.begin(),
		driver_devices.end());
	_devices.sort(compare_devices);

	return driver_devices;
}

string DeviceManager::format_device_title(const sr_dev_inst *const sdi)
{
	ostringstream s;

	assert(sdi);

	if (sdi->vendor && sdi->vendor[0]) {
		s << sdi->vendor;
		if ((sdi->model && sdi->model[0]) ||
			(sdi->version && sdi->version[0]))
			s << ' ';
	}

	if (sdi->model && sdi->model[0]) {
		s << sdi->model;
		if (sdi->version && sdi->version[0])
			s << ' ';
	}

	if (sdi->version && sdi->version[0])
		s << sdi->version;

	return s.str();
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
	for (map<sr_dev_inst*, SigSession*>::iterator i = _used_devices.begin();
        i != _used_devices.end();)
        release_device((*i++).first);

	_used_devices.clear();

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
	assert(driver);
//    map<sr_dev_inst*, SigSession*>::iterator i = _used_devices.begin();
//    while(i != _used_devices.end()) {
//        if((*i).first->driver == driver)
//        {
//            // Notify the current owner of the device
//            (*i).second->release_device((*i).first);

//            // Close the device instance
//            sr_dev_close((*i).first);

//            // Remove it from the used device list
//            i = _used_devices.erase(i);
//        } else {
//            i++;
//        }
//    }
    for (map<sr_dev_inst*, SigSession*>::iterator i = _used_devices.begin();
        i != _used_devices.end();)
        if((*i).first->driver == driver)
        {
            // Notify the current owner of the device
            (*i).second->release_device((*i).first);

            // Close the device instance
            sr_dev_close((*i).first);

            // Remove it from the used device list
            _used_devices.erase(i++);
        } else {
            i++;
        }

	// Clear all the old device instances from this driver
	sr_dev_clear(driver);
}

bool DeviceManager::compare_devices(const sr_dev_inst *const a,
	const sr_dev_inst *const b)
{
	return format_device_title(a).compare(format_device_title(b)) < 0;
}

int DeviceManager::test_device(sr_dev_inst *sdi)
{
    assert(sdi);
    return sdi->driver->dev_test(sdi);
}

} // namespace pv
