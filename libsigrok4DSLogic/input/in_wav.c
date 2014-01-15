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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "libsigrok.h"
#include "libsigrok-internal.h"

/* Message logging helpers with subsystem-specific prefix string. */
#define LOG_PREFIX "input/wav: "
#define sr_log(l, s, args...) sr_log(l, LOG_PREFIX s, ## args)
#define sr_spew(s, args...) sr_spew(LOG_PREFIX s, ## args)
#define sr_dbg(s, args...) sr_dbg(LOG_PREFIX s, ## args)
#define sr_info(s, args...) sr_info(LOG_PREFIX s, ## args)
#define sr_warn(s, args...) sr_warn(LOG_PREFIX s, ## args)
#define sr_err(s, args...) sr_err(LOG_PREFIX s, ## args)

#define CHUNK_SIZE 4096

struct context {
	uint64_t samplerate;
	int samplesize;
	int num_channels;
};

static int get_wav_header(const char *filename, char *buf)
{
	struct stat st;
	int fd, l;

	l = strlen(filename);
	if (l <= 4 || strcasecmp(filename + l - 4, ".wav"))
		return SR_ERR;

	if (stat(filename, &st) == -1)
		return SR_ERR;
	if (st.st_size <= 45)
		/* Minimum size of header + 1 8-bit mono PCM sample. */
		return SR_ERR;

	if ((fd = open(filename, O_RDONLY)) == -1)
		return SR_ERR;

	l = read(fd, buf, 40);
	close(fd);
	if (l != 40)
		return SR_ERR;

	return SR_OK;
}

static int format_match(const char *filename)
{
	char buf[40];

	if (get_wav_header(filename, buf) != SR_OK)
		return FALSE;

	if (strncmp(buf, "RIFF", 4))
		return FALSE;
	if (strncmp(buf + 8, "WAVE", 4))
		return FALSE;
	if (strncmp(buf + 12, "fmt ", 4))
		return FALSE;
	if (GUINT16_FROM_LE(*(uint16_t *)(buf + 20)) != 1)
		/* Not PCM. */
		return FALSE;
	if (strncmp(buf + 36, "data", 4))
		return FALSE;

	return TRUE;
}

static int init(struct sr_input *in, const char *filename)
{
	struct sr_probe *probe;
	struct context *ctx;
	char buf[40], probename[8];
	int i;

	if (get_wav_header(filename, buf) != SR_OK)
		return SR_ERR;

	if (!(ctx = g_try_malloc0(sizeof(struct context))))
		return SR_ERR_MALLOC;

	/* Create a virtual device. */
	in->sdi = sr_dev_inst_new(LOGIC, 0, SR_ST_ACTIVE, NULL, NULL, NULL);
	in->sdi->priv = ctx;

   	ctx->samplerate = GUINT32_FROM_LE(*(uint32_t *)(buf + 24));
	ctx->samplesize = GUINT16_FROM_LE(*(uint16_t *)(buf + 34)) / 8;
	if (ctx->samplesize != 1 && ctx->samplesize != 2 && ctx->samplesize != 4) {
		sr_err("only 8, 16 or 32 bits per sample supported.");
		return SR_ERR;
	}

	if ((ctx->num_channels = GUINT16_FROM_LE(*(uint16_t *)(buf + 22))) > 20) {
		sr_err("%d channels seems crazy.", ctx->num_channels);
		return SR_ERR;
	}

	for (i = 0; i < ctx->num_channels; i++) {
		snprintf(probename, 8, "CH%d", i + 1);
		if (!(probe = sr_probe_new(0, SR_PROBE_ANALOG, TRUE, probename)))
			return SR_ERR;
		in->sdi->probes = g_slist_append(in->sdi->probes, probe);
	}

	return SR_OK;
}

static int loadfile(struct sr_input *in, const char *filename)
{
	struct sr_datafeed_packet packet;
	struct sr_datafeed_meta meta;
	struct sr_datafeed_analog analog;
	struct sr_config *src;
	struct context *ctx;
	float fdata[CHUNK_SIZE];
	uint64_t sample;
	int num_samples, chunk_samples, s, c, fd, l;
	char buf[CHUNK_SIZE];

	ctx = in->sdi->priv;

	/* Send header packet to the session bus. */
	std_session_send_df_header(in->sdi, LOG_PREFIX);

	packet.type = SR_DF_META;
	packet.payload = &meta;
	src = sr_config_new(SR_CONF_SAMPLERATE,
			g_variant_new_uint64(ctx->samplerate));
	meta.config = g_slist_append(NULL, src);
	sr_session_send(in->sdi, &packet);
	sr_config_free(src);

	if ((fd = open(filename, O_RDONLY)) == -1)
		return SR_ERR;

	lseek(fd, 40, SEEK_SET);
	l = read(fd, buf, 4);
	num_samples = GUINT32_FROM_LE((uint32_t)*(buf));
	num_samples /= ctx->samplesize / ctx->num_channels;
	while (TRUE) {
		if ((l = read(fd, buf, CHUNK_SIZE)) < 1)
			break;
		chunk_samples = l / ctx->samplesize / ctx->num_channels;
		for (s = 0; s < chunk_samples; s++) {
			for (c = 0; c < ctx->num_channels; c++) {
				sample = 0;
				memcpy(&sample, buf + s * ctx->samplesize + c, ctx->samplesize);
				switch (ctx->samplesize) {
				case 1:
					/* 8-bit PCM samples are unsigned. */
					fdata[s + c] = (uint8_t)sample / 255.0;
					break;
				case 2:
					fdata[s + c] = GINT16_FROM_LE(sample) / 32767.0;
					break;
				case 4:
					fdata[s + c] = GINT32_FROM_LE(sample) / 65535.0;
					break;
				}
			}
		}
		packet.type = SR_DF_ANALOG;
		packet.payload = &analog;
		analog.probes = in->sdi->probes;
		analog.num_samples = chunk_samples;
		analog.mq = 0;
		analog.unit = 0;
		analog.data = fdata;
		sr_session_send(in->sdi, &packet);
	}

	close(fd);
	packet.type = SR_DF_END;
	sr_session_send(in->sdi, &packet);

	return SR_OK;
}


SR_PRIV struct sr_input_format input_wav = {
	.id = "wav",
	.description = "WAV file",
	.format_match = format_match,
	.init = init,
	.loadfile = loadfile,
};

