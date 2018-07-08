/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2013 DreamSourceLab <dreamsourcelab@dreamsourcelab.com>
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

#include "libsigrok.h"
#include "libsigrok-internal.h"

#include "dsl.h"
#include "command.h"


enum {
    /** Buffer mode */
    OP_BUFFER = 0,
    /** Stream mode */
    OP_STREAM = 1,
    /** Internal pattern test mode */
    OP_INTEST = 2,
    /** External pattern test mode */
    OP_EXTEST = 3,
    /** SDRAM loopback test mode */
    OP_LPTEST = 4,
};
static const char *opmodes[] = {
    "Buffer Mode",
    "Stream Mode",
    "Internal Test",
    "External Test",
    "DRAM Loopback Test",
};

static const char *bufoptions[] = {
    "Stop immediately",
    "Upload captured data",
};

static const char *thresholds[] = {
    "1.8/2.5/3.3V Level",
    "5.0V Level",
};

static const char *filters[] = {
    "None",
    "1 Sample Clock",
};

static const char *maxHeights[] = {
    "1X",
    "2X",
    "3X",
    "4X",
    "5X",
};

static const int32_t hwoptions[] = {
    SR_CONF_OPERATION_MODE,
    SR_CONF_BUFFER_OPTIONS,
    SR_CONF_THRESHOLD,
    SR_CONF_FILTER,
    SR_CONF_MAX_HEIGHT,
    SR_CONF_RLE_SUPPORT,
    SR_CONF_CLOCK_TYPE,
    SR_CONF_CLOCK_EDGE,
};

static const int32_t hwoptions_pro[] = {
    SR_CONF_OPERATION_MODE,
    SR_CONF_BUFFER_OPTIONS,
    SR_CONF_VTH,
    SR_CONF_FILTER,
    SR_CONF_MAX_HEIGHT,
    SR_CONF_RLE_SUPPORT,
    SR_CONF_CLOCK_TYPE,
    SR_CONF_CLOCK_EDGE,
};

static const int32_t sessions[] = {
    SR_CONF_MAX_HEIGHT,
    SR_CONF_OPERATION_MODE,
    SR_CONF_BUFFER_OPTIONS,
    SR_CONF_CHANNEL_MODE,
    SR_CONF_SAMPLERATE,
    SR_CONF_LIMIT_SAMPLES,
    SR_CONF_RLE_SUPPORT,
    SR_CONF_CLOCK_TYPE,
    SR_CONF_CLOCK_EDGE,
    SR_CONF_THRESHOLD,
    SR_CONF_FILTER,
    SR_CONF_TRIGGER_SLOPE,
    SR_CONF_TRIGGER_SOURCE,
    SR_CONF_HORIZ_TRIGGERPOS,
    SR_CONF_TRIGGER_HOLDOFF,
    SR_CONF_TRIGGER_MARGIN,
};

static const int32_t sessions_pro[] = {
    SR_CONF_MAX_HEIGHT,
    SR_CONF_OPERATION_MODE,
    SR_CONF_BUFFER_OPTIONS,
    SR_CONF_CHANNEL_MODE,
    SR_CONF_SAMPLERATE,
    SR_CONF_LIMIT_SAMPLES,
    SR_CONF_RLE_SUPPORT,
    SR_CONF_CLOCK_TYPE,
    SR_CONF_CLOCK_EDGE,
    SR_CONF_VTH,
    SR_CONF_FILTER,
    SR_CONF_TRIGGER_SLOPE,
    SR_CONF_TRIGGER_SOURCE,
	SR_CONF_TRIGGER_CHANNEL,
    SR_CONF_HORIZ_TRIGGERPOS,
    SR_CONF_TRIGGER_HOLDOFF,
    SR_CONF_TRIGGER_MARGIN,
};


static uint16_t opmodes_show_count = 3;

SR_PRIV struct sr_dev_driver DSLogic_driver_info;
static struct sr_dev_driver *di = &DSLogic_driver_info;

static struct DSL_context *DSLogic_dev_new(const struct DSL_profile *prof)
{
    struct DSL_context *devc;
    unsigned int i;

    if (!(devc = g_try_malloc(sizeof(struct DSL_context)))) {
        sr_err("Device context malloc failed.");
		return NULL;
	}

    for (i = 0; i < ARRAY_SIZE(channel_modes); i++)
        assert(channel_modes[i].id == i);

    devc->channel = NULL;
    devc->profile = prof;
	devc->fw_updated = 0;
    devc->cur_samplerate = devc->profile->dev_caps.default_samplerate;
    devc->limit_samples = devc->profile->dev_caps.default_samplelimit;
    devc->clock_type = FALSE;
    devc->clock_edge = FALSE;
    devc->rle_mode = FALSE;
    devc->instant = FALSE;
    devc->op_mode = OP_STREAM;
    devc->test_mode = SR_TEST_NONE;
    devc->stream = (devc->op_mode == OP_STREAM);
    devc->buf_options = SR_BUF_UPLOAD;
    devc->ch_mode = devc->profile->dev_caps.default_channelmode;
    devc->th_level = SR_TH_3V3;
    devc->vth = 1.0;
    devc->filter = SR_FILTER_NONE;
    devc->timebase = 10000;
    devc->trigger_slope = DSO_TRIGGER_RISING;
    devc->trigger_source = DSO_TRIGGER_AUTO;
    devc->trigger_hpos = 0x0;
    devc->trigger_hrate = 0;
    devc->trigger_holdoff = 0;
    devc->zero = FALSE;

    devc->mstatus_valid = FALSE;
    devc->data_lock = FALSE;
    devc->max_height = 0;
    devc->trigger_margin = 8;
    devc->trigger_channel = 0;

    dsl_adjust_samplerate(devc);

	return devc;
}

static int dev_clear(void)
{
	return std_dev_clear(di, NULL);
}

static int init(struct sr_context *sr_ctx)
{
    return std_hw_init(sr_ctx, di, LOG_PREFIX);
}

