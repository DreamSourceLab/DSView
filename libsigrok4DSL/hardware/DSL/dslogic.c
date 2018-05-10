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


static struct sr_dev_mode mode_list[] = {
    {"LA", LOGIC},
    {"DAQ", ANALOG},
    {"OSC", DSO},
};

static struct sr_dev_mode pro_mode_list[] = {
    {"LA", LOGIC},
};

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

static const char *stream_ch_modes[] = {
    "Use 16 Channels (Max 20MHz)",
    "Use 12 Channels (Max 25MHz)",
    "Use 6 Channels (Max 50MHz)",
    "Use 3 Channels (Max 100MHz)",
};

static const uint16_t stream_ch_num[] = {
    16,
    12,
    6,
    3,
};

static const char *buffer_ch_modes[] = {
    "Use Channels 0~15 (Max 100MHz)",
    "Use Channels 0~7 (Max 200MHz)",
    "Use Channels 0~3 (Max 400MHz)",
};

static const uint16_t buffer_ch_num[] = {
    MAX_LOGIC_PROBES,
    MAX_LOGIC_PROBES / 2,
    MAX_LOGIC_PROBES / 4,
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

static const int32_t hwopts[] = {
    SR_CONF_CONN,
};

static const int32_t hwcaps[] = {
    SR_CONF_LOGIC_ANALYZER,
    SR_CONF_TRIGGER_TYPE,
    SR_CONF_SAMPLERATE,

	/* These are really implemented in the driver, not the hardware. */
    SR_CONF_LIMIT_SAMPLES,
    SR_CONF_CONTINUOUS,
};

static const int32_t hwoptions[] = {
    SR_CONF_OPERATION_MODE,
    SR_CONF_BUFFER_OPTIONS,
    SR_CONF_THRESHOLD,
    SR_CONF_FILTER,
    SR_CONF_MAX_HEIGHT,
    SR_CONF_CLOCK_TYPE,
    SR_CONF_CLOCK_EDGE,
};

static const int32_t hwoptions_pro[] = {
    SR_CONF_OPERATION_MODE,
    SR_CONF_BUFFER_OPTIONS,
    SR_CONF_VTH,
    SR_CONF_FILTER,
    SR_CONF_MAX_HEIGHT,
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

static const char *probe_names[] = {
	"0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",
	"8",  "9", "10", "11", "12", "13", "14", "15",
	NULL,
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
    SR_MHZ(25),
    SR_MHZ(50),
    SR_MHZ(100),
    SR_MHZ(200),
    SR_MHZ(400),
};

//static const uint64_t samplecounts[] = {
//    SR_KB(1),
//    SR_KB(2),
//    SR_KB(5),
//    SR_KB(10),
//    SR_KB(20),
//    SR_KB(50),
//    SR_KB(100),
//    SR_KB(200),
//    SR_KB(500),
//    SR_MB(1),
//    SR_MB(2),
//    SR_MB(5),
//    SR_MB(10),
//    SR_MB(16),
//};

static const uint64_t samplecounts[] = {
    SR_KB(1),
    SR_KB(2),
    SR_KB(4),
    SR_KB(8),
    SR_KB(16),
    SR_KB(32),
    SR_KB(64),
    SR_KB(128),
    SR_KB(256),
    SR_KB(512),
    SR_MB(1),
    SR_MB(2),
    SR_MB(4),
    SR_MB(8),
    SR_MB(16),
    SR_MB(32),
    SR_MB(64),
    SR_MB(128),
    SR_MB(256),
    SR_MB(512),
    SR_GB(1),
    SR_GB(2),
    SR_GB(4),
    SR_GB(8),
    SR_GB(16),
};

static uint16_t opmodes_show_count = 3;

SR_PRIV struct sr_dev_driver DSLogic_driver_info;
static struct sr_dev_driver *di = &DSLogic_driver_info;

static int counts_size(const struct sr_dev_inst *sdi)
{
    struct DSL_context *devc = sdi->priv;
    if (strcmp(sdi->model, "DSLogic Basic") == 0)
        if (sdi->mode == ANALOG)
            return 5;
        else if (!devc || devc->stream)
            return ARRAY_SIZE(samplecounts);
        else
            return 15;
    else
        if (sdi->mode == ANALOG)
            return 15;
        else
            return ARRAY_SIZE(samplecounts);
}

static void probe_init(struct sr_dev_inst *sdi)
{
    int i;
    GSList *l;
    for (l = sdi->channels; l; l = l->next) {
        struct sr_channel *probe = (struct sr_channel *)l->data;
        if (sdi->mode == DSO) {
            probe->vdiv = 1000;
            probe->vfactor = 1;
            probe->vpos = 0;
            probe->coupling = SR_DC_COUPLING;
            probe->trig_value = 0x80;
            probe->ms_show = TRUE;
            for (i = DSO_MS_BEGIN; i < DSO_MS_END; i++)
                probe->ms_en[i] = default_ms_en[i];
        }
    }
}

static int setup_probes(struct sr_dev_inst *sdi, int num_probes)
{
    uint16_t j;
    struct sr_channel *probe;

    for (j = 0; j < num_probes; j++) {
        if (!(probe = sr_channel_new(j, (sdi->mode == LOGIC) ? SR_CHANNEL_LOGIC : ((sdi->mode == DSO) ? SR_CHANNEL_DSO : SR_CHANNEL_ANALOG),
                                   TRUE, probe_names[j])))
            return SR_ERR;
        sdi->channels = g_slist_append(sdi->channels, probe);
    }
    probe_init(sdi);
    return SR_OK;
}

static int adjust_probes(struct sr_dev_inst *sdi, int num_probes)
{
    uint16_t j;
    struct sr_channel *probe;

    assert(num_probes > 0);

    j = g_slist_length(sdi->channels);
    while(j < num_probes) {
        if (!(probe = sr_channel_new(j, (sdi->mode == LOGIC) ? SR_CHANNEL_LOGIC : ((sdi->mode == DSO) ? SR_CHANNEL_DSO : SR_CHANNEL_ANALOG),
                                   TRUE, probe_names[j])))
            return SR_ERR;
        sdi->channels = g_slist_append(sdi->channels, probe);
        j++;
    }

    while(j > num_probes) {
        sdi->channels = g_slist_delete_link(sdi->channels, g_slist_last(sdi->channels));
        j--;
    }

    return SR_OK;
}

static struct DSL_context *DSLogic_dev_new(const struct sr_dev_inst *sdi)
{
    struct DSL_context *devc;

    if (!(devc = g_try_malloc(sizeof(struct DSL_context)))) {
        sr_err("Device context malloc failed.");
		return NULL;
	}

    devc->channel = NULL;
	devc->profile = NULL;
	devc->fw_updated = 0;
    devc->cur_samplerate = DEFAULT_SAMPLERATE;
    devc->limit_samples = DEFAULT_SAMPLELIMIT;
    devc->sample_wide = TRUE;
    devc->clock_type = FALSE;
    devc->clock_edge = FALSE;
    devc->rle_mode = FALSE;
    devc->instant = FALSE;
    devc->op_mode = OP_STREAM;
    devc->test_mode = SR_TEST_NONE;
    devc->stream = (devc->op_mode == OP_STREAM);
    devc->buf_options = SR_BUF_UPLOAD;
    devc->ch_mode = 0;
    devc->samplerates_size = 11;
    devc->samplecounts_size = counts_size(sdi);
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
    devc->unit_bits = 8;
    devc->trigger_margin = 8;
    devc->trigger_channel = 0;

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
	int devcnt, num_logic_probes, ret, i, j;
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
        sdi = sr_dev_inst_new(LOGIC, devcnt, SR_ST_INITIALIZING,
			prof->vendor, prof->model, prof->model_version);
		if (!sdi)
			return NULL;
		sdi->driver = di;

		/* Fill in probelist according to this device's profile. */
		num_logic_probes = prof->dev_caps & DEV_CAPS_16BIT ? 16 : 8;
        if (setup_probes(sdi, num_logic_probes) != SR_OK)
            return NULL;

        devc = DSLogic_dev_new(sdi);
		devc->profile = prof;
		sdi->priv = devc;
		drvc->instances = g_slist_append(drvc->instances, sdi);
        //devices = g_slist_append(devices, sdi);

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

static GSList *dev_mode_list(const struct sr_dev_inst *sdi)
{
    GSList *l = NULL;
    unsigned int i;

    if (strcmp(sdi->model, "DSLogic") == 0) {
        for(i = 0; i < ARRAY_SIZE(mode_list); i++) {
            l = g_slist_append(l, &mode_list[i]);
        }
    } else {
        for(i = 0; i < ARRAY_SIZE(pro_mode_list); i++) {
            l = g_slist_append(l, &pro_mode_list[i]);
        }
    }
    return l;
}

static uint64_t dso_cmd_gen(const struct sr_dev_inst *sdi, struct sr_channel* ch, int id)
{
    struct DSL_context *devc;
    uint64_t cmd = 0;
    int channel_cnt = 0;
    GSList *l;
    struct sr_channel *en_probe;

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
        if(channel_cnt == 1)
            cmd += 0xC00000;
        else if(ch->index == 0)
            cmd += 0x400000;
        else if(ch->index == 1)
            cmd += 0x800000;
        else
            cmd += 0x000000;
//        if(ch->index == 0)
//            cmd += 0x400000;
//        else if(ch->index == 1)
//            cmd += 0x800000;
//        else
//            cmd += 0x000000;

        // --Header
        cmd += 0x55000000;
        break;
    case SR_CONF_SAMPLERATE:
        for (l = sdi->channels; l; l = l->next) {
            struct sr_channel *probe = (struct sr_channel *)l->data;
            channel_cnt += probe->enabled;
        }
        cmd += 0x18;
        uint32_t divider = (uint32_t)ceil(DSLOGIC_MAX_DSO_SAMPLERATE * 1.0  / devc->cur_samplerate / channel_cnt);
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
    struct DSL_context *devc;
    int ret;

    ret = dsl_config_get(id, data, sdi, ch, cg);
    if (ret != SR_OK) {
        switch (id) {
        case SR_CONF_OPERATION_MODE:
            if (!sdi)
                return SR_ERR;
            devc = sdi->priv;
            *data = g_variant_new_string(opmodes[devc->op_mode]);
            break;
        case SR_CONF_FILTER:
            if (!sdi)
                return SR_ERR;
            devc = sdi->priv;
            *data = g_variant_new_string(filters[devc->filter]);
            break;
        case SR_CONF_RLE:
            if (!sdi)
                return SR_ERR;
            devc = sdi->priv;
            *data = g_variant_new_boolean(devc->rle_mode);
            break;
		case SR_CONF_TEST:
            if (!sdi)
                return SR_ERR;
            devc = sdi->priv;
            *data = g_variant_new_boolean(devc->test_mode != SR_TEST_NONE);
            break;
        case SR_CONF_ACTUAL_SAMPLES:
            if (!sdi)
                return SR_ERR;
            devc = sdi->priv;
            *data = g_variant_new_uint64(devc->actual_samples);
            break;
        case SR_CONF_WAIT_UPLOAD:
            if (!sdi)
                return SR_ERR;
            devc = sdi->priv;
            if (devc->buf_options == SR_BUF_UPLOAD &&
                devc->status == DSL_START) {
                devc->status = DSL_ABORT;
                dsl_wr_reg(sdi, EEWP_ADDR, bmFORCE_STOP);
                *data = g_variant_new_boolean(TRUE);
            } else {
                *data = g_variant_new_boolean(FALSE);
            }
            break;
        case SR_CONF_BUFFER_OPTIONS:
            if (!sdi)
                return SR_ERR;
            devc = sdi->priv;
            *data = g_variant_new_string(bufoptions[devc->buf_options]);
            break;
        case SR_CONF_CHANNEL_MODE:
            if (!sdi)
                return SR_ERR;
            devc = sdi->priv;
            if (devc->stream)
                *data = g_variant_new_string(stream_ch_modes[devc->ch_mode]);
            else
                *data = g_variant_new_string(buffer_ch_modes[devc->ch_mode]);
            break;
        case SR_CONF_MAX_HEIGHT:
            if (!sdi)
                return SR_ERR;
            devc = sdi->priv;
            *data = g_variant_new_string(maxHeights[devc->max_height]);
            break;
        case SR_CONF_MAX_HEIGHT_VALUE:
            if (!sdi)
                return SR_ERR;
            devc = sdi->priv;
            *data = g_variant_new_byte(devc->max_height);
            break;
        case SR_CONF_THRESHOLD:
            if (!sdi)
                return SR_ERR;
            devc = sdi->priv;
            *data = g_variant_new_string(thresholds[devc->th_level]);
            break;
        case SR_CONF_VTH:
            if (!sdi)
                return SR_ERR;
            devc = sdi->priv;
            *data = g_variant_new_double(devc->vth);
            break;
        case SR_CONF_ZERO:
            if (!sdi)
                return SR_ERR;
            devc = sdi->priv;
            if (sdi->mode == DSO)
                *data = g_variant_new_boolean(devc->zero);
            else
                *data = g_variant_new_boolean(FALSE);
            break;
        case SR_CONF_STREAM:
            if (!sdi)
                return SR_ERR;
            devc = sdi->priv;
            *data = g_variant_new_boolean(devc->stream);
            break;
        case SR_CONF_MAX_DSO_SAMPLERATE:
            if (!sdi)
                return SR_ERR;
            *data = g_variant_new_uint64(DSLOGIC_MAX_DSO_SAMPLERATE);
            break;
        case SR_CONF_MAX_DSO_SAMPLELIMITS:
            if (!sdi)
                return SR_ERR;
            *data = g_variant_new_uint64(DSLOGIC_MAX_DSO_DEPTH);
            break;
        case SR_CONF_HW_DEPTH:
            if (!sdi)
                return SR_ERR;
            devc = sdi->priv;
            *data = g_variant_new_uint64(dsl_channel_depth(sdi));
            break;
        case SR_CONF_VLD_CH_NUM:
            if (!sdi)
                return SR_ERR;
            devc = sdi->priv;
            if (devc->stream)
                *data = g_variant_new_int16(stream_ch_num[devc->ch_mode]);
            else
                *data = g_variant_new_int16(buffer_ch_num[devc->ch_mode]);
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
    int ret, num_probes;
    struct sr_usb_dev_inst *usb;
    unsigned int i;
    struct drv_context *drvc;

    (void)cg;

    if (sdi->status != SR_ST_ACTIVE)
        return SR_ERR;

    drvc = di->priv;
	devc = sdi->priv;
    usb = sdi->conn;

    ret = SR_OK;

    if (id == SR_CONF_CLOCK_TYPE) {
        devc->clock_type = g_variant_get_boolean(data);
    } else if (id == SR_CONF_CLOCK_EDGE) {
        devc->clock_edge = g_variant_get_boolean(data);
    } else if (id == SR_CONF_LIMIT_SAMPLES) {
        devc->limit_samples = g_variant_get_uint64(data);
    } else if (id == SR_CONF_DATALOCK) {
        while(libusb_try_lock_events(drvc->sr_ctx->libusb_ctx));
        devc->data_lock = g_variant_get_boolean(data);
        libusb_unlock_events(drvc->sr_ctx->libusb_ctx);
    } else if (id == SR_CONF_PROBE_VDIV) {
        ch->vdiv = g_variant_get_uint64(data);
        if (sdi->mode == DSO) {
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
        if (sdi->mode == DSO) {
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
        if (devc->test_mode != SR_TEST_NONE) {
            devc->cur_samplerate = g_variant_get_uint64(data);
            if(sdi->mode == DSO) {
                devc->sample_wide = (devc->cur_samplerate <= DSLOGIC_MAX_DSO_SAMPLERATE);
                ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, 0, SR_CONF_SAMPLERATE));
            } else {
                devc->sample_wide = (devc->cur_samplerate <= DSLOGIC_MAX_LOGIC_SAMPLERATE);
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
                    devc->limit_samples = DSLOGIC_INSTANT_DEPTH / dsl_en_ch_num(sdi);
                else
                    devc->limit_samples = DSLOGIC_MAX_DSO_DEPTH / dsl_en_ch_num(sdi);
            }
        }
    } else if (id == SR_CONF_DEVICE_MODE) {
        sdi->mode = g_variant_get_int16(data);
        if (sdi->mode == LOGIC) {
            dsl_wr_reg(sdi, EEWP_ADDR, bmSCOPE_CLR);
            num_probes = devc->profile->dev_caps & DEV_CAPS_16BIT ? 16 : 8;
        } else if (sdi->mode == DSO) {
            dsl_wr_reg(sdi, EEWP_ADDR, bmSCOPE_SET);
            num_probes = devc->profile->dev_caps & DEV_CAPS_16BIT ? MAX_DSO_PROBES_NUM : 1;
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, NULL, SR_CONF_DSO_SYNC));
            if (ret != SR_OK)
                sr_dbg("%s: DSO configuration sync failed", __func__);
            devc->cur_samplerate = DSLOGIC_MAX_DSO_SAMPLERATE / num_probes;
            devc->limit_samples = DSLOGIC_MAX_DSO_DEPTH / num_probes;
            devc->samplerates_size = 15;
        } else {
            dsl_wr_reg(sdi, EEWP_ADDR, bmSCOPE_CLR);
            num_probes = devc->profile->dev_caps & DEV_CAPS_16BIT ? MAX_ANALOG_PROBES_NUM : 1;
            devc->op_mode = OP_STREAM;
            devc->test_mode = SR_TEST_NONE;
            devc->stream = TRUE;
            devc->samplerates_size = 10;
        }
        devc->samplecounts_size = counts_size(sdi);
        sr_dev_probes_free(sdi);
        setup_probes(sdi, num_probes);
        sr_dbg("%s: setting mode to %d", __func__, sdi->mode);
        if (sdi->mode == DSO) {
            dso_init(sdi);
        }
    } else if (id == SR_CONF_OPERATION_MODE) {
        stropt = g_variant_get_string(data, NULL);
        if (sdi->mode == LOGIC) {
            if (!strcmp(stropt, opmodes[OP_BUFFER]) && (devc->op_mode != OP_BUFFER)) {
                devc->op_mode = OP_BUFFER;
                devc->test_mode = SR_TEST_NONE;
                devc->stream = FALSE;
                devc->ch_mode = 0;
                devc->samplerates_size = 14;
                adjust_probes(sdi, MAX_LOGIC_PROBES);
            } else if (!strcmp(stropt, opmodes[OP_STREAM]) && (devc->op_mode != OP_STREAM)) {
                devc->op_mode = OP_STREAM;
                devc->test_mode = SR_TEST_NONE;
                devc->stream = TRUE;
                devc->ch_mode = 0;
                devc->samplerates_size = 11;
                adjust_probes(sdi, MAX_LOGIC_PROBES);
            } else if (!strcmp(stropt, opmodes[OP_INTEST]) && (devc->op_mode != OP_INTEST)) {
                devc->op_mode = OP_INTEST;
                devc->test_mode = SR_TEST_INTERNAL;
                if (strcmp(sdi->model, "DSLogic Basic") == 0) {
                    devc->stream = TRUE;
                    devc->samplerates_size = 10;
                } else {
                    devc->stream = FALSE;
                    devc->samplerates_size = 14;
                }
                devc->ch_mode = 0;
                adjust_probes(sdi, MAX_LOGIC_PROBES);
                devc->limit_samples = DSLOGIC_MAX_LOGIC_DEPTH;
                devc->cur_samplerate = DSLOGIC_MAX_LOGIC_SAMPLERATE;
                devc->sample_wide = TRUE;
            } else if (!strcmp(stropt, opmodes[OP_EXTEST]) && (devc->op_mode != OP_EXTEST)) {
                devc->op_mode = OP_EXTEST;
                devc->test_mode = SR_TEST_EXTERNAL;
                if (strcmp(sdi->model, "DSLogic Basic") == 0) {
                    devc->stream = TRUE;
                    devc->samplerates_size = 11;
                } else {
                    devc->stream = FALSE;
                    devc->samplerates_size = 14;
                }
                devc->ch_mode = 0;
                adjust_probes(sdi, MAX_LOGIC_PROBES);
                devc->limit_samples = DSLOGIC_MAX_LOGIC_DEPTH;
                devc->cur_samplerate = DSLOGIC_MAX_LOGIC_SAMPLERATE;
                devc->sample_wide = TRUE;
            } else if (!strcmp(stropt, opmodes[OP_LPTEST]) && (devc->op_mode != OP_LPTEST)) {
                devc->op_mode = OP_LPTEST;
                devc->test_mode = SR_TEST_LOOPBACK;
                devc->stream = FALSE;
                devc->ch_mode = 0;
                devc->samplerates_size = 14;
                adjust_probes(sdi, MAX_LOGIC_PROBES);
                devc->limit_samples = DSLOGIC_MAX_LOGIC_DEPTH;
                devc->cur_samplerate = DSLOGIC_MAX_LOGIC_SAMPLERATE;
                devc->sample_wide = TRUE;
            } else {
                ret = SR_ERR;
            }
            if (devc->cur_samplerate > samplerates[devc->samplerates_size-1]) {
                devc->cur_samplerate = samplerates[devc->samplerates_size-1];
                devc->sample_wide = (devc->cur_samplerate <= DSLOGIC_MAX_DSO_SAMPLERATE);
            }
        } else if (sdi->mode == ANALOG) {
            devc->op_mode = OP_STREAM;
            devc->test_mode = SR_TEST_NONE;
            devc->stream = TRUE;
            devc->samplerates_size = 10;
        }
        devc->samplecounts_size = counts_size(sdi);
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
            if (devc->stream) {
                for (i = 0; i < ARRAY_SIZE(stream_ch_modes); i++)
                    if (!strcmp(stropt, stream_ch_modes[i])) {
                        devc->ch_mode = i;
                        devc->samplerates_size = 11 + i;
                        adjust_probes(sdi, MAX_LOGIC_PROBES);
                        break;
                    }
            } else {
                for (i = 0; i < ARRAY_SIZE(buffer_ch_modes); i++)
                    if (!strcmp(stropt, buffer_ch_modes[i])) {
                        devc->ch_mode = i;
                        devc->samplerates_size = 14 + i;
                        adjust_probes(sdi, buffer_ch_num[i]);
                        break;
                    }
            }
            if (devc->cur_samplerate > samplerates[devc->samplerates_size-1]) {
                devc->cur_samplerate = samplerates[devc->samplerates_size-1];
                devc->sample_wide = (devc->cur_samplerate <= DSLOGIC_MAX_DSO_SAMPLERATE);
            }
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
    } else if (id == SR_CONF_STREAM) {
        devc->stream = g_variant_get_boolean(data);
    } else {
        ret = SR_ERR_NA;
	}

	return ret;
}

static int config_list(int key, GVariant **data, const struct sr_dev_inst *sdi,
                       const struct sr_channel_group *cg)
{
    struct DSL_context *devc;
	GVariant *gvar;
	GVariantBuilder gvb;

    //(void)sdi;
    (void)cg;
    devc = sdi->priv;

	switch (key) {
    case SR_CONF_SCAN_OPTIONS:
//		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_INT32,
//				hwopts, ARRAY_SIZE(hwopts), sizeof(int32_t));
        *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                hwopts, ARRAY_SIZE(hwopts)*sizeof(int32_t), TRUE, NULL, NULL);
		break;
    case SR_CONF_DEVICE_OPTIONS:
//		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_INT32,
//				hwcaps, ARRAY_SIZE(hwcaps), sizeof(int32_t));
        *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                hwcaps, ARRAY_SIZE(hwcaps)*sizeof(int32_t), TRUE, NULL, NULL);
		break;
    case SR_CONF_DEVICE_CONFIGS:
//		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_INT32,
//				hwcaps, ARRAY_SIZE(hwcaps), sizeof(int32_t));
        if (strcmp(sdi->model, "DSLogic") == 0)
            *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                    hwoptions, ARRAY_SIZE(hwoptions)*sizeof(int32_t), TRUE, NULL, NULL);
        else
            *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                    hwoptions_pro, ARRAY_SIZE(hwoptions_pro)*sizeof(int32_t), TRUE, NULL, NULL);
        break;
    case SR_CONF_DEVICE_SESSIONS:
        if (strcmp(sdi->model, "DSLogic") == 0)
            *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                    sessions, ARRAY_SIZE(sessions)*sizeof(int32_t), TRUE, NULL, NULL);
        else
            *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                    sessions_pro, ARRAY_SIZE(sessions_pro)*sizeof(int32_t), TRUE, NULL, NULL);
        break;
    case SR_CONF_SAMPLERATE:
		g_variant_builder_init(&gvb, G_VARIANT_TYPE("a{sv}"));
