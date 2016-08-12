/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2010 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2011 Olivier Fauchon <olivier@aixmarseille.com>
 * Copyright (C) 2012 Alexandru Gagniuc <mr.nuke.me@gmail.com>
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
#include <unistd.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#define pipe(fds) _pipe(fds, 4096, _O_BINARY)
#endif
#include "libsigrok.h"
#include "libsigrok-internal.h"

/* Message logging helpers with subsystem-specific prefix string. */
#define LOG_PREFIX "demo: "
#define sr_log(l, s, args...) sr_log(l, LOG_PREFIX s, ## args)
#define sr_spew(s, args...) sr_spew(LOG_PREFIX s, ## args)
#define sr_dbg(s, args...) sr_dbg(LOG_PREFIX s, ## args)
#define sr_info(s, args...) sr_info(LOG_PREFIX s, ## args)
#define sr_warn(s, args...) sr_warn(LOG_PREFIX s, ## args)
#define sr_err(s, args...) sr_err(LOG_PREFIX s, ## args)

/* TODO: Number of probes should be configurable. */
#define NUM_PROBES             16

#define DEMONAME               "Demo device"

/* The size of chunks to send through the session bus. */
/* TODO: Should be configurable. */
#define BUFSIZE                512*1024
#define DSO_BUFSIZE            10*1024

#define PERIOD                  4000

#define PI 3.14159265

#define CONST_LEN               50

#define DEMO_MAX_LOGIC_DEPTH SR_MB(100)
#define DEMO_MAX_LOGIC_SAMPLERATE SR_MHZ(400)
#define DEMO_MAX_DSO_DEPTH SR_KB(20)
#define DEMO_MAX_DSO_SAMPLERATE SR_MHZ(200)
#define DEMO_MAX_DSO_PROBES_NUM 2

/* Supported patterns which we can generate */
enum {
    PATTERN_SINE = 0,
    PATTERN_SQUARE = 1,
    PATTERN_TRIANGLE = 2,
    PATTERN_SAWTOOTH = 3,
    PATTERN_RANDOM = 4,
};
static const char *pattern_strings[] = {
    "Sine",
    "Square",
    "Triangle",
    "Sawtooth",
    "Random",
};

static const char *maxHeights[] = {
    "1X",
    "2X",
    "3X",
    "4X",
    "5X",
};

static struct sr_dev_mode mode_list[] = {
    {"LA", LOGIC},
    {"DAQ", ANALOG},
    {"OSC", DSO},
};

/* Private, per-device-instance driver context. */
struct dev_context {
	struct sr_dev_inst *sdi;
	int pipe_fds[2];
	GIOChannel *channel;
	uint64_t cur_samplerate;
	uint64_t limit_samples;
    uint64_t limit_samples_show;
	uint64_t limit_msec;
	uint8_t sample_generator;
	uint64_t samples_counter;
	void *cb_data;
	int64_t starttime;
    int stop;
    uint64_t timebase;
    gboolean instant;
    gboolean data_lock;
    uint8_t max_height;
    uint8_t dso_bits;

    uint16_t *buf;
    uint64_t pre_index;
    struct sr_status mstatus;

    int trigger_stage;
    uint16_t trigger_mask;
    uint16_t trigger_value;
    uint16_t trigger_edge;
    uint8_t trigger_slope;
    uint8_t trigger_source;
};

static const int hwcaps[] = {
	SR_CONF_LOGIC_ANALYZER,
	SR_CONF_DEMO_DEV,
	SR_CONF_SAMPLERATE,
	SR_CONF_PATTERN_MODE,
	SR_CONF_LIMIT_SAMPLES,
	SR_CONF_LIMIT_MSEC,
	SR_CONF_CONTINUOUS,
};

static const int hwoptions[] = {
    SR_CONF_PATTERN_MODE,
    SR_CONF_MAX_HEIGHT,
};

static const int32_t sessions[] = {
    SR_CONF_SAMPLERATE,
    SR_CONF_LIMIT_SAMPLES,
    SR_CONF_PATTERN_MODE,
};

static const int const_dc = 1.95 / 10 * 255;
static const int sinx[] = {
  0,   2,   3,   5,   6,   8,   9,  11,  12,  14,  16,  17,  18,  20,  21,  23,  24,  26,  27,  28,
 30,  31,  32,  33,  34,  35,  37,  38,  39,  40,  41,  41,  42,  43,  44,  45,  45,  46,  47,  47,
 48,  48,  49,  49,  49,  49,  50,  50,  50,  50,  50,  50,  50,  50,  50,  49,  49,  49,  48,  48,
 47,  47,  46,  46,  45,  44,  44,  43,  42,  41,  40,  39,  38,  37,  36,  35,  34,  33,  31,  30,
 29,  28,  26,  25,  24,  22,  21,  19,  18,  16,  15,  13,  12,  10,   9,   7,   6,   4,   2,   1,
 -1,  -2,  -4,  -6,  -7,  -9, -10, -12, -13, -15, -16, -18, -19, -21, -22, -24, -25, -26, -28, -29,
-30, -31, -33, -34, -35, -36, -37, -38, -39, -40, -41, -42, -43, -44, -44, -45, -46, -46, -47, -47,
-48, -48, -49, -49, -49, -50, -50, -50, -50, -50, -50, -50, -50, -50, -49, -49, -49, -49, -48, -48,
-47, -47, -46, -45, -45, -44, -43, -42, -41, -41, -40, -39, -38, -37, -35, -34, -33, -32, -31, -30,
-28, -27, -26, -24, -23, -21, -20, -18, -17, -16, -14, -12, -11,  -9,  -8,  -6,  -5,  -3,  -2,   0,
};

static const int sqrx[] = {
 50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,
 50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,
 50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,
 50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,
 50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,
-50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
-50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
-50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
-50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
-50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
};

static const int trix[] = {
  0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,
 20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,
 40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  49,  48,  47,  46,  45,  44,  43,  42,  41,
 40,  39,  38,  37,  36,  35,  34,  33,  32,  31,  30,  29,  28,  27,  26,  25,  24,  23,  22,  21,
 20,  19,  18,  17,  16,  15,  14,  13,  12,  11,  10,   9,   8,   7,   6,   5,   4,   3,   2,   1,
  0,  -1,  -2,  -3,  -4,  -5,  -6,  -7,  -8,  -9, -10, -11, -12, -13, -14, -15, -16, -17, -18, -19,
-20, -21, -22, -23, -24, -25, -26, -27, -28, -29, -30, -31, -32, -33, -34, -35, -36, -37, -38, -39,
-40, -41, -42, -43, -44, -45, -46, -47, -48, -49, -50, -49, -48, -47, -46, -45, -44, -43, -42, -41,
-40, -39, -38, -37, -36, -35, -34, -33, -32, -31, -30, -29, -28, -27, -26, -25, -24, -23, -22, -21,
-20, -19, -18, -17, -16, -15, -14, -13, -12, -11, -10,  -9,  -8,  -7,  -6,  -5,  -4,  -3,  -2,  -1,
};

static const int sawx[] = {
  0,   0,   1,   1,   2,   2,   3,   3,   4,   4,   5,   5,   6,   6,   7,   7,   8,   8,   9,   9,
 10,  10,  11,  11,  12,  12,  13,  13,  14,  14,  15,  15,  16,  16,  17,  17,  18,  18,  19,  19,
 20,  20,  21,  21,  22,  22,  23,  23,  24,  24,  25,  25,  26,  26,  27,  27,  28,  28,  29,  29,
 30,  30,  31,  31,  32,  32,  33,  33,  34,  34,  35,  35,  36,  36,  37,  37,  38,  38,  39,  39,
 40,  40,  41,  41,  42,  42,  43,  43,  44,  44,  45,  45,  46,  46,  47,  47,  48,  48,  49,  50,
-50, -49, -48, -48, -47, -47, -46, -46, -45, -45, -44, -44, -43, -43, -42, -42, -41, -41, -40, -40,
-39, -39, -38, -38, -37, -37, -36, -36, -35, -35, -34, -34, -33, -33, -32, -32, -31, -31, -30, -30,
-29, -29, -28, -28, -27, -27, -26, -26, -25, -25, -24, -24, -23, -23, -22, -22, -21, -21, -20, -20,
-19, -19, -18, -18, -17, -17, -16, -16, -15, -15, -14, -14, -13, -13, -12, -12, -11, -11, -10, -10,
 -9,  -9,  -8,  -8,  -7,  -7,  -6,  -6,  -5,  -5,  -4,  -4,  -3,  -3,  -2,  -2,  -1,  -1,   0,   0,
};

static const int ranx[] = {
 -4,  47, -49,  -1,  -3,   6, -29,  26,   1,  14, -39, -38,  36,  17,  26, -37,  -2,  27, -20, -15,
-49, -46,  36,  16,  29,  23, -30,  -3,  28,  -2,  -6,  46,  43,  50, -42,  30,  48, -50, -38, -30,
  7, -36, -20, -24, -10, -34, -24,   3, -48,  46, -11,  22,  19,  28,  39, -49, -31,  34,   2, -29,
  9,  35,   8,  10,  38,  30,  17,  48,  -3,  -6, -28,  46, -19,  18, -43,  -9, -31, -32, -41,  16,
-10,  46,  -4,   4, -32, -43, -45, -39, -33,  28,  24, -17, -43,  42,  -7,  36, -44,  -5,   9,  39,
 17, -40,  12,  16, -42,  -1,   2,  -9,  50,  -8,  27,  27,  14,   8, -18,  12,  -8,  26,  -8,  12,
-35,  49,  35,   2, -26, -24, -31,  33,  15, -47,  34,  46,  -1, -12,  14,  32, -25, -31, -35, -18,
-48, -21,  -5,   1, -27, -14,  12,  49, -11,  33,  31,  35, -36,  19,  20,  44,  29, -48,  14, -43,
  1,  30, -12,  44,  20,  49,  29, -43,  42,  30, -34,  24,  20, -40,  33, -12,  13, -45,  45, -24,
-41,  36,  -8,  46,  47, -34,  28, -39,   7, -32,  38, -27,  28,  -3,  -8,  43, -37, -24,   6,   3,
};

static const uint64_t samplerates[] = {
    SR_HZ(100),
    SR_HZ(200),
    SR_HZ(500),
    SR_KHZ(1),
    SR_KHZ(2),
    SR_KHZ(5),
    SR_KHZ(10),
    SR_KHZ(20),
    SR_KHZ(50),
    SR_KHZ(100),
    SR_KHZ(200),
    SR_KHZ(500),
    SR_MHZ(1),
    SR_MHZ(2),
    SR_MHZ(5),
    SR_MHZ(10),
    SR_MHZ(20),
    SR_MHZ(50),
    SR_MHZ(100),
    SR_MHZ(200),
    SR_MHZ(400),
};

//static const uint64_t samplecounts[] = {
//    SR_KB(1),
//    SR_KB(2),
//    SR_KB(4),
//    SR_KB(8),
//    SR_KB(16),
//    SR_KB(32),
//    SR_KB(64),
//    SR_KB(128),
//    SR_KB(256),
//    SR_KB(512),
//    SR_MB(1),
//    SR_MB(2),
//    SR_MB(4),
//    SR_MB(8),
//    SR_MB(16),
//    SR_MB(32),
//    SR_MB(64),
//    SR_MB(128),
//};

static const uint64_t samplecounts[] = {
    SR_KB(1),
    SR_KB(2),
    SR_KB(5),
    SR_KB(10),
    SR_KB(20),
    SR_KB(50),
    SR_KB(100),
    SR_KB(200),
    SR_KB(500),
    SR_MB(1),
    SR_MB(2),
    SR_MB(5),
    SR_MB(10),
    SR_MB(20),
    SR_MB(50),
    SR_MB(100),
};

/* We name the probes 0-7 on our demo driver. */
static const char *probe_names[NUM_PROBES + 1] = {
    "CH0", "CH1", "CH2", "CH3",
    "CH4", "CH5", "CH6", "CH7",
    "CH8", "CH9", "CH10", "CH11",
    "CH12", "CH13", "CH14", "CH15",
	NULL,

};

static const gboolean default_ms_en[DSO_MS_END - DSO_MS_BEGIN] = {
    FALSE, /* DSO_MS_BEGIN */
    TRUE,  /* DSO_MS_FREQ */
    FALSE, /* DSO_MS_PERD */
    TRUE,  /* DSO_MS_VMAX */
    TRUE,  /* DSO_MS_VMIN */
    FALSE, /* DSO_MS_VRMS */
    FALSE, /* DSO_MS_VMEA */
    FALSE, /* DSO_MS_VP2P */
    FALSE, /* DSO_MS_END */
};

/* Private, per-device-instance driver context. */
/* TODO: struct context as with the other drivers. */

/* List of struct sr_dev_inst, maintained by dev_open()/dev_close(). */
SR_PRIV struct sr_dev_driver demo_driver_info;
static struct sr_dev_driver *di = &demo_driver_info;

extern struct ds_trigger *trigger;

static int hw_dev_acquisition_stop(struct sr_dev_inst *sdi, void *cb_data);

static int clear_instances(void)
{
	/* Nothing needed so far. */

	return SR_OK;
}

static int hw_init(struct sr_context *sr_ctx)
{
	return std_hw_init(sr_ctx, di, LOG_PREFIX);
}

static GSList *hw_scan(GSList *options)
{
	struct sr_dev_inst *sdi;
	struct sr_channel *probe;
	struct drv_context *drvc;
	struct dev_context *devc;
	GSList *devices;
    uint16_t i;

	(void)options;

	drvc = di->priv;

	devices = NULL;

    sdi = sr_dev_inst_new(LOGIC, 0, SR_ST_INITIALIZING, DEMONAME, NULL, NULL);
	if (!sdi) {
        sr_err("Device instance creation failed.");
		return NULL;
	}
	sdi->driver = di;

	devices = g_slist_append(devices, sdi);
	drvc->instances = g_slist_append(drvc->instances, sdi);

	if (!(devc = g_try_malloc(sizeof(struct dev_context)))) {
		sr_err("Device context malloc failed.");
		return NULL;
	}

	devc->sdi = sdi;
    devc->cur_samplerate = SR_MHZ(1);
    devc->limit_samples = SR_MB(1);
    devc->limit_samples_show = devc->limit_samples;
	devc->limit_msec = 0;
    devc->sample_generator = PATTERN_SINE;
    devc->timebase = 500;
    devc->data_lock = FALSE;
    devc->max_height = 0;
    devc->dso_bits = 8;

	sdi->priv = devc;

    if (sdi->mode == LOGIC) {
        for (i = 0; probe_names[i]; i++) {
            if (!(probe = sr_channel_new(i, SR_CHANNEL_LOGIC, TRUE,
                    probe_names[i])))
                return NULL;
            sdi->channels = g_slist_append(sdi->channels, probe);
        }
    } else if (sdi->mode == DSO) {
        for (i = 0; i < DS_MAX_DSO_PROBES_NUM; i++) {
            if (!(probe = sr_channel_new(i, SR_CHANNEL_DSO, TRUE,
                    probe_names[i])))
                return NULL;
            sdi->channels = g_slist_append(sdi->channels, probe);
        }
    } else if (sdi->mode == ANALOG) {
        for (i = 0; i < DS_MAX_ANALOG_PROBES_NUM; i++) {
            if (!(probe = sr_channel_new(i, SR_CHANNEL_ANALOG, TRUE,
                    probe_names[i])))
                return NULL;
            sdi->channels = g_slist_append(sdi->channels, probe);
        }
    }

	return devices;
}

static GSList *hw_dev_list(void)
{
	return ((struct drv_context *)(di->priv))->instances;
}

static GSList *hw_dev_mode_list(const struct sr_dev_inst *sdi)
{
    (void)sdi;
    GSList *l = NULL;
    int i;

    for(i = 0; i < ARRAY_SIZE(mode_list); i++) {
        l = g_slist_append(l, &mode_list[i]);
    }

    return l;
}

static int hw_dev_open(struct sr_dev_inst *sdi)
{
    //(void)sdi;
    struct dev_context *const devc = sdi->priv;

    sdi->status = SR_ST_ACTIVE;

    if (pipe(devc->pipe_fds)) {
        /* TODO: Better error message. */
        sr_err("%s: pipe() failed", __func__);
        return SR_ERR;
    }
    devc->channel = g_io_channel_unix_new(devc->pipe_fds[0]);
    g_io_channel_set_flags(devc->channel, G_IO_FLAG_NONBLOCK, NULL);
    /* Set channel encoding to binary (default is UTF-8). */
    g_io_channel_set_encoding(devc->channel, NULL, NULL);
    /* Make channels to unbuffered. */
    g_io_channel_set_buffered(devc->channel, FALSE);

	return SR_OK;
}

static int hw_dev_close(struct sr_dev_inst *sdi)
{
    //(void)sdi;
    struct dev_context *const devc = sdi->priv;

    if (sdi->status == SR_ST_ACTIVE && devc->channel) {
        g_io_channel_shutdown(devc->channel, FALSE, NULL);
        g_io_channel_unref(devc->channel);
        devc->channel = NULL;
    }
    sdi->status = SR_ST_INACTIVE;

    return SR_OK;
}

static int hw_cleanup(void)
{
	GSList *l;
	struct sr_dev_inst *sdi;
	struct drv_context *drvc;
	int ret = SR_OK;

	if (!(drvc = di->priv))
		return SR_OK;

	/* Properly close and free all devices. */
	for (l = drvc->instances; l; l = l->next) {
		if (!(sdi = l->data)) {
			/* Log error, but continue cleaning up the rest. */
			sr_err("%s: sdi was NULL, continuing", __func__);
			ret = SR_ERR_BUG;
			continue;
		}
		sr_dev_inst_free(sdi);
	}
	g_slist_free(drvc->instances);
	drvc->instances = NULL;

	return ret;
}

static int en_ch_num(const struct sr_dev_inst *sdi)
{
    GSList *l;
    int channel_en_cnt = 0;

    for (l = sdi->channels; l; l = l->next) {
        struct sr_channel *probe = (struct sr_channel *)l->data;
        channel_en_cnt += probe->enabled;
    }
    channel_en_cnt += (channel_en_cnt == 0);

    return channel_en_cnt;
}

static int config_get(int id, GVariant **data, const struct sr_dev_inst *sdi,
                      const struct sr_channel *ch,
                      const struct sr_channel_group *cg)
{
    (void) cg;

    struct dev_context *const devc = sdi->priv;

	switch (id) {
	case SR_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(devc->cur_samplerate);
		break;
	case SR_CONF_LIMIT_SAMPLES:
        *data = g_variant_new_uint64(devc->limit_samples_show);
		break;
	case SR_CONF_LIMIT_MSEC:
		*data = g_variant_new_uint64(devc->limit_msec);
		break;
    case SR_CONF_DEVICE_MODE:
        *data = g_variant_new_int16(sdi->mode);
        break;
    case SR_CONF_TEST:
        *data = g_variant_new_boolean(FALSE);
        break;
    case SR_CONF_INSTANT:
        *data = g_variant_new_boolean(devc->instant);
        break;
    case SR_CONF_PATTERN_MODE:
        *data = g_variant_new_string(pattern_strings[devc->sample_generator]);
		break;
    case SR_CONF_MAX_HEIGHT:
        *data = g_variant_new_string(maxHeights[devc->max_height]);
        break;
    case SR_CONF_MAX_HEIGHT_VALUE:
        *data = g_variant_new_byte(devc->max_height);
        break;
    case SR_CONF_VPOS:
        *data = g_variant_new_double(ch->vpos);
        break;
    case SR_CONF_VDIV:
        *data = g_variant_new_uint64(ch->vdiv);
        break;
    case SR_CONF_FACTOR:
        *data = g_variant_new_uint64(ch->vfactor);
        break;
    case SR_CONF_TIMEBASE:
        *data = g_variant_new_uint64(devc->timebase);
        break;
    case SR_CONF_COUPLING:
        *data = g_variant_new_byte(ch->coupling);
        break;
    case SR_CONF_TRIGGER_VALUE:
        *data = g_variant_new_byte(ch->trig_value);
        break;
    case SR_CONF_EN_CH:
        *data = g_variant_new_uint64(ch->enabled);
        break;
    case SR_CONF_DATALOCK:
        *data = g_variant_new_boolean(devc->data_lock);
        break;
    case SR_CONF_MAX_DSO_SAMPLERATE:
        *data = g_variant_new_uint64(DEMO_MAX_DSO_SAMPLERATE);
        break;
    case SR_CONF_MAX_DSO_SAMPLELIMITS:
        *data = g_variant_new_uint64(DEMO_MAX_DSO_DEPTH);
        break;
    case SR_CONF_MAX_LOGIC_SAMPLERATE:
        *data = g_variant_new_uint64(DEMO_MAX_LOGIC_SAMPLERATE);
        break;
    case SR_CONF_MAX_LOGIC_SAMPLELIMITS:
        *data = g_variant_new_uint64(DEMO_MAX_LOGIC_DEPTH);
        break;
    case SR_CONF_RLE_SAMPLELIMITS:
        *data = g_variant_new_uint64(DEMO_MAX_LOGIC_DEPTH);
        break;
    case SR_CONF_DSO_BITS:
        *data = g_variant_new_byte(devc->dso_bits);
        break;
    default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_set(int id, GVariant *data, struct sr_dev_inst *sdi,
                      struct sr_channel *ch,
                      const struct sr_channel_group *cg)
{
    uint16_t i, j;
    int ret;
	const char *stropt;
    struct sr_channel *probe;
    uint64_t tmp_u64;

    (void) cg;

	struct dev_context *const devc = sdi->priv;

	if (sdi->status != SR_ST_ACTIVE)
		return SR_ERR_DEV_CLOSED;

	if (id == SR_CONF_SAMPLERATE) {
		devc->cur_samplerate = g_variant_get_uint64(data);
        devc->samples_counter = 0;
        devc->pre_index = 0;
		sr_dbg("%s: setting samplerate to %" PRIu64, __func__,
		       devc->cur_samplerate);
		ret = SR_OK;
	} else if (id == SR_CONF_LIMIT_SAMPLES) {
        devc->limit_msec = 0;
        devc->limit_samples = g_variant_get_uint64(data);
        devc->limit_samples_show = devc->limit_samples;
        if (sdi->mode == DSO && en_ch_num(sdi) == 1) {
            devc->limit_samples /= 2;
        }
		sr_dbg("%s: setting limit_samples to %" PRIu64, __func__,
		       devc->limit_samples);
		ret = SR_OK;
	} else if (id == SR_CONF_LIMIT_MSEC) {
		devc->limit_msec = g_variant_get_uint64(data);
		devc->limit_samples = 0;
        devc->limit_samples_show = devc->limit_samples;
		sr_dbg("%s: setting limit_msec to %" PRIu64, __func__,
		       devc->limit_msec);
        ret = SR_OK;
    } else if (id == SR_CONF_DEVICE_MODE) {
        sdi->mode = g_variant_get_int16(data);
        ret = SR_OK;
        if (sdi->mode == LOGIC) {
            sr_dev_probes_free(sdi);
            for (i = 0; probe_names[i]; i++) {
                if (!(probe = sr_channel_new(i, SR_CHANNEL_LOGIC, TRUE,
                        probe_names[i])))
                    ret = SR_ERR;
                else
                    sdi->channels = g_slist_append(sdi->channels, probe);
            }
            devc->cur_samplerate = SR_MHZ(1);
            devc->limit_samples = SR_MB(1);
            devc->limit_samples_show = devc->limit_samples;
        } else if (sdi->mode == DSO) {
            sr_dev_probes_free(sdi);
            for (i = 0; i < DEMO_MAX_DSO_PROBES_NUM; i++) {
                if (!(probe = sr_channel_new(i, SR_CHANNEL_DSO, TRUE,
                        probe_names[i])))
                    ret = SR_ERR;
                else {
                    probe->vdiv = 1000;
                    probe->vfactor = 1;
                    probe->coupling = SR_AC_COUPLING;
                    probe->trig_value = 0x80;
                    probe->vpos = (probe->index == 0 ? 0.5 : -0.5)*probe->vdiv;
                    sdi->channels = g_slist_append(sdi->channels, probe);
                    probe->ms_show = TRUE;
                    for (j = DSO_MS_BEGIN; j < DSO_MS_END; j++)
                        probe->ms_en[j] = default_ms_en[j];
                }
            }
            devc->cur_samplerate = DEMO_MAX_DSO_SAMPLERATE / DEMO_MAX_DSO_PROBES_NUM;
            devc->limit_samples = DEMO_MAX_DSO_DEPTH / DEMO_MAX_DSO_PROBES_NUM;
            devc->limit_samples_show = devc->limit_samples;
        } else if (sdi->mode == ANALOG) {
            sr_dev_probes_free(sdi);
            for (i = 0; i < DS_MAX_ANALOG_PROBES_NUM; i++) {
                if (!(probe = sr_channel_new(i, SR_CHANNEL_ANALOG, TRUE,
                        probe_names[i])))
                    ret = SR_ERR;
                else
                    sdi->channels = g_slist_append(sdi->channels, probe);
            }
            devc->cur_samplerate = SR_HZ(100);
            devc->limit_samples = SR_KB(1);
            devc->limit_samples_show = devc->limit_samples;
        } else {
            ret = SR_ERR;
        }
        sr_dbg("%s: setting mode to %d", __func__, sdi->mode);
    }else if (id == SR_CONF_PATTERN_MODE) {
        stropt = g_variant_get_string(data, NULL);
        ret = SR_OK;
        if (!strcmp(stropt, pattern_strings[PATTERN_SINE])) {
            devc->sample_generator = PATTERN_SINE;
        } else if (!strcmp(stropt, pattern_strings[PATTERN_SQUARE])) {
            devc->sample_generator = PATTERN_SQUARE;
        } else if (!strcmp(stropt, pattern_strings[PATTERN_TRIANGLE])) {
            devc->sample_generator = PATTERN_TRIANGLE;
        } else if (!strcmp(stropt, pattern_strings[PATTERN_SAWTOOTH])) {
            devc->sample_generator = PATTERN_SAWTOOTH;
        } else if (!strcmp(stropt, pattern_strings[PATTERN_RANDOM])) {
            devc->sample_generator = PATTERN_RANDOM;
		} else {
            ret = SR_ERR;
		}
        sr_dbg("%s: setting pattern to %d",
			__func__, devc->sample_generator);
    } else if (id == SR_CONF_MAX_HEIGHT) {
        stropt = g_variant_get_string(data, NULL);
        ret = SR_OK;
        for (i = 0; i < ARRAY_SIZE(maxHeights); i++) {
            if (!strcmp(stropt, maxHeights[i])) {
                devc->max_height = i;
                break;
            }
        }
        sr_dbg("%s: setting Signal Max Height to %d",
            __func__, devc->max_height);
    } else if (id == SR_CONF_INSTANT) {
        devc->instant = g_variant_get_boolean(data);
        sr_dbg("%s: setting INSTANT mode to %d", __func__,
               devc->instant);
        ret = SR_OK;
    } else if (id == SR_CONF_HORIZ_TRIGGERPOS) {
        ret = SR_OK;
    } else if (id == SR_CONF_TRIGGER_HOLDOFF) {
        ret = SR_OK;
    } else if (id == SR_CONF_TRIGGER_MARGIN) {
        ret = SR_OK;
    } else if (id == SR_CONF_EN_CH) {
        ch->enabled = g_variant_get_boolean(data);
        sr_dbg("%s: setting ENABLE of channel %d to %d", __func__,
               ch->index, ch->enabled);
        ret = SR_OK;
    } else if (id == SR_CONF_DATALOCK) {
        devc->data_lock = g_variant_get_boolean(data);
        sr_dbg("%s: setting data lock to %d", __func__,
               devc->data_lock);
        ret = SR_OK;
    } else if (id == SR_CONF_VDIV) {
        tmp_u64 = g_variant_get_uint64(data);
        ch->vpos = (tmp_u64 * 1.0 / ch->vdiv) * ch->vpos;
        ch->vdiv = tmp_u64;
        sr_dbg("%s: setting VDIV of channel %d to %" PRIu64, __func__,
               ch->index, ch->vdiv);
        ret = SR_OK;
    } else if (id == SR_CONF_FACTOR) {
        ch->vfactor = g_variant_get_uint64(data);
        sr_dbg("%s: setting FACTOR of channel %d to %" PRIu64, __func__,
               ch->index, ch->vfactor);
        ret = SR_OK;
    } else if (id == SR_CONF_VPOS) {
        //ch->vpos = g_variant_get_double(data);
        sr_dbg("%s: setting VPOS of channel %d to %lf", __func__,
               ch->index, ch->vpos);
        ret = SR_OK;
    } else if (id == SR_CONF_TIMEBASE) {
        devc->timebase = g_variant_get_uint64(data);
        sr_dbg("%s: setting TIMEBASE to %" PRIu64, __func__,
               devc->timebase);
        ret = SR_OK;
    } else if (id == SR_CONF_COUPLING) {
        ch->coupling = g_variant_get_byte(data);
        sr_dbg("%s: setting AC COUPLING of channel %d to %d", __func__,
               ch->index, ch->coupling);
        ret = SR_OK;
    } else if (id == SR_CONF_TRIGGER_SOURCE) {
        devc->trigger_source = g_variant_get_byte(data);
        sr_dbg("%s: setting Trigger Source to %d",
            __func__, devc->trigger_source);
        ret = SR_OK;
    } else if (id == SR_CONF_TRIGGER_SLOPE) {
        devc->trigger_slope = g_variant_get_byte(data);
        sr_dbg("%s: setting Trigger Slope to %d",
            __func__, devc->trigger_slope);
        ret = SR_OK;
    } else if (id == SR_CONF_TRIGGER_VALUE) {
        ch->trig_value = g_variant_get_byte(data);
        sr_dbg("%s: setting channel %d Trigger Value to %d",
            __func__, ch->index, ch->trig_value);
        ret = SR_OK;
    } else {
        ret = SR_ERR_NA;
	}

	return ret;
}

static int config_list(int key, GVariant **data, const struct sr_dev_inst *sdi,
                       const struct sr_channel_group *cg)
{
	GVariant *gvar;
	GVariantBuilder gvb;

	(void)sdi;
    (void)cg;

	switch (key) {
    case SR_CONF_DEVICE_OPTIONS:
//		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_INT32,
//				hwcaps, ARRAY_SIZE(hwcaps), sizeof(int32_t));
		*data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                hwcaps, ARRAY_SIZE(hwcaps)*sizeof(int32_t), TRUE, NULL, NULL);
		break;
    case SR_CONF_DEVICE_CONFIGS:
//		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_INT32,
//				hwcaps, ARRAY_SIZE(hwcaps), sizeof(int32_t));
        *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                hwoptions, ARRAY_SIZE(hwoptions)*sizeof(int32_t), TRUE, NULL, NULL);
        break;
    case SR_CONF_DEVICE_SESSIONS:
        *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                sessions, ARRAY_SIZE(sessions)*sizeof(int32_t), TRUE, NULL, NULL);
        break;
    case SR_CONF_SAMPLERATE:
		g_variant_builder_init(&gvb, G_VARIANT_TYPE("a{sv}"));
//		gvar = g_variant_new_fixed_array(G_VARIANT_TYPE("t"), samplerates,
//				ARRAY_SIZE(samplerates), sizeof(uint64_t));
		gvar = g_variant_new_from_data(G_VARIANT_TYPE("at"),
				samplerates, ARRAY_SIZE(samplerates)*sizeof(uint64_t), TRUE, NULL, NULL);
        g_variant_builder_add(&gvb, "{sv}", "samplerates", gvar);
		*data = g_variant_builder_end(&gvb);
		break;
    case SR_CONF_LIMIT_SAMPLES:
        g_variant_builder_init(&gvb, G_VARIANT_TYPE("a{sv}"));
        gvar = g_variant_new_from_data(G_VARIANT_TYPE("at"),
                samplecounts, ARRAY_SIZE(samplecounts)*sizeof(uint64_t), TRUE, NULL, NULL);
        g_variant_builder_add(&gvb, "{sv}", "samplecounts", gvar);
        *data = g_variant_builder_end(&gvb);
        break;
    case SR_CONF_PATTERN_MODE:
		*data = g_variant_new_strv(pattern_strings, ARRAY_SIZE(pattern_strings));
		break;
    case SR_CONF_MAX_HEIGHT:
        *data = g_variant_new_strv(maxHeights, ARRAY_SIZE(maxHeights));
        break;
	default:
        return SR_ERR_NA;
	}

    return SR_OK;
}