static GSList *scan(GSList *options)
{
	struct drv_context *drvc;
    struct DSL_context *devc;
    struct sr_dev_inst *sdi;
    struct sr_usb_dev_inst *usb;
    struct sr_config *src;
    const struct DSL_profile *prof;
	GSList *l, *devices, *conn_devices;
	struct libusb_device_descriptor des;
	libusb_device **devlist;
    int devcnt, ret, i, j;
	const char *conn;

	drvc = di->priv;

	conn = NULL;
	for (l = options; l; l = l->next) {
		src = l->data;
		switch (src->key) {
        case SR_CONF_CONN:
            conn = g_variant_get_string(src->data, NULL);
			break;
		}
	}
	if (conn)
        conn_devices = sr_usb_find(drvc->sr_ctx->libusb_ctx, conn);
	else
		conn_devices = NULL;

    /* Find all DSLogic compatible devices and upload firmware to them. */
	devices = NULL;
    libusb_get_device_list(drvc->sr_ctx->libusb_ctx, &devlist);
	for (i = 0; devlist[i]; i++) {
		if (conn) {
			usb = NULL;
			for (l = conn_devices; l; l = l->next) {
				usb = l->data;
				if (usb->bus == libusb_get_bus_number(devlist[i])
					&& usb->address == libusb_get_device_address(devlist[i]))
					break;
			}
			if (!l)
				/* This device matched none of the ones that
				 * matched the conn specification. */
				continue;
		}

		if ((ret = libusb_get_device_descriptor( devlist[i], &des)) != 0) {
            sr_warn("Failed to get device descriptor: %s.",
				libusb_error_name(ret));
			continue;
		}

		prof = NULL;
        for (j = 0; supported_DSLogic[j].vid; j++) {
            if (des.idVendor == supported_DSLogic[j].vid &&
                des.idProduct == supported_DSLogic[j].pid) {
                prof = &supported_DSLogic[j];
			}
		}

		/* Skip if the device was not found. */
		if (!prof)
			continue;

		devcnt = g_slist_length(drvc->instances);
        devc = DSLogic_dev_new(prof);
        if (!devc)
            return NULL;
        sdi = sr_dev_inst_new(channel_modes[devc->ch_mode].mode, devcnt, SR_ST_INITIALIZING,
			prof->vendor, prof->model, prof->model_version);
        if (!sdi) {
            g_free(devc);
			return NULL;
        }
        sdi->priv = devc;
		sdi->driver = di;

		drvc->instances = g_slist_append(drvc->instances, sdi);
        //devices = g_slist_append(devices, sdi);

        /* Fill in probelist according to this device's profile. */
        if (dsl_setup_probes(sdi, channel_modes[devc->ch_mode].num) != SR_OK)
            return NULL;

        if (dsl_check_conf_profile(devlist[i])) {
			/* Already has the firmware, so fix the new address. */
            sr_dbg("Found an DSLogic device.");
            sdi->status = SR_ST_INACTIVE;
            sdi->inst_type = SR_INST_USB;
            sdi->conn = sr_usb_dev_inst_new(libusb_get_bus_number(devlist[i]),
					libusb_get_device_address(devlist[i]), NULL);
            /* only report device after firmware is ready */
            devices = g_slist_append(devices, sdi);
		} else {
            char *firmware;
            if (!(firmware = g_try_malloc(strlen(DS_RES_PATH)+strlen(prof->firmware)+1))) {
                sr_err("Firmware path malloc error!");
                return NULL;
            }
            strcpy(firmware, DS_RES_PATH);
            strcat(firmware, prof->firmware);
            if (ezusb_upload_firmware(devlist[i], USB_CONFIGURATION,
                firmware) == SR_OK)
				/* Store when this device's FW was updated. */
				devc->fw_updated = g_get_monotonic_time();
			else
                sr_err("Firmware upload failed for "
				       "device %d.", devcnt);
            g_free(firmware);
            sdi->inst_type = SR_INST_USB;
            sdi->conn = sr_usb_dev_inst_new (libusb_get_bus_number(devlist[i]),
					0xff, NULL);
		}
	}
	libusb_free_device_list(devlist, 1);
    g_slist_free_full(conn_devices, (GDestroyNotify)sr_usb_dev_inst_free);

	return devices;
}

static GSList *dev_list(void)
{
	return ((struct drv_context *)(di->priv))->instances;
}

static const GSList *dev_mode_list(const struct sr_dev_inst *sdi)
{
    return dsl_mode_list(sdi);
}

static uint64_t dso_cmd_gen(const struct sr_dev_inst *sdi, struct sr_channel* ch, int id)
{
    struct DSL_context *devc;
    uint64_t cmd = 0;
    int channel_cnt = 0;
    GSList *l;
    struct sr_channel *en_probe = ch;

    devc = sdi->priv;

    switch (id) {
    case SR_CONF_PROBE_VDIV:
    case SR_CONF_PROBE_EN:
    case SR_CONF_TIMEBASE:
    case SR_CONF_PROBE_COUPLING:
        for (l = sdi->channels; l; l = l->next) {
            struct sr_channel *probe = (struct sr_channel *)l->data;
            if (probe->enabled) {
                channel_cnt += probe->index + 0x1;
                en_probe = probe;
            }
        }
        if (channel_cnt == 0)
            return 0x0;

        //  --VDBS
        if (channel_cnt != 1)
            en_probe = ch;
        switch(en_probe->vdiv){
        case 5:     cmd += 0x247000; break;
        case 10:    cmd += 0x23D000; break;
        case 20:    cmd += 0x22F000; break;
        case 50:    cmd += 0x21C800; break;
        case 100:   cmd += 0x20E800; break;
        case 200:   cmd += 0x200800; break;
        case 500:   cmd += 0x2F000; break;
        case 1000:  cmd += 0x21100; break;
        case 2000:  cmd += 0x13000; break;
        case 5000:  cmd += 0x00800; break;
        default: cmd += 0x21100; break;
        }
        // --DC/AC
        if (channel_cnt == 1) {
            for (l = sdi->channels; l; l = l->next) {
                struct sr_channel *probe = (struct sr_channel *)l->data;
                if (probe->coupling == SR_AC_COUPLING)
                    cmd += 0x100000;
                break;
            }
        } else {
            if(ch->coupling == SR_AC_COUPLING)
                cmd += 0x100000;
        }

        // --Channel
        if (sdi->mode != LOGIC) {
            if(channel_cnt == 1)
                cmd += 0xC00000;
            else if(ch->index == 0)
                cmd += 0x400000;
            else if(ch->index == 1)
                cmd += 0x800000;
            else
                cmd += 0x000000;
        }

        // --Header
        cmd += 0x55000000;
        break;
    case SR_CONF_SAMPLERATE:
        for (l = sdi->channels; l; l = l->next) {
            struct sr_channel *probe = (struct sr_channel *)l->data;
            channel_cnt += probe->enabled;
        }
        cmd += 0x18;
        uint32_t divider = (uint32_t)ceil(channel_modes[devc->ch_mode].max_samplerate * 1.0  / devc->cur_samplerate / channel_cnt);
        cmd += divider << 8;
        break;
    case SR_CONF_HORIZ_TRIGGERPOS:
        cmd += 0x20;
        cmd += devc->trigger_hpos << 8;
        break;
    case SR_CONF_TRIGGER_SLOPE:
        cmd += 0x28;
        cmd += devc->trigger_slope << 8;
        break;
    case SR_CONF_TRIGGER_SOURCE:
        cmd += 0x30;
        cmd += devc->trigger_source << 8;
        break;
    case SR_CONF_TRIGGER_VALUE:
        cmd += 0x38;
        for (l = sdi->channels; l; l = l->next) {
            struct sr_channel *probe = (struct sr_channel *)l->data;
            cmd += probe->trig_value << (8 * (probe->index + 1));
        }
        break;
    case SR_CONF_TRIGGER_MARGIN:
        cmd += 0x40;
        cmd += ((uint64_t)devc->trigger_margin << 8);
        break;
    case SR_CONF_TRIGGER_HOLDOFF:
        cmd += 0x58;
        cmd += devc->trigger_holdoff << 8;
        break;
    case SR_CONF_DSO_SYNC:
        cmd = 0xa5a5a500;
        break;
    default:
        cmd = 0xFFFFFFFF;
    }

    return cmd;
}

static int dso_init(const struct sr_dev_inst *sdi)
{
    int ret;
    GSList *l;

    for(l = sdi->channels; l; l = l->next) {
        struct sr_channel *probe = (struct sr_channel *)l->data;
        ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe, SR_CONF_PROBE_COUPLING));
        if (ret != SR_OK) {
            sr_err("DSO set coupling of channel %d command failed!", probe->index);
            return ret;
        }
        ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe, SR_CONF_PROBE_VDIV));
        if (ret != SR_OK) {
            sr_err("Set VDIV of channel %d command failed!", probe->index);
            return ret;
        }
    }

    ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, 0, SR_CONF_SAMPLERATE));
    if (ret != SR_OK) {
        sr_err("Set Sample Rate command failed!");
        return ret;
    }
    ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, NULL, SR_CONF_HORIZ_TRIGGERPOS));
    if (ret != SR_OK) {
        sr_err("Set Horiz Trigger Position command failed!");
        return ret;
    }
    ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, NULL, SR_CONF_TRIGGER_HOLDOFF));
    if (ret != SR_OK) {
        sr_err("Set Trigger Holdoff Time command failed!");
        return ret;
    }
    ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, NULL, SR_CONF_TRIGGER_SLOPE));
    if (ret != SR_OK) {
        sr_err("Set Trigger Slope command failed!");
        return ret;
    }
    ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, NULL, SR_CONF_TRIGGER_SOURCE));
    if (ret != SR_OK) {
        sr_err("Set Trigger Source command failed!");
        return ret;
    }
    ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, NULL, SR_CONF_TRIGGER_VALUE));
    if (ret != SR_OK) {
        sr_err("Set Trigger Value command failed!");
        return ret;
    }
    ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, NULL, SR_CONF_TRIGGER_MARGIN));
    if (ret != SR_OK) {
        sr_err("Set Trigger Margin command failed!");
        return ret;
    }
    return ret;
}

static int config_get(int id, GVariant **data, const struct sr_dev_inst *sdi,
                      const struct sr_channel *ch,
                      const struct sr_channel_group *cg)
{
    struct DSL_context *devc = sdi->priv;
    int ret;

    ret = dsl_config_get(id, data, sdi, ch, cg);
    if (ret != SR_OK) {
        switch (id) {
        case SR_CONF_OPERATION_MODE:
            if (!sdi)
                return SR_ERR;
            *data = g_variant_new_string(opmodes[devc->op_mode]);
            break;
        case SR_CONF_FILTER:
            if (!sdi)
                return SR_ERR;
            *data = g_variant_new_string(filters[devc->filter]);
            break;
        case SR_CONF_RLE:
            if (!sdi)
                return SR_ERR;
            *data = g_variant_new_boolean(devc->rle_mode);
            break;
		case SR_CONF_TEST:
            if (!sdi)
                return SR_ERR;
            *data = g_variant_new_boolean(devc->test_mode != SR_TEST_NONE);
            break;
        case SR_CONF_ACTUAL_SAMPLES:
            if (!sdi)
                return SR_ERR;
            *data = g_variant_new_uint64(devc->actual_samples);
            break;
        case SR_CONF_WAIT_UPLOAD:
            if (!sdi)
                return SR_ERR;
            if (devc->buf_options == SR_BUF_UPLOAD &&
                devc->status == DSL_START) {
                devc->status = DSL_ABORT;
                dsl_wr_reg(sdi, CTR0_ADDR, bmFORCE_STOP);
                *data = g_variant_new_boolean(TRUE);
            } else {
                *data = g_variant_new_boolean(FALSE);
            }
            break;
        case SR_CONF_BUFFER_OPTIONS:
            if (!sdi)
                return SR_ERR;
            *data = g_variant_new_string(bufoptions[devc->buf_options]);
            break;
        case SR_CONF_CHANNEL_MODE:
            if (!sdi)
                return SR_ERR;
            *data = g_variant_new_string(channel_modes[devc->ch_mode].descr);
            break;
        case SR_CONF_MAX_HEIGHT:
            if (!sdi)
                return SR_ERR;
            *data = g_variant_new_string(maxHeights[devc->max_height]);
            break;
        case SR_CONF_MAX_HEIGHT_VALUE:
            if (!sdi)
                return SR_ERR;
            *data = g_variant_new_byte(devc->max_height);
            break;
        case SR_CONF_THRESHOLD:
            if (!sdi)
                return SR_ERR;
            *data = g_variant_new_string(thresholds[devc->th_level]);
            break;
        case SR_CONF_VTH:
            if (!sdi)
                return SR_ERR;
            *data = g_variant_new_double(devc->vth);
            break;
        case SR_CONF_STREAM:
            if (!sdi)
                return SR_ERR;
            *data = g_variant_new_boolean(devc->stream);
            break;
        case SR_CONF_MAX_DSO_SAMPLERATE:
            if (!sdi)
                return SR_ERR;
            *data = g_variant_new_uint64(channel_modes[devc->ch_mode].max_samplerate);
            break;
        case SR_CONF_MAX_DSO_SAMPLELIMITS:
            if (!sdi)
                return SR_ERR;
            *data = g_variant_new_uint64(devc->profile->dev_caps.dso_depth);
            break;
        case SR_CONF_HW_DEPTH:
            if (!sdi)
                return SR_ERR;
            *data = g_variant_new_uint64(dsl_channel_depth(sdi));
            break;
        case SR_CONF_VLD_CH_NUM:
            if (!sdi)
                return SR_ERR;
            *data = g_variant_new_int16(channel_modes[devc->ch_mode].num);
            break;
        default:
            return SR_ERR_NA;
        }
    }

    return SR_OK;
}

static int config_set(int id, GVariant *data, struct sr_dev_inst *sdi,
                      struct sr_channel *ch,
                      struct sr_channel_group *cg )
{
    struct DSL_context *devc;
    const char *stropt;
    int ret, num_probes = 0;
    struct sr_usb_dev_inst *usb;
    unsigned int i;

    (void)cg;

    if (sdi->status != SR_ST_ACTIVE) {
        return SR_ERR;
    }

	devc = sdi->priv;
    usb = sdi->conn;

    ret = SR_OK;

