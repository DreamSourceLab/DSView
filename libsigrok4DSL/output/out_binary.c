/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2010 Uwe Hermann <uwe@hermann-uwe.de>
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

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrok.h"
#include "libsigrok-internal.h"

/* Message logging helpers with subsystem-specific prefix string. */
#define LOG_PREFIX "output/binary: "
#define sr_log(l, s, args...) sr_log(l, LOG_PREFIX s, ## args)
#define sr_spew(s, args...) sr_spew(LOG_PREFIX s, ## args)
#define sr_dbg(s, args...) sr_dbg(LOG_PREFIX s, ## args)
#define sr_info(s, args...) sr_info(LOG_PREFIX s, ## args)
#define sr_warn(s, args...) sr_warn(LOG_PREFIX s, ## args)
#define sr_err(s, args...) sr_err(LOG_PREFIX s, ## args)

static int data(struct sr_output *o, const uint8_t *data_in,
		uint64_t length_in, uint8_t **data_out, uint64_t *length_out)
{
	uint8_t *outbuf;

	(void)o;

	if (!data_in) {
		sr_err("%s: data_in was NULL", __func__);
		return SR_ERR_ARG;
	}

	if (!length_out) {
		sr_err("%s: length_out was NULL", __func__);
		return SR_ERR_ARG;
	}

	if (length_in == 0) {
		sr_err("%s: length_in was 0", __func__);
		return SR_ERR_ARG;
	}

	if (!(outbuf = g_try_malloc0(length_in))) {
		sr_err("%s: outbuf malloc failed", __func__);
		return SR_ERR_MALLOC;
	}

	memcpy(outbuf, data_in, length_in);
	*data_out = outbuf;
	*length_out = length_in;

	return SR_OK;
}

SR_PRIV struct sr_output_format output_binary = {
	.id = "binary",
	.description = "Raw binary",
	.df_type = SR_DF_LOGIC,
	.init = NULL,
	.data = data,
	.event = NULL,
};