static void samples_generator(uint16_t *buf, uint64_t size,
                              const struct sr_dev_inst *sdi,
                              struct dev_context *devc)
{
    uint64_t i, pre0_i, pre1_i, index;
    GSList *l;
    struct sr_channel *probe;
    int offset;
    int start_rand;
    const uint64_t span = DEMO_MAX_DSO_SAMPLERATE / devc->cur_samplerate;
    const uint64_t len = ARRAY_SIZE(sinx) - 1;
    int *pre_buf;

    switch (devc->sample_generator) {
    case PATTERN_SINE: /* Sine */
        pre_buf = sinx;
        break;
    case PATTERN_SQUARE:
        pre_buf = sqrx;
        break;
    case PATTERN_TRIANGLE:
        pre_buf = trix;
        break;
    case PATTERN_SAWTOOTH:
        pre_buf = sawx;
        break;
    case PATTERN_RANDOM:
        pre_buf = ranx;
        break;
    default:
        pre_buf = sinx;
        break;
    }

    if (devc->samples_counter == devc->limit_samples &&
        size != devc->limit_samples) {
        for (i = 0; i < devc->limit_samples; i++)
            *(buf + i) = *(buf + ((i + size)%devc->limit_samples));
    } else if (sdi->mode != DSO) {
        start_rand = rand()%len;
        for (i = 0; i < size; i++) {
            index = (i/10/g_slist_length(sdi->channels)+start_rand)%len;
            *(buf + i) = (uint16_t)(((const_dc+pre_buf[index]) << 8) + (const_dc+pre_buf[index]));
        }
    } else {
        if (devc->pre_index == 0) {
            devc->mstatus.ch0_max = 0;
            devc->mstatus.ch0_min = 255;
            devc->mstatus.ch1_max = 0;
            devc->mstatus.ch1_min = 255;
            devc->mstatus.ch0_period = 0;
            devc->mstatus.ch0_pcnt = 1;
            devc->mstatus.ch1_period = 0;
            devc->mstatus.ch1_pcnt = 1;
        }
        memset(buf+devc->pre_index, 0, size*sizeof(uint16_t));
        for (l = sdi->channels; l; l = l->next) {
            start_rand = devc->pre_index == 0 ? rand()%len : 0;
            probe = (struct sr_channel *)l->data;
            //offset = ceil((0.5 - (probe->vpos/probe->vdiv/10.0)) * 255);
            offset = 128;
            pre0_i = devc->pre_index;
            pre1_i = devc->pre_index;
            for (i = devc->pre_index; i < devc->pre_index + size; i++) {
                if (probe->coupling == SR_DC_COUPLING) {
                    *(buf + i) += (uint8_t)(offset + (1000.0/probe->vdiv) * (pre_buf[(i*span+start_rand)%len] - const_dc)) << (probe->index * 8);
                } else if (probe->coupling == SR_AC_COUPLING) {
                    *(buf + i) += (uint8_t)(offset + (1000.0/probe->vdiv) * pre_buf[(i*span+start_rand)%len]) << (probe->index * 8);
                } else {
                    *(buf + i) += offset << (probe->index * 8);
                }

                if (probe->index == 0) {
                    devc->mstatus.ch0_max = MAX(devc->mstatus.ch0_max, (*(buf + i) & 0x00ff));
                    devc->mstatus.ch0_min = MIN(devc->mstatus.ch0_min, (*(buf + i) & 0x00ff));
                    if (i > devc->pre_index &&
                        pre_buf[(i*span+start_rand)%len] < 0 &&
                        pre_buf[((i-1)*span+start_rand)%len] > 0) {
                        devc->mstatus.ch0_period = 2*(i - pre0_i)*pow(10, 8)/DEMO_MAX_DSO_SAMPLERATE;
                        pre0_i = i;
                    }
                } else {
                    devc->mstatus.ch1_max = MAX(devc->mstatus.ch1_max, ((*(buf + i) & 0xff00) >> 8));
                    devc->mstatus.ch1_min = MIN(devc->mstatus.ch1_min, ((*(buf + i) & 0xff00) >> 8));
                    if (i > devc->pre_index &&
                        pre_buf[(i*span+start_rand)%len] < 0 &&
                        pre_buf[((i-1)*span+start_rand)%len] > 0) {
                        devc->mstatus.ch1_period = 2*(i - pre1_i)*pow(10, 8)/DEMO_MAX_DSO_SAMPLERATE;
                        pre1_i = i;
                    }
                }
            }
        }

        for (l = sdi->channels; l; l = l->next) {
            probe = (struct sr_channel *)l->data;
            if (!probe->enabled) {
                devc->mstatus.ch1_max = MAX(devc->mstatus.ch0_max, devc->mstatus.ch1_max);
                devc->mstatus.ch1_min = MIN(devc->mstatus.ch0_min, devc->mstatus.ch1_min);
                devc->mstatus.ch0_max = MAX(devc->mstatus.ch0_max, devc->mstatus.ch1_max);
                devc->mstatus.ch0_min = MIN(devc->mstatus.ch0_min, devc->mstatus.ch1_min);
                break;
            }
        }
    }
}

