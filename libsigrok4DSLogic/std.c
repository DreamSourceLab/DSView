/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Uwe Hermann <uwe@hermann-uwe.de>
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

#include <glib.h>
#include "libsigrok.h"
#include "libsigrok-internal.h"

/**
 * Standard sr_driver_init() API helper.
 *
 * This function can be used to simplify most driver's hw_init() API callback.
 *
 * It creates a new 'struct drv_context' (drvc), assigns sr_ctx to it, and
 * then 'drvc' is assigned to the 'struct sr_dev_driver' (di) that is passed.
 *
 * @param sr_ctx The libsigrok context to assign.
 * @param di The driver instance to use.
 * @param prefix A driver-specific prefix string used for log messages.
 *
 * @return SR_OK upon success, SR_ERR_ARG upon invalid arguments, or
 *         SR_ERR_MALLOC upon memory allocation errors.
 */
SR_PRIV int std_hw_init(struct sr_context *sr_ctx, struct sr_dev_driver *di,
			const char *prefix)
{
	struct drv_context *drvc;

	if (!di) {
		sr_err("%sInvalid driver, cannot initialize.", prefix);
		return SR_ERR_ARG;
	}

	if (!(drvc = g_try_malloc(sizeof(struct drv_context)))) {
		sr_err("%sDriver context malloc failed.", prefix);
		return SR_ERR_MALLOC;
	}

	drvc->sr_ctx = sr_ctx;
	drvc->instances = NULL;
	di->priv = drvc;

	return SR_OK;
}

/**
 * Standard API helper for sending an SR_DF_HEADER packet.
 *
 * This function can be used to simplify most driver's
 * hw_dev_acquisition_start() API callback.
 *
 * @param sdi The device instance to use.
 * @param prefix A driver-specific prefix string used for log messages.
 * 		 Must not be NULL. An empty string is allowed.
 *
 * @return SR_OK upon success, SR_ERR_ARG upon invalid arguments, or
 *         SR_ERR upon other errors.
 */
SR_PRIV int std_session_send_df_header(const struct sr_dev_inst *sdi,
				       const char *prefix)
{
	int ret;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_header header;

	if (!prefix) {
		sr_err("Invalid prefix.");
		return SR_ERR_ARG;
	}

	sr_dbg("%sStarting acquisition.", prefix);

	/* Send header packet to the session bus. */
	sr_dbg("%sSending SR_DF_HEADER packet.", prefix);
	packet.type = SR_DF_HEADER;
	packet.payload = (uint8_t *)&header;
	header.feed_version = 1;
	gettimeofday(&header.starttime, NULL);

	if ((ret = sr_session_send(sdi, &packet)) < 0) {
		sr_err("%sFailed to send header packet: %d.", prefix, ret);
		return ret;
	}

	return SR_OK;
}

/*
 * Standard sr_session_stop() API helper.
 *
 * This function can be used to simplify most (serial port based) driver's
 * hw_dev_acquisition_stop() API callback.
 *
 * @param sdi The device instance for which acquisition should stop.
 *            Must not be NULL.
 * @param cb_data Opaque 'cb_data' pointer. Must not be NULL.
 * @param hw_dev_close_fn Function pointer to the driver's hw_dev_close().
 *               	  Must not be NULL.
 * @param serial The serial device instance (struct serial_dev_inst *).
 *               Must not be NULL.
 * @param prefix A driver-specific prefix string used for log messages.
 *               Must not be NULL. An empty string is allowed.
 *
 * @return SR_OK upon success, SR_ERR_ARG upon invalid arguments, or
 *         SR_ERR upon other errors.
 */
SR_PRIV int std_hw_dev_acquisition_stop_serial(struct sr_dev_inst *sdi,
			void *cb_data, dev_close_t hw_dev_close_fn,
			struct sr_serial_dev_inst *serial, const char *prefix)
{
	int ret;
	struct sr_datafeed_packet packet;

	if (!prefix) {
		sr_err("Invalid prefix.");
		return SR_ERR_ARG;
	}

	if (sdi->status != SR_ST_ACTIVE) {
		sr_err("%sDevice inactive, can't stop acquisition.", prefix);
		return SR_ERR;
	}

	sr_dbg("%sStopping acquisition.", prefix);

	if ((ret = sr_source_remove(serial->fd)) < 0) {
		sr_err("%sFailed to remove source: %d.", prefix, ret);
		return ret;
	}

	if ((ret = hw_dev_close_fn(sdi)) < 0) {
		sr_err("%sFailed to close device: %d.", prefix, ret);
		return ret;
	}

	/* Send SR_DF_END packet to the session bus. */
	sr_dbg("%sSending SR_DF_END packet.", prefix);
	packet.type = SR_DF_END;
	packet.payload = NULL;
	if ((ret = sr_session_send(cb_data, &packet)) < 0) {
		sr_err("%sFailed to send SR_DF_END packet: %d.", prefix, ret);
		return ret;
	}

	return SR_OK;
}

/*
 * Standard driver dev_clear() helper.
 *
 * This function can be used to implement the dev_clear() driver API
 * callback. dev_close() is called before every sr_dev_inst is cleared.
 *
 * The only limitation is driver-specific device contexts (sdi->priv).
 * These are freed, but any dynamic allocation within structs stored
 * there cannot be freed.
 *
 * @param driver The driver which will have its instances released.
 *
 * @return SR_OK on success.
 */
SR_PRIV int std_dev_clear(const struct sr_dev_driver *driver,
		std_dev_clear_t clear_private)
{
	struct sr_dev_inst *sdi;
	struct drv_context *drvc;
	struct dev_context *devc;
	GSList *l;
	int ret;

	if (!(drvc = driver->priv))
		/* Driver was never initialized, nothing to do. */
		return SR_OK;

	ret = SR_OK;
	for (l = drvc->instances; l; l = l->next) {
		/* Log errors, but continue cleaning up the rest. */
		if (!(sdi = l->data)) {
			ret = SR_ERR_BUG;
			continue;
		}
		if (!(devc = sdi->priv)) {
			ret = SR_ERR_BUG;
			continue;
		}
		if (driver->dev_close)
			driver->dev_close(sdi);

		if (sdi->conn) {
			if (sdi->inst_type == SR_INST_USB)
#if HAVE_LIBUSB_1_0
				sr_usb_dev_inst_free(sdi->conn);
#else
				;
#endif
			else if (sdi->inst_type == SR_INST_SERIAL)
				sr_serial_dev_inst_free(sdi->conn);
		}
		if (clear_private)
			clear_private(sdi->priv);
		sdi = l->data;
		sr_dev_inst_free(sdi);
	}

	g_slist_free(drvc->instances);
	drvc->instances = NULL;

	return ret;
}
