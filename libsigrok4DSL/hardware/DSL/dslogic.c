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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <assert.h>

#include <sys/stat.h>
#include <inttypes.h>
//#include <libusb.h>
#include "dsl.h"
#include "command.h"

#undef min
#define min(a,b) ((a)<(b)?(a):(b))

static const int single_buffer_time = 20;
static const int total_buffer_time = 200;
static const int buffer_size = 1024 * 1024;
static const int instant_buffer_size = 1024 * 1024;
static const int cons_buffer_size = 128;
static const int buffer_cnt = 4;

static struct sr_dev_mode mode_list[] = {
    {"LA", LOGIC},
    {"DAQ", ANALOG},
    {"OSC", DSO},
};

static struct sr_dev_mode pro_mode_list[] = {
    {"LA", LOGIC},
};

static const char *opmodes[] = {
    "Buffer Mode",
    "Stream Mode",
    "Internal Test",
    "External Test",
    "DRAM Loopback Test",
};

static uint16_t opmodes_show_count = 3;

static const char *stream_ch_modes[] = {
    "Use Channels 0~15 (Max 10MHz)",
    "Use Channels 0~7 (Max 25MHz)",
};

static const uint16_t stream_ch_num[] = {
    16,
    8,
};

static const char *buffer_ch_modes[] = {
    "Use Channels 0~15 (Max 100MHz)",
    "Use Channels 0~7 (Max 200MHz)",
    "Use Channels 0~3 (Max 400MHz)",
};

static const uint16_t buffer_ch_num[] = {
    16,
    8,
    4,
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
    SR_CONF_THRESHOLD,
    SR_CONF_FILTER,
    SR_CONF_MAX_HEIGHT,
    SR_CONF_CLOCK_TYPE,
    SR_CONF_CLOCK_EDGE,
};

static const int32_t hwoptions_pro[] = {
    SR_CONF_OPERATION_MODE,
    SR_CONF_VTH,
    SR_CONF_FILTER,
    SR_CONF_MAX_HEIGHT,
    SR_CONF_CLOCK_TYPE,
    SR_CONF_CLOCK_EDGE,
};

static const int32_t sessions[] = {
    SR_CONF_OPERATION_MODE,
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
    SR_CONF_OPERATION_MODE,
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

static const int32_t ch_sessions[] = {
    SR_CONF_VDIV
};

static const char *probe_names[] = {
	"0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",
	"8",  "9", "10", "11", "12", "13", "14", "15",
	NULL,
};

static uint16_t test_sample_value;
static uint16_t test_init = 1;

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
};

static const uint8_t zero_base_addr = 0x80;

SR_PRIV struct sr_dev_driver DSLogic_driver_info;
static struct sr_dev_driver *di = &DSLogic_driver_info;

extern struct ds_trigger *trigger;

struct sr_status mstatus;

/**
 * Check the USB configuration to determine if this is an DSLogic device.
 *
 * @return TRUE if the device's configuration profile match DSLogic
 *         configuration, FALSE otherwise.
 */
