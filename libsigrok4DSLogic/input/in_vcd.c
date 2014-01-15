/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2012 Petteri Aimonen <jpa@sr.mail.kapsi.fi>
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

/* The VCD input module has the following options:
 *
 * numprobes:   Maximum number of probes to use. The probes are
 *              detected in the same order as they are listed
 *              in the $var sections of the VCD file.
 *
 * skip:        Allows skipping until given timestamp in the file.
 *              This can speed up analyzing of long captures.
 *            
 *              Value < 0: Skip until first timestamp listed in
 *              the file. (default)
 *
 *              Value = 0: Do not skip, instead generate samples
 *              beginning from timestamp 0.
 *
 *              Value > 0: Start at the given timestamp.
 *
 * downsample:  Divide the samplerate by the given factor.
 *              This can speed up analyzing of long captures.
 *
 * compress:    Compress idle periods longer than this value.
 *              This can speed up analyzing of long captures.
 *              Default 0 = don't compress.
 *
 * Based on Verilog standard IEEE Std 1364-2001 Version C
 *
 * Supported features:
 * - $var with 'wire' and 'reg' types of scalar variables
 * - $timescale definition for samplerate
 * - multiple character variable identifiers
 *
 * Most important unsupported features:
 * - vector variables (bit vectors etc.)
 * - analog, integer and real number variables
 * - $dumpvars initial value declaration
 * - $scope namespaces
 */

/*  */

#include <stdlib.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include "libsigrok.h"
#include "libsigrok-internal.h"

/* Message logging helpers with subsystem-specific prefix string. */
#define LOG_PREFIX "input/vcd: "
#define sr_log(l, s, args...) sr_log(l, LOG_PREFIX s, ## args)
#define sr_spew(s, args...) sr_spew(LOG_PREFIX s, ## args)
#define sr_dbg(s, args...) sr_dbg(LOG_PREFIX s, ## args)
#define sr_info(s, args...) sr_info(LOG_PREFIX s, ## args)
#define sr_warn(s, args...) sr_warn(LOG_PREFIX s, ## args)
#define sr_err(s, args...) sr_err(LOG_PREFIX s, ## args)

#define DEFAULT_NUM_PROBES 8

/* Read until specific type of character occurs in file.
 * Skip input if dest is NULL.
 * Modes:
 * 'W' read until whitespace
 * 'N' read until non-whitespace, and ungetc() the character
 * '$' read until $end
 */
static gboolean read_until(FILE *file, GString *dest, char mode)
{
	char prev[4] = "";
	long startpos = ftell(file);
	for(;;)
	{
		int c = fgetc(file);

		if (c == EOF)
		{
			if (mode == '$')
				sr_err("Unexpected EOF, read started at %ld.", startpos);
			return FALSE;
		}
		
		if (mode == 'W' && g_ascii_isspace(c))
			return TRUE;
		
		if (mode == 'N' && !g_ascii_isspace(c))
		{
			ungetc(c, file);
			return TRUE;
		}
		
		if (mode == '$')
		{
			prev[0] = prev[1]; prev[1] = prev[2]; prev[2] = prev[3]; prev[3] = c;
			if (prev[0] == '$' && prev[1] == 'e' && prev[2] == 'n' && prev[3] == 'd')
			{
				if (dest != NULL)
					g_string_truncate(dest, dest->len - 3);
					
				return TRUE;
			}
		}

		if (dest != NULL)
			g_string_append_c(dest, c);
	}
}

/* Reads a single VCD section from input file and parses it to structure.
 * e.g. $timescale 1ps $end  => "timescale" "1ps"
 */
static gboolean parse_section(FILE *file, gchar **name, gchar **contents)
{
	gboolean status;
	GString *sname, *scontents;
	
	/* Skip any initial white-space */
	if (!read_until(file, NULL, 'N')) return FALSE;
	
	/* Section tag should start with $. */
	if (fgetc(file) != '$')
	{
		sr_err("Expected $ at beginning of section.");
		return FALSE;
	}
	
	/* Read the section tag */	
	sname = g_string_sized_new(32);
	status = read_until(file, sname, 'W');
	
	/* Skip whitespace before content */
	status = status && read_until(file, NULL, 'N');
	
	/* Read the content */
	scontents = g_string_sized_new(128);
	status = status && read_until(file, scontents, '$');
	g_strchomp(scontents->str);

	/* Release strings if status is FALSE, return them if status is TRUE */	
	*name = g_string_free(sname, !status);
	*contents = g_string_free(scontents, !status);
	return status;
}

struct probe
{
	gchar *name;
	gchar *identifier;
};

struct context
{
	uint64_t samplerate;
	int maxprobes;
	int probecount;
	int downsample;
	unsigned compress;
	int64_t skip;
	GSList *probes;
};

static void free_probe(void *data)
{
	struct probe *probe = data;
	g_free(probe->name);
	g_free(probe->identifier);
	g_free(probe);
}

static void release_context(struct context *ctx)
{
	g_slist_free_full(ctx->probes, free_probe);
	g_free(ctx);
}

/* Remove empty parts from an array returned by g_strsplit. */
static void remove_empty_parts(gchar **parts)
{
	gchar **src = parts;
	gchar **dest = parts;
	while (*src != NULL)
	{
		if (**src != '\0')
		{
			*dest++ = *src;
		}
		
		src++;
	}
	
	*dest = NULL;
}

/* Parse VCD header to get values for context structure.
 * The context structure should be zeroed before calling this.
 */
static gboolean parse_header(FILE *file, struct context *ctx)
{
	uint64_t p, q;
	gchar *name = NULL, *contents = NULL;
	gboolean status = FALSE;
	struct probe *probe;

	while (parse_section(file, &name, &contents))
	{
		sr_dbg("Section '%s', contents '%s'.", name, contents);
	
		if (g_strcmp0(name, "enddefinitions") == 0)
		{
			status = TRUE;
			break;
		}
		else if (g_strcmp0(name, "timescale") == 0)
		{
			/* The standard allows for values 1, 10 or 100
			 * and units s, ms, us, ns, ps and fs. */
			if (sr_parse_period(contents, &p, &q) == SR_OK)
			{
				ctx->samplerate = q / p;
				if (q % p != 0)
				{
					/* Does not happen unless time value is non-standard */
					sr_warn("Inexact rounding of samplerate, %" PRIu64 " / %" PRIu64 " to %" PRIu64 " Hz.",
						q, p, ctx->samplerate);
				}
				
				sr_dbg("Samplerate: %" PRIu64, ctx->samplerate);
			}
			else
			{
				sr_err("Parsing timescale failed.");
			}
		}
		else if (g_strcmp0(name, "var") == 0)
		{
			/* Format: $var type size identifier reference $end */
			gchar **parts = g_strsplit_set(contents, " \r\n\t", 0);
			remove_empty_parts(parts);
			
			if (g_strv_length(parts) != 4)
			{
				sr_warn("$var section should have 4 items");
			}
			else if (g_strcmp0(parts[0], "reg") != 0 && g_strcmp0(parts[0], "wire") != 0)
			{
				sr_info("Unsupported signal type: '%s'", parts[0]);
			}
			else if (strtol(parts[1], NULL, 10) != 1)
			{
				sr_info("Unsupported signal size: '%s'", parts[1]);
			}
			else if (ctx->probecount >= ctx->maxprobes)
			{
				sr_warn("Skipping '%s' because only %d probes requested.", parts[3], ctx->maxprobes);
			}
			else
			{
				sr_info("Probe %d is '%s' identified by '%s'.", ctx->probecount, parts[3], parts[2]);
				probe = g_malloc(sizeof(struct probe));
				probe->identifier = g_strdup(parts[2]);
				probe->name = g_strdup(parts[3]);
				ctx->probes = g_slist_append(ctx->probes, probe);
				ctx->probecount++;
			}
			
			g_strfreev(parts);
		}
		
		g_free(name); name = NULL;
		g_free(contents); contents = NULL;
	}
	
	g_free(name);
	g_free(contents);
	
	return status;
}