    if (id == SR_CONF_CLOCK_TYPE) {
        devc->clock_type = g_variant_get_boolean(data);
    } else if (id == SR_CONF_RLE_SUPPORT) {
        devc->rle_support = g_variant_get_boolean(data);
    } else if (id == SR_CONF_CLOCK_EDGE) {
        devc->clock_edge = g_variant_get_boolean(data);
    } else if (id == SR_CONF_LIMIT_SAMPLES) {
        devc->limit_samples = g_variant_get_uint64(data);
    } else if (id == SR_CONF_PROBE_VDIV) {
        ch->vdiv = g_variant_get_uint64(data);
        if (sdi->mode != LOGIC) {
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, ch, SR_CONF_PROBE_VDIV));
        }
        if (ret == SR_OK)
            sr_dbg("%s: setting VDIV of channel %d to %d mv",
                __func__, ch->index, ch->vdiv);
        else
            sr_dbg("%s: setting VDIV of channel %d to %d mv failed",
                __func__, ch->index, ch->vdiv);
    } else if (id == SR_CONF_PROBE_FACTOR) {
        ch->vfactor = g_variant_get_uint64(data);
        sr_dbg("%s: setting Factor of channel %d to %d", __func__,
               ch->index, ch->vfactor);
    } else if (id == SR_CONF_TIMEBASE) {
        devc->timebase = g_variant_get_uint64(data);
    } else if (id == SR_CONF_PROBE_COUPLING) {
        ch->coupling = g_variant_get_byte(data);
        if (ch->coupling == SR_GND_COUPLING)
            ch->coupling = SR_DC_COUPLING;
        if (sdi->mode != LOGIC) {
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, ch, SR_CONF_PROBE_COUPLING));
        }
        if (ret == SR_OK)
            sr_dbg("%s: setting AC COUPLING of channel %d to %d",
                __func__, ch->index, ch->coupling);
        else
            sr_dbg("%s: setting AC COUPLING of channel %d to %d failed",
                __func__, ch->index, ch->coupling);
    } else if (id == SR_CONF_TRIGGER_SLOPE) {
        devc->trigger_slope = g_variant_get_byte(data);
        if (sdi->mode == DSO) {
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, NULL, SR_CONF_TRIGGER_SLOPE));
        }
        if (ret == SR_OK)
            sr_dbg("%s: setting DSO Trigger Slope to %d",
                __func__, devc->trigger_slope);
        else
            sr_dbg("%s: setting DSO Trigger Slope to %d failed",
                __func__, devc->trigger_slope);
    } else if (id == SR_CONF_TRIGGER_VALUE) {
        ch->trig_value = g_variant_get_byte(data);
        if (sdi->mode == DSO) {
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, ch, SR_CONF_TRIGGER_VALUE));
        }
        if (ret == SR_OK)
            sr_dbg("%s: setting channel %d Trigger Value to %d",
                __func__, ch->index, ch->trig_value);
        else
            sr_dbg("%s: setting DSO Trigger Value to %d failed",
                __func__, ch->index, ch->trig_value);
    } else if (id == SR_CONF_HORIZ_TRIGGERPOS) {
        if (sdi->mode == DSO) {
            devc->trigger_hrate = g_variant_get_byte(data);
            //devc->trigger_hpos = devc->trigger_hrate * dsl_en_ch_num(sdi) * devc->limit_samples / 200.0;
            /*
             * devc->trigger_hpos should be updated before each acquisition
             * because the samplelimits may changed
             */
            devc->trigger_hpos = devc->trigger_hrate * dsl_en_ch_num(sdi) * devc->limit_samples / 200.0;
            if ((ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, NULL, SR_CONF_HORIZ_TRIGGERPOS))) == SR_OK)
                sr_dbg("%s: setting DSO Horiz Trigger Position to %d",
                    __func__, devc->trigger_hpos);
            else
                sr_dbg("%s: setting DSO Horiz Trigger Position to %d failed",
                    __func__, devc->trigger_hpos);
        } else {
            devc->trigger_hpos = g_variant_get_byte(data) * devc->limit_samples / 100.0;
        }
    } else if (id == SR_CONF_TRIGGER_HOLDOFF) {
        devc->trigger_holdoff = g_variant_get_uint64(data);
        if (sdi->mode == DSO) {
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, NULL, SR_CONF_TRIGGER_HOLDOFF));
        }
        if (ret == SR_OK)
            sr_dbg("%s: setting Trigger Holdoff Time to %d",
                __func__, devc->trigger_holdoff);
        else
            sr_dbg("%s: setting Trigger Holdoff Time to %d failed",
                __func__, devc->trigger_holdoff);
    } else if (id == SR_CONF_TRIGGER_MARGIN) {
        devc->trigger_margin = g_variant_get_byte(data);
        if (sdi->mode == DSO) {
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, NULL, SR_CONF_TRIGGER_MARGIN));
        }
        if (ret == SR_OK)
            sr_dbg("%s: setting Trigger Margin to %d",
                __func__, devc->trigger_margin);
        else
            sr_dbg("%s: setting Trigger Margin to %d failed",
                __func__, devc->trigger_margin);
    } else if (id == SR_CONF_SAMPLERATE) {
        if (devc->test_mode == SR_TEST_NONE) {
            devc->cur_samplerate = g_variant_get_uint64(data);
            if(sdi->mode != LOGIC) {
                ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, 0, SR_CONF_SAMPLERATE));
            }
        }
    } else if (id == SR_CONF_FILTER) {
        stropt = g_variant_get_string(data, NULL);
        if (!strcmp(stropt, filters[SR_FILTER_NONE])) {
            devc->filter = SR_FILTER_NONE;
        } else if (!strcmp(stropt, filters[SR_FILTER_1T])) {
            devc->filter = SR_FILTER_1T;
        } else {
            ret = SR_ERR;
        }
        sr_dbg("%s: setting filter to %d",
            __func__, devc->filter);
    } else if (id == SR_CONF_RLE) {
        devc->rle_mode = g_variant_get_boolean(data);
    } else if (id == SR_CONF_INSTANT) {
        if (sdi->mode == DSO) {
            devc->instant = g_variant_get_boolean(data);
            if (dsl_en_ch_num(sdi) != 0) {
                if (devc->instant)
                    devc->limit_samples = devc->profile->dev_caps.hw_depth / dsl_en_ch_num(sdi);
                else
                    devc->limit_samples = devc->profile->dev_caps.dso_depth / dsl_en_ch_num(sdi);
            }
        }
    } else if (id == SR_CONF_DEVICE_MODE) {
        sdi->mode = g_variant_get_int16(data);
        if (sdi->mode == LOGIC) {
            dsl_wr_reg(sdi, CTR0_ADDR, bmSCOPE_CLR);
            for (i = 0; i < ARRAY_SIZE(channel_modes); i++) {
                if (channel_modes[i].mode == LOGIC &&
                    devc->profile->dev_caps.channels & (1 << i)) {
                    devc->ch_mode = channel_modes[i].id;
                    num_probes = channel_modes[i].num;
                    devc->stream = channel_modes[i].stream;
                    dsl_adjust_samplerate(devc);
                    break;
                }
            }
        } else if (sdi->mode == DSO) {
            dsl_wr_reg(sdi, CTR0_ADDR, bmSCOPE_SET);
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, NULL, SR_CONF_DSO_SYNC));
            if (ret != SR_OK)
                sr_dbg("%s: DSO configuration sync failed", __func__);
            for (i = 0; i < ARRAY_SIZE(channel_modes); i++) {
                if (channel_modes[i].mode == DSO &&
                    devc->profile->dev_caps.channels & (1 << i)) {
                    devc->ch_mode = channel_modes[i].id;
                    num_probes = channel_modes[i].num;
                    devc->stream = channel_modes[i].stream;
                    devc->cur_samplerate = channel_modes[i].max_samplerate / num_probes;
                    dsl_adjust_samplerate(devc);
                    break;
                }
            }
            devc->limit_samples = devc->profile->dev_caps.dso_depth / num_probes;
        } else if (sdi->mode == ANALOG)  {
            dsl_wr_reg(sdi, CTR0_ADDR, bmSCOPE_SET);
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, NULL, SR_CONF_DSO_SYNC));
            if (ret != SR_OK)
                sr_dbg("%s: DAQ configuration sync failed", __func__);
            devc->op_mode = OP_STREAM;
            devc->test_mode = SR_TEST_NONE;
            for (i = 0; i < ARRAY_SIZE(channel_modes); i++) {
                if (channel_modes[i].mode == ANALOG &&
                    devc->profile->dev_caps.channels & (1 << i)) {
                    devc->ch_mode = channel_modes[i].id;
                    num_probes = channel_modes[i].num;
                    devc->stream = channel_modes[i].stream;
                    dsl_adjust_samplerate(devc);
                    break;
                }
            }
        } else {
            ret = SR_ERR;
        }
        assert(num_probes != 0);
        sr_dev_probes_free(sdi);
        dsl_setup_probes(sdi, num_probes);
        sr_dbg("%s: setting mode to %d", __func__, sdi->mode);
        if (sdi->mode != LOGIC) {
            dso_init(sdi);
        }
    } else if (id == SR_CONF_OPERATION_MODE) {
        stropt = g_variant_get_string(data, NULL);
        if (sdi->mode == LOGIC) {
            if (!strcmp(stropt, opmodes[OP_BUFFER]) && (devc->op_mode != OP_BUFFER)) {
                devc->op_mode = OP_BUFFER;
                devc->test_mode = SR_TEST_NONE;
                devc->stream = FALSE;
                for (i = 0; i < ARRAY_SIZE(channel_modes); i++) {
                    if (channel_modes[i].mode == LOGIC &&
                        channel_modes[i].stream == devc->stream &&
                        devc->profile->dev_caps.channels & (1 << i)) {
                        devc->ch_mode = channel_modes[i].id;
                        break;
                    }
                }
            } else if (!strcmp(stropt, opmodes[OP_STREAM]) && (devc->op_mode != OP_STREAM)) {
                devc->op_mode = OP_STREAM;
                devc->test_mode = SR_TEST_NONE;
                devc->stream = TRUE;
                for (i = 0; i < ARRAY_SIZE(channel_modes); i++) {
                    if (channel_modes[i].mode == LOGIC &&
                        channel_modes[i].stream == devc->stream &&
                        devc->profile->dev_caps.channels & (1 << i)) {
                        devc->ch_mode = channel_modes[i].id;
                        break;
                    }
                }
            } else if (!strcmp(stropt, opmodes[OP_INTEST]) && (devc->op_mode != OP_INTEST)) {
                devc->op_mode = OP_INTEST;
                devc->test_mode = SR_TEST_INTERNAL;
                devc->ch_mode = devc->profile->dev_caps.intest_channel;
                devc->stream = !(devc->profile->dev_caps.feature_caps & CAPS_FEATURE_BUF);
            } else {
                ret = SR_ERR;
            }
            dsl_adjust_probes(sdi, channel_modes[devc->ch_mode].num);
            dsl_adjust_samplerate(devc);
            if (devc->op_mode == OP_INTEST) {
                devc->cur_samplerate = devc->stream ? channel_modes[devc->ch_mode].max_samplerate / 2 :
                                                      channel_modes[devc->ch_mode].max_samplerate;
                devc->limit_samples = devc->stream ? devc->cur_samplerate * 3 :
                                                     devc->profile->dev_caps.hw_depth / dsl_en_ch_num(sdi);
            }
        }
        sr_dbg("%s: setting pattern to %d",
            __func__, devc->op_mode);
    } else if (id == SR_CONF_BUFFER_OPTIONS) {
        stropt = g_variant_get_string(data, NULL);
        if (sdi->mode == LOGIC) {
            if (!strcmp(stropt, bufoptions[SR_BUF_STOP]))
                devc->buf_options = SR_BUF_STOP;
            else if (!strcmp(stropt, bufoptions[SR_BUF_UPLOAD]))
                devc->buf_options = SR_BUF_UPLOAD;
        }
    } else if (id == SR_CONF_CHANNEL_MODE) {
        stropt = g_variant_get_string(data, NULL);
        if (sdi->mode == LOGIC) {
            for (i = 0; i < ARRAY_SIZE(channel_modes); i++) {
                if (!strcmp(stropt, channel_modes[i].descr)) {
                    devc->ch_mode = channel_modes[i].id;
                    break;
                }
            }
            dsl_adjust_probes(sdi, channel_modes[devc->ch_mode].num);
            dsl_adjust_samplerate(devc);
        }
        sr_dbg("%s: setting channel mode to %d",
            __func__, devc->ch_mode);
    } else if (id == SR_CONF_THRESHOLD) {
        if (sdi->mode == LOGIC) {
            stropt = g_variant_get_string(data, NULL);
            if (strcmp(stropt, thresholds[devc->th_level])) {
                if (!strcmp(stropt, thresholds[SR_TH_3V3])) {
                    devc->th_level = SR_TH_3V3;
                } else if (!strcmp(stropt, thresholds[SR_TH_5V0])) {
                    devc->th_level = SR_TH_5V0;
                } else {
                    ret = SR_ERR;
                }
                char *fpga_bit;
                if (!(fpga_bit = g_try_malloc(strlen(DS_RES_PATH)+strlen(devc->profile->fpga_bit33)+1))) {
                    sr_err("fpag_bit path malloc error!");
                    return SR_ERR_MALLOC;
                }
                strcpy(fpga_bit, DS_RES_PATH);
                switch(devc->th_level) {
                case SR_TH_3V3:
                    strcat(fpga_bit, devc->profile->fpga_bit33);
                    break;
                case SR_TH_5V0:
                    strcat(fpga_bit, devc->profile->fpga_bit50);
                    break;
                default:
                    return SR_ERR;
                }
                ret = dsl_fpga_config(usb->devhdl, fpga_bit);
                g_free(fpga_bit);
                if (ret != SR_OK) {
                    sr_err("Configure FPGA failed!");
                }
            }
            sr_dbg("%s: setting threshold to %d",
                __func__, devc->th_level);
        }
    } else if (id == SR_CONF_VTH) {
        devc->vth = g_variant_get_double(data);
        ret = dsl_wr_reg(sdi, VTH_ADDR, (uint8_t)(devc->vth/5.0*255));
    } else if (id == SR_CONF_MAX_HEIGHT) {
        stropt = g_variant_get_string(data, NULL);
        for (i = 0; i < ARRAY_SIZE(maxHeights); i++) {
            if (!strcmp(stropt, maxHeights[i])) {
                devc->max_height = i;
                break;
            }
        }
        sr_dbg("%s: setting Signal Max Height to %d",
            __func__, devc->max_height);
    } else if (id == SR_CONF_PROBE_EN) {
        ch->enabled = g_variant_get_boolean(data);
        if (sdi->mode == DSO) {
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, ch, SR_CONF_PROBE_EN));
            uint16_t channel_cnt = 0;
            GSList *l;
            for (l = sdi->channels; l; l = l->next) {
                struct sr_channel *probe = (struct sr_channel *)l->data;
                channel_cnt += probe->enabled;
            }
            if (channel_cnt != 0)
                ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, 0, SR_CONF_SAMPLERATE));
        }
        if (ret == SR_OK)
            sr_dbg("%s: setting ENABLE of channel %d to %d",
                __func__, ch->index, ch->enabled);
        else
            sr_dbg("%s: setting ENABLE of channel %d to %d",
                __func__, ch->index, ch->enabled);
    } else if (id == SR_CONF_PROBE_VPOS) {
        ch->vpos = g_variant_get_double(data);
        sr_dbg("%s: setting VPOS of channel %d to %lf", __func__,
               ch->index, ch->vpos);
    } else if (id == SR_CONF_TRIGGER_SOURCE) {
        devc->trigger_source = g_variant_get_byte(data);
        if (sdi->mode == DSO) {
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, NULL, SR_CONF_TRIGGER_SOURCE));
        }
        if (ret == SR_OK)
            sr_dbg("%s: setting DSO Trigger Source to %d",
                __func__, devc->trigger_source);
        else
            sr_dbg("%s: setting DSO Trigger Source to %d failed",
                __func__, devc->trigger_source);
    } else if (id == SR_CONF_TRIGGER_CHANNEL) {
        devc->trigger_source = (g_variant_get_byte(data) << 4) + (devc->trigger_source & 0x0f);
        if (sdi->mode == DSO) {
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, NULL, SR_CONF_TRIGGER_SOURCE));
        }
        if (ret == SR_OK)
            sr_dbg("%s: setting DSO Trigger Source to %d",
                __func__, devc->trigger_source);
        else
            sr_dbg("%s: setting DSO Trigger Source to %d failed",
                __func__, devc->trigger_source);
    } else if (id == SR_CONF_STREAM) {
        devc->stream = g_variant_get_boolean(data);
    } else if (id == SR_CONF_PROBE_MAP_UNIT) {
        ch->map_unit = g_variant_get_string(data, NULL);
    } else if (id == SR_CONF_PROBE_MAP_MIN) {
        ch->map_min = g_variant_get_double(data);
    } else if (id == SR_CONF_PROBE_MAP_MAX) {
        ch->map_max = g_variant_get_double(data);
    } else {
        ret = SR_ERR_NA;
	}

	return ret;
}