/* Callback handling data */
static int receive_data(int fd, int revents, const struct sr_dev_inst *sdi)
{
    struct dev_context *devc = sdi->priv;
    struct sr_datafeed_packet packet;
    struct sr_datafeed_logic logic;
    struct sr_datafeed_dso dso;
    struct sr_datafeed_analog analog;
    static uint64_t samples_to_send = 0, expected_samplenum, sending_now;
	int64_t time, elapsed;
    static uint16_t last_sample = 0;
    uint16_t cur_sample;
    int i;

	(void)fd;
	(void)revents;

	/* How many "virtual" samples should we have collected by now? */
	time = g_get_monotonic_time();
	elapsed = time - devc->starttime;
    devc->starttime = time;
    //expected_samplenum = ceil(elapsed / 1000000.0 * devc->cur_samplerate);
	/* Of those, how many do we still have to send? */
    //samples_to_send = (expected_samplenum - devc->samples_counter) / CONST_LEN * CONST_LEN;
    //samples_to_send = expected_samplenum / CONST_LEN * CONST_LEN;
    samples_to_send += ceil(elapsed / 1000000.0 * devc->cur_samplerate);

    if (devc->limit_samples) {
        if (sdi->mode == DSO && !devc->instant)
            samples_to_send = MIN(samples_to_send,
                                  devc->limit_samples - devc->pre_index);
        else if (sdi->mode == ANALOG)
            samples_to_send = MIN(samples_to_send * g_slist_length(sdi->channels),
                                  devc->limit_samples - devc->pre_index);
        else
            samples_to_send = MIN(samples_to_send,
                     devc->limit_samples - devc->samples_counter);
    }

    if (samples_to_send > 0 && !devc->stop) {
        sending_now = MIN(samples_to_send, (sdi->mode == DSO ) ? DSO_BUFSIZE : BUFSIZE);
        samples_generator(devc->buf, sending_now, sdi, devc);

        if (devc->trigger_stage != 0) {
            for (i = 0; i < sending_now; i++) {
                if (devc->trigger_edge == 0) {
                    if ((*(devc->buf + i) | devc->trigger_mask) ==
                            (devc->trigger_value | devc->trigger_mask)) {
                        devc->trigger_stage = 0;
                        break;
                    }
                } else {
                    cur_sample = *(devc->buf + i);
                    if (((last_sample & devc->trigger_edge) ==
                         (~devc->trigger_value & devc->trigger_edge)) &&
                        ((cur_sample | devc->trigger_mask) ==
                         (devc->trigger_value | devc->trigger_mask)) &&
                        ((cur_sample & devc->trigger_edge) ==
                         (devc->trigger_value & devc->trigger_edge))) {
                        devc->trigger_stage = 0;
                        break;
                    }
                    last_sample = cur_sample;
                }
            }
            if (devc->trigger_stage == 0) {
                struct ds_trigger_pos demo_trigger_pos;
                demo_trigger_pos.real_pos = i;
                packet.type = SR_DF_TRIGGER;
                packet.payload = &demo_trigger_pos;
                sr_session_send(sdi, &packet);
            }
        }

        if (sdi->mode == ANALOG)
            devc->samples_counter += sending_now/g_slist_length(sdi->channels);
        else
            devc->samples_counter += sending_now;
        if (sdi->mode == DSO && !devc->instant &&
            devc->samples_counter > devc->limit_samples)
            devc->samples_counter = devc->limit_samples;

        if (devc->trigger_stage == 0){
            samples_to_send -= sending_now;
            if (sdi->mode == LOGIC) {
                packet.type = SR_DF_LOGIC;
                packet.payload = &logic;
                logic.length = sending_now * (NUM_PROBES >> 3);
                logic.unitsize = (NUM_PROBES >> 3);
                logic.data = devc->buf;
            } else if (sdi->mode == DSO) {
                packet.type = SR_DF_DSO;
                packet.payload = &dso;
                dso.probes = sdi->channels;
                if (devc->instant)
                    dso.num_samples = sending_now;
                else
                    dso.num_samples = devc->samples_counter;
                if (en_ch_num(sdi) == 1)
                    dso.num_samples *= 2;
                dso.mq = SR_MQ_VOLTAGE;
                dso.unit = SR_UNIT_VOLT;
                dso.mqflags = SR_MQFLAG_AC;
                dso.data = devc->buf;
            }else {
                packet.type = SR_DF_ANALOG;
                packet.payload = &analog;
                analog.probes = sdi->channels;
                analog.num_samples = sending_now / g_slist_length(sdi->channels);
                analog.mq = SR_MQ_VOLTAGE;
                analog.unit = SR_UNIT_VOLT;
                analog.mqflags = SR_MQFLAG_AC;
                analog.data = devc->buf;
            }

            if (sdi->mode == DSO && !devc->instant) {
                devc->pre_index += sending_now;
                if (devc->pre_index >= devc->limit_samples)
                    devc->pre_index = 0;
            }

            sr_session_send(sdi, &packet);

            devc->mstatus.trig_hit = (devc->trigger_stage == 0);
            devc->mstatus.captured_cnt0 = devc->samples_counter;
            devc->mstatus.captured_cnt1 = devc->samples_counter >> 8;
            devc->mstatus.captured_cnt2 = devc->samples_counter >> 16;
            devc->mstatus.captured_cnt3 = devc->samples_counter >> 32;
        }
	}

    if ((sdi->mode != DSO || devc->instant) && devc->limit_samples &&
        devc->samples_counter >= devc->limit_samples) {
        sr_info("Requested number of samples reached.");
        hw_dev_acquisition_stop(sdi, NULL);
        return TRUE;
    }

    return TRUE;
}