static int format_match(const char *filename)
{
	FILE *file;
	gchar *name = NULL, *contents = NULL;
	gboolean status;
	
	file = fopen(filename, "r");
	if (file == NULL)
		return FALSE;

	/* If we can parse the first section correctly,
	 * then it is assumed to be a VCD file.
	 */
	status = parse_section(file, &name, &contents);
	status = status && (*name != '\0');
	
	g_free(name);
	g_free(contents);
	fclose(file);
	
	return status;
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
	ctx->downsample = 1;
	ctx->skip = -1;

	if (in->param) {
		param = g_hash_table_lookup(in->param, "numprobes");
		if (param) {
			num_probes = strtoul(param, NULL, 10);
			if (num_probes < 1)
			{
				release_context(ctx);
				return SR_ERR;
			}
		}
		
		param = g_hash_table_lookup(in->param, "downsample");
		if (param) {
			ctx->downsample = strtoul(param, NULL, 10);
			if (ctx->downsample < 1)
			{
				ctx->downsample = 1;
			}
		}
		
		param = g_hash_table_lookup(in->param, "compress");
		if (param) {
			ctx->compress = strtoul(param, NULL, 10);
		}
		
		param = g_hash_table_lookup(in->param, "skip");
		if (param) {
			ctx->skip = strtoul(param, NULL, 10) / ctx->downsample;
		}
	}
	
	/* Maximum number of probes to parse from the VCD */
	ctx->maxprobes = num_probes;

	/* Create a virtual device. */
	in->sdi = sr_dev_inst_new(LOGIC, 0, SR_ST_ACTIVE, NULL, NULL, NULL);
	in->internal = ctx;

	for (i = 0; i < num_probes; i++) {
		snprintf(name, SR_MAX_PROBENAME_LEN, "%d", i);
		
		if (!(probe = sr_probe_new(i, SR_PROBE_LOGIC, TRUE, name)))
		{
			release_context(ctx);
			return SR_ERR;
		}
			
		in->sdi->probes = g_slist_append(in->sdi->probes, probe);
	}

	return SR_OK;
}

#define CHUNKSIZE 1024

/* Send N samples of the given value. */
static void send_samples(const struct sr_dev_inst *sdi, uint64_t sample, uint64_t count)
{
	struct sr_datafeed_packet packet;
	struct sr_datafeed_logic logic;
	uint64_t buffer[CHUNKSIZE];
	uint64_t i;
	unsigned chunksize = CHUNKSIZE;
		
	if (count < chunksize)
		chunksize = count;

	for (i = 0; i < chunksize; i++)
	{
		buffer[i] = sample;
	}
	
	packet.type = SR_DF_LOGIC;
	packet.payload = &logic;	
	logic.unitsize = sizeof(uint64_t);
	logic.data = buffer;
	
	while (count)
	{
		if (count < chunksize)
			chunksize = count;
	
		logic.length = sizeof(uint64_t) * chunksize;
	
		sr_session_send(sdi, &packet);
		count -= chunksize;
	}
}

