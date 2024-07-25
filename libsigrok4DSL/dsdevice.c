/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "libsigrok-internal.h"
#include <stdio.h>
#include <glib.h>
#include "config.h" /* Needed for HAVE_LIBUSB_1_0 and others. */
#include "log.h"
#include <string.h>
#include <assert.h>

#undef LOG_PREFIX
#define LOG_PREFIX "device: "

/**
 * @file
 *
 * Device handling in libsigrok.
 */

/**
 * @defgroup grp_devices Devices
 *
 * Device handling in libsigrok.
 *
 * @{
 */

/** @private */
SR_PRIV struct sr_channel *sr_channel_new(uint16_t index, int type, gboolean enabled, const char *name)
{
	struct sr_channel *probe;

	probe = g_try_malloc0(sizeof(struct sr_channel));
	if (probe == NULL) {
		sr_err("%s,ERROR:failed to alloc memory.", __func__);
		return NULL;
	}

	probe->index = index;
	probe->type = type;
	probe->enabled = enabled;
	if (name)
		probe->name = g_strdup(name);
    probe->vga_ptr = NULL;

	return probe;
}

/**
 * Set the name of the specified probe in the specified device.
 *
 * If the probe already has a different name assigned to it, it will be
 * removed, and the new name will be saved instead.
 *
 * @param sdi The device instance the probe is connected to.
 * @param probenum The number of the probe whose name to set.
 *                 Note that the probe numbers start at 0.
 * @param name The new name that the specified probe should get. A copy
 *             of the string is made.
 *
 * @return SR_OK on success, or SR_ERR_ARG on invalid arguments.
 *
 * @since 0.1.0 (but the API changed in 0.2.0)
 */
SR_PRIV int sr_dev_probe_name_set(const struct sr_dev_inst *sdi,
		int probenum, const char *name)
{
	GSList *l;
	struct sr_channel *probe;
	int ret;

	if (!sdi) {
		sr_err("%s: sdi was NULL", __func__);
		return SR_ERR_ARG;
	}

	ret = SR_ERR_ARG;
	for (l = sdi->channels; l; l = l->next) {
		probe = l->data;
		if (probe->index == probenum) {
			g_free(probe->name);
			probe->name = g_strdup(name);
			ret = SR_OK;
			break;
		}
	}

	return ret;
}

/**
 * Enable or disable a probe on the specified device.
 *
 * @param sdi The device instance the probe is connected to.
 * @param probenum The probe number, starting from 0.
 * @param state TRUE to enable the probe, FALSE to disable.
 *
 * @return SR_OK on success, or SR_ERR_ARG on invalid arguments.
 *
 * @since 0.2.0
 */
SR_PRIV int sr_dev_probe_enable(const struct sr_dev_inst *sdi, int probenum,
		gboolean state)
{
	GSList *l;
	struct sr_channel *probe;
	int ret;

	if (!sdi)
		return SR_ERR_ARG;

	ret = SR_ERR_ARG;
	for (l = sdi->channels; l; l = l->next) {
		probe = l->data;
		if (probe->index == probenum) {
			probe->enabled = state;
			ret = SR_OK;
			break;
		}
	}

	return ret;
}

/**
 * Add a trigger to the specified device (and the specified probe).
 *
 * If the specified probe of this device already has a trigger, it will
 * be silently replaced.
 *
 * @param sdi Must not be NULL.
 * @param probenum The probe number, starting from 0.
 * @param trigger Trigger string, in the format used by sigrok-cli
 *
 * @return SR_OK on success, or SR_ERR_ARG on invalid arguments.
 *
 * @since 0.1.0 (but the API changed in 0.2.0)
 */
SR_PRIV int sr_dev_trigger_set(const struct sr_dev_inst *sdi, uint16_t probenum,
		const char *trigger)
{
	GSList *l;
	struct sr_channel *probe;
	int ret;

	if (!sdi)
		return SR_ERR_ARG;

	ret = SR_ERR_ARG;
	for (l = sdi->channels; l; l = l->next) {
		probe = l->data;
		if (probe->index == probenum) {
			/* If the probe already has a trigger, kill it first. */
            safe_free(probe->trigger);
            probe->trigger = g_strdup(trigger);
			ret = SR_OK;
			break;
		}
	}

	return ret;
}

