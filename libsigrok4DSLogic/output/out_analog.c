/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2012 Bert Vermeulen <bert@biot.com>
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
#include <string.h>
#include <math.h>
#include <glib.h>
#include "libsigrok.h"
#include "libsigrok-internal.h"

/* Message logging helpers with subsystem-specific prefix string. */
#define LOG_PREFIX "output/analog: "
#define sr_log(l, s, args...) sr_log(l, LOG_PREFIX s, ## args)
#define sr_spew(s, args...) sr_spew(LOG_PREFIX s, ## args)
#define sr_dbg(s, args...) sr_dbg(LOG_PREFIX s, ## args)
#define sr_info(s, args...) sr_info(LOG_PREFIX s, ## args)
#define sr_warn(s, args...) sr_warn(LOG_PREFIX s, ## args)
#define sr_err(s, args...) sr_err(LOG_PREFIX s, ## args)

struct context {
	int num_enabled_probes;
	GPtrArray *probelist;
};

static int init(struct sr_output *o)
{
	struct context *ctx;
	struct sr_probe *probe;
	GSList *l;

	sr_spew("Initializing output module.");

	if (!o || !o->sdi)
		return SR_ERR_ARG;

	if (!(ctx = g_try_malloc0(sizeof(struct context)))) {
		sr_err("Output module context malloc failed.");
		return SR_ERR_MALLOC;
	}
	o->internal = ctx;

	/* Get the number of probes and their names. */
	ctx->probelist = g_ptr_array_new();
	for (l = o->sdi->probes; l; l = l->next) {
		probe = l->data;
		if (!probe || !probe->enabled)
			continue;
		g_ptr_array_add(ctx->probelist, probe->name);
		ctx->num_enabled_probes++;
	}

	return SR_OK;
}

static void si_printf(float value, GString *out, char *unitstr)
{
	float v;

	if (signbit(value))
		v = -(value);
	else
		v = value;

	if (v < 1e-12 || v > 1e+12)
		g_string_append_printf(out, "%f %s", value, unitstr);
	else if (v > 1e+9)
		g_string_append_printf(out, "%f G%s", value / 1e+9, unitstr);
	else if (v > 1e+6)
		g_string_append_printf(out, "%f M%s", value / 1e+6, unitstr);
	else if (v > 1e+3)
		g_string_append_printf(out, "%f k%s", value / 1e+3, unitstr);
	else if (v < 1e-9)
		g_string_append_printf(out, "%f n%s", value * 1e+9, unitstr);
	else if (v < 1e-6)
		g_string_append_printf(out, "%f u%s", value * 1e+6, unitstr);
	else if (v < 1e-3)
		g_string_append_printf(out, "%f m%s", value * 1e+3, unitstr);
	else
		g_string_append_printf(out, "%f %s", value, unitstr);

}

