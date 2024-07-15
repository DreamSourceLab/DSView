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

#include "libsigrok-internal.h"
#include <glib.h>
#include "log.h"
#include <assert.h>

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

	if (!(drvc = malloc(sizeof(struct drv_context)))) {
		sr_err("%sDriver context malloc failed.", prefix);
		return SR_ERR_MALLOC;
	}
	// not need init.

	drvc->sr_ctx = sr_ctx;
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
    packet.status = SR_PKT_OK;
	packet.payload = (uint8_t *)&header;
	header.feed_version = 1;
	gettimeofday(&header.starttime, NULL);

	if ((ret = ds_data_forward(sdi, &packet)) < 0) {
		sr_err("%sFailed to send header packet: %d.", prefix, ret);
		return ret;
	}

	return SR_OK;
}