static int hw_dev_acquisition_start(const struct sr_dev_inst *sdi,
		void *cb_data)
{
	struct dev_context *const devc = sdi->priv;

    (void)cb_data;

    if (sdi->status != SR_ST_ACTIVE)
        return SR_ERR_DEV_CLOSED;

    //devc->cb_data = cb_data;
	devc->samples_counter = 0;
    devc->pre_index = 0;
    devc->mstatus.captured_cnt0 = 0;
    devc->mstatus.captured_cnt1 = 0;
    devc->mstatus.captured_cnt2 = 0;
    devc->mstatus.captured_cnt3 = 0;
    devc->stop = FALSE;

    /*
     * trigger setting
     */
    if (!trigger->trigger_en || sdi->mode != LOGIC) {
        devc->trigger_stage = 0;
    } else {
        devc->trigger_mask = ds_trigger_get_mask0(TriggerStages);
        devc->trigger_value = ds_trigger_get_value0(TriggerStages);
        devc->trigger_edge = ds_trigger_get_edge0(TriggerStages);
        if (devc->trigger_edge != 0)
            devc->trigger_stage = 2;
        else
            devc->trigger_stage = 1;
    }

	/*
	 * Setting two channels connected by a pipe is a remnant from when the
	 * demo driver generated data in a thread, and collected and sent the
	 * data in the main program loop.
	 * They are kept here because it provides a convenient way of setting
	 * up a timeout-based polling mechanism.
	 */

    sr_session_source_add_channel(devc->channel, G_IO_IN | G_IO_ERR,
            (sdi->mode == DSO) ? 50 : 10, receive_data, sdi);

	/* Send header packet to the session bus. */
    //std_session_send_df_header(cb_data, LOG_PREFIX);
    std_session_send_df_header(sdi, LOG_PREFIX);

    if (!(devc->buf = g_try_malloc(((sdi->mode == DSO ) ? DSO_BUFSIZE : BUFSIZE)*sizeof(uint16_t)))) {
        sr_err("buf for receive_data malloc failed.");
        return FALSE;
    }

	/* We use this timestamp to decide how many more samples to send. */
	devc->starttime = g_get_monotonic_time();

	return SR_OK;
}