static void fancyprint(int unit, int mqflags, float value, GString *out)
{
	switch (unit) {
	case SR_UNIT_VOLT:
		si_printf(value, out, "V");
		break;
	case SR_UNIT_AMPERE:
		si_printf(value, out, "A");
		break;
	case SR_UNIT_OHM:
		si_printf(value, out, "");
		g_string_append_unichar(out, 0x2126);
		break;
	case SR_UNIT_FARAD:
		si_printf(value, out, "F");
		break;
	case SR_UNIT_KELVIN:
		si_printf(value, out, "K");
		break;
	case SR_UNIT_CELSIUS:
		si_printf(value, out, "");
		g_string_append_unichar(out, 0x00b0);
		g_string_append_c(out, 'C');
		break;
	case SR_UNIT_FAHRENHEIT:
		si_printf(value, out, "");
		g_string_append_unichar(out, 0x00b0);
		g_string_append_c(out, 'F');
		break;
	case SR_UNIT_HERTZ:
		si_printf(value, out, "Hz");
		break;
	case SR_UNIT_PERCENTAGE:
		g_string_append_printf(out, "%f%%", value);
		break;
	case SR_UNIT_BOOLEAN:
		if (value > 0)
			g_string_append_printf(out, "TRUE");
		else
			g_string_append_printf(out, "FALSE");
		break;
	case SR_UNIT_SECOND:
		si_printf(value, out, "s");
		break;
	case SR_UNIT_SIEMENS:
		si_printf(value, out, "S");
		break;
	case SR_UNIT_DECIBEL_MW:
		si_printf(value, out, "dBu");
		break;
	case SR_UNIT_DECIBEL_VOLT:
		si_printf(value, out, "dBV");
		break;
	case SR_UNIT_DECIBEL_SPL:
		if (mqflags & SR_MQFLAG_SPL_FREQ_WEIGHT_A)
			si_printf(value, out, "dB(A)");
		else if (mqflags & SR_MQFLAG_SPL_FREQ_WEIGHT_C)
			si_printf(value, out, "dB(C)");
		else if (mqflags & SR_MQFLAG_SPL_FREQ_WEIGHT_Z)
			si_printf(value, out, "dB(Z)");
		else
			/* No frequency weighting, or non-standard "flat" */
			si_printf(value, out, "dB(SPL)");
		if (mqflags & SR_MQFLAG_SPL_TIME_WEIGHT_S)
			g_string_append(out, " S");
		else if (mqflags & SR_MQFLAG_SPL_TIME_WEIGHT_F)
			g_string_append(out, " F");
		if (mqflags & SR_MQFLAG_SPL_LAT)
			g_string_append(out, " LAT");
		else if (mqflags & SR_MQFLAG_SPL_PCT_OVER_ALARM)
			/* Not a standard function for SLMs, so this is
			 * a made-up notation. */
			g_string_append(out, " %oA");
		break;
	case SR_UNIT_CONCENTRATION:
		g_string_append_printf(out, "%f ppm", value * 1000000);
		break;
	default:
		si_printf(value, out, "");
		break;
	}
	if ((mqflags & (SR_MQFLAG_AC | SR_MQFLAG_DC)) == (SR_MQFLAG_AC | SR_MQFLAG_DC))
		g_string_append_printf(out, " AC+DC");
	else if (mqflags & SR_MQFLAG_AC)
		g_string_append_printf(out, " AC");
	else if (mqflags & SR_MQFLAG_DC)
		g_string_append_printf(out, " DC");
	g_string_append_c(out, '\n');
}

static int receive(struct sr_output *o, const struct sr_dev_inst *sdi,
		const struct sr_datafeed_packet *packet, GString **out)
{
	const struct sr_datafeed_analog *analog;
	struct sr_probe *probe;
	GSList *l;
	const float *fdata;
	int i, p;

	(void)sdi;

	*out = NULL;
	if (!o || !o->sdi)
		return SR_ERR_ARG;

	switch (packet->type) {
	case SR_DF_FRAME_BEGIN:
		*out = g_string_new("FRAME-BEGIN\n");
		break;
	case SR_DF_FRAME_END:
		*out = g_string_new("FRAME-END\n");
		break;
	case SR_DF_ANALOG:
		analog = packet->payload;
		fdata = (const float *)analog->data;
		*out = g_string_sized_new(512);
		for (i = 0; i < analog->num_samples; i++) {
			for (l = analog->probes, p = 0; l; l = l->next, p++) {
				probe = l->data;
				g_string_append_printf(*out, "%s: ", probe->name);
				fancyprint(analog->unit, analog->mqflags,
						fdata[i + p], *out);
			}
		}
		break;
	}

	return SR_OK;
}

static int cleanup(struct sr_output *o)
{
	struct context *ctx;

	if (!o || !o->sdi)
		return SR_ERR_ARG;
	ctx = o->internal;

	g_ptr_array_free(ctx->probelist, 1);
	g_free(ctx);
	o->internal = NULL;

	return SR_OK;
}

SR_PRIV struct sr_output_format output_analog = {
	.id = "analog",
	.description = "Analog data",
	.df_type = SR_DF_ANALOG,
	.init = init,
	.receive = receive,
	.cleanup = cleanup
};