static int config_list(int key, GVariant **data, const struct sr_dev_inst *sdi,
                       const struct sr_channel_group *cg)
{
    struct DSL_context *devc;
    GVariantBuilder gvb;
    unsigned int i;

    (void)cg;
    devc = sdi->priv;

    if (dsl_config_list(key, data, sdi, cg) == SR_OK) {
        return SR_OK;
    }

	switch (key) {
    case SR_CONF_DEVICE_OPTIONS:
        if (devc->profile->dev_caps.feature_caps & CAPS_FEATURE_VTH)
            *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                    hwoptions_pro, ARRAY_SIZE(hwoptions_pro)*sizeof(int32_t), TRUE, NULL, NULL);
        else
            *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                    hwoptions, ARRAY_SIZE(hwoptions)*sizeof(int32_t), TRUE, NULL, NULL);
        break;
    case SR_CONF_DEVICE_SESSIONS:
        if (devc->profile->dev_caps.feature_caps & CAPS_FEATURE_VTH)
            *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                    sessions_pro, ARRAY_SIZE(sessions_pro)*sizeof(int32_t), TRUE, NULL, NULL);
        else
            *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                    sessions, ARRAY_SIZE(sessions)*sizeof(int32_t), TRUE, NULL, NULL);
        break;
    case SR_CONF_OPERATION_MODE:
        *data = g_variant_new_strv(opmodes, opmodes_show_count);
        break;
    case SR_CONF_BUFFER_OPTIONS:
        *data = g_variant_new_strv(bufoptions, ARRAY_SIZE(bufoptions));
        break;
    case SR_CONF_CHANNEL_MODE:
        g_variant_builder_init(&gvb, G_VARIANT_TYPE("as"));
        for (i = 0; i < ARRAY_SIZE(channel_modes); i++) {
            if (channel_modes[i].stream == devc->stream &&
                devc->profile->dev_caps.channels & (1 << i)) {
                g_variant_builder_add(&gvb, "s", channel_modes[i].descr);
                if (devc->test_mode != SR_TEST_NONE)
                    break;
            }
        }
        *data = g_variant_builder_end(&gvb);
        break;
    case SR_CONF_THRESHOLD:
        *data = g_variant_new_strv(thresholds, ARRAY_SIZE(thresholds));
        break;
    case SR_CONF_FILTER:
        *data = g_variant_new_strv(filters, ARRAY_SIZE(filters));
        break;
    case SR_CONF_MAX_HEIGHT:
        *data = g_variant_new_strv(maxHeights, ARRAY_SIZE(maxHeights));
        break;
    default:
        return SR_ERR_NA;
    }

    return SR_OK;
}

