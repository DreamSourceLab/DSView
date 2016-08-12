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

#ifndef DSVIEW_PV_DEVICE_DEVINST_H
#define DSVIEW_PV_DEVICE_DEVINST_H

#include <string>

#include <boost/shared_ptr.hpp>

#include <QObject>

#include <glib.h>

#include <stdint.h>

struct sr_dev_inst;
struct sr_channel;
struct sr_channel_group;

namespace pv {

class SigSession;

namespace device {

class DevInst : public QObject
{
	Q_OBJECT

protected:
	DevInst();
    ~DevInst();

public:
	virtual sr_dev_inst* dev_inst() const = 0;

	virtual void use(SigSession *owner) throw(QString);

	virtual void release();

	SigSession* owner() const;

    virtual QString format_device_title() const = 0;

    GVariant* get_config(const sr_channel *ch, const sr_channel_group *group, int key);

    bool set_config(const sr_channel *ch, const sr_channel_group *group, int key, GVariant *data);

	GVariant* list_config(const sr_channel_group *group, int key);

	void enable_probe(const sr_channel *probe, bool enable = true);

	/**
	 * @brief Gets the sample limit from the driver.
	 *
	 * @return The returned sample limit from the driver, or 0 if the
	 * 	sample limit could not be read.
	 */
	uint64_t get_sample_limit();

    /**
     * @brief Gets the sample rate from the driver.
     *
     * @return The returned sample rate from the driver, or 0 if the
     * 	sample rate could not be read.
     */
    uint64_t get_sample_rate();

    /**
     * @brief Gets the sample time from the driver.
     *
     * @return The returned sample time from the driver, or 0 if the
     * 	sample time could not be read.
     */
    double get_sample_time();

    /**
     * @brief Gets the time base from the driver.
     *
     * @return The returned time base from the driver, or 0 if the
     * 	time base could not be read.
     */
    uint64_t get_time_base();

    /**
     * @brief Gets the device mode list from the driver.
     *
     * @return The returned device mode list from the driver, or NULL if the
     * 	mode list could not be read.
     */
    GSList* get_dev_mode_list();

    /**
     * @brief Get the device name from the driver
     *
     * @return device name
     */
    QString name();

	virtual bool is_trigger_enabled() const;

public:
	virtual void start();

	virtual void run();

    virtual void* get_id() const;

signals:
    void device_updated();
	void config_changed();

protected:
	SigSession *_owner;
    void *_id;
};

} // device
} // pv

#endif // DSVIEW_PV_DEVICE_DEVINST_H