static gboolean check_conf_profile(libusb_device *dev)
{
	struct libusb_device_descriptor des;
    struct libusb_device_handle *hdl;
	gboolean ret;
	unsigned char strdesc[64];

	hdl = NULL;
	ret = FALSE;
	while (!ret) {
		/* Assume the FW has not been loaded, unless proven wrong. */
		if (libusb_get_device_descriptor(dev, &des) != 0)
			break;

		if (libusb_open(dev, &hdl) != 0)
			break;

		if (libusb_get_string_descriptor_ascii(hdl,
		    des.iManufacturer, strdesc, sizeof(strdesc)) < 0)
			break;
        if (strncmp((const char *)strdesc, "DreamSourceLab", 14))
			break;

		if (libusb_get_string_descriptor_ascii(hdl,
				des.iProduct, strdesc, sizeof(strdesc)) < 0)
			break;
        if (strncmp((const char *)strdesc, "USB-based Instrument", 20))
			break;

        /* If we made it here, it must be an DSLogic. */
		ret = TRUE;
	}
	if (hdl)
		libusb_close(hdl);

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

static int fpga_setting(const struct sr_dev_inst *sdi)
{
    struct DSL_context *devc;
    struct sr_usb_dev_inst *usb;
    struct libusb_device_handle *hdl;
    struct DSL_setting setting;
    int ret;
    int transferred;
    int result;
    int i;
    int channel_en_cnt = 0;
    int channel_cnt = 0;
    GSList *l;

    devc = sdi->priv;
    usb = sdi->conn;
    hdl = usb->devhdl;

    setting.sync = 0xf5a5f5a5;
    setting.mode_header = 0x0001;
    setting.divider_header = 0x0102ffff;
    setting.count_header = 0x0302ffff;
    setting.trig_pos_header = 0x0502ffff;
    setting.trig_glb_header = 0x0701;
    setting.trig_adp_header = 0x0a02ffff;
    setting.trig_sda_header = 0x0c02ffff;
    setting.trig_mask0_header = 0x1010ffff;
    setting.trig_mask1_header = 0x1110ffff;
    //setting.trig_mask2_header = 0x1210ffff;
    //setting.trig_mask3_header = 0x1310ffff;
    setting.trig_value0_header = 0x1410ffff;
    setting.trig_value1_header = 0x1510ffff;
    //setting.trig_value2_header = 0x1610ffff;
    //setting.trig_value3_header = 0x1710ffff;
    setting.trig_edge0_header = 0x1810ffff;
    setting.trig_edge1_header = 0x1910ffff;
    //setting.trig_edge2_header = 0x1a10ffff;
    //setting.trig_edge3_header = 0x1b10ffff;
    setting.trig_count0_header = 0x1c20ffff;
    setting.trig_count1_header = 0x1d20ffff;
    //setting.trig_count2_header = 0x1e10ffff;
    //setting.trig_count3_header = 0x1f10ffff;
    setting.trig_logic0_header = 0x2010ffff;
    setting.trig_logic1_header = 0x2110ffff;
    //setting.trig_logic2_header = 0x2210ffff;
    //setting.trig_logic3_header = 0x2310ffff;
    setting.end_sync = 0xfa5afa5a;

    devc->rle_mode = FALSE;
    if (sdi->mode == LOGIC &&
        !devc->stream &&
        devc->limit_samples > DSLOGIC_MAX_LOGIC_DEPTH*ceil(devc->cur_samplerate * 1.0 / DSLOGIC_MAX_LOGIC_SAMPLERATE))
        devc->rle_mode = TRUE;

    //setting.mode = (test_mode ? 0x8000 : 0x0000) + trigger->trigger_en + (sdi->mode << 4);
    setting.mode = ((devc->op_mode == SR_OP_INTERNAL_TEST) << 15) +
                   ((devc->op_mode == SR_OP_EXTERNAL_TEST) << 14) +
                   ((devc->op_mode == SR_OP_LOOPBACK_TEST) << 13) +
                   ((devc->stream) << 12) +
                   ((trigger->trigger_mode == SERIAL_TRIGGER) << 11) +
                   trigger->trigger_en +
                   ((sdi->mode == DSO) << 4) + (devc->clock_type << 1) + (devc->clock_edge << 2) +
                   (devc->rle_mode << 3) +
                   ((((devc->cur_samplerate == (2 * DSLOGIC_MAX_LOGIC_SAMPLERATE)) && sdi->mode != DSO) || (sdi->mode == ANALOG)) << 5) +
                   ((devc->cur_samplerate == (4 * DSLOGIC_MAX_LOGIC_SAMPLERATE)) << 6) +
                   ((sdi->mode == ANALOG) << 7) +
                   ((devc->filter == SR_FILTER_1T) << 8) +
                   (devc->instant << 9);

    setting.divider  = (sdi->mode == DSO) ?    (uint32_t)ceil(DSLOGIC_MAX_DSO_SAMPLERATE * 1.0 / devc->cur_samplerate / en_ch_num(sdi)) :
                                               (uint32_t)ceil(DSLOGIC_MAX_LOGIC_SAMPLERATE * 1.0 / devc->cur_samplerate);

    // analog: 16bits, but sample with half mode(0-7 valid only)
    setting.count    = (sdi->mode == DSO) ?    (uint32_t)(devc->limit_samples / (g_slist_length(sdi->channels) / en_ch_num(sdi))) :
                       (sdi->mode == ANALOG) ? (uint32_t)(devc->limit_samples * g_slist_length(sdi->channels) * 4) :
                                               (uint32_t)(devc->limit_samples);
    setting.trig_pos = (uint32_t)(trigger->trigger_pos / 100.0 * devc->limit_samples);
    setting.trig_glb = trigger->trigger_stages;
    setting.trig_adp = setting.count - setting.trig_pos - 1;
    setting.trig_sda = 0x0;
    if (trigger->trigger_mode == SIMPLE_TRIGGER) {
        setting.trig_mask0[0] = ds_trigger_get_mask0(TriggerStages);
        setting.trig_mask1[0] = ds_trigger_get_mask1(TriggerStages);

        setting.trig_value0[0] = ds_trigger_get_value0(TriggerStages);
        setting.trig_value1[0] = ds_trigger_get_value1(TriggerStages);

        setting.trig_edge0[0] = ds_trigger_get_edge0(TriggerStages);
        setting.trig_edge1[0] = ds_trigger_get_edge1(TriggerStages);

        setting.trig_count0[0] = trigger->trigger0_count[TriggerStages];
        setting.trig_count1[0] = trigger->trigger1_count[TriggerStages];

        setting.trig_logic0[0] = (trigger->trigger_logic[TriggerStages] << 1) + trigger->trigger0_inv[TriggerStages];
        setting.trig_logic1[0] = (trigger->trigger_logic[TriggerStages] << 1) + trigger->trigger1_inv[TriggerStages];

        for (i = 1; i < NUM_TRIGGER_STAGES; i++) {
            setting.trig_mask0[i] = 0xffff;
            setting.trig_mask1[i] = 0xffff;

            setting.trig_value0[i] = 0;
            setting.trig_value1[i] = 0;

            setting.trig_edge0[i] = 0;
            setting.trig_edge1[i] = 0;

            setting.trig_count0[i] = 0;
            setting.trig_count1[i] = 0;

            setting.trig_logic0[i] = 2;
            setting.trig_logic1[i] = 2;
        }
    } else {
        for (i = 0; i < NUM_TRIGGER_STAGES; i++) {
            setting.trig_mask0[i] = ds_trigger_get_mask0(i);
            setting.trig_mask1[i] = ds_trigger_get_mask1(i);

            setting.trig_value0[i] = ds_trigger_get_value0(i);
            setting.trig_value1[i] = ds_trigger_get_value1(i);

            setting.trig_edge0[i] = ds_trigger_get_edge0(i);
            setting.trig_edge1[i] = ds_trigger_get_edge1(i);

            setting.trig_count0[i] = trigger->trigger0_count[i];
            setting.trig_count1[i] = trigger->trigger1_count[i];

            setting.trig_logic0[i] = (trigger->trigger_logic[i] << 1) + trigger->trigger0_inv[i];
            setting.trig_logic1[i] = (trigger->trigger_logic[i] << 1) + trigger->trigger1_inv[i];
        }
    }

    result  = SR_OK;
    ret = libusb_bulk_transfer(hdl, 2 | LIBUSB_ENDPOINT_OUT,
                               &setting, sizeof(struct DSL_setting),
                               &transferred, 1000);

    if (ret < 0) {
        sr_err("Unable to setting FPGA of DSLogic: %s.",
                libusb_error_name(ret));
        result = SR_ERR;
    } else if (transferred != sizeof(struct DSL_setting)) {
        sr_err("Setting FPGA error: expacted transfer size %d; actually %d",
                sizeof(struct DSL_setting), transferred);
        result = SR_ERR;
    }

    if (result == SR_OK)
        sr_info("FPGA setting done");

    return result;
}

static int fpga_config(struct libusb_device_handle *hdl, const char *filename)
{
    FILE *fw;
    int offset, chunksize, ret, result;
    unsigned char *buf;
    int transferred;
    uint64_t filesize;
	struct stat f_stat;

    sr_info("Configure FPGA using %s", filename);
    if ((fw = fopen(filename, "rb")) == NULL) {
        sr_err("Unable to open FPGA bit file %s for reading: %s",
               filename, strerror(errno));
        return SR_ERR;
    }
	
    if (stat(filename, &f_stat) == -1)
        return SR_ERR;

    filesize = (uint64_t)f_stat.st_size;
	
    if (!(buf = g_try_malloc(filesize))) {
            sr_err("FPGA configure bit malloc failed.");
            return SR_ERR;
    }

    result = SR_OK;
    offset = 0;
    while (1) {
        chunksize = fread(buf, 1, filesize, fw);
        if (chunksize == 0)
            break;

        //do {
            ret = libusb_bulk_transfer(hdl, 2 | LIBUSB_ENDPOINT_OUT,
                                       buf, chunksize,
                                       &transferred, 1000);
        //} while(ret == LIBUSB_ERROR_TIMEOUT);

        if (ret < 0) {
            sr_err("Unable to configure FPGA of DSLogic: %s.",
                    libusb_error_name(ret));
            result = SR_ERR;
            break;
        } else if (transferred != chunksize) {
            sr_err("Configure FPGA error: expacted transfer size %d; actually %d",
                    chunksize, transferred);
            result = SR_ERR;
            break;
        }
        sr_info("Configure %d bytes", chunksize);
        offset += chunksize;
    }
    fclose(fw);
    g_free(buf);
    if (result == SR_OK)
        sr_info("FPGA configure done");

    return result;
}

static int DSLogic_dev_open(struct sr_dev_inst *sdi)
{
	libusb_device **devlist;
    struct sr_usb_dev_inst *usb;
	struct libusb_device_descriptor des;
    struct DSL_context *devc;
	struct drv_context *drvc;
	struct version_info vi;
	int ret, skip, i, device_count;
	uint8_t revid;

	drvc = di->priv;
	devc = sdi->priv;
	usb = sdi->conn;

    if (sdi->status == SR_ST_ACTIVE)
		/* Device is already in use. */
        return SR_ERR;

	skip = 0;
    device_count = libusb_get_device_list(drvc->sr_ctx->libusb_ctx, &devlist);
	if (device_count < 0) {
        sr_err("Failed to get device list: %s.",
		       libusb_error_name(device_count));
        return SR_ERR;
	}

	for (i = 0; i < device_count; i++) {
		if ((ret = libusb_get_device_descriptor(devlist[i], &des))) {
            sr_err("Failed to get device descriptor: %s.",
			       libusb_error_name(ret));
			continue;
		}

		if (des.idVendor != devc->profile->vid
		    || des.idProduct != devc->profile->pid)
			continue;

        if (sdi->status == SR_ST_INITIALIZING) {
			if (skip != sdi->index) {
				/* Skip devices of this type that aren't the one we want. */
				skip += 1;
				continue;
			}
        } else if (sdi->status == SR_ST_INACTIVE) {
			/*
			 * This device is fully enumerated, so we need to find
			 * this device by vendor, product, bus and address.
			 */
			if (libusb_get_bus_number(devlist[i]) != usb->bus
				|| libusb_get_device_address(devlist[i]) != usb->address)
				/* This is not the one. */
				continue;
		}

		if (!(ret = libusb_open(devlist[i], &usb->devhdl))) {
			if (usb->address == 0xff)
				/*
				 * First time we touch this device after FW
				 * upload, so we don't know the address yet.
				 */
				usb->address = libusb_get_device_address(devlist[i]);
		} else {
            sr_err("Failed to open device: %s.",
			       libusb_error_name(ret));
			break;
		}

		ret = command_get_fw_version(usb->devhdl, &vi);
        if (ret != SR_OK) {
            sr_err("Failed to get firmware version.");
			break;
		}

		ret = command_get_revid_version(usb->devhdl, &revid);
        if (ret != SR_OK) {
            sr_err("Failed to get REVID.");
			break;
		}

		/*
		 * Changes in major version mean incompatible/API changes, so
		 * bail out if we encounter an incompatible version.
		 * Different minor versions are OK, they should be compatible.
		 */
        if (vi.major != DSL_REQUIRED_VERSION_MAJOR) {
            sr_err("Expected firmware version %d.x, "
                   "got %d.%d.", DSL_REQUIRED_VERSION_MAJOR,
                   vi.major, vi.minor);
            break;
        }

        sdi->status = SR_ST_ACTIVE;
        sr_info("Opened device %d on %d.%d, "
			"interface %d, firmware %d.%d.",
			sdi->index, usb->bus, usb->address,
			USB_INTERFACE, vi.major, vi.minor);

        sr_info("Detected REVID=%d, it's a Cypress CY7C68013%s.",
			revid, (revid != 1) ? " (FX2)" : "A (FX2LP)");

		break;
	}
	libusb_free_device_list(devlist, 1);

    if (sdi->status != SR_ST_ACTIVE)
        return SR_ERR;

    return SR_OK;
}

static int configure_probes(const struct sr_dev_inst *sdi)
{
    struct DSL_context *devc;
    struct sr_channel *probe;
	GSList *l;
	int probe_bit, stage, i;
	char *tc;

	devc = sdi->priv;
	for (i = 0; i < NUM_TRIGGER_STAGES; i++) {
		devc->trigger_mask[i] = 0;
		devc->trigger_value[i] = 0;
	}

	stage = -1;
    for (l = sdi->channels; l; l = l->next) {
        probe = (struct sr_channel *)l->data;
		if (probe->enabled == FALSE)
			continue;

		probe_bit = 1 << (probe->index);
		if (!(probe->trigger))
			continue;

		stage = 0;
		for (tc = probe->trigger; *tc; tc++) {
			devc->trigger_mask[stage] |= probe_bit;
			if (*tc == '1')
				devc->trigger_value[stage] |= probe_bit;
			stage++;
			if (stage > NUM_TRIGGER_STAGES)
                return SR_ERR;
		}
	}

	if (stage == -1)
		/*
		 * We didn't configure any triggers, make sure acquisition
		 * doesn't wait for any.
		 */
		devc->trigger_stage = TRIGGER_FIRED;
	else
		devc->trigger_stage = 0;

    return SR_OK;
}

static struct DSL_context *DSLogic_dev_new(void)
{
    struct DSL_context *devc;

    if (!(devc = g_try_malloc(sizeof(struct DSL_context)))) {
        sr_err("Device context malloc failed.");
		return NULL;
	}

	devc->profile = NULL;
	devc->fw_updated = 0;
    devc->cur_samplerate = DEFAULT_SAMPLERATE;
    devc->limit_samples = DEFAULT_SAMPLELIMIT;
    devc->sample_wide = TRUE;
    devc->clock_type = FALSE;
    devc->clock_edge = FALSE;
    devc->rle_mode = FALSE;
    devc->instant = FALSE;
    devc->op_mode = SR_OP_BUFFER;
    devc->ch_mode = 0;
    devc->samplerates_size = 14;
    devc->samplecounts_size = ARRAY_SIZE(samplecounts);
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
    devc->stream = FALSE;
    devc->mstatus_valid = FALSE;
    devc->data_lock = FALSE;
    devc->max_height = 0;
    devc->dso_bits = 8;
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

static int probe_init(struct sr_dev_inst *sdi)
{
    int i;
    GList *l;
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

static int set_probes(struct sr_dev_inst *sdi, int num_probes)
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
    GSList *l;
    struct sr_channel *probe;
    GSList *p;

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
        g_slist_delete_link(sdi->channels, g_slist_last(sdi->channels));
        j--;
    }

    return SR_OK;
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
        if (set_probes(sdi, num_logic_probes) != SR_OK)
            return NULL;

        devc = DSLogic_dev_new();
		devc->profile = prof;
		sdi->priv = devc;
		drvc->instances = g_slist_append(drvc->instances, sdi);
        //devices = g_slist_append(devices, sdi);

        if (check_conf_profile(devlist[i])) {
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
    int i;

    if (strcmp(sdi->model, "DSLogic Pro") == 0) {
        for(i = 0; i < ARRAY_SIZE(pro_mode_list); i++) {
            l = g_slist_append(l, &pro_mode_list[i]);
        }
    } else {
        for(i = 0; i < ARRAY_SIZE(mode_list); i++) {
            l = g_slist_append(l, &mode_list[i]);
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
    case SR_CONF_VDIV:
    case SR_CONF_EN_CH:
    case SR_CONF_TIMEBASE:
    case SR_CONF_COUPLING:
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

static int dso_init(const struct sr_dev_inst *sdi, gboolean from_eep)
{
    int ret, i;
    GSList *l;
    struct sr_usb_dev_inst *usb = sdi->conn;
    gboolean zeroed = FALSE;

    for(l = sdi->channels; l; l = l->next) {
        struct sr_channel *probe = (struct sr_channel *)l->data;
        ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, probe, SR_CONF_COUPLING));
        if (ret != SR_OK) {
            sr_err("DSO set coupling of channel %d command failed!", probe->index);
            return ret;
        }
        ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, probe, SR_CONF_VDIV));
        if (ret != SR_OK) {
            sr_err("Set VDIV of channel %d command failed!", probe->index);
            return ret;
        }
    }

    ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, 0, SR_CONF_SAMPLERATE));
    if (ret != SR_OK) {
        sr_err("Set Sample Rate command failed!");
        return ret;
    }
    ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, NULL, SR_CONF_HORIZ_TRIGGERPOS));
    if (ret != SR_OK) {
        sr_err("Set Horiz Trigger Position command failed!");
        return ret;
    }
    ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, NULL, SR_CONF_TRIGGER_HOLDOFF));
    if (ret != SR_OK) {
        sr_err("Set Trigger Holdoff Time command failed!");
        return ret;
    }
    ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, NULL, SR_CONF_TRIGGER_SLOPE));
    if (ret != SR_OK) {
        sr_err("Set Trigger Slope command failed!");
        return ret;
    }
    ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, NULL, SR_CONF_TRIGGER_SOURCE));
    if (ret != SR_OK) {
        sr_err("Set Trigger Source command failed!");
        return ret;
    }
    ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, NULL, SR_CONF_TRIGGER_VALUE));
    if (ret != SR_OK) {
        sr_err("Set Trigger Value command failed!");
        return ret;
    }
    ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, NULL, SR_CONF_TRIGGER_MARGIN));
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
    struct sr_usb_dev_inst *usb;
	char str[128];

    (void)cg;

	switch (id) {
    case SR_CONF_CONN:
		if (!sdi || !sdi->conn)
            return SR_ERR_ARG;
		usb = sdi->conn;
		if (usb->address == 255)
			/* Device still needs to re-enumerate after firmware
			 * upload, so we don't know its (future) address. */
            return SR_ERR;
		snprintf(str, 128, "%d.%d", usb->bus, usb->address);
        *data = g_variant_new_string(str);
		break;
    case SR_CONF_LIMIT_SAMPLES:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_uint64(devc->limit_samples);
        break;
    case SR_CONF_ACTUAL_SAMPLES:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_uint64(devc->actual_samples);
        break;
    case SR_CONF_SAMPLERATE:
		if (!sdi)
            return SR_ERR;
		devc = sdi->priv;
		*data = g_variant_new_uint64(devc->cur_samplerate);
		break;
    case SR_CONF_CLOCK_TYPE:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_boolean(devc->clock_type);
        break;
    case SR_CONF_CLOCK_EDGE:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_boolean(devc->clock_edge);
        break;
    case SR_CONF_RLE:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_boolean(devc->rle_mode);
        break;
    case SR_CONF_INSTANT:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_boolean(devc->instant);
        break;		
    case SR_CONF_OPERATION_MODE:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_string(opmodes[devc->op_mode]);
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
    case SR_CONF_TEST:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_boolean((devc->op_mode != SR_OP_BUFFER) &&
                                      (devc->op_mode != SR_OP_STREAM));
        break;
    case SR_CONF_FILTER:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_string(filters[devc->filter]);
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
    case SR_CONF_VDIV:
        if (!ch)
            return SR_ERR;
        *data = g_variant_new_uint64(ch->vdiv);
        break;
    case SR_CONF_FACTOR:
        if (!ch)
            return SR_ERR;
        *data = g_variant_new_uint64(ch->vfactor);
        break;
    case SR_CONF_VPOS:
        if (!ch)
            return SR_ERR;
        *data = g_variant_new_double(ch->vpos);
        break;
    case SR_CONF_TIMEBASE:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_uint64(devc->timebase);
        break;
    case SR_CONF_COUPLING:
        if (!ch)
            return SR_ERR;
        *data = g_variant_new_byte(ch->coupling);
        break;
    case SR_CONF_EN_CH:
        if (!ch)
            return SR_ERR;
        *data = g_variant_new_boolean(ch->enabled);
        break;
    case SR_CONF_DATALOCK:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_boolean(devc->data_lock);
        break;
    case SR_CONF_TRIGGER_SLOPE:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_byte(devc->trigger_slope);
        break;
    case SR_CONF_TRIGGER_SOURCE:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_byte(devc->trigger_source&0x0f);
        break;
    case SR_CONF_TRIGGER_CHANNEL:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_byte(devc->trigger_source>>4);
        break;
    case SR_CONF_TRIGGER_VALUE:
        if (!ch)
            return SR_ERR;
        *data = g_variant_new_byte(ch->trig_value);
        break;
    case SR_CONF_HORIZ_TRIGGERPOS:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
		if (sdi->mode == DSO) {
            *data = g_variant_new_byte(devc->trigger_hrate);
		} else {
            *data = g_variant_new_byte(devc->trigger_hpos);
		}
        break;
    case SR_CONF_TRIGGER_HOLDOFF:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_uint64(devc->trigger_holdoff);
        break;
    case SR_CONF_TRIGGER_MARGIN:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_byte(devc->trigger_margin);
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
    case SR_CONF_ROLL:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_boolean(devc->roll);
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
    case SR_CONF_MAX_LOGIC_SAMPLERATE:
        if (!sdi)
            return SR_ERR;
        *data = g_variant_new_uint64(DSLOGIC_MAX_LOGIC_SAMPLERATE);
        break;
    case SR_CONF_MAX_LOGIC_SAMPLELIMITS:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_uint64(DSLOGIC_MAX_LOGIC_DEPTH*ceil(devc->cur_samplerate * 1.0 / DSLOGIC_MAX_LOGIC_SAMPLERATE));
        break;
    case SR_CONF_RLE_SAMPLELIMITS:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_uint64(DSLOGIC_MAX_LOGIC_DEPTH*ceil(samplerates[devc->samplerates_size-1] * 1.0 / DSLOGIC_MAX_LOGIC_SAMPLERATE));
        break;
    case SR_CONF_DSO_BITS:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_byte(devc->dso_bits);
        break;
    default:
        return SR_ERR_NA;
	}

    return SR_OK;
}