/** @private */
SR_PRIV struct sr_dev_inst *sr_dev_inst_new(int mode, int status,
		const char *vendor, const char *model, const char *version)
{
	struct sr_dev_inst *sdi;
	if (!(sdi = g_try_malloc0(sizeof(struct sr_dev_inst)))) {
		sr_err("%s,ERROR:failed to alloc memory.", __func__);
		return NULL;
	}
	memset(sdi, 0, sizeof(struct sr_dev_inst));
 
    sdi->mode = mode;
	sdi->status = status;
    sdi->handle = (ds_device_handle)sdi;

	if (vendor != NULL){
		sdi->vendor = g_strdup(vendor);
	}
	if (version != NULL){
		sdi->version = g_strdup(version);
	}

	if (model && *model){ 
		sdi->name = g_strdup(model);
	}

	return sdi;
}

/** @private */
SR_PRIV void sr_dev_probes_free(struct sr_dev_inst *sdi)
{
    struct sr_channel *probe;
    GSList *l;

    for (l = sdi->channels; l; l = l->next) {
        probe = l->data;
        safe_free(probe->name);
        safe_free(probe->trigger);
		safe_free(probe->vga_ptr);
        g_free(probe);
    }
	g_safe_free_list(sdi->channels);
}

SR_PRIV void sr_dev_inst_free(struct sr_dev_inst *sdi)
{
	if (sdi == NULL)
		return;

	sr_dev_probes_free(sdi);
	
	safe_free(sdi->conn);
	safe_free(sdi->priv);
	safe_free(sdi->vendor);
	safe_free(sdi->version);
	safe_free(sdi->path);
	safe_free(sdi->name);

	g_free(sdi);
}

/** @private */
SR_PRIV struct sr_usb_dev_inst *sr_usb_dev_inst_new(uint8_t bus, uint8_t address)
{
	struct sr_usb_dev_inst *udi;
	if (!(udi = g_try_malloc0(sizeof(struct sr_usb_dev_inst)))) {
		sr_err("%s,ERROR:failed to alloc memory.", __func__);
		return NULL;
	}
	memset(udi, 0, sizeof(struct sr_usb_dev_inst));

	udi->bus = bus;
	udi->address = address;

	return udi;
}

/** @private */
SR_PRIV void sr_usb_dev_inst_free(struct sr_usb_dev_inst *usb)
{
	(void)usb;

	/* Nothing to do for this device instance type. */
}

/**
 * @private
 *
 * Both parameters are copied to newly allocated strings, and freed
 * automatically by sr_serial_dev_inst_free().
 *
 * @param pathname OS-specific serial port specification. Examples:
 *                 "/dev/ttyUSB0", "/dev/ttyACM1", "/dev/tty.Modem-0", "COM1".
 * @param serialcomm A serial communication parameters string, in the form
 *                   of <speed>/<data bits><parity><stopbits>, for example
 *                   "9600/8n1" or "600/7o2". This is an optional parameter;
 *                   it may be filled in later.
 *
 * @return A pointer to a newly initialized struct sr_serial_dev_inst,
 *         or NULL on error.
 */
SR_PRIV struct sr_serial_dev_inst *sr_serial_dev_inst_new(const char *port,
		const char *serialcomm)
{
	struct sr_serial_dev_inst *serial;

	if (!port) {
		sr_err("Serial port required.");
		return NULL;
	}

	if (!(serial = g_try_malloc0(sizeof(struct sr_serial_dev_inst)))) {
		sr_err("%s,ERROR:failed to alloc memory.", __func__);
		return NULL;
	}
	memset(serial, 0, sizeof(struct sr_serial_dev_inst));

	serial->port = g_strdup(port);
	if (serialcomm)
		serial->serialcomm = g_strdup(serialcomm);
	serial->fd = -1;

	return serial;
}

/** @private */
SR_PRIV void sr_serial_dev_inst_free(struct sr_serial_dev_inst *serial)
{
	g_free(serial->port);
	g_free(serial->serialcomm);
	g_free(serial);
}

SR_PRIV int sr_enable_device_channel(struct sr_dev_inst *sdi, const struct sr_channel *probe, gboolean enable)
{
	GSList *l;
	int ret;
	struct sr_channel *ch;	

	if (sdi == NULL || probe == NULL){
		return SR_ERR_ARG;
	}

	ret = SR_ERR_CALL_STATUS;
	for (l=sdi->channels; l; l = l->next){
		if (l->data == probe){
			ch = l->data;
			ch->enabled = enable;
			ret = SR_OK;
			break;
		}
	}

	return ret;
}
 

/** @} */