static int dev_open(struct sr_dev_inst *sdi)
{
    gboolean fpga_done;
    int ret;
    struct DSL_context *devc;

    devc = sdi->priv;

    if ((ret = dsl_dev_open(di, sdi, &fpga_done)) == SR_OK) {
        // set threshold
        ret = dsl_wr_reg(sdi, VTH_ADDR, (uint8_t)(devc->vth/5.0*255));
    }

    return ret;
}

static int dev_close(struct sr_dev_inst *sdi)
{
    int ret;
    ret = dsl_dev_close(sdi);
    return ret;
}

static int cleanup(void)
{
	int ret;
	struct drv_context *drvc;

	if (!(drvc = di->priv))
        return SR_OK;

	ret = dev_clear();

	g_free(drvc);
	di->priv = NULL;

	return ret;
}

static void remove_sources(struct DSL_context *devc)
{
    int i;
    sr_info("%s: remove fds from polling", __func__);
    /* Remove fds from polling. */
    for (i = 0; devc->usbfd[i] != -1; i++)
        sr_source_remove(devc->usbfd[i]);
    g_free(devc->usbfd);
}

static int receive_data(int fd, int revents, const struct sr_dev_inst *sdi)
{
    int completed = 0;
    struct timeval tv;
    struct drv_context *drvc;
    struct DSL_context *devc;
    struct sr_usb_dev_inst *usb;
    struct ctl_rd_cmd rd_cmd;
    uint8_t hw_info;
    int ret;

    (void)fd;
    (void)revents;

    drvc = di->priv;
    devc = sdi->priv;
    usb = sdi->conn;

    tv.tv_sec = tv.tv_usec = 0;
    libusb_handle_events_timeout_completed(drvc->sr_ctx->libusb_ctx, &tv, &completed);

    // overflow check
    if (devc->stream && devc->trf_completed) {
        rd_cmd.header.dest = DSL_CTL_HW_STATUS;
        rd_cmd.header.size = 1;
        hw_info = 0;
        rd_cmd.data = &hw_info;
        if ((ret = command_ctl_rd(usb->devhdl, rd_cmd)) != SR_OK)
            sr_err("Failed to get hardware infos.");
        else
            devc->overflow = (hw_info & bmSYS_OVERFLOW) != 0;
    }

    if (devc->status == DSL_FINISH) {
        /* Remove polling */
        remove_sources(devc);
    }

    devc->trf_completed = 0;
    return TRUE;
}