static int config_set(int id, GVariant *data, struct sr_dev_inst *sdi,
                      struct sr_channel *ch,
                      const struct sr_channel_group *cg )
{
    struct DSL_context *devc;
    const char *stropt;
    int ret, num_probes;
    struct sr_usb_dev_inst *usb;
    int i;
    struct drv_context *drvc;

    (void)cg;

    if (sdi->status != SR_ST_ACTIVE)
        return SR_ERR;

    drvc = di->priv;
	devc = sdi->priv;
    usb = sdi->conn;

    ret = SR_OK;
    if (id == SR_CONF_SAMPLERATE) {
		devc->cur_samplerate = g_variant_get_uint64(data);
        if(sdi->mode == DSO) {
            devc->sample_wide = (devc->cur_samplerate <= DSLOGIC_MAX_DSO_SAMPLERATE);
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, 0, SR_CONF_SAMPLERATE));
        } else {
            devc->sample_wide = (devc->cur_samplerate <= DSLOGIC_MAX_LOGIC_SAMPLERATE);
        }
    } else if (id == SR_CONF_CLOCK_TYPE) {
        devc->clock_type = g_variant_get_boolean(data);
    } else if (id == SR_CONF_CLOCK_EDGE) {
        devc->clock_edge = g_variant_get_boolean(data);
    } else if (id == SR_CONF_RLE) {
        devc->rle_mode = g_variant_get_boolean(data);
    } else if (id == SR_CONF_INSTANT) {
        if (sdi->mode == DSO) {
            devc->instant = g_variant_get_boolean(data);
            if (en_ch_num(sdi) != 0) {
                if (devc->instant)
                    devc->limit_samples = DSLOGIC_INSTANT_DEPTH / en_ch_num(sdi);
                else
                    devc->limit_samples = DSLOGIC_MAX_DSO_DEPTH / en_ch_num(sdi);
            }
        }
	} else if (id == SR_CONF_LIMIT_SAMPLES) {
		devc->limit_samples = g_variant_get_uint64(data);
    } else if (id == SR_CONF_DEVICE_MODE) {
        sdi->mode = g_variant_get_int16(data);
        if (sdi->mode == LOGIC) {
            num_probes = devc->profile->dev_caps & DEV_CAPS_16BIT ? 16 : 8;
            devc->samplecounts_size = ARRAY_SIZE(samplecounts);
        } else if (sdi->mode == DSO) {
            num_probes = devc->profile->dev_caps & DEV_CAPS_16BIT ? MAX_DSO_PROBES_NUM : 1;
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, NULL, SR_CONF_DSO_SYNC));
            if (ret != SR_OK)
                sr_dbg("%s: DSO configuration sync failed", __func__);
            devc->cur_samplerate = DSLOGIC_MAX_DSO_SAMPLERATE / num_probes;
            devc->limit_samples = DSLOGIC_MAX_DSO_DEPTH / num_probes;
            devc->samplerates_size = 15;
            devc->samplecounts_size = ARRAY_SIZE(samplecounts);
        } else if (sdi->mode == ANALOG){
            num_probes = devc->profile->dev_caps & DEV_CAPS_16BIT ? MAX_ANALOG_PROBES_NUM : 1;
            devc->op_mode = SR_OP_STREAM;
            devc->stream = TRUE;
            devc->samplerates_size = 10;
            devc->samplecounts_size = 15;
        }
        sr_dev_probes_free(sdi);
        set_probes(sdi, num_probes);
        sr_dbg("%s: setting mode to %d", __func__, sdi->mode);
        if (sdi->mode == DSO) {
            dso_init(sdi, 0);
        }
    } else if (id == SR_CONF_OPERATION_MODE) {
        stropt = g_variant_get_string(data, NULL);
        if (sdi->mode == LOGIC) {
            if (!strcmp(stropt, opmodes[SR_OP_BUFFER]) && (devc->op_mode != SR_OP_BUFFER)) {
                devc->op_mode = SR_OP_BUFFER;
                devc->stream = FALSE;
                devc->ch_mode = 0;
                devc->samplerates_size = 14;
                adjust_probes(sdi, buffer_ch_num[0]);
            } else if (!strcmp(stropt, opmodes[SR_OP_STREAM]) && (devc->op_mode != SR_OP_STREAM)) {
                devc->op_mode = SR_OP_STREAM;
                devc->stream = TRUE;
                devc->ch_mode = 0;
                devc->samplerates_size = 10;
                adjust_probes(sdi, stream_ch_num[0]);
            } else if (!strcmp(stropt, opmodes[SR_OP_INTERNAL_TEST]) && (devc->op_mode != SR_OP_INTERNAL_TEST)) {
                devc->op_mode = SR_OP_INTERNAL_TEST;
                devc->stream = FALSE;
                devc->ch_mode = 0;
                devc->samplerates_size = 14;
                adjust_probes(sdi, buffer_ch_num[0]);
                devc->limit_samples = DSLOGIC_MAX_LOGIC_DEPTH;
                devc->cur_samplerate = DSLOGIC_MAX_LOGIC_SAMPLERATE;
                devc->sample_wide = TRUE;
            } else if (!strcmp(stropt, opmodes[SR_OP_EXTERNAL_TEST]) && (devc->op_mode != SR_OP_EXTERNAL_TEST)) {
                devc->op_mode = SR_OP_EXTERNAL_TEST;
                devc->stream = FALSE;
                devc->ch_mode = 0;
                devc->samplerates_size = 14;
                adjust_probes(sdi, buffer_ch_num[0]);
                devc->limit_samples = DSLOGIC_MAX_LOGIC_DEPTH;
                devc->cur_samplerate = DSLOGIC_MAX_LOGIC_SAMPLERATE;
                devc->sample_wide = TRUE;
            } else if (!strcmp(stropt, opmodes[SR_OP_LOOPBACK_TEST]) && (devc->op_mode != SR_OP_LOOPBACK_TEST)) {
                devc->op_mode = SR_OP_LOOPBACK_TEST;
                devc->stream = FALSE;
                devc->ch_mode = 0;
                devc->samplerates_size = 14;
                adjust_probes(sdi, buffer_ch_num[0]);
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
            devc->op_mode = SR_OP_STREAM;
            devc->stream = TRUE;
            devc->samplerates_size = 10;
            devc->samplecounts_size = 15;
        }
        sr_dbg("%s: setting pattern to %d",
            __func__, devc->op_mode);
    } else if (id == SR_CONF_CHANNEL_MODE) {
        stropt = g_variant_get_string(data, NULL);
        if (sdi->mode == LOGIC) {
            if (devc->stream) {
                for (i = 0; i < ARRAY_SIZE(stream_ch_modes); i++)
                    if (!strcmp(stropt, stream_ch_modes[i])) {
                        devc->ch_mode = i;
                        devc->samplerates_size = 10 + i * 2;
                        adjust_probes(sdi, stream_ch_num[i]);
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
        stropt = g_variant_get_string(data, NULL);
        if (!strcmp(stropt, thresholds[SR_TH_3V3])) {
            devc->th_level = SR_TH_3V3;
        } else if (!strcmp(stropt, thresholds[SR_TH_5V0])) {
            devc->th_level = SR_TH_5V0;
        } else {
            ret = SR_ERR;
        }
        if (sdi->mode == LOGIC) {
            if ((ret = command_fpga_config(usb->devhdl)) != SR_OK) {
                sr_err("Send FPGA configure command failed!");
            } else {
                /* Takes >= 10ms for the FX2 to be ready for FPGA configure. */
                g_usleep(10 * 1000);
                char *fpga_bit;
                if (!(fpga_bit = g_try_malloc(strlen(DS_RES_PATH)+strlen(devc->profile->fpga_bit33)+1))) {
                    sr_err("fpag_bit path malloc error!");
                    return SR_ERR_MALLOC;
                }
                strcpy(fpga_bit, DS_RES_PATH);
                switch(devc->th_level) {
                case SR_TH_3V3:
                    strcat(fpga_bit, devc->profile->fpga_bit33);;
                    break;
                case SR_TH_5V0:
                    strcat(fpga_bit, devc->profile->fpga_bit50);;
                    break;
                default:
                    return SR_ERR;
                }
                ret = fpga_config(usb->devhdl, fpga_bit);
                if (ret != SR_OK) {
                    sr_err("Configure FPGA failed!");
                }
                g_free(fpga_bit);
            }
        }
        sr_dbg("%s: setting threshold to %d",
            __func__, devc->th_level);
    } else if (id == SR_CONF_VTH) {
        devc->vth = g_variant_get_double(data);
        if ((ret = command_wr_reg(usb->devhdl, (uint8_t)(devc->vth/5.0*255), VTH_ADDR)) == SR_OK) {
            sr_err("%s: setting threshold voltage to %f",
                __func__, devc->vth);
        } else {
            sr_info("%s: setting threshold voltage to %f failed",
                __func__, devc->vth);
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
    } else if (id == SR_CONF_EN_CH) {
        ch->enabled = g_variant_get_boolean(data);
        if (sdi->mode == DSO) {
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, ch, SR_CONF_EN_CH));
            uint16_t channel_cnt = 0;
            GSList *l;
            for (l = sdi->channels; l; l = l->next) {
                struct sr_channel *probe = (struct sr_channel *)l->data;
                channel_cnt += probe->enabled;
            }
            if (channel_cnt != 0)
                ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, 0, SR_CONF_SAMPLERATE));
        }
        if (ret == SR_OK)
            sr_dbg("%s: setting ENABLE of channel %d to %d",
                __func__, ch->index, ch->enabled);
        else
            sr_dbg("%s: setting ENABLE of channel %d to %d",
                __func__, ch->index, ch->enabled);
    } else if (id == SR_CONF_DATALOCK) {
        while(libusb_try_lock_events(drvc->sr_ctx->libusb_ctx));
        devc->data_lock = g_variant_get_boolean(data);
        libusb_unlock_events(drvc->sr_ctx->libusb_ctx);
    } else if (id == SR_CONF_VDIV) {
        ch->vdiv = g_variant_get_uint64(data);
        if (sdi->mode == DSO) {
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, ch, SR_CONF_VDIV));
        }
        if (ret == SR_OK)
            sr_dbg("%s: setting VDIV of channel %d to %d mv",
                __func__, ch->index, ch->vdiv);
        else
            sr_dbg("%s: setting VDIV of channel %d to %d mv failed",
                __func__, ch->index, ch->vdiv);
    } else if (id == SR_CONF_FACTOR) {
        ch->vfactor = g_variant_get_uint64(data);
        sr_dbg("%s: setting Factor of channel %d to %d", __func__,
               ch->index, ch->vfactor);
    } else if (id == SR_CONF_VPOS) {
        ch->vpos = g_variant_get_double(data);
        sr_dbg("%s: setting VPOS of channel %d to %lf", __func__,
               ch->index, ch->vpos);
    } else if (id == SR_CONF_TIMEBASE) {
        devc->timebase = g_variant_get_uint64(data);
    } else if (id == SR_CONF_COUPLING) {
        ch->coupling = g_variant_get_byte(data);
        if (ch->coupling == SR_GND_COUPLING)
            ch->coupling = SR_DC_COUPLING;
        if (sdi->mode == DSO) {
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, ch, SR_CONF_COUPLING));
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
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, NULL, SR_CONF_TRIGGER_SLOPE));
        }
        if (ret == SR_OK)
            sr_dbg("%s: setting DSO Trigger Slope to %d",
                __func__, devc->trigger_slope);
        else
            sr_dbg("%s: setting DSO Trigger Slope to %d failed",
                __func__, devc->trigger_slope);
    } else if (id == SR_CONF_TRIGGER_SOURCE) {
        devc->trigger_source = g_variant_get_byte(data);
        if (sdi->mode == DSO) {
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, NULL, SR_CONF_TRIGGER_SOURCE));
        }
        if (ret == SR_OK)
            sr_dbg("%s: setting DSO Trigger Source to %d",
                __func__, devc->trigger_source);
        else
            sr_dbg("%s: setting DSO Trigger Source to %d failed",
                __func__, devc->trigger_source);
    } else if (id == SR_CONF_TRIGGER_VALUE) {
        ch->trig_value = g_variant_get_byte(data);
        if (sdi->mode == DSO) {
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, ch, SR_CONF_TRIGGER_VALUE));
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
            //devc->trigger_hpos = devc->trigger_hrate * en_ch_num(sdi) * devc->limit_samples / 200.0;
            /*
             * devc->trigger_hpos should be updated before each acquisition
             * because the samplelimits may changed
             */
            devc->trigger_hpos = devc->trigger_hrate * en_ch_num(sdi) * devc->limit_samples / 200.0;
            if ((ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, 1, SR_CONF_HORIZ_TRIGGERPOS))) == SR_OK)
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
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, NULL, SR_CONF_TRIGGER_HOLDOFF));
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
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, NULL, SR_CONF_TRIGGER_MARGIN));
        }
        if (ret == SR_OK)
            sr_dbg("%s: setting Trigger Margin to %d",
                __func__, devc->trigger_margin);
        else
            sr_dbg("%s: setting Trigger Margin to %d failed",
                __func__, devc->trigger_margin);
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
        if (strcmp(sdi->model, "DSLogic Pro") == 0)
            *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                    hwoptions_pro, ARRAY_SIZE(hwoptions_pro)*sizeof(int32_t), TRUE, NULL, NULL);
        else
            *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                    hwoptions, ARRAY_SIZE(hwoptions)*sizeof(int32_t), TRUE, NULL, NULL);
        break;
    case SR_CONF_DEVICE_SESSIONS:
        if (strcmp(sdi->model, "DSLogic Pro") == 0)
            *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                    sessions_pro, ARRAY_SIZE(sessions_pro)*sizeof(int32_t), TRUE, NULL, NULL);
        else
            *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                    sessions, ARRAY_SIZE(sessions)*sizeof(int32_t), TRUE, NULL, NULL);
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
    case SR_CONF_CHANNEL_MODE:
        if (devc->stream)
            *data = g_variant_new_strv(stream_ch_modes, ARRAY_SIZE(stream_ch_modes));
        else if (devc->op_mode != SR_OP_BUFFER)
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
    struct sr_usb_dev_inst *usb;
    struct DSL_context *devc;
    GSList *l;
	int ret;
	int64_t timediff_us, timediff_ms;
    uint8_t fpga_done;

	devc = sdi->priv;
	usb = sdi->conn;

	/*
     * If the firmware was recently uploaded, no dev_open operation should be called.
     * Just wait for renumerate -> detach -> attach
	 */
    ret = SR_ERR;
	if (devc->fw_updated > 0) {
        return SR_ERR;
    } else {
        sr_info("%s: Firmware upload was not needed.", __func__);
        ret = DSLogic_dev_open(sdi);
    }

    if (ret != SR_OK) {
        sr_err("%s: Unable to open device.", __func__);
        return SR_ERR;
	}

	ret = libusb_claim_interface(usb->devhdl, USB_INTERFACE);
	if (ret != 0) {
		switch(ret) {
		case LIBUSB_ERROR_BUSY:
            sr_err("%s: Unable to claim USB interface. Another "
                   "program or driver has already claimed it.", __func__);
			break;
		case LIBUSB_ERROR_NO_DEVICE:
            sr_err("%s: Device has been disconnected.", __func__);
			break;
		default:
            sr_err("%s: Unable to claim interface: %s.",
                   __func__, libusb_error_name(ret));
			break;
		}

        return SR_ERR;
	}

    ret = command_get_fpga_done(usb->devhdl, &fpga_done);
    if (ret != SR_OK) {
        sr_err("Failed to get fpga done infos.");
        return SR_ERR;
    }

    if (fpga_done == 0) {
        if ((ret = command_fpga_config(usb->devhdl)) != SR_OK) {
            sr_err("%s: Send FPGA configure command failed!", __func__);
        } else {
            /* Takes >= 10ms for the FX2 to be ready for FPGA configure. */
            g_usleep(10 * 1000);
            char *fpga_bit;
            if (!(fpga_bit = g_try_malloc(strlen(DS_RES_PATH)+strlen(devc->profile->fpga_bit33)+1))) {
                sr_err("fpag_bit path malloc error!");
                return SR_ERR_MALLOC;
            }
            strcpy(fpga_bit, DS_RES_PATH);
            switch(devc->th_level) {
            case SR_TH_3V3:
                strcat(fpga_bit, devc->profile->fpga_bit33);;
                break;
            case SR_TH_5V0:
                strcat(fpga_bit, devc->profile->fpga_bit50);;
                break;
            default:
                return SR_ERR;
            }
            ret = fpga_config(usb->devhdl, fpga_bit);
            if (ret != SR_OK) {
                sr_err("%s: Configure FPGA failed!", __func__);
            }
            g_free(fpga_bit);

            if ((ret = command_wr_reg(usb->devhdl, (uint8_t)(devc->vth/5.0*255), VTH_ADDR)) == SR_OK) {
                sr_err("%s: setting threshold voltage to %f",
                    __func__, devc->vth);
            } else {
                sr_info("%s: setting threshold voltage to %f failed",
                    __func__, devc->vth);
            }
        }
    }

    return SR_OK;
}