/* Parse the data section of VCD */
static void parse_contents(FILE *file, const struct sr_dev_inst *sdi, struct context *ctx)
{
	GString *token = g_string_sized_new(32);
	
	uint64_t prev_timestamp = 0;
	uint64_t prev_values = 0;
	
	/* Read one space-delimited token at a time. */
	while (read_until(file, NULL, 'N') && read_until(file, token, 'W'))
	{
		if (token->str[0] == '#' && g_ascii_isdigit(token->str[1]))
		{
			/* Numeric value beginning with # is a new timestamp value */
			uint64_t timestamp;
			timestamp = strtoull(token->str + 1, NULL, 10);
			
			if (ctx->downsample > 1)
				timestamp /= ctx->downsample;
			
			/* Skip < 0 => skip until first timestamp.
			 * Skip = 0 => don't skip
			 * Skip > 0 => skip until timestamp >= skip.
			 */
			if (ctx->skip < 0)
			{
				ctx->skip = timestamp;
				prev_timestamp = timestamp;
			}
			else if (ctx->skip > 0 && timestamp < (uint64_t)ctx->skip)
			{
				prev_timestamp = ctx->skip;
			}
			else if (timestamp == prev_timestamp)
			{
				/* Ignore repeated timestamps (e.g. sigrok outputs these) */
			}
			else
			{
				if (ctx->compress != 0 && timestamp - prev_timestamp > ctx->compress)
				{
					/* Compress long idle periods */
					prev_timestamp = timestamp - ctx->compress;
				}
			
				sr_dbg("New timestamp: %" PRIu64, timestamp);
			
				/* Generate samples from prev_timestamp up to timestamp - 1. */
				send_samples(sdi, prev_values, timestamp - prev_timestamp);
				prev_timestamp = timestamp;
			}
		}
		else if (token->str[0] == '$' && token->len > 1)
		{
			/* This is probably a $dumpvars, $comment or similar.
			 * $dump* contain useful data, but other tags will be skipped until $end. */
			if (g_strcmp0(token->str, "$dumpvars") == 0 ||
			    g_strcmp0(token->str, "$dumpon") == 0 ||
			    g_strcmp0(token->str, "$dumpoff") == 0 ||
			    g_strcmp0(token->str, "$end") == 0)
			{
				/* Ignore, parse contents as normally. */
			}
			else
			{
				/* Skip until $end */
				read_until(file, NULL, '$');
			}
		}
		else if (strchr("bBrR", token->str[0]) != NULL)
		{
			/* A vector value. Skip it and also the following identifier. */
			read_until(file, NULL, 'N');
			read_until(file, NULL, 'W');
		}
		else if (strchr("01xXzZ", token->str[0]) != NULL)
		{
			/* A new 1-bit sample value */
			int i, bit;
			GSList *l;
			struct probe *probe;

			bit = (token->str[0] == '1');
		
			g_string_erase(token, 0, 1);
			if (token->len == 0)
			{
				/* There was a space between value and identifier.
				 * Read in the rest.
				 */
				read_until(file, NULL, 'N');
				read_until(file, token, 'W');
			}
			
			for (i = 0, l = ctx->probes; i < ctx->probecount && l; i++, l = l->next)
			{
				probe = l->data;

				if (g_strcmp0(token->str, probe->identifier) == 0)
				{
					sr_dbg("Probe %d new value %d.", i, bit);
				
					/* Found our probe */
					if (bit)
						prev_values |= (1 << i);
					else
						prev_values &= ~(1 << i);
					
					break;
				}
			}
			
			if (i == ctx->probecount)
			{
				sr_dbg("Did not find probe for identifier '%s'.", token->str);
			}
		}
		else
		{
			sr_warn("Skipping unknown token '%s'.", token->str);
		}
		
		g_string_truncate(token, 0);
	}
	
	g_string_free(token, TRUE);
}

static int loadfile(struct sr_input *in, const char *filename)
{
	struct sr_datafeed_packet packet;
	struct sr_datafeed_meta meta;
	struct sr_config *src;
	FILE *file;
	struct context *ctx;
	uint64_t samplerate;

	ctx = in->internal;

	if ((file = fopen(filename, "r")) == NULL)
		return SR_ERR;

	if (!parse_header(file, ctx))
	{
		sr_err("VCD parsing failed");
		fclose(file);
		return SR_ERR;
	}

	/* Send header packet to the session bus. */
	std_session_send_df_header(in->sdi, LOG_PREFIX);

	/* Send metadata about the SR_DF_LOGIC packets to come. */
	packet.type = SR_DF_META;
	packet.payload = &meta;
	samplerate = ctx->samplerate / ctx->downsample;
	src = sr_config_new(SR_CONF_SAMPLERATE, g_variant_new_uint64(samplerate));
	meta.config = g_slist_append(NULL, src);
	sr_session_send(in->sdi, &packet);
	sr_config_free(src);

	/* Parse the contents of the VCD file */
	parse_contents(file, in->sdi, ctx);
	
	/* Send end packet to the session bus. */
	packet.type = SR_DF_END;
	sr_session_send(in->sdi, &packet);

	fclose(file);
	release_context(ctx);
	in->internal = NULL;

	return SR_OK;
}

SR_PRIV struct sr_input_format input_vcd = {
	.id = "vcd",
	.description = "Value Change Dump",
	.format_match = format_match,
	.init = init,
	.loadfile = loadfile,
};
