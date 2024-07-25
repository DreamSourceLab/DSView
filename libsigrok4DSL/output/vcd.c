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

 
#include "../libsigrok-internal.h"
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "../config.h" /* Needed for PACKAGE and others. */
#include "../log.h"
#include <stdio.h>

#undef LOG_PREFIX
#define LOG_PREFIX "vcd: "
 
struct context {
	int num_enabled_channels;
	GArray *channelindices;
	uint8_t *prevsample;
	gboolean header_done;
	int period;
	int *channel_index;
	uint64_t samplerate;
	uint64_t samplecount;
};

static int init(struct sr_output *o, GHashTable *options)
{
	struct context *ctx;
	struct sr_channel *ch;
	GSList *l;
	int num_enabled_channels, i;

	(void)options;

	num_enabled_channels = 0;
	for (l = o->sdi->channels; l; l = l->next) {
		ch = l->data;
		if (ch->type != SR_CHANNEL_LOGIC)
			continue;
		if (!ch->enabled)
			continue;
		num_enabled_channels++;
	}
	if (num_enabled_channels > 94) {
		sr_err("VCD only supports 94 channels.");
		return SR_ERR;
	}

	ctx = g_try_malloc0(sizeof(struct context));
	if (ctx == NULL){
		sr_err("%s,ERROR:failed to alloc memory.", __func__);
		return SR_ERR;
	}
    memset(ctx, 0, sizeof(struct context));

	o->priv = ctx;
	ctx->num_enabled_channels = num_enabled_channels;
	ctx->channel_index = g_try_malloc0(sizeof(int) * ctx->num_enabled_channels);

	if (ctx->channel_index == NULL){
		sr_err("%s,ERROR:failed to alloc memory.", __func__);
		return SR_ERR;
	}

	/* Once more to map the enabled channels. */
	for (i = 0, l = o->sdi->channels; l; l = l->next) {
		ch = l->data;
		if (ch->type != SR_CHANNEL_LOGIC)
			continue;
		if (!ch->enabled)
			continue;
		ctx->channel_index[i++] = ch->index;
	}

	return SR_OK;
}

static GString *gen_header(const struct sr_output *o)
{
	struct context *ctx;
	struct sr_channel *ch;
	GVariant *gvar;
	GString *header;
	GSList *l;
	time_t t;
    int num_channels, i, p;
	char *samplerate_s, *frequency_s, *timestamp;

	ctx = o->priv;
	header = g_string_sized_new(512);
	num_channels = g_slist_length(o->sdi->channels);

	/* timestamp */
	t = time(NULL);
	timestamp = g_strdup(ctime(&t));
	timestamp[strlen(timestamp)-1] = 0;
	g_string_printf(header, "$date %s $end\n", timestamp);
	g_free(timestamp);

	/* generator */
	g_string_append_printf(header, "$version %s %s $end\n",
			PACKAGE, PACKAGE_VERSION);
	g_string_append_printf(header, "$comment\n  Acquisition with "
			"%d/%d channels", ctx->num_enabled_channels, num_channels);

	if (ctx->samplerate == 0) {
		if (sr_config_get(o->sdi->driver, o->sdi, NULL, NULL, SR_CONF_SAMPLERATE,
				&gvar) == SR_OK) {
            if (gvar != NULL) {
                ctx->samplerate = g_variant_get_uint64(gvar);
                g_variant_unref(gvar);
            }
		}
	}
	if (ctx->samplerate != 0) {
		samplerate_s = sr_samplerate_string(ctx->samplerate);
		g_string_append_printf(header, " at %s", samplerate_s);
		g_free(samplerate_s);
	}
	g_string_append_printf(header, "\n$end\n");

	/* timescale */
	/* VCD can only handle 1/10/100 (s - fs), so scale up first */
	if (ctx->samplerate > SR_MHZ(1))
		ctx->period = SR_GHZ(1);
	else if (ctx->samplerate > SR_KHZ(1))
		ctx->period = SR_MHZ(1);
	else
		ctx->period = SR_KHZ(1);
	frequency_s = sr_period_string(ctx->period);
	g_string_append_printf(header, "$timescale %s $end\n", frequency_s);
	g_free(frequency_s);

	/* scope */
	g_string_append_printf(header, "$scope module %s $end\n", PACKAGE);

	/* Wires / channels */
    p = 0;
	for (i = 0, l = o->sdi->channels; l; l = l->next, i++) {
		ch = l->data;
		if (ch->type != SR_CHANNEL_LOGIC)
			continue;
		if (!ch->enabled)
			continue;
		g_string_append_printf(header, "$var wire 1 %c %s $end\n",
                (char)('!' + p), ch->name);
        p++;
	}

	g_string_append(header, "$upscope $end\n$enddefinitions $end\n");

	return header;
}

