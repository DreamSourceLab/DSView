/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2010 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
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
#include "config.h" /* Needed for PACKAGE and others. */
#include "libsigrok.h"
#include "libsigrok-internal.h"

/* Message logging helpers with subsystem-specific prefix string. */
#define LOG_PREFIX "output/vcd: "
#define sr_log(l, s, args...) sr_log(l, LOG_PREFIX s, ## args)
#define sr_spew(s, args...) sr_spew(LOG_PREFIX s, ## args)
#define sr_dbg(s, args...) sr_dbg(LOG_PREFIX s, ## args)
#define sr_info(s, args...) sr_info(LOG_PREFIX s, ## args)
#define sr_warn(s, args...) sr_warn(LOG_PREFIX s, ## args)
#define sr_err(s, args...) sr_err(LOG_PREFIX s, ## args)

struct context {
	int num_enabled_probes;
	GArray *probeindices;
	GString *header;
	uint8_t *prevsample;
	int period;
	uint64_t samplerate;
	unsigned int unitsize;
};

static const char *vcd_header_comment = "\
$comment\n  Acquisition with %d/%d probes at %s\n$end\n";

static int init(struct sr_output *o)
{
	struct context *ctx;
	struct sr_probe *probe;
	GSList *l;
	GVariant *gvar;
	int num_probes, i;
	char *samplerate_s, *frequency_s, *timestamp;
	time_t t;

	if (!(ctx = g_try_malloc0(sizeof(struct context)))) {
		sr_err("%s: ctx malloc failed", __func__);
		return SR_ERR_MALLOC;
	}

	o->internal = ctx;
	ctx->num_enabled_probes = 0;
	ctx->probeindices = g_array_new(FALSE, FALSE, sizeof(int));

	for (l = o->sdi->probes; l; l = l->next) {
		probe = l->data;
		if (!probe->enabled)
			continue;
		ctx->probeindices = g_array_append_val(
				ctx->probeindices, probe->index);
		ctx->num_enabled_probes++;
	}
	if (ctx->num_enabled_probes > 94) {
		sr_err("VCD only supports 94 probes.");
		return SR_ERR;
	}

	ctx->unitsize = (ctx->num_enabled_probes + 7) / 8;
	ctx->header = g_string_sized_new(512);
	num_probes = g_slist_length(o->sdi->probes);

	/* timestamp */
	t = time(NULL);
	timestamp = g_strdup(ctime(&t));
	timestamp[strlen(timestamp)-1] = 0;
	g_string_printf(ctx->header, "$date %s $end\n", timestamp);
	g_free(timestamp);

	/* generator */
	g_string_append_printf(ctx->header, "$version %s %s $end\n",
			PACKAGE, PACKAGE_VERSION);

	if (sr_config_get(o->sdi->driver, SR_CONF_SAMPLERATE, &gvar,
			o->sdi) == SR_OK) {
		ctx->samplerate = g_variant_get_uint64(gvar);
		g_variant_unref(gvar);
		if (!((samplerate_s = sr_samplerate_string(ctx->samplerate)))) {
			g_string_free(ctx->header, TRUE);
			g_free(ctx);
			return SR_ERR;
		}
		g_string_append_printf(ctx->header, vcd_header_comment,
				 ctx->num_enabled_probes, num_probes, samplerate_s);
		g_free(samplerate_s);
	}

	/* timescale */
	/* VCD can only handle 1/10/100 (s - fs), so scale up first */
	if (ctx->samplerate > SR_MHZ(1))
		ctx->period = SR_GHZ(1);
	else if (ctx->samplerate > SR_KHZ(1))
		ctx->period = SR_MHZ(1);
	else
		ctx->period = SR_KHZ(1);
	if (!(frequency_s = sr_period_string(ctx->period))) {
		g_string_free(ctx->header, TRUE);
		g_free(ctx);
		return SR_ERR;
	}
	g_string_append_printf(ctx->header, "$timescale %s $end\n", frequency_s);
	g_free(frequency_s);

	/* scope */
	g_string_append_printf(ctx->header, "$scope module %s $end\n", PACKAGE);

	/* Wires / channels */
	for (i = 0, l = o->sdi->probes; l; l = l->next, i++) {
		probe = l->data;
		if (!probe->enabled)
			continue;
		g_string_append_printf(ctx->header, "$var wire 1 %c %s $end\n",
				(char)('!' + i), probe->name);
	}

	g_string_append(ctx->header, "$upscope $end\n"
			"$enddefinitions $end\n$dumpvars\n");

	if (!(ctx->prevsample = g_try_malloc0(ctx->unitsize))) {
		g_string_free(ctx->header, TRUE);
		g_free(ctx);
		sr_err("%s: ctx->prevsample malloc failed", __func__);
		return SR_ERR_MALLOC;
	}

	return SR_OK;
}

static int receive(struct sr_output *o, const struct sr_dev_inst *sdi,
		const struct sr_datafeed_packet *packet, GString **out)
{
	const struct sr_datafeed_logic *logic;
	struct context *ctx;
	unsigned int i;
	int p, curbit, prevbit, index;
	uint8_t *sample;
	static uint64_t samplecount = 0;

	(void)sdi;

	*out = NULL;
	if (!o || !o->internal)
		return SR_ERR_ARG;
	ctx = o->internal;

	if (packet->type == SR_DF_END) {
		*out = g_string_new("$dumpoff\n$end\n");
		return SR_OK;
	} else if (packet->type != SR_DF_LOGIC)
		return SR_OK;

	if (ctx->header) {
		/* The header is still here, this must be the first packet. */
		*out = ctx->header;
		ctx->header = NULL;
	} else {
		*out = g_string_sized_new(512);
	}

	logic = packet->payload;
	for (i = 0; i <= logic->length - logic->unitsize; i += logic->unitsize) {
		samplecount++;

		sample = logic->data + i;

		for (p = 0; p < ctx->num_enabled_probes; p++) {
			index = g_array_index(ctx->probeindices, int, p);
			curbit = (sample[p / 8] & (((uint8_t) 1) << index)) >> index;
			prevbit = (ctx->prevsample[p / 8] & (((uint64_t) 1) << index)) >> index;

			/* VCD only contains deltas/changes of signals. */
			if (prevbit == curbit)
				continue;

			/* Output which signal changed to which value. */
			g_string_append_printf(*out, "#%" PRIu64 "\n%i%c\n",
					(uint64_t)(((float)samplecount / ctx->samplerate)
					* ctx->period), curbit, (char)('!' + p));
		}

		memcpy(ctx->prevsample, sample, ctx->unitsize);
	}

	return SR_OK;
}

static int cleanup(struct sr_output *o)
{
	struct context *ctx;

	if (!o || !o->internal)
		return SR_ERR_ARG;

	ctx = o->internal;
	g_free(ctx);

	return SR_OK;
}

struct sr_output_format output_vcd = {
	.id = "vcd",
	.description = "Value Change Dump (VCD)",
	.df_type = SR_DF_LOGIC,
	.init = init,
	.receive = receive,
	.cleanup = cleanup,
};
