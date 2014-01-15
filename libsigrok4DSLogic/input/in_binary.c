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

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include "libsigrok.h"
#include "libsigrok-internal.h"

/* Message logging helpers with subsystem-specific prefix string. */
#define LOG_PREFIX "input/binary: "
#define sr_log(l, s, args...) sr_log(l, LOG_PREFIX s, ## args)
#define sr_spew(s, args...) sr_spew(LOG_PREFIX s, ## args)
#define sr_dbg(s, args...) sr_dbg(LOG_PREFIX s, ## args)
#define sr_info(s, args...) sr_info(LOG_PREFIX s, ## args)
#define sr_warn(s, args...) sr_warn(LOG_PREFIX s, ## args)
#define sr_err(s, args...) sr_err(LOG_PREFIX s, ## args)

#define CHUNKSIZE             (512 * 1024)
#define DEFAULT_NUM_PROBES    8

struct context {
	uint64_t samplerate;
};

static int format_match(const char *filename)
{
	(void)filename;

	/* This module will handle anything you throw at it. */
	return TRUE;
}

static int init(struct sr_input *in, const char *filename)
{
	struct sr_probe *probe;
	int num_probes, i;
	char name[SR_MAX_PROBENAME_LEN + 1];
	char *param;
	struct context *ctx;

	(void)filename;

	if (!(ctx = g_try_malloc0(sizeof(*ctx)))) {
		sr_err("Input format context malloc failed.");
		return SR_ERR_MALLOC;
	}

	num_probes = DEFAULT_NUM_PROBES;
	ctx->samplerate = 0;

	if (in->param) {
		param = g_hash_table_lookup(in->param, "numprobes");
		if (param) {
			num_probes = strtoul(param, NULL, 10);
			if (num_probes < 1)
				return SR_ERR;
		}

		param = g_hash_table_lookup(in->param, "samplerate");
		if (param) {
			if (sr_parse_sizestring(param, &ctx->samplerate) != SR_OK)
				return SR_ERR;
		}
	}

	/* Create a virtual device. */
	in->sdi = sr_dev_inst_new(LOGIC, 0, SR_ST_ACTIVE, NULL, NULL, NULL);
	in->internal = ctx;

	for (i = 0; i < num_probes; i++) {
		snprintf(name, SR_MAX_PROBENAME_LEN, "%d", i);
		/* TODO: Check return value. */
		if (!(probe = sr_probe_new(i, SR_PROBE_LOGIC, TRUE, name)))
			return SR_ERR;
		in->sdi->probes = g_slist_append(in->sdi->probes, probe);
	}

	return SR_OK;
}

static int loadfile(struct sr_input *in, const char *filename)
{
	struct sr_datafeed_packet packet;
	struct sr_datafeed_meta meta;
	struct sr_datafeed_logic logic;
	struct sr_config *src;
	unsigned char buffer[CHUNKSIZE];
	int fd, size, num_probes;
	struct context *ctx;

	ctx = in->internal;

	if ((fd = open(filename, O_RDONLY)) == -1)
		return SR_ERR;

	num_probes = g_slist_length(in->sdi->probes);

	/* Send header packet to the session bus. */
	std_session_send_df_header(in->sdi, LOG_PREFIX);

	if (ctx->samplerate) {
		packet.type = SR_DF_META;
		packet.payload = &meta;
		src = sr_config_new(SR_CONF_SAMPLERATE,
				g_variant_new_uint64(ctx->samplerate));
		meta.config = g_slist_append(NULL, src);
		sr_session_send(in->sdi, &packet);
		sr_config_free(src);
	}

	/* Chop up the input file into chunks & send it to the session bus. */
	packet.type = SR_DF_LOGIC;
	packet.payload = &logic;
	logic.unitsize = (num_probes + 7) / 8;
	logic.data = buffer;
	while ((size = read(fd, buffer, CHUNKSIZE)) > 0) {
		logic.length = size;
		sr_session_send(in->sdi, &packet);
	}
	close(fd);

	/* Send end packet to the session bus. */
	packet.type = SR_DF_END;
	sr_session_send(in->sdi, &packet);

	g_free(ctx);
	in->internal = NULL;

	return SR_OK;
}

SR_PRIV struct sr_input_format input_binary = {
	.id = "binary",
	.description = "Raw binary",
	.format_match = format_match,
	.init = init,
	.loadfile = loadfile,
};
