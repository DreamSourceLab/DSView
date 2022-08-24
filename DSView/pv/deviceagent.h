/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 * 
 * Copyright (C) 2022 DreamSourceLab <support@dreamsourcelab.com>
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

#ifndef DEVICE_AGENT_H
#define DEVICE_AGENT_H
 
#include <glib.h>
#include <stdint.h>
#include <libsigrok.h>
#include <QString>
#include <QObject>

class DeviceAgent: public QObject
{
    Q_OBJECT

signals:
    void device_updated();
	void config_changed();

public:
    DeviceAgent();

    void update();

    inline bool have_instance(){
        return _dev_handle != NULL;
    }

    inline QString name(){
        return _dev_name;
    }

    inline bool isFile(){
        return _dev_type == DEV_TYPE_FILELOG;
    }

    inline bool isDemo(){
        return _dev_type == DEV_TYPE_DEMO;
    }

    inline bool isHardware(){
        return _dev_type == DEV_TYPE_USB;
    }

    GVariant* get_config(const sr_channel *ch, const sr_channel_group *group, int key);

    bool set_config(sr_channel *ch, sr_channel_group *group, int key, GVariant *data);

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
     * @brief Gets the time base from the driver.
     *
     * @return The returned time base from the driver, or 0 if the
     * 	time base could not be read.
     */
    uint64_t get_time_base();

     /**
     * @brief Gets the sample time from the driver.
     *
     * @return The returned sample time from the driver, or 0 if the
     * 	sample time could not be read.
     */
    double get_sample_time();

    /**
     * @brief Gets the device mode list from the driver.
     *
     * @return The returned device mode list from the driver, or NULL if the
     * 	mode list could not be read.
     */
    const GSList *get_dev_mode_list();

    /**
     * Check whether the trigger exists
     */
    bool is_trigger_enabled();

    /**
     * Start collect data.
     */
    bool start();

    /**
     * Stop collect
     */
    bool stop();


private:
    ds_device_handle _dev_handle;
    int         _dev_type;
    QString     _dev_name;


};

#endif