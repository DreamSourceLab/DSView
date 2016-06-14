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
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <zip.h>
#include "libsigrok.h"
#include "libsigrok-internal.h"

/* Message logging helpers with subsystem-specific prefix string. */
#define LOG_PREFIX "virtual-session: "
#define sr_log(l, s, args...) sr_log(l, LOG_PREFIX s, ## args)
#define sr_spew(s, args...) sr_spew(LOG_PREFIX s, ## args)
#define sr_dbg(s, args...) sr_dbg(LOG_PREFIX s, ## args)
#define sr_info(s, args...) sr_info(LOG_PREFIX s, ## args)
#define sr_warn(s, args...) sr_warn(LOG_PREFIX s, ## args)
#define sr_err(s, args...) sr_err(LOG_PREFIX s, ## args)

/* size of payloads sent across the session bus */
/** @cond PRIVATE */
#define CHUNKSIZE (512 * 1024)
/** @endcond */

static uint64_t samplerates[1];
static uint64_t samplecounts[1];

struct session_vdev {
	char *sessionfile;
	char *capturefile;
	struct zip *archive;
	struct zip_file *capfile;
    void *buf;
	int bytes_read;
	uint64_t samplerate;
    uint64_t total_samples;
    int64_t trig_time;
    uint64_t trig_pos;
	int unitsize;
	int num_probes;
    uint64_t timebase;
    struct sr_status mstatus;
};

static GSList *dev_insts = NULL;
static const int hwcaps[] = {
	SR_CONF_CAPTUREFILE,
	SR_CONF_CAPTURE_UNITSIZE,
	0,
};

static int receive_data(int fd, int revents, const struct sr_dev_inst *cb_sdi)
{
	struct sr_dev_inst *sdi;
	struct session_vdev *vdev;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_logic logic;
    struct sr_datafeed_dso dso;
    struct sr_datafeed_analog analog;
	GSList *l;
	int ret, got_data;
	(void)fd;
	(void)revents;

	sr_dbg("Feed chunk.");

	got_data = FALSE;
	for (l = dev_insts; l; l = l->next) {
		sdi = l->data;
		vdev = sdi->priv;
		if (!vdev)
			/* already done with this instance */
			continue;

        ret = zip_fread(vdev->capfile, vdev->buf, CHUNKSIZE);
		if (ret > 0) {
			got_data = TRUE;
            if (sdi->mode == DSO) {
                packet.type = SR_DF_DSO;
                packet.payload = &dso;
                dso.num_samples = ret / vdev->unitsize;
                dso.data = vdev->buf;
                dso.probes = sdi->channels;
                dso.mq = SR_MQ_VOLTAGE;
                dso.unit = SR_UNIT_VOLT;
                dso.mqflags = SR_MQFLAG_AC;
            } else if (sdi->mode == ANALOG){
                packet.type = SR_DF_ANALOG;
                packet.payload = &analog;
                analog.probes = sdi->channels;
                analog.num_samples = ret / vdev->unitsize;
                analog.mq = SR_MQ_VOLTAGE;
                analog.unit = SR_UNIT_VOLT;
                analog.mqflags = SR_MQFLAG_AC;
                analog.data = vdev->buf;
            } else {
                packet.type = SR_DF_LOGIC;
                packet.payload = &logic;
                logic.length = ret;
                logic.unitsize = vdev->unitsize;
                logic.data = vdev->buf;
            }
			vdev->bytes_read += ret;
			sr_session_send(cb_sdi, &packet);
		} else {
			/* done with this capture file */
			zip_fclose(vdev->capfile);
            //g_free(vdev->capturefile);
            //g_free(vdev);
            //sdi->priv = NULL;
		}
	}

	if (!got_data) {
		packet.type = SR_DF_END;
		sr_session_send(cb_sdi, &packet);
		sr_session_source_remove(-1);
	}

	return TRUE;
}

/* driver callbacks */
static int dev_clear(void);

static int init(struct sr_context *sr_ctx)
{
	(void)sr_ctx;

	return SR_OK;
}

static int dev_clear(void)
{
	GSList *l;

	for (l = dev_insts; l; l = l->next)
		sr_dev_inst_free(l->data);
	g_slist_free(dev_insts);
	dev_insts = NULL;

	return SR_OK;
}

static int dev_open(struct sr_dev_inst *sdi)
{
	if (!(sdi->priv = g_try_malloc0(sizeof(struct session_vdev)))) {
		sr_err("%s: sdi->priv malloc failed", __func__);
		return SR_ERR_MALLOC;
	}

    struct session_vdev *vdev;
    vdev = sdi->priv;
    if (!(vdev->buf = g_try_malloc(CHUNKSIZE))) {
        sr_err("%s: vdev->buf malloc failed", __func__);
        return SR_ERR_MALLOC;
    }
    vdev->trig_pos = 0;
    vdev->trig_time = 0;

	dev_insts = g_slist_append(dev_insts, sdi);

	return SR_OK;
}

static int dev_close(struct sr_dev_inst *sdi)
{
    const struct session_vdev *const vdev = sdi->priv;
    g_free(vdev->sessionfile);
    g_free(vdev->capturefile);
    g_free(vdev->buf);

    g_free(sdi->priv);
    sdi->priv = NULL;

    return SR_OK;
}

static int config_get(int id, GVariant **data, const struct sr_dev_inst *sdi,
                      const struct sr_channel *ch,
                      const struct sr_channel_group *cg)
{
	struct session_vdev *vdev;

	switch (id) {
	case SR_CONF_SAMPLERATE:
		if (sdi) {
			vdev = sdi->priv;
			*data = g_variant_new_uint64(vdev->samplerate);
		} else
			return SR_ERR;
		break;
    case SR_CONF_LIMIT_SAMPLES:
        if (sdi) {
            vdev = sdi->priv;
            *data = g_variant_new_uint64(vdev->total_samples);
        } else
            return SR_ERR;
        break;
    case SR_CONF_TRIGGER_TIME:
        if (sdi) {
            vdev = sdi->priv;
            *data = g_variant_new_int64(vdev->trig_time);
        } else
            return SR_ERR;
        break;
    case SR_CONF_TIMEBASE:
        if (sdi) {
            vdev = sdi->priv;
            *data = g_variant_new_uint64(vdev->timebase);
        } else
            return SR_ERR;
        break;
    case SR_CONF_EN_CH:
        if (sdi && ch) {
            *data = g_variant_new_boolean(ch->enabled);
        } else
            return SR_ERR;
        break;
    case SR_CONF_COUPLING:
        if (sdi && ch) {
            *data = g_variant_new_byte(ch->coupling);
        } else
            return SR_ERR;
        break;
    case SR_CONF_VDIV:
        if (sdi && ch) {
            *data = g_variant_new_uint64(ch->vdiv);
        } else
            return SR_ERR;
        break;
    case SR_CONF_FACTOR:
        if (sdi && ch) {
            *data = g_variant_new_uint64(ch->vfactor);
        } else
            return SR_ERR;
        break;
    case SR_CONF_VPOS:
        if (sdi && ch) {
            *data = g_variant_new_double(ch->vpos);
        } else
            return SR_ERR;
        break;
    case SR_CONF_MAX_DSO_SAMPLERATE:
        if (!sdi)
            return SR_ERR;
        *data = g_variant_new_uint64(vdev->samplerate);
        break;
    case SR_CONF_MAX_DSO_SAMPLELIMITS:
        if (!sdi)
            return SR_ERR;
        *data = g_variant_new_uint64(vdev->total_samples);
        break;
    case SR_CONF_RLE_SAMPLELIMITS:
        *data = g_variant_new_uint64(UINT64_MAX);
        break;
    default:
		return SR_ERR_ARG;
	}

	return SR_OK;
}