static int receive(const struct sr_output *o, const struct sr_datafeed_packet *packet,
		GString **out)
{
	const struct sr_datafeed_meta *meta;
	const struct sr_datafeed_logic *logic;
	const struct sr_config *src;
	GSList *l;
	struct context *ctx;
	unsigned int i;
	int p, curbit, prevbit, index;
	uint8_t *sample;
	gboolean timestamp_written;

	*out = NULL;
	if (!o || !o->priv)
		return SR_ERR_BUG;
	ctx = o->priv;

	switch (packet->type) {
	case SR_DF_META:
		meta = packet->payload;
		for (l = meta->config; l; l = l->next) {
			src = l->data;
			if (src->key != SR_CONF_SAMPLERATE)
				continue;
			ctx->samplerate = g_variant_get_uint64(src->data);
		}
		break;
	case SR_DF_LOGIC:
		logic = packet->payload;

		if (!ctx->header_done) {
			*out = gen_header(o);
			ctx->header_done = TRUE;
		} else {
			*out = g_string_sized_new(512);
		}

		if (!ctx->prevsample) {
			/* Can't allocate this until we know the stream's unitsize. */
			ctx->prevsample = g_try_malloc0(logic->unitsize);

			if (ctx->prevsample == NULL){
				sr_err("%s,ERROR:failed to alloc memory.", __func__);
			}
		}

		for (i = 0; i <= logic->length - logic->unitsize; i += logic->unitsize) {
			sample = logic->data + i;
			timestamp_written = FALSE;

			for (p = 0; p < ctx->num_enabled_channels; p++) {
                //index = ctx->channel_index[p];
                index = p;

				curbit = ((unsigned)sample[index / 8]
						>> (index % 8)) & 1;
				prevbit = ((unsigned)ctx->prevsample[index / 8]
						>> (index % 8)) & 1;

				/* VCD only contains deltas/changes of signals. */
				if (prevbit == curbit && ctx->samplecount > 0)
					continue;

				/* Output timestamp of subsequent signal changes. */
				if (!timestamp_written)
					g_string_append_printf(*out, "#%.0f",
						(double)ctx->samplecount /
							ctx->samplerate * ctx->period);

				/* Output which signal changed to which value. */
				g_string_append_c(*out, ' ');
				g_string_append_c(*out, '0' + curbit);
				g_string_append_c(*out, '!' + p);

				timestamp_written = TRUE;
			}

			if (timestamp_written)
				g_string_append_c(*out, '\n');

			ctx->samplecount++;
			memcpy(ctx->prevsample, sample, logic->unitsize);
		}
		break;
	case SR_DF_END:
		/* Write final timestamp as length indicator. */
		*out = g_string_sized_new(512);
		g_string_printf(*out, "#%.0f\n",
				(double)ctx->samplecount / ctx->samplerate * ctx->period);
		break;
	}

	return SR_OK;
}

static int cleanup(struct sr_output *o)
{
	struct context *ctx;

	if (!o || !o->priv)
		return SR_ERR_ARG;

	ctx = o->priv;
	g_free(ctx->prevsample);
	g_free(ctx->channel_index);
	g_free(ctx);

	return SR_OK;
}

struct sr_output_module output_vcd = {
	.id = "vcd",
	.name = "VCD",
	.desc = "Value Change Dump",
	.exts = (const char*[]){"vcd", NULL},
	.options = NULL,
	.init = init,
	.receive = receive,
	.cleanup = cleanup,
};