static int dev_close(struct sr_dev_inst *sdi)
{
    struct sr_usb_dev_inst *usb;
    struct DSL_context *devc;

	usb = sdi->conn;
	if (usb->devhdl == NULL)
        return SR_ERR;
    devc = sdi->priv;

    sr_info("DSLogic: Closing device %d on %d.%d interface %d.",
		sdi->index, usb->bus, usb->address, USB_INTERFACE);
	libusb_release_interface(usb->devhdl, USB_INTERFACE);
	libusb_close(usb->devhdl);
	usb->devhdl = NULL;
    sdi->status = SR_ST_INACTIVE;

    return SR_OK;
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

static void finish_acquisition(struct DSL_context *devc)
{
    struct sr_datafeed_packet packet;

    sr_err("%s: send SR_DF_END packet", __func__);
    /* Terminate session. */
    packet.type = SR_DF_END;
    sr_session_send(devc->cb_data, &packet);
	
    if (devc->num_transfers != 0) {
        devc->num_transfers = 0;
        g_free(devc->transfers);
    }

    devc->status = DSL_FINISH;
}

static void free_transfer(struct libusb_transfer *transfer)
{
    struct DSL_context *devc;
	unsigned int i;

    devc = transfer->user_data;

	g_free(transfer->buffer);
	transfer->buffer = NULL;
	libusb_free_transfer(transfer);

	for (i = 0; i < devc->num_transfers; i++) {
		if (devc->transfers[i] == transfer) {
			devc->transfers[i] = NULL;
			break;
		}
	}

	devc->submitted_transfers--;
    if (devc->submitted_transfers == 0 && devc->status != DSL_TRIGGERED)
        finish_acquisition(devc);
}

static void resubmit_transfer(struct libusb_transfer *transfer)
{
    int ret;

    if ((ret = libusb_submit_transfer(transfer)) == LIBUSB_SUCCESS)
        return;

    free_transfer(transfer);
    /* TODO: Stop session? */

    sr_err("%s: %s", __func__, libusb_error_name(ret));
}


static void receive_transfer(struct libusb_transfer *transfer)
{
    gboolean packet_has_error = FALSE;
    struct sr_datafeed_packet packet;
    struct sr_datafeed_logic logic;
    struct sr_datafeed_dso dso;
    struct sr_datafeed_analog analog;

    int trigger_offset, i;
	int trigger_offset_bytes;

    const uint8_t *cur_buf = transfer->buffer;
    struct DSL_context *devc = transfer->user_data;
    struct sr_dev_inst *sdi = devc->cb_data;
	const int sample_width = (devc->sample_wide) ? 2 : 1;
    int cur_sample_count = transfer->actual_length / sample_width;
	
    if (devc->data_lock) {
        resubmit_transfer(transfer);
        return;
    }

    if (devc->abort)
        devc->status = DSL_STOP;

    sr_info("receive_transfer(): status %d; timeout %d; received %d bytes.",
        transfer->status, transfer->timeout, transfer->actual_length);

    switch (transfer->status) {
    case LIBUSB_TRANSFER_NO_DEVICE:
    case LIBUSB_TRANSFER_CANCELLED:
        devc->status = DSL_ERROR;
        break;
    case LIBUSB_TRANSFER_COMPLETED:
    case LIBUSB_TRANSFER_TIMED_OUT: /* We may have received some data though. */
        break;
    default:
        packet_has_error = TRUE;
        break;
    }

    if (devc->status == DSL_DATA &&
        (transfer->actual_length == 0 ||
        packet_has_error)) {
        devc->empty_transfer_count++;
        if (devc->empty_transfer_count > MAX_EMPTY_TRANSFERS) {
            devc->status = DSL_ERROR;
        }
    } else {
        devc->empty_transfer_count = 0;
    }

    trigger_offset = 0;
    if (devc->status == DSL_DATA && devc->trigger_stage >= 0) {
        for (i = 0; i < cur_sample_count; i++) {
            const uint16_t cur_sample = devc->sample_wide ?
                *((const uint16_t*)cur_buf + i) :
                *((const uint8_t*)cur_buf + i);

            if ((cur_sample & devc->trigger_mask[devc->trigger_stage]) ==
                devc->trigger_value[devc->trigger_stage]) {
                /* Match on this trigger stage. */
                devc->trigger_buffer[devc->trigger_stage] = cur_sample;
                devc->trigger_stage++;

                if (devc->trigger_stage == NUM_TRIGGER_STAGES ||
                    devc->trigger_mask[devc->trigger_stage] == 0) {
                    /* Match on all trigger stages, we're done. */
                    trigger_offset = i + 1;

                    /*
                     * TODO: Send pre-trigger buffer to session bus.
                     * Tell the frontend we hit the trigger here.
                     */
                    packet.type = SR_DF_TRIGGER;
                    packet.payload = NULL;
                    sr_session_send(sdi, &packet);

                    /*
                     * Send the samples that triggered it,
                     * since we're skipping past them.
                     */
                    packet.type = SR_DF_LOGIC;
                    packet.payload = &logic;
                    logic.unitsize = sizeof(*devc->trigger_buffer);
                    logic.length = devc->trigger_stage * logic.unitsize;
                    logic.data = devc->trigger_buffer;
                    sr_session_send(sdi, &packet);

                    devc->trigger_stage = TRIGGER_FIRED;
                    break;
                }
            } else if (devc->trigger_stage > 0) {
                /*
                 * We had a match before, but not in the next sample. However, we may
                 * have a match on this stage in the next bit -- trigger on 0001 will
                 * fail on seeing 00001, so we need to go back to stage 0 -- but at
                 * the next sample from the one that matched originally, which the
                 * counter increment at the end of the loop takes care of.
                 */
                i -= devc->trigger_stage;
                if (i < -1)
                    i = -1; /* Oops, went back past this buffer. */
                /* Reset trigger stage. */
                devc->trigger_stage = 0;
            }
        }
    }

    if (devc->status == DSL_DATA && devc->trigger_stage == TRIGGER_FIRED) {
        /* Send the incoming transfer to the session bus. */
        trigger_offset_bytes = trigger_offset * sample_width;
        // check packet type
        if (sdi->mode == LOGIC) {
            packet.type = SR_DF_LOGIC;
            packet.payload = &logic;
            logic.length = transfer->actual_length - trigger_offset_bytes;
            logic.unitsize = sample_width;
            logic.data_error = 0;
            logic.data = cur_buf + trigger_offset_bytes;
        } else if (sdi->mode == DSO) {
            if (!devc->instant) {
                const uint32_t mstatus_offset = devc->limit_samples / (g_slist_length(sdi->channels)/en_ch_num(sdi));
                mstatus.pkt_id = *((const uint16_t*)cur_buf + mstatus_offset);
                mstatus.ch0_max = *((const uint8_t*)cur_buf + mstatus_offset*2 + 1*2);
                mstatus.ch0_min = *((const uint8_t*)cur_buf + mstatus_offset*2 + 3);
                mstatus.ch0_period = *((const uint32_t*)cur_buf + mstatus_offset/2 + 2/2);
                mstatus.ch0_period += ((uint64_t)*((const uint32_t*)cur_buf + mstatus_offset/2 + 4/2)) << 32;
                mstatus.ch0_pcnt = *((const uint32_t*)cur_buf + mstatus_offset/2 + 6/2);
                mstatus.ch1_max = *((const uint8_t*)cur_buf + mstatus_offset*2 + 9*2);
                mstatus.ch1_min = *((const uint8_t*)cur_buf + mstatus_offset*2 + 19);
                mstatus.ch1_period = *((const uint32_t*)cur_buf + mstatus_offset/2 + 10/2);
                mstatus.ch1_period += ((uint64_t)*((const uint32_t*)cur_buf + mstatus_offset/2 + 12/2)) << 32;
                mstatus.ch1_pcnt = *((const uint32_t*)cur_buf + mstatus_offset/2 + 14/2);
                mstatus.vlen = *((const uint32_t*)cur_buf + mstatus_offset/2 + 16/2) & 0x7fffffff;
                mstatus.stream_mode = *((const uint32_t*)cur_buf + mstatus_offset/2 + 16/2) & 0x80000000;
                mstatus.sample_divider = *((const uint32_t*)cur_buf + mstatus_offset/2 + 18/2) & 0x0fffffff;
                mstatus.sample_divider_tog = *((const uint32_t*)cur_buf + mstatus_offset/2 + 18/2) & 0x80000000;
                mstatus.trig_flag = *((const uint32_t*)cur_buf + mstatus_offset/2 + 18/2) & 0x40000000;
            } else {
                mstatus.vlen = instant_buffer_size;
            }

            const uint32_t divider = devc->zero ? 0x1 : (uint32_t)ceil(DSCOPE_MAX_SAMPLERATE * 1.0 / devc->cur_samplerate / en_ch_num(sdi));
            if ((mstatus.pkt_id == DSO_PKTID &&
                 mstatus.sample_divider == divider &&
                 mstatus.vlen != 0 &&
                 mstatus.vlen <= (transfer->actual_length - 512) / sample_width) ||
                devc->instant) {
                devc->roll = (mstatus.stream_mode != 0);
                devc->mstatus_valid = TRUE;
                packet.type = SR_DF_DSO;
                packet.payload = &dso;
                dso.probes = sdi->channels;
                //dso.num_samples = (transfer->actual_length - 512) / sample_width;
                cur_sample_count = 2 * mstatus.vlen / en_ch_num(sdi) ;
                dso.num_samples = cur_sample_count;
                dso.mq = SR_MQ_VOLTAGE;
                dso.unit = SR_UNIT_VOLT;
                dso.mqflags = SR_MQFLAG_AC;
                dso.samplerate_tog = (mstatus.sample_divider_tog != 0);
                dso.trig_flag = (mstatus.trig_flag != 0);
                dso.data = cur_buf + trigger_offset_bytes;
            } else {
                packet.type = SR_DF_ABANDON;
                devc->mstatus_valid = FALSE;
            }
        } else {
            packet.type = SR_DF_ANALOG;
            packet.payload = &analog;
            analog.probes = sdi->channels;
            cur_sample_count = transfer->actual_length / (sample_width * g_slist_length(analog.probes));
            analog.num_samples = cur_sample_count;
            analog.mq = SR_MQ_VOLTAGE;
            analog.unit = SR_UNIT_VOLT;
            analog.mqflags = SR_MQFLAG_AC;
            analog.data = cur_buf + trigger_offset_bytes;
        }

        if ((devc->limit_samples && devc->num_samples < devc->actual_samples) ||
            (*(struct sr_dev_inst *)(devc->cb_data)).mode != LOGIC ) {
            const uint64_t remain_length= (devc->actual_samples - devc->num_samples) * sample_width;
            logic.length = min(logic.length, remain_length);

            /* in test mode, check data content*/
            if (devc->op_mode == SR_OP_INTERNAL_TEST) {
                //for (i = 0; i < logic.length / sample_width; i++) {
                for (i = 0; i < logic.length / 2; i++) {
//                    const uint16_t cur_sample = devc->sample_wide ?
//                        *((const uint16_t*)cur_buf + i) :
//                        *((const uint8_t*)cur_buf + i);
                    const uint16_t cur_sample = *((const uint16_t*)cur_buf + i);
                    if (test_init == 1) {
                        test_sample_value = cur_sample;
                        test_init = 0;
                    }
                    if (cur_sample != test_sample_value) {
                        logic.data_error = 1;
                        sr_err("exp: %d; act: %d", test_sample_value, cur_sample);
                        break;
                    }
                    test_sample_value++;
                }
            }
            if (devc->op_mode == SR_OP_EXTERNAL_TEST) {
                for (i = 0; i < logic.length / 2; i++) {
                    const uint16_t cur_sample = *((const uint16_t*)cur_buf + i);
                    if (test_init == 1) {
                        test_sample_value = cur_sample;
                        test_init = 0;
                    }
                    if (cur_sample != test_sample_value) {
                        logic.data_error = 1;
                        sr_err("exp: %d; act: %d", test_sample_value, cur_sample);
                        break;
                    }
                    test_sample_value = (test_sample_value + 1) % 65001;
                    //test_sample_value = test_sample_value + 1;
                }
            }

            /* send data to session bus */
            if (packet.type != SR_DF_ABANDON)
                sr_session_send(sdi, &packet);
        }

        devc->num_samples += cur_sample_count;
        if ((sdi->mode != DSO || devc->instant) &&
            devc->limit_samples &&
            (unsigned int)devc->num_samples >= devc->actual_samples) {
            devc->status = DSL_STOP;
        }
    }

    if (devc->status == DSL_DATA)
        resubmit_transfer(transfer);
    else
        free_transfer(transfer);
}

static unsigned int to_bytes_per_ms(struct DSL_context *devc)
{
    if (devc->cur_samplerate > SR_MHZ(100))
        return SR_MHZ(100) / 1000 * (devc->sample_wide ? 2 : 1);
    else
        return devc->cur_samplerate / 1000 * (devc->sample_wide ? 2 : 1) ;
}

static size_t get_buffer_size(struct DSL_context *devc)
{
    size_t s;

    /*
     * The buffer should be large enough to hold 10ms of data and
     * a multiple of 512.
     */
    s = single_buffer_time * to_bytes_per_ms(devc);
    //s = to_bytes_per_ms(devc->cur_samplerate);
    return (s + 511) & ~511;
}

static unsigned int get_number_of_transfers(struct DSL_context *devc)
{
    unsigned int n;
    /* Total buffer size should be able to hold about 100ms of data. */
    n = 100 * to_bytes_per_ms(devc) / get_buffer_size(devc);

    if (n > NUM_SIMUL_TRANSFERS)
        return NUM_SIMUL_TRANSFERS;

    return n;
}

static unsigned int get_timeout(struct DSL_context *devc)
{
    size_t total_size;
    unsigned int timeout;

    total_size = get_buffer_size(devc) * get_number_of_transfers(devc);
    timeout = total_size / to_bytes_per_ms(devc);
    //return timeout + timeout / 4; /* Leave a headroom of 25% percent. */
    return 1000;
}

static int dev_transfer_start(const struct sr_dev_inst *sdi)
{
    struct DSL_context *devc;
    struct sr_usb_dev_inst *usb;
    struct libusb_transfer *transfer;
    unsigned int i, num_transfers;
    int ret;
    unsigned char *buf;
    size_t size;
    int dso_buffer_size;

    devc = sdi->priv;
    usb = sdi->conn;

    if (devc->instant)
        dso_buffer_size = instant_buffer_size * g_slist_length(sdi->channels);
    else
        dso_buffer_size = devc->limit_samples * en_ch_num(sdi) + 512;

    num_transfers = 1;
    size = (sdi->mode == DSO) ? dso_buffer_size :
           (devc->op_mode == SR_OP_STREAM) ? get_buffer_size(devc) : instant_buffer_size;

    devc->submitted_transfers = 0;

    devc->transfers = g_try_malloc0(sizeof(*devc->transfers) * num_transfers);
    if (!devc->transfers) {
        sr_err("%s: USB transfers malloc failed.", __func__);
        return SR_ERR_MALLOC;
    }

    for (i = 0; i < num_transfers; i++) {
        if (!(buf = g_try_malloc(size))) {
            sr_err("%s: USB transfer buffer malloc failed.", __func__);
            return SR_ERR_MALLOC;
        }
        transfer = libusb_alloc_transfer(0);
        libusb_fill_bulk_transfer(transfer, usb->devhdl,
                6 | LIBUSB_ENDPOINT_IN, buf, size,
                receive_transfer, devc, 0);
        if ((ret = libusb_submit_transfer(transfer)) != 0) {
            sr_err("%s: Failed to submit transfer: %s.",
                   __func__, libusb_error_name(ret));
            libusb_free_transfer(transfer);
            g_free(buf);
            devc->status = DSL_ERROR;
            devc->abort = TRUE;
            return SR_ERR;
        }
        devc->transfers[i] = transfer;
        devc->submitted_transfers++;
        devc->num_transfers++;
    }

    devc->status = DSL_DATA;

    return SR_OK;
}


static int receive_data(int fd, int revents, const struct sr_dev_inst *sdi)
{
    int completed = 0;
    struct timeval tv;
    struct drv_context *drvc;
    struct DSL_context *devc;
    struct sr_usb_dev_inst *usb;
    int i;
    int ret;

    (void)fd;
    (void)revents;

    drvc = di->priv;
    devc = sdi->priv;
    usb = sdi->conn;

    tv.tv_sec = tv.tv_usec = 0;
    libusb_handle_events_timeout_completed(drvc->sr_ctx->libusb_ctx, &tv, &completed);

    if (devc->status == DSL_FINISH) {
        if (libusb_try_lock_events(drvc->sr_ctx->libusb_ctx) == 0) {
            if (libusb_event_handling_ok(drvc->sr_ctx->libusb_ctx)) {
                /* Stop GPIF acquisition */
                usb = ((struct sr_dev_inst *)devc->cb_data)->conn;
                if ((ret = command_stop_acquisition (usb->devhdl)) != SR_OK)
                    sr_err("%s: Sent acquisition stop command failed!", __func__);
                else
                    sr_info("%s: Sent acquisition stop command!", __func__);

                remove_sources(devc);
            }
            libusb_unlock_events(drvc->sr_ctx->libusb_ctx);
        }
    }

    return TRUE;
}

static void receive_trigger_pos(struct libusb_transfer *transfer)
{
    struct DSL_context *devc;
    struct sr_datafeed_packet packet;
    struct ds_trigger_pos *trigger_pos;
    const struct sr_dev_inst *sdi;
    int ret;

    devc = transfer->user_data;
	sdi = devc->cb_data;
    trigger_pos = (struct ds_trigger_pos *)transfer->buffer;
    devc->status = DSL_ERROR;
    if (transfer->status == LIBUSB_TRANSFER_COMPLETED &&
        trigger_pos->check_id == TRIG_CHECKID) {
        sr_info("receive_trigger_pos(): status %d; timeout %d; received %d bytes.",
            transfer->status, transfer->timeout, transfer->actual_length);
        if (transfer->actual_length == sizeof(struct ds_trigger_pos)) {
            if (sdi->mode != LOGIC || devc->stream || trigger_pos->remain_cnt < devc->limit_samples) {
                if (sdi->mode == LOGIC && !devc->stream)
                    devc->actual_samples = (devc->limit_samples - ceil(devc->cur_samplerate * 1.0 / DSLOGIC_MAX_LOGIC_SAMPLERATE) * (trigger_pos->remain_cnt));
                packet.type = SR_DF_TRIGGER;
                packet.payload = trigger_pos;
                sr_session_send(sdi, &packet);

                devc->status = DSL_TRIGGERED;
                devc->num_transfers = 0;
                devc->empty_transfer_count = 0;
            }
        }
    }

    if (devc->status == DSL_TRIGGERED) {
        // successfull
        free_transfer(transfer);
        if ((ret = dev_transfer_start(devc->cb_data)) != SR_OK) {
            sr_err("%s: could not start data transfer"
                   "(%d)%d", __func__, ret, errno);
        }
    } else {
        // failed
        free_transfer(transfer);
    }
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi, void *cb_data)
{
    struct DSL_context *devc;
    struct drv_context *drvc;
    struct sr_usb_dev_inst *usb;
    struct libusb_transfer *transfer;
    struct ds_trigger_pos *trigger_pos;
    const struct libusb_pollfd **lupfd;
	unsigned int i;
    int ret;
    int transferred;
    struct sr_datafeed_packet packet;
    int header_transferred;

    test_init = 1;

    if (sdi->status != SR_ST_ACTIVE)
        return SR_ERR_DEV_CLOSED;

    drvc = di->priv;
    devc = sdi->priv;
    usb = sdi->conn;

    //devc->cb_data = cb_data;
    devc->cb_data = sdi;
    devc->num_samples = 0;
    devc->empty_transfer_count = 0;
    devc->status = DSL_INIT;
    devc->num_transfers = 0;
    devc->submitted_transfers = 0;
    devc->actual_samples = devc->limit_samples;
	test_sample_value = 0;
	devc->abort = FALSE;

	/* Configures devc->trigger_* and devc->sample_wide */
    if (configure_probes(sdi) != SR_OK) {
        sr_err("%s: Failed to configure probes.", __func__);
        return SR_ERR;
	}

    /* Stop Previous GPIF acquisition */
    if ((ret = command_stop_acquisition (usb->devhdl)) != SR_OK) {
        sr_err("%s: Stop DSLogic acquisition failed!", __func__);
        return ret;
    } else {
        sr_info("%s: Stop Previous DSLogic acquisition!", __func__);
    }

    /* Setting FPGA before acquisition start*/
    if ((ret = command_fpga_setting(usb->devhdl, sizeof(struct DSL_setting) / sizeof(uint16_t))) != SR_OK) {
        sr_err("%s: Send FPGA setting command failed!", __func__);
    } else {
        if ((ret = fpga_setting(sdi)) != SR_OK) {
            sr_err("%s: Configure FPGA failed!", __func__);
            return ret;
        }
    }

    /*
     * settings must be updated before acquisition
     */
    if (sdi->mode == DSO) {
        devc->trigger_hpos =  devc->trigger_hrate * en_ch_num(sdi) * devc->limit_samples / 200.0;
        if ((ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, 1, SR_CONF_HORIZ_TRIGGERPOS))) == SR_OK)
            sr_dbg("%s: setting DSO Horiz Trigger Position to %d",
                __func__, devc->trigger_hpos);
        else
            sr_dbg("%s: setting DSO Horiz Trigger Position to %d failed",
                __func__, devc->trigger_hpos);
    }

    /* poll trigger status transfer*/
    if (!(trigger_pos = g_try_malloc0(sizeof(struct ds_trigger_pos)))) {
        sr_err("%s: USB trigger_pos buffer malloc failed.", __func__);
        return SR_ERR_MALLOC;
    }
    devc->transfers = g_try_malloc0(sizeof(*devc->transfers));
    if (!devc->transfers) {
        sr_err("%s: USB trigger_pos transfer malloc failed.", __func__);
        return SR_ERR_MALLOC;
	}
    transfer = libusb_alloc_transfer(0);
    libusb_fill_bulk_transfer(transfer, usb->devhdl,
            6 | LIBUSB_ENDPOINT_IN, trigger_pos, sizeof(struct ds_trigger_pos),
            receive_trigger_pos, devc, 0);
    if ((ret = libusb_submit_transfer(transfer)) != 0) {
        sr_err("%s: Failed to submit trigger_pos transfer: %s.",
               __func__, libusb_error_name(ret));
        libusb_free_transfer(transfer);
        g_free(trigger_pos);
        return SR_ERR;
    } else {
        devc->num_transfers++;
        devc->transfers[0] = transfer;
        devc->submitted_transfers++;
    }

    /* setup callback function for data transfer */
    lupfd = libusb_get_pollfds(drvc->sr_ctx->libusb_ctx);
    for (i = 0; lupfd[i]; i++);
    if (!(devc->usbfd = g_try_malloc(sizeof(struct libusb_pollfd) * (i + 1))))
    return SR_ERR;
    for (i = 0; lupfd[i]; i++) {
        sr_source_add(lupfd[i]->fd, lupfd[i]->events,
                  get_timeout(devc), receive_data, sdi);
        devc->usbfd[i] = lupfd[i]->fd;
    }
    devc->usbfd[i] = -1;
    free(lupfd);

    devc->status = DSL_START;
    devc->mstatus_valid = FALSE;
    if ((ret = command_start_acquisition (usb->devhdl,
        devc->cur_samplerate, devc->sample_wide, (sdi->mode == LOGIC))) != SR_OK) {
        devc->status = DSL_ERROR;
        devc->abort = TRUE;
        return ret;
    }

	/* Send header packet to the session bus. */
    //std_session_send_df_header(cb_data, LOG_PREFIX);
    std_session_send_df_header(sdi, LOG_PREFIX);

    return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi, void *cb_data)
{
	(void)cb_data;

    struct DSL_context *devc;
    struct sr_usb_dev_inst *usb;

    devc = sdi->priv;
    usb = sdi->conn;

    if (!devc->abort) {
        devc->abort = TRUE;
        command_wr_reg(usb->devhdl, 3, EEWP_ADDR);
    }

    return SR_OK;
}

static int dev_test(struct sr_dev_inst *sdi)
{
    if (sdi) {
        struct sr_usb_dev_inst *usb;
        struct version_info vi;
        int ret = SR_ERR;

        usb = sdi->conn;
        ret = command_get_fw_version(usb->devhdl, &vi);
        if (ret != SR_OK)
            sr_err("Device don't exist!");

        return ret;
    } else {
        return SR_ERR;
    }
}

static int dev_status_get(struct sr_dev_inst *sdi, struct sr_status *status, int begin, int end)
{
	int ret = SR_ERR;
    if (sdi) {
        struct DSL_context *devc;
        struct sr_usb_dev_inst *usb;

        devc = sdi->priv;
        usb = sdi->conn;
		if (devc->status == DSL_START) {
			ret = command_get_status(usb->devhdl, (unsigned char*)status, begin, end);
        } else if (devc->mstatus_valid) {
            *status = mstatus;
            ret = SR_OK;
        }
    }

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
    .dev_test = dev_test,
    .dev_status_get = dev_status_get,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.priv = NULL,
};