//		gvar = g_variant_new_fixed_array(G_VARIANT_TYPE("t"), samplerates,
//				ARRAY_SIZE(samplerates), sizeof(uint64_t));
        gvar = g_variant_new_from_data(G_VARIANT_TYPE("at"),
                samplerates, devc->samplerates_size*sizeof(uint64_t), TRUE, NULL, NULL);
        g_variant_builder_add(&gvb, "{sv}", "samplerates", gvar);
		*data = g_variant_builder_end(&gvb);
		break;
    case SR_CONF_LIMIT_SAMPLES:
        g_variant_builder_init(&gvb, G_VARIANT_TYPE("a{sv}"));
        gvar = g_variant_new_from_data(G_VARIANT_TYPE("at"),
                samplecounts, devc->samplecounts_size*sizeof(uint64_t), TRUE, NULL, NULL);
        g_variant_builder_add(&gvb, "{sv}", "samplecounts", gvar);
        *data = g_variant_builder_end(&gvb);
        break;
    case SR_CONF_TRIGGER_TYPE:
        *data = g_variant_new_string(TRIGGER_TYPE);
		break;
    case SR_CONF_OPERATION_MODE:
        *data = g_variant_new_strv(opmodes, opmodes_show_count);
        break;
    case SR_CONF_BUFFER_OPTIONS:
        *data = g_variant_new_strv(bufoptions, ARRAY_SIZE(bufoptions));
        break;
    case SR_CONF_CHANNEL_MODE:
        if (devc->stream)
            *data = g_variant_new_strv(stream_ch_modes, ARRAY_SIZE(stream_ch_modes));
        else if (devc->test_mode != SR_TEST_NONE)
            *data = g_variant_new_strv(buffer_ch_modes, 1);
        else
            *data = g_variant_new_strv(buffer_ch_modes, ARRAY_SIZE(buffer_ch_modes));
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
    int ret = dsl_dev_close(sdi);
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
    sr_err("%s: remove fds from polling", __func__);
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
