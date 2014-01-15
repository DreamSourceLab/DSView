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
#define BUFSIZE                1024*1024

#define PERIOD                  4000

#define PI 3.14159265

#define CONST_LEN               50

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

/* Private, per-device-instance driver context. */
struct dev_context {
	struct sr_dev_inst *sdi;
	int pipe_fds[2];
	GIOChannel *channel;
	uint64_t cur_samplerate;
	uint64_t limit_samples;
	uint64_t limit_msec;
	uint8_t sample_generator;
	uint64_t samples_counter;
	void *cb_data;
	int64_t starttime;
    int stop;

    int trigger_stage;
    uint16_t trigger_mask;
    uint16_t trigger_value;
    uint16_t trigger_edge;
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
};

static const uint64_t samplerates[] = {
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
};



/* We name the probes 0-7 on our demo driver. */
static const char *probe_names[NUM_PROBES + 1] = {
    "Channel 0", "Channel 1", "Channel 2", "Channel 3",
    "Channel 4", "Channel 5", "Channel 6", "Channel 7",
    "Channel 8", "Channel 9", "Channel 10", "Channel 11",
    "Channel 12", "Channel 13", "Channel 14", "Channel 15",
	NULL,
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
	struct sr_probe *probe;
	struct drv_context *drvc;
	struct dev_context *devc;
	GSList *devices;
	int i;

	(void)options;

	drvc = di->priv;

	devices = NULL;

    sdi = sr_dev_inst_new(LOGIC, 0, SR_ST_ACTIVE, DEMONAME, NULL, NULL);
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
    devc->cur_samplerate = SR_MHZ(200);
	devc->limit_samples = 0;
	devc->limit_msec = 0;
    devc->sample_generator = PATTERN_SINE;

	sdi->priv = devc;

    if (sdi->mode == LOGIC) {
        for (i = 0; probe_names[i]; i++) {
            if (!(probe = sr_probe_new(i, SR_PROBE_LOGIC, TRUE,
                    probe_names[i])))
                return NULL;
            sdi->probes = g_slist_append(sdi->probes, probe);
        }
    } else if (sdi->mode == ANALOG) {
        for (i = 0; i < DS_MAX_ANALOG_PROBES_NUM; i++) {
            if (!(probe = sr_probe_new(i, SR_PROBE_ANALOG, TRUE,
                    probe_names[i])))
                return NULL;
            sdi->probes = g_slist_append(sdi->probes, probe);
        }
    }

	return devices;
}

static GSList *hw_dev_list(void)
{
	return ((struct drv_context *)(di->priv))->instances;
}

static int hw_dev_open(struct sr_dev_inst *sdi)
{
	(void)sdi;

	sdi->status = SR_ST_ACTIVE;

	return SR_OK;
}