static int hw_dev_acquisition_stop(struct sr_dev_inst *sdi, void *cb_data)
{
    (void)cb_data;

    struct dev_context *const devc = sdi->priv;
    struct sr_datafeed_packet packet;
    if (devc->stop)
        return SR_OK;

	sr_dbg("Stopping acquisition.");

    devc->stop = TRUE;

    sr_session_source_remove_channel(devc->channel);

    g_free(devc->buf);

	/* Send last packet. */
    packet.type = SR_DF_END;
    sr_session_send(sdi, &packet);

	return SR_OK;
}

static int hw_dev_test(struct sr_dev_inst *sdi)
{
    if (sdi)
        return SR_OK;
    else
        return SR_ERR;
}

static int hw_dev_status_get(struct sr_dev_inst *sdi, struct sr_status *status, int begin, int end)
{
    (void)begin;
    (void)end;
    if (sdi) {
        struct dev_context *const devc = sdi->priv;
        *status = devc->mstatus;
        return SR_OK;
    } else {
        return SR_ERR;
    }
}

SR_PRIV struct sr_dev_driver demo_driver_info = {
    .name = "virtual-demo",
	.longname = "Demo driver and pattern generator",
	.api_version = 1,
	.init = hw_init,
	.cleanup = hw_cleanup,
	.scan = hw_scan,
	.dev_list = hw_dev_list,
    .dev_mode_list = hw_dev_mode_list,
	.dev_clear = clear_instances,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = hw_dev_open,
	.dev_close = hw_dev_close,
    .dev_test = hw_dev_test,
    .dev_status_get = hw_dev_status_get,
	.dev_acquisition_start = hw_dev_acquisition_start,
	.dev_acquisition_stop = hw_dev_acquisition_stop,
	.priv = NULL,
};