static int config_set(int id, GVariant *data, const struct sr_dev_inst *sdi,
                      struct sr_channel *ch,
                      const struct sr_channel_group *cg)
{
	struct session_vdev *vdev;

	vdev = sdi->priv;

	switch (id) {
	case SR_CONF_SAMPLERATE:
		vdev->samplerate = g_variant_get_uint64(data);
        samplerates[0] = vdev->samplerate;
		sr_info("Setting samplerate to %" PRIu64 ".", vdev->samplerate);
		break;
    case SR_CONF_TIMEBASE:
        vdev->timebase = g_variant_get_uint64(data);
        sr_info("Setting timebase to %" PRIu64 ".", vdev->timebase);
        break;
    case SR_CONF_SESSIONFILE:
        vdev->sessionfile = g_strdup(g_variant_get_bytestring(data));
		sr_info("Setting sessionfile to '%s'.", vdev->sessionfile);
		break;
	case SR_CONF_CAPTUREFILE:
        vdev->capturefile = g_strdup(g_variant_get_bytestring(data));
		sr_info("Setting capturefile to '%s'.", vdev->capturefile);
		break;
	case SR_CONF_CAPTURE_UNITSIZE:
		vdev->unitsize = g_variant_get_uint64(data);
		break;
    case SR_CONF_LIMIT_SAMPLES:
        vdev->total_samples = g_variant_get_uint64(data);
        samplecounts[0] = vdev->total_samples;
        sr_info("Setting limit samples to %" PRIu64 ".", vdev->total_samples);
        break;
    case SR_CONF_TRIGGER_TIME:
        vdev->trig_time = g_variant_get_int64(data);
        sr_info("Setting trigger time to %" PRId64 ".", vdev->trig_time);
        break;
    case SR_CONF_TRIGGER_POS:
        vdev->trig_pos = g_variant_get_uint64(data);
        sr_info("Setting trigger position to %" PRIu64 ".", vdev->trig_pos);
        break;
    case SR_CONF_CAPTURE_NUM_PROBES:
		vdev->num_probes = g_variant_get_uint64(data);
		break;
    case SR_CONF_EN_CH:
        ch->enabled = g_variant_get_boolean(data);
        break;
    case SR_CONF_COUPLING:
        ch->coupling = g_variant_get_byte(data);
        break;
    case SR_CONF_VDIV:
        ch->vdiv = g_variant_get_uint64(data);
        break;
    case SR_CONF_FACTOR:
        ch->vfactor = g_variant_get_uint64(data);
        break;
    case SR_CONF_VPOS:
        ch->vpos = g_variant_get_double(data);
        break;
    case SR_CONF_STATUS_PERIOD:
        if (ch->index == 0)
            vdev->mstatus.ch0_period = g_variant_get_uint64(data);
        else
            vdev->mstatus.ch1_period = g_variant_get_uint64(data);
        break;
    case SR_CONF_STATUS_PCNT:
        if (ch->index == 0)
            vdev->mstatus.ch0_pcnt = g_variant_get_uint64(data);
        else
            vdev->mstatus.ch1_pcnt = g_variant_get_uint64(data);
        break;
    case SR_CONF_STATUS_MAX:
        if (ch->index == 0)
            vdev->mstatus.ch0_max = g_variant_get_uint64(data);
        else
            vdev->mstatus.ch1_max = g_variant_get_uint64(data);
        break;
    case SR_CONF_STATUS_MIN:
        if (ch->index == 0)
            vdev->mstatus.ch0_min = g_variant_get_uint64(data);
        else
            vdev->mstatus.ch1_min = g_variant_get_uint64(data);
        break;
    default:
		sr_err("Unknown capability: %d.", id);
		return SR_ERR;
	}

	return SR_OK;
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
	default:
		return SR_ERR_ARG;
	}

	return SR_OK;
}

static int dev_status_get(struct sr_dev_inst *sdi, struct sr_status *status, int begin, int end)
{
    (void)begin;
    (void)end;

    struct session_vdev *vdev;

    if (sdi) {
        vdev = sdi->priv;
        *status = vdev->mstatus;
        return SR_OK;
    } else {
        return SR_ERR;
    }
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi,
		void *cb_data)
{
	struct zip_stat zs;
	struct session_vdev *vdev;
    struct sr_datafeed_packet packet;
	int ret;

	vdev = sdi->priv;

	sr_info("Opening archive %s file %s", vdev->sessionfile,
		vdev->capturefile);

	if (!(vdev->archive = zip_open(vdev->sessionfile, 0, &ret))) {
		sr_err("Failed to open session file '%s': "
		       "zip error %d\n", vdev->sessionfile, ret);
		return SR_ERR;
	}

	if (zip_stat(vdev->archive, vdev->capturefile, 0, &zs) == -1) {
		sr_err("Failed to check capture file '%s' in "
		       "session file '%s'.", vdev->capturefile, vdev->sessionfile);
		return SR_ERR;
	}

	if (!(vdev->capfile = zip_fopen(vdev->archive, vdev->capturefile, 0))) {
		sr_err("Failed to open capture file '%s' in "
		       "session file '%s'.", vdev->capturefile, vdev->sessionfile);
		return SR_ERR;
	}

	/* Send header packet to the session bus. */
    std_session_send_df_header(sdi, LOG_PREFIX);

    /* Send trigger packet to the session bus */
    if (vdev->trig_pos != 0) {
        struct ds_trigger_pos session_trigger;
        session_trigger.real_pos = vdev->trig_pos;
        packet.type = SR_DF_TRIGGER;
        packet.payload = &session_trigger;
        sr_session_send(sdi, &packet);
    }

	/* freewheeling source */
    sr_session_source_add(-1, 0, 0, receive_data, sdi);

	return SR_OK;
}

/** @private */
SR_PRIV struct sr_dev_driver session_driver = {
    .name = "virtual-session",
    .longname = "Session-emulating driver",
    .api_version = 1,
    .init = init,
    .cleanup = dev_clear,
    .scan = NULL,
    .dev_list = NULL,
    .dev_mode_list = NULL,
    .dev_clear = dev_clear,
    .config_get = config_get,
    .config_set = config_set,
    .config_list = config_list,
    .dev_open = dev_open,
    .dev_close = dev_close,
    .dev_test = NULL,
    .dev_status_get = dev_status_get,
    .dev_acquisition_start = dev_acquisition_start,
    .dev_acquisition_stop = NULL,
    .priv = NULL,
};