static int dev_acquisition_start(struct sr_dev_inst *sdi, void *cb_data)
{
    (void)cb_data;

    struct DSL_context *devc;
    struct sr_usb_dev_inst *usb;
    struct drv_context *drvc;
    const struct libusb_pollfd **lupfd;
	unsigned int i;
    int ret;
    struct ctl_wr_cmd wr_cmd;

    if (sdi->status != SR_ST_ACTIVE)
        return SR_ERR_DEV_CLOSED;

    drvc = di->priv;
    devc = sdi->priv;
    usb = sdi->conn;

    //devc->cb_data = cb_data;
    devc->cb_data = sdi;
    devc->num_samples = 0;
    devc->num_bytes = 0;
    devc->empty_transfer_count = 0;
    devc->status = DSL_INIT;
    devc->num_transfers = 0;
    devc->submitted_transfers = 0;
    if (sdi->mode != LOGIC)
        devc->actual_samples = (devc->limit_samples + 1023) & ~1023;
    else
        devc->actual_samples = devc->limit_samples;
    devc->actual_bytes = devc->actual_samples / DSLOGIC_ATOMIC_SAMPLES * dsl_en_ch_num(sdi) * DSLOGIC_ATOMIC_SIZE;
	devc->abort = FALSE;
    devc->mstatus_valid = FALSE;
    devc->overflow = FALSE;

	/* Configures devc->trigger_* and devc->sample_wide */
    if (dsl_configure_probes(sdi) != SR_OK) {
        sr_err("%s: Failed to configure probes.", __func__);
        return SR_ERR;
	}

    /* Stop Previous GPIF acquisition */
    wr_cmd.header.dest = DSL_CTL_STOP;
    wr_cmd.header.size = 0;
    if ((ret = command_ctl_wr(usb->devhdl, wr_cmd)) != SR_OK) {
        sr_err("%s: Stop DSLogic acquisition failed!", __func__);
        return ret;
    } else {
        sr_info("%s: Stop Previous DSLogic acquisition!", __func__);
    }

    /* Setting FPGA before acquisition start*/
    if ((ret = dsl_fpga_arm(sdi)) != SR_OK) {
        sr_err("%s: Arm FPGA failed!", __func__);
        return ret;
    }

    /*
     * settings must be updated before acquisition
     */
    if (sdi->mode == DSO) {
        devc->trigger_hpos =  devc->trigger_hrate * dsl_en_ch_num(sdi) * devc->limit_samples / 200.0;
        if ((ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, NULL, SR_CONF_HORIZ_TRIGGERPOS))) == SR_OK)
            sr_dbg("%s: setting DSO Horiz Trigger Position to %d",
                __func__, devc->trigger_hpos);
        else
            sr_dbg("%s: setting DSO Horiz Trigger Position to %d failed",
                __func__, devc->trigger_hpos);
    }

    /* setup and submit usb transfer */
    if ((ret = dsl_start_transfers(devc->cb_data)) != SR_OK) {
        sr_err("%s: Could not submit usb transfer"
               "(%d)%d", __func__, ret, errno);
        return ret;
    }

    /* setup callback function for data transfer */
    lupfd = libusb_get_pollfds(drvc->sr_ctx->libusb_ctx);
    for (i = 0; lupfd[i]; i++);
    if (!(devc->usbfd = g_try_malloc(sizeof(struct libusb_pollfd) * (i + 1))))
    	return SR_ERR;
    for (i = 0; lupfd[i]; i++) {
        sr_source_add(lupfd[i]->fd, lupfd[i]->events,
                  dsl_get_timeout(devc), receive_data, sdi);
        devc->usbfd[i] = lupfd[i]->fd;
    }
    devc->usbfd[i] = -1;
    free(lupfd);

    wr_cmd.header.dest = DSL_CTL_START;
    wr_cmd.header.size = 0;
    if ((ret = command_ctl_wr(usb->devhdl, wr_cmd)) != SR_OK) {
        devc->status = DSL_ERROR;
        devc->abort = TRUE;
        return ret;
    }
    devc->status = DSL_START;

	/* Send header packet to the session bus. */
    //std_session_send_df_header(cb_data, LOG_PREFIX);
    std_session_send_df_header(sdi, LOG_PREFIX);

    return SR_OK;
}

static int dev_acquisition_stop(const struct sr_dev_inst *sdi, void *cb_data)
{
    int ret = dsl_dev_acquisition_stop(sdi, cb_data);
    return ret;
}

static int dev_status_get(const struct sr_dev_inst *sdi, struct sr_status *status, gboolean prg, int begin, int end)
{
    int ret = dsl_dev_status_get(sdi, status, prg, begin, end);
    return ret;
}

SR_PRIV struct sr_dev_driver DSLogic_driver_info = {
    .name = "DSLogic",
    .longname = "DSLogic (generic driver for DSLogic LA)",
	.api_version = 1,
	.init = init,
	.cleanup = cleanup,
	.scan = scan,
	.dev_list = dev_list,
    .dev_mode_list = dev_mode_list,
	.dev_clear = dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
    .dev_status_get = dev_status_get,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.priv = NULL,
};