static int hw_dev_close(struct sr_dev_inst *sdi)
{
	(void)sdi;

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

static int config_get(int id, GVariant **data, const struct sr_dev_inst *sdi)
{
	struct dev_context *const devc = sdi->priv;

	switch (id) {
	case SR_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(devc->cur_samplerate);
		break;
	case SR_CONF_LIMIT_SAMPLES:
		*data = g_variant_new_uint64(devc->limit_samples);
		break;
	case SR_CONF_LIMIT_MSEC:
		*data = g_variant_new_uint64(devc->limit_msec);
		break;
    case SR_CONF_DEVICE_MODE:
        *data = g_variant_new_string(mode_strings[sdi->mode]);
        break;
    case SR_CONF_PATTERN_MODE:
        *data = g_variant_new_string(pattern_strings[devc->sample_generator]);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_set(int id, GVariant *data, struct sr_dev_inst *sdi)
{
    int i, ret;
	const char *stropt;
    struct sr_probe *probe;

	struct dev_context *const devc = sdi->priv;

	if (sdi->status != SR_ST_ACTIVE)
		return SR_ERR_DEV_CLOSED;

	if (id == SR_CONF_SAMPLERATE) {
		devc->cur_samplerate = g_variant_get_uint64(data);
		sr_dbg("%s: setting samplerate to %" PRIu64, __func__,
		       devc->cur_samplerate);
		ret = SR_OK;
	} else if (id == SR_CONF_LIMIT_SAMPLES) {
		devc->limit_msec = 0;
		devc->limit_samples = g_variant_get_uint64(data);
		sr_dbg("%s: setting limit_samples to %" PRIu64, __func__,
		       devc->limit_samples);
		ret = SR_OK;
	} else if (id == SR_CONF_LIMIT_MSEC) {
		devc->limit_msec = g_variant_get_uint64(data);
		devc->limit_samples = 0;
		sr_dbg("%s: setting limit_msec to %" PRIu64, __func__,
		       devc->limit_msec);
        ret = SR_OK;
    } else if (id == SR_CONF_DEVICE_MODE) {
        stropt = g_variant_get_string(data, NULL);
        ret = SR_OK;
        if (!strcmp(stropt, mode_strings[LOGIC])) {
            sdi->mode = LOGIC;
            sr_dev_probes_free(sdi);
            for (i = 0; probe_names[i]; i++) {
                if (!(probe = sr_probe_new(i, SR_PROBE_LOGIC, TRUE,
                        probe_names[i])))
                    ret = SR_ERR;
                else
                    sdi->probes = g_slist_append(sdi->probes, probe);
            }
        } else if (!strcmp(stropt, mode_strings[ANALOG])) {
            sdi->mode = ANALOG;
            sr_dev_probes_free(sdi);
            for (i = 0; i < DS_MAX_ANALOG_PROBES_NUM; i++) {
                if (!(probe = sr_probe_new(i, SR_PROBE_ANALOG, TRUE,
                        probe_names[i])))
                    ret = SR_ERR;
                else
                    sdi->probes = g_slist_append(sdi->probes, probe);
            }
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
	} else {
        ret = SR_ERR_NA;
	}

	return ret;
}

static int config_list(int key, GVariant **data, const struct sr_dev_inst *sdi)
{
	GVariant *gvar;
	GVariantBuilder gvb;

	(void)sdi;

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
    case SR_CONF_SAMPLERATE:
		g_variant_builder_init(&gvb, G_VARIANT_TYPE("a{sv}"));
//		gvar = g_variant_new_fixed_array(G_VARIANT_TYPE("t"), samplerates,
//				ARRAY_SIZE(samplerates), sizeof(uint64_t));
		gvar = g_variant_new_from_data(G_VARIANT_TYPE("at"),
				samplerates, ARRAY_SIZE(samplerates)*sizeof(uint64_t), TRUE, NULL, NULL);
        g_variant_builder_add(&gvb, "{sv}", "samplerates", gvar);
		*data = g_variant_builder_end(&gvb);
		break;
    case SR_CONF_DEVICE_MODE:
        *data = g_variant_new_strv(mode_strings, ARRAY_SIZE(mode_strings));
        break;
    case SR_CONF_PATTERN_MODE:
		*data = g_variant_new_strv(pattern_strings, ARRAY_SIZE(pattern_strings));
		break;
	default:
        return SR_ERR_NA;
	}

    return SR_OK;
}

static void samples_generator(uint16_t *buf, uint64_t size,
			      struct dev_context *devc)
{
    static uint16_t p = 0;
	uint64_t i;
    uint16_t demo_data;

	switch (devc->sample_generator) {
    case PATTERN_SINE: /* Sine */
        for (i = 0; i < size; i++) {
            if (i%CONST_LEN == 0) {
                demo_data = 0x8000 * sin(2 * PI * p / 0xffff) + 0x8000;
                p += CONST_LEN;
            }
            *(buf + i) = demo_data;
        }
		break;
    case PATTERN_SQUARE:
        for (i = 0; i < size; i++) {
            if (i%CONST_LEN == 0) {
                demo_data = p > 0x7fff ? 0xf000 : 0x1000;
                p += CONST_LEN;
            }
            *(buf + i) = demo_data;
        }
        break;
    case PATTERN_TRIANGLE:
        for (i = 0; i < size; i++) {
            if (i%CONST_LEN == 0) {
                demo_data = p > 0x7fff ? 0xffff * (2.0f * (0x10000 - p) / 0x10000) :
                                         0xffff * (2.0f * p / 0x10000);
                p += CONST_LEN;
            }
            *(buf + i) = demo_data;
        }
        break;
    case PATTERN_SAWTOOTH:
        for (i = 0; i < size; i++) {
            if (i%CONST_LEN == 0) {
                demo_data = p;
                p += CONST_LEN;
            }
            *(buf + i) = demo_data;
        }
        break;
	case PATTERN_RANDOM: /* Random */
        for (i = 0; i < size; i++) {
            if (i%CONST_LEN == 0)
                demo_data = (uint16_t)(rand() * (0x10000 * 1.0f / RAND_MAX));
            *(buf + i) = demo_data;
        }
		break;
	default:
        sr_err("Unknown pattern: %d.", devc->sample_generator);
		break;
	}
}

/* Callback handling data */
static int receive_data(int fd, int revents, const struct sr_dev_inst *sdi)
{
    struct dev_context *devc = sdi->priv;
    struct sr_datafeed_packet packet;
    struct sr_datafeed_logic logic;
    struct sr_datafeed_analog analog;
    //uint16_t buf[BUFSIZE];
    uint16_t *buf;
	static uint64_t samples_to_send, expected_samplenum, sending_now;
	int64_t time, elapsed;
    static uint16_t last_sample = 0;
    uint16_t cur_sample;
    int i;

	(void)fd;
	(void)revents;

    if (!(buf = g_try_malloc(BUFSIZE*sizeof(uint16_t)))) {
        sr_err("buf for receive_data malloc failed.");
        return FALSE;
    }

	/* How many "virtual" samples should we have collected by now? */
	time = g_get_monotonic_time();
	elapsed = time - devc->starttime;
    devc->starttime = time;
	expected_samplenum = elapsed * devc->cur_samplerate / 1000000;
	/* Of those, how many do we still have to send? */
    //samples_to_send = (expected_samplenum - devc->samples_counter) / CONST_LEN * CONST_LEN;
    samples_to_send = expected_samplenum / CONST_LEN * CONST_LEN;

    if (devc->limit_samples) {
        if (sdi->mode == LOGIC)
            samples_to_send = MIN(samples_to_send,
                     devc->limit_samples - devc->samples_counter);
        else if (sdi->mode == ANALOG)
            samples_to_send = MIN(samples_to_send,
                     devc->limit_samples);
    }

    while (samples_to_send > 0) {
        sending_now = MIN(samples_to_send, BUFSIZE);
		samples_generator(buf, sending_now, devc);

        if (devc->trigger_stage != 0) {
            for (i = 0; i < sending_now; i++) {
                if (devc->trigger_edge == 0) {
                    if ((*(buf + i) | devc->trigger_mask) ==
                            (devc->trigger_value | devc->trigger_mask)) {
                        devc->trigger_stage = 0;
                        break;
                    }
                } else {
                    cur_sample = *(buf + i);
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

        if (devc->trigger_stage == 0){
            samples_to_send -= sending_now;
            if (sdi->mode == LOGIC) {
                packet.type = SR_DF_LOGIC;
                packet.payload = &logic;
                logic.length = sending_now * (NUM_PROBES >> 3);
                logic.unitsize = (NUM_PROBES >> 3);
                logic.data = buf;
            } else if (sdi->mode == ANALOG) {
                packet.type = SR_DF_ANALOG;
                packet.payload = &analog;
                analog.probes = sdi->probes;
                analog.num_samples = sending_now;
                analog.mq = SR_MQ_VOLTAGE;
                analog.unit = SR_UNIT_VOLT;
                analog.mqflags = SR_MQFLAG_AC;
                analog.data = buf;
            }

            sr_session_send(sdi, &packet);
            if (sdi->mode == LOGIC)
                devc->samples_counter += sending_now;
            else if (sdi->mode == ANALOG)
                devc->samples_counter = (devc->samples_counter + sending_now) % devc->limit_samples;
        } else {
            break;
        }
	}

    if (sdi->mode == LOGIC &&
        devc->limit_samples &&
        devc->samples_counter >= devc->limit_samples) {
        sr_info("Requested number of samples reached.");
        hw_dev_acquisition_stop(sdi, NULL);
        g_free(buf);
        return TRUE;
    }

    g_free(buf);

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
    devc->stop = FALSE;

    /*
     * trigger setting
     */
    if (!trigger->trigger_en || sdi->mode == ANALOG) {
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

    sr_session_source_add_channel(devc->channel, G_IO_IN | G_IO_ERR,
            100, receive_data, sdi);

	/* Send header packet to the session bus. */
    //std_session_send_df_header(cb_data, LOG_PREFIX);
    std_session_send_df_header(sdi, LOG_PREFIX);

	/* We use this timestamp to decide how many more samples to send. */
	devc->starttime = g_get_monotonic_time();

	return SR_OK;
}

static int hw_dev_acquisition_stop(struct sr_dev_inst *sdi, void *cb_data)
{
	struct dev_context *const devc = sdi->priv;
	struct sr_datafeed_packet packet;

	(void)cb_data;

	sr_dbg("Stopping aquisition.");

    devc->stop = TRUE;
    sr_session_source_remove_channel(devc->channel);
	g_io_channel_shutdown(devc->channel, FALSE, NULL);
	g_io_channel_unref(devc->channel);
	devc->channel = NULL;

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

SR_PRIV struct sr_dev_driver demo_driver_info = {
	.name = "demo",
	.longname = "Demo driver and pattern generator",
	.api_version = 1,
	.init = hw_init,
	.cleanup = hw_cleanup,
	.scan = hw_scan,
	.dev_list = hw_dev_list,
	.dev_clear = clear_instances,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = hw_dev_open,
	.dev_close = hw_dev_close,
    .dev_test = hw_dev_test,
	.dev_acquisition_start = hw_dev_acquisition_start,
	.dev_acquisition_stop = hw_dev_acquisition_stop,
	.priv = NULL,
};
