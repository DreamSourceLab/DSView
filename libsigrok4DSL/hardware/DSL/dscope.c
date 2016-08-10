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
    {"OSC", DSO},
};

static const char *opmodes[] = {
    "Normal",
    "Internal Test",
    "External Test",
    "DRAM Loopback Test",
};

static uint16_t opmodes_show_count = 2;

static const char *thresholds[] = {
    "1.8/2.5/3.3V Level",
    "5.0V Level",
};

static const char *filters[] = {
    "None",
    "1 Sample Clock",
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
};

static const int32_t sessions[] = {
    SR_CONF_SAMPLERATE,
    SR_CONF_LIMIT_SAMPLES,
    SR_CONF_OPERATION_MODE,
    SR_CONF_TIMEBASE,
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
};

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
};

static const uint8_t zero_base_addr = 0x40;

SR_PRIV struct sr_dev_driver DSCope_driver_info;
static struct sr_dev_driver *di = &DSCope_driver_info;

extern struct ds_trigger *trigger;

gboolean mstatus_valid = FALSE;
struct sr_status mstatus;

static const uint64_t DSCOPE_DEFAULT_VGAIN[] = {
    DSCOPE_DEFAULT_VGAIN0,
    DSCOPE_DEFAULT_VGAIN1,
    DSCOPE_DEFAULT_VGAIN2,
    DSCOPE_DEFAULT_VGAIN3,
    DSCOPE_DEFAULT_VGAIN4,
    DSCOPE_DEFAULT_VGAIN5,
    DSCOPE_DEFAULT_VGAIN6,
    DSCOPE_DEFAULT_VGAIN7,
};

static const uint64_t DSCOPE20_DEFAULT_VGAIN[] = {
    DSCOPE20_DEFAULT_VGAIN0,
    DSCOPE20_DEFAULT_VGAIN1,
    DSCOPE20_DEFAULT_VGAIN2,
    DSCOPE20_DEFAULT_VGAIN3,
    DSCOPE20_DEFAULT_VGAIN4,
    DSCOPE20_DEFAULT_VGAIN5,
    DSCOPE20_DEFAULT_VGAIN6,
    DSCOPE20_DEFAULT_VGAIN7,
};

struct DSL_vga DSCope_vga[] = {
    {10,  DSCOPE_DEFAULT_VGAIN0, DSCOPE_DEFAULT_VGAIN0, DSCOPE_DEFAULT_VOFF, DSCOPE_DEFAULT_VOFF},
    {20,  DSCOPE_DEFAULT_VGAIN1, DSCOPE_DEFAULT_VGAIN1, DSCOPE_DEFAULT_VOFF, DSCOPE_DEFAULT_VOFF},
    {50,  DSCOPE_DEFAULT_VGAIN2, DSCOPE_DEFAULT_VGAIN2, DSCOPE_DEFAULT_VOFF, DSCOPE_DEFAULT_VOFF},
    {100, DSCOPE_DEFAULT_VGAIN3, DSCOPE_DEFAULT_VGAIN3, DSCOPE_DEFAULT_VOFF, DSCOPE_DEFAULT_VOFF},
    {200, DSCOPE_DEFAULT_VGAIN4, DSCOPE_DEFAULT_VGAIN4, DSCOPE_DEFAULT_VOFF, DSCOPE_DEFAULT_VOFF},
    {500, DSCOPE_DEFAULT_VGAIN5, DSCOPE_DEFAULT_VGAIN5, DSCOPE_DEFAULT_VOFF, DSCOPE_DEFAULT_VOFF},
    {1000,DSCOPE_DEFAULT_VGAIN6, DSCOPE_DEFAULT_VGAIN6, DSCOPE_DEFAULT_VOFF, DSCOPE_DEFAULT_VOFF},
    {2000,DSCOPE_DEFAULT_VGAIN7, DSCOPE_DEFAULT_VGAIN7, DSCOPE_DEFAULT_VOFF, DSCOPE_DEFAULT_VOFF},
    {0, 0, 0, 0},
};
struct DSL_vga DSCope20_vga[] = {
    {10,  DSCOPE20_DEFAULT_VGAIN0, DSCOPE20_DEFAULT_VGAIN0, DSCOPE20_DEFAULT_VOFF, CALI_VOFF_RANGE-DSCOPE20_DEFAULT_VOFF},
    {20,  DSCOPE20_DEFAULT_VGAIN1, DSCOPE20_DEFAULT_VGAIN1, DSCOPE20_DEFAULT_VOFF, CALI_VOFF_RANGE-DSCOPE20_DEFAULT_VOFF},
    {50,  DSCOPE20_DEFAULT_VGAIN2, DSCOPE20_DEFAULT_VGAIN2, DSCOPE20_DEFAULT_VOFF, CALI_VOFF_RANGE-DSCOPE20_DEFAULT_VOFF},
    {100, DSCOPE20_DEFAULT_VGAIN3, DSCOPE20_DEFAULT_VGAIN3, DSCOPE20_DEFAULT_VOFF, CALI_VOFF_RANGE-DSCOPE20_DEFAULT_VOFF},
    {200, DSCOPE20_DEFAULT_VGAIN4, DSCOPE20_DEFAULT_VGAIN4, DSCOPE20_DEFAULT_VOFF, CALI_VOFF_RANGE-DSCOPE20_DEFAULT_VOFF},
    {500, DSCOPE20_DEFAULT_VGAIN5, DSCOPE20_DEFAULT_VGAIN5, DSCOPE20_DEFAULT_VOFF, CALI_VOFF_RANGE-DSCOPE20_DEFAULT_VOFF},
    {1000,DSCOPE20_DEFAULT_VGAIN6, DSCOPE20_DEFAULT_VGAIN6, DSCOPE20_DEFAULT_VOFF, CALI_VOFF_RANGE-DSCOPE20_DEFAULT_VOFF},
    {2000,DSCOPE20_DEFAULT_VGAIN7, DSCOPE20_DEFAULT_VGAIN7, DSCOPE20_DEFAULT_VOFF, CALI_VOFF_RANGE-DSCOPE20_DEFAULT_VOFF},
    {0, 0, 0, 0},
};

/**
 * Check the USB configuration to determine if this is an DSCope device.
 *
 * @return TRUE if the device's configuration profile match DSCope
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

        /* If we made it here, it must be an DSCope. */
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

    //setting.mode = (test_mode ? 0x8000 : 0x0000) + trigger->trigger_en + (sdi->mode << 4);
    setting.mode = ((devc->op_mode == SR_OP_INTERNAL_TEST) << 15) +
                   ((devc->op_mode == SR_OP_EXTERNAL_TEST) << 14) +
                   ((devc->op_mode == SR_OP_LOOPBACK_TEST) << 13) +
                   trigger->trigger_en +
                   ((sdi->mode == DSO) << 4) + (devc->clock_type << 1) + (devc->clock_edge << 2) +
                   (((devc->cur_samplerate == SR_MHZ(200) && sdi->mode != DSO) || (sdi->mode == ANALOG)) << 5) +
                   ((devc->cur_samplerate == SR_MHZ(400)) << 6) +
                   ((sdi->mode == ANALOG) << 7) +
                   ((devc->filter == SR_FILTER_1T) << 8) +
                   (devc->instant << 9);
    setting.divider = (uint32_t)ceil(DSCOPE_MAX_SAMPLERATE * 1.0 / devc->cur_samplerate / en_ch_num(sdi));
    setting.count = (uint32_t)(devc->limit_samples / (g_slist_length(sdi->channels) / en_ch_num(sdi)));
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
            setting.trig_mask0[i] = 0xff;
            setting.trig_mask1[i] = 0xff;

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
        sr_err("Unable to setting FPGA of DSCope: %s.",
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
            sr_err("Unable to configure FPGA of DSCope: %s.",
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

static int DSCope_dev_open(struct sr_dev_inst *sdi)
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

        if ((probe->index > 7 && probe->type == SR_CHANNEL_LOGIC) ||
            (probe->type == SR_CHANNEL_ANALOG || probe->type == SR_CHANNEL_DSO))
			devc->sample_wide = TRUE;
        else
            devc->sample_wide = FALSE;

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

static struct DSL_context *DSCope_dev_new(void)
{
    struct DSL_context *devc;

    if (!(devc = g_try_malloc(sizeof(struct DSL_context)))) {
        sr_err("Device context malloc failed.");
		return NULL;
	}

    devc->profile = NULL;
    devc->fw_updated = 0;
    devc->cur_samplerate = DSCOPE_MAX_SAMPLERATE / MAX_DSO_PROBES_NUM;
    devc->limit_samples = DSCOPE_MAX_DEPTH / MAX_DSO_PROBES_NUM;
    devc->sample_wide = 0;
    devc->clock_type = FALSE;
    devc->clock_edge = FALSE;
    devc->instant = FALSE;
    devc->op_mode = SR_OP_BUFFER;
    devc->th_level = SR_TH_3V3;
    devc->filter = SR_FILTER_NONE;
    devc->timebase = 10000;
    devc->trigger_slope = DSO_TRIGGER_RISING;
    devc->trigger_source = DSO_TRIGGER_AUTO;
    devc->trigger_holdoff = 0;
    devc->trigger_hpos = 0x0;
    devc->trigger_hrate = 0;
    devc->zero = FALSE;
    devc->data_lock = FALSE;
    devc->cali = FALSE;
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


static struct DSL_vga* get_vga_ptr(struct sr_dev_inst *sdi)
{
    struct DSL_vga *vga_ptr = NULL;
    if (strcmp(sdi->model, "DSCope") == 0)
        vga_ptr = DSCope_vga;
    else if (strcmp(sdi->model, "DSCope20") == 0)
        vga_ptr = DSCope20_vga;

    return vga_ptr;
}

static uint16_t get_default_trans(struct sr_dev_inst *sdi)
{
    uint16_t trans = 1;
    if (strcmp(sdi->model, "DSCope") == 0)
        trans = DSCOPE_DEFAULT_TRANS;
    else if (strcmp(sdi->model, "DSCope20") == 0)
        trans = DSCOPE20_DEFAULT_TRANS;

    return trans;
}

static uint16_t get_default_voff(struct sr_dev_inst *sdi, int ch_index)
{
    uint16_t voff = 0;
    if (strcmp(sdi->model, "DSCope") == 0)
        voff = DSCOPE_DEFAULT_VOFF;
    else if (strcmp(sdi->model, "DSCope20") == 0)
        if (ch_index == 1)
            voff = CALI_VOFF_RANGE - DSCOPE20_DEFAULT_VOFF;
        else
            voff = DSCOPE20_DEFAULT_VOFF;

    return voff;
}

static uint64_t get_default_vgain(struct sr_dev_inst *sdi, int num)
{
    uint64_t vgain = 0;
    if (strcmp(sdi->model, "DSCope") == 0) {
        assert(num < sizeof(DSCOPE_DEFAULT_VGAIN));
        vgain = DSCOPE_DEFAULT_VGAIN[num];
    }
    else if (strcmp(sdi->model, "DSCope20") == 0) {
        assert(num < sizeof(DSCOPE20_DEFAULT_VGAIN));
        vgain = DSCOPE20_DEFAULT_VGAIN[num];
    }

    return vgain;
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
            probe->vpos_trans = get_default_trans(sdi);
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

    /* Find all DSCope compatible devices and upload firmware to them. */
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
        for (j = 0; supported_DSCope[j].vid; j++) {
            if (des.idVendor == supported_DSCope[j].vid &&
                des.idProduct == supported_DSCope[j].pid) {
                prof = &supported_DSCope[j];
			}
		}

		/* Skip if the device was not found. */
		if (!prof)
			continue;

		devcnt = g_slist_length(drvc->instances);
        sdi = sr_dev_inst_new(DSO, devcnt, SR_ST_INITIALIZING,
			prof->vendor, prof->model, prof->model_version);
		if (!sdi)
			return NULL;
		sdi->driver = di;

		/* Fill in probelist according to this device's profile. */
        if (set_probes(sdi, 2) != SR_OK)
            return NULL;

        devc = DSCope_dev_new();
        devc->profile = prof;
        sdi->priv = devc;
        drvc->instances = g_slist_append(drvc->instances, sdi);
        //devices = g_slist_append(devices, sdi);

		if (check_conf_profile(devlist[i])) {
			/* Already has the firmware, so fix the new address. */
            sr_dbg("Found an DSCope device.");
            sdi->status = SR_ST_INACTIVE;
            sdi->inst_type = SR_INST_USB;
            sdi->conn = sr_usb_dev_inst_new(libusb_get_bus_number(devlist[i]),
					libusb_get_device_address(devlist[i]), NULL);
            /* only report device after firmware is ready */
            devices = g_slist_append(devices, sdi);
		} else {
            char *firmware = malloc(strlen(DS_RES_PATH)+strlen(prof->firmware)+1);
            if (firmware == NULL)
                return NULL;
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
    (void)sdi;
    GSList *l = NULL;
    int i;

    for(i = 0; i < ARRAY_SIZE(mode_list); i++) {
        l = g_slist_append(l, &mode_list[i]);
    }

    return l;
}

static uint64_t dso_vga(struct sr_dev_inst *sdi, struct sr_channel* ch)
{
    int i;
    struct DSL_vga *vga_ptr = get_vga_ptr(sdi);
    for (i = 0; vga_ptr && (vga_ptr+i)->key; i++) {
        if ((vga_ptr+i)->key == ch->vdiv)
            return (ch->index == 0) ? (vga_ptr+i)->vgain0 : (vga_ptr+i)->vgain1;
    }

    return 0;
}

static uint64_t dso_voff(struct sr_dev_inst *sdi, struct sr_channel* ch)
{
    int i;
    struct DSL_vga *vga_ptr =  get_vga_ptr(sdi);
    for (i = 0; vga_ptr && (vga_ptr+i)->key; i++) {
        if ((vga_ptr+i)->key == ch->vdiv)
            return (ch->index == 0) ? (vga_ptr+i)->voff0 : (vga_ptr+i)->voff1;
    }
    return 0;
}

static uint64_t dso_vpos(struct sr_dev_inst *sdi, struct sr_channel* ch)
{
    uint64_t vpos;
    int vpos_coarse, vpos_fine;
    int trans_coarse, trans_fine;
    struct DSL_context *devc = sdi->priv;
    const double voltage = (devc->zero && devc->zero_comb == -1) ? 0 : ch->vpos;
    if (strcmp(sdi->model, "DSCope") == 0) {
        trans_coarse = (ch->vpos_trans & 0xFF00) >> 8;
        trans_fine = (ch->vpos_trans & 0x00FF);
        if (ch->vdiv < 500) {
            vpos_coarse = floor(-voltage*DSCOPE_TRANS_CMULTI/trans_coarse + 0.5);
            vpos_fine = floor((voltage + vpos_coarse*trans_coarse/DSCOPE_TRANS_CMULTI)*1000.0/trans_fine + 0.5);
        } else {
            vpos_coarse = floor(-voltage/trans_coarse + 0.5);
            vpos_fine = floor((voltage + vpos_coarse*trans_coarse)*DSCOPE_TRANS_FMULTI/trans_fine + 0.5);
        }
        //vpos = (vpos_coarse << 16) + vpos_fine;
    } else if (strcmp(sdi->model, "DSCope20") == 0) {
        vpos = ((ch->vdiv*5.0) - voltage)/(ch->vdiv*10.0)*ch->vpos_trans;
    }

    const uint64_t voff = dso_voff(sdi, ch);
    if (strcmp(sdi->model, "DSCope") == 0)
        return ((vpos_coarse+DSCOPE_CONSTANT_BIAS+(voff>>10)) << 16)+vpos_fine+(voff&0x03ff);
    else if (strcmp(sdi->model, "DSCope20") == 0)
        return vpos+voff;
    else
        return 0;
}

static uint64_t dso_cmd_gen(struct sr_dev_inst *sdi, struct sr_channel* ch, int id)
{
    struct DSL_context *devc;
    uint64_t cmd = 0;
    uint64_t vpos;
    GSList *l;
    const int ch_bit = 7;
    devc = sdi->priv;

    switch (id) {
    case SR_CONF_EN_CH:
    case SR_CONF_COUPLING:
        if (devc->zero || en_ch_num(sdi) == 2) {
            cmd += 0x0E00;
            //cmd += 0x000;
        } else if (en_ch_num(sdi) == 1) {
            if (((ch->index == 0) && ch->enabled) || ((ch->index == 1) && !ch->enabled))
                cmd += 0x1600;
            else if (((ch->index == 1) && ch->enabled) || ((ch->index == 0) && !ch->enabled))
                cmd += 0x1A00;
        } else {
            return 0x0;
        }

        cmd += ch->index << ch_bit;
        if (devc->zero || ch->coupling == SR_DC_COUPLING)
            cmd += 0x100;
        else if (ch->coupling == SR_GND_COUPLING)
            cmd &= 0xFFFFFDFF;
        break;
    case SR_CONF_VDIV:
    case SR_CONF_TIMEBASE:
        cmd += 0x8;
        cmd += ch->index << ch_bit;
        //  --VGAIN
        cmd += dso_vga(sdi, ch);
        break;
    case SR_CONF_VPOS:
        cmd += 0x10;
        cmd += ch->index << ch_bit;
        vpos = dso_vpos(sdi, ch);
        cmd += (vpos << 8);
        break;
    case SR_CONF_SAMPLERATE:
        cmd += 0x18;
        uint32_t divider = devc->zero ? 0x1 : (uint32_t)ceil(DSCOPE_MAX_SAMPLERATE * 1.0 / devc->cur_samplerate / en_ch_num(sdi));
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
        cmd += devc->zero ? 0x0 : devc->trigger_source << 8;
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
        cmd += ((uint64_t)devc->trigger_holdoff << 8);
        break;
    case SR_CONF_DSO_SYNC:
        cmd = 0xa5a5a500;
        break;
    default:
        cmd = 0xFFFFFFFF;
    }

    return cmd;
}

static gboolean dso_load_eep(struct sr_dev_inst *sdi, struct sr_channel *probe, gboolean fpga_done)
{
    int ret, i;
    struct sr_usb_dev_inst *usb = sdi->conn;
    struct cmd_zero_info zero_info;
    uint8_t dst_addr = (zero_base_addr +
                        probe->index * (sizeof(struct cmd_zero_info) + sizeof(struct cmd_vga_info)));
    zero_info.zero_addr = dst_addr;
    if ((ret = command_rd_nvm(usb->devhdl, (unsigned char *)&zero_info, zero_info.zero_addr, sizeof(struct cmd_zero_info))) != SR_OK) {
        return FALSE;
        sr_err("%s: Send Get Zero command failed!", __func__);
    } else {
        if (zero_info.zero_addr == dst_addr) {
            uint8_t* voff_ptr = &zero_info.zero_addr + 1;
            struct DSL_vga *vga_ptr =  get_vga_ptr(sdi);
             for (i = 0; vga_ptr && (vga_ptr+i)->key; i++) {
                 if (probe->index == 0)
                     (vga_ptr+i)->voff0 = (*(voff_ptr + 2*i+1) << 8) + *(voff_ptr + 2*i);
                 else
                     (vga_ptr+i)->voff1 = (*(voff_ptr + 2*i+1) << 8) + *(voff_ptr + 2*i);
             }
             if (i != 0) {
                 probe->comb_diff_top = *(voff_ptr + 2*i);
                 probe->comb_diff_bom = *(voff_ptr + 2*i + 1);
                 probe->vpos_trans = *(voff_ptr + 2*i + 2) + (*(voff_ptr + 2*i + 3) << 8);
                 if (!fpga_done) {
                     const double slope = (probe->comb_diff_bom - probe->comb_diff_top)/(2.0*255.0);
                     for (i = 0; i < 256; i++) {
                         ret = command_wr_reg(usb->devhdl, i, COMB_ADDR + probe->index*2);
                         int value = i+i*slope+probe->comb_diff_top*0.5+0.5;
                         value = (value < 0) ? 0 :
                                 (value > 255) ? 255 : value;
                         ret = command_wr_reg(usb->devhdl, value, COMB_ADDR + probe->index*2 + 1);
                     }
                 }
             }
        } else {
            return FALSE;
        }
    }

    struct cmd_vga_info vga_info;
    vga_info.vga_addr = dst_addr + sizeof(struct cmd_zero_info);
    if ((ret = command_rd_nvm(usb->devhdl, (unsigned char *)&vga_info, vga_info.vga_addr, sizeof(struct cmd_vga_info))) != SR_OK) {
        return FALSE;
        sr_err("%s: Send Get Zero command failed!", __func__);
    } else {
        if (vga_info.vga_addr == dst_addr + sizeof(struct cmd_zero_info)) {
            uint16_t* vgain_ptr = &vga_info.vga0;
            struct DSL_vga *vga_ptr =  get_vga_ptr(sdi);
             for (i = 0; vga_ptr && (vga_ptr+i)->key; i++) {
                 if (probe->index == 0)
                     (vga_ptr+i)->vgain0 = *(vgain_ptr + i) << 8;
                 else
                     (vga_ptr+i)->vgain1 = *(vgain_ptr + i) << 8;
             }
        } else {
            return FALSE;
        }
    }

    return TRUE;
}

static int config_get(int id, GVariant **data, const struct sr_dev_inst *sdi,
                      const struct sr_channel *ch,
                      const struct sr_channel_group *cg)
{
    struct DSL_context *devc;
    struct sr_usb_dev_inst *usb;
    char str[128];
    int i;
    struct DSL_vga *vga_ptr;

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
    case SR_CONF_FILTER:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_string(filters[devc->filter]);
        break;
    case SR_CONF_THRESHOLD:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_string(thresholds[devc->th_level]);
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
        *data = g_variant_new_boolean(devc->zero);
        break;
    case SR_CONF_CALI:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_boolean(devc->cali);
        break;
    case SR_CONF_ROLL:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_boolean(devc->roll);
        break;
    case SR_CONF_TEST:
        if (!sdi)
            return SR_ERR;
        *data = g_variant_new_boolean(FALSE);
        break;
    case SR_CONF_MAX_DSO_SAMPLERATE:
        if (!sdi)
            return SR_ERR;
        *data = g_variant_new_uint64(DSCOPE_MAX_SAMPLERATE);
        break;
    case SR_CONF_MAX_DSO_SAMPLELIMITS:
        if (!sdi)
            return SR_ERR;
        *data = g_variant_new_uint64(DSCOPE_MAX_DEPTH);
        break;
    case SR_CONF_MAX_LOGIC_SAMPLERATE:
        if (!sdi)
            return SR_ERR;
        *data = g_variant_new_uint64(DSCOPE_MAX_SAMPLERATE);
        break;
    case SR_CONF_MAX_LOGIC_SAMPLELIMITS:
        if (!sdi)
            return SR_ERR;
        *data = g_variant_new_uint64(DSCOPE_MAX_DEPTH);
        break;
    case SR_CONF_RLE_SAMPLELIMITS:
        if (!sdi)
            return SR_ERR;
        *data = g_variant_new_uint64(DSCOPE_MAX_DEPTH);
        break;
    case SR_CONF_VGAIN:
        if (!sdi || !ch)
            return SR_ERR;
        *data = g_variant_new_uint64(dso_vga(sdi, ch)>>8);
        break;
    case SR_CONF_VGAIN_DEFAULT:
        if (!sdi || !ch)
            return SR_ERR;
        vga_ptr =  get_vga_ptr(sdi);
        for (i = 0; vga_ptr && (vga_ptr+i)->key; i++) {
            if ((vga_ptr+i)->key == ch->vdiv)
                break;
        }
        *data = g_variant_new_uint64(get_default_vgain(sdi, i)>>8);
        break;
    case SR_CONF_VGAIN_RANGE:
        if (!sdi)
            return SR_ERR;
        vga_ptr =  get_vga_ptr(sdi);
        for (i = 0; vga_ptr && (vga_ptr+i)->key; i++) {
            if ((vga_ptr+i)->key == ch->vdiv)
                break;
        }
        uint16_t vgain_default= (get_default_vgain(sdi, i)>>8) & 0x0FFF;
        *data = g_variant_new_uint16(min(CALI_VGAIN_RANGE, vgain_default*2));
        break;
    case SR_CONF_VOFF:
        if (!sdi || !ch)
            return SR_ERR;
        uint16_t voff = dso_voff(sdi, ch);
        uint16_t voff_default = get_default_voff(sdi, ch->index);
        if (strcmp(sdi->model, "DSCope") == 0) {
            int voff_skew_coarse = (voff >> 10) - (voff_default >> 10);
            int voff_skew_fine = (voff & 0x03ff) - (voff_default & 0x03ff);
            double trans_coarse = (ch->vdiv < 500) ? (ch->vpos_trans >> 8)/DSCOPE_TRANS_CMULTI : (ch->vpos_trans >> 8);
            double trans_fine = (ch->vdiv < 500) ? (ch->vpos_trans & 0x00ff) / 1000.0 : (ch->vpos_trans & 0x00ff) / DSCOPE_TRANS_FMULTI;
            double voff_rate = (voff_skew_coarse*trans_coarse - voff_skew_fine*trans_fine) / ch->vdiv;
            voff = (voff_rate * 0.5 + 0.5) * CALI_VOFF_RANGE;
        }
        *data = g_variant_new_uint16(voff);
        break;
    case SR_CONF_VOFF_DEFAULT:
        if (!sdi || !ch)
            return SR_ERR;
        *data = g_variant_new_uint16(get_default_voff(sdi, ch->index));
        break;
    case SR_CONF_VOFF_RANGE:
        if (!sdi)
            return SR_ERR;
        *data = g_variant_new_uint16(CALI_VOFF_RANGE);
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
        if (sdi->mode == LOGIC) {
            if (devc->cur_samplerate >= SR_MHZ(200)) {
                adjust_probes(sdi, SR_MHZ(1600)/devc->cur_samplerate);
            } else {
                adjust_probes(sdi, 16);
            }
        } else if(sdi->mode == DSO) {
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, 0, SR_CONF_SAMPLERATE));
        }
    } else if (id == SR_CONF_CLOCK_TYPE) {
        devc->clock_type = g_variant_get_boolean(data);
    } else if (id == SR_CONF_CLOCK_EDGE) {
        devc->clock_edge = g_variant_get_boolean(data);
    } else if (id == SR_CONF_INSTANT) {
        devc->instant = g_variant_get_boolean(data);
        if (en_ch_num(sdi) != 0) {
            if (devc->instant)
                devc->limit_samples = DSCOPE_INSTANT_DEPTH / en_ch_num(sdi);
            else
                devc->limit_samples = DSCOPE_MAX_DEPTH / en_ch_num(sdi);
        }
    } else if (id == SR_CONF_LIMIT_SAMPLES) {
        devc->limit_samples = g_variant_get_uint64(data);
    } else if (id == SR_CONF_DEVICE_MODE) {
        sdi->mode = g_variant_get_int16(data);
        if (sdi->mode == LOGIC) {
            num_probes = devc->profile->dev_caps & DEV_CAPS_16BIT ? 16 : 8;
        } else if (sdi->mode == DSO) {
            sdi->mode = DSO;
            num_probes = devc->profile->dev_caps & DEV_CAPS_16BIT ? MAX_DSO_PROBES_NUM : 1;
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, NULL, SR_CONF_DSO_SYNC));
            if (ret != SR_OK)
                sr_dbg("%s: DSO configuration sync failed", __func__);
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, sdi->channels->data, SR_CONF_VDIV));
            if (ret == SR_OK)
                sr_dbg("%s: Initial setting for DSO mode", __func__);
            else
                sr_dbg("%s: Initial setting for DSO mode failed", __func__);
            devc->cur_samplerate = DSCOPE_MAX_SAMPLERATE / num_probes;
            devc->limit_samples = DSCOPE_MAX_DEPTH / num_probes;
        } else if (sdi->mode == ANALOG){
            num_probes = devc->profile->dev_caps & DEV_CAPS_16BIT ? MAX_ANALOG_PROBES_NUM : 1;
        }
        sr_dev_probes_free(sdi);
        set_probes(sdi, num_probes);
        sr_dbg("%s: setting mode to %d", __func__, sdi->mode);
    } else if (id == SR_CONF_OPERATION_MODE) {
        stropt = g_variant_get_string(data, NULL);
        if (!strcmp(stropt, opmodes[SR_OP_BUFFER])) {
            devc->op_mode = SR_OP_BUFFER;
        } else if (!strcmp(stropt, opmodes[SR_OP_INTERNAL_TEST])) {
            devc->op_mode = SR_OP_INTERNAL_TEST;
        } else if (!strcmp(stropt, opmodes[SR_OP_EXTERNAL_TEST])) {
            devc->op_mode = SR_OP_EXTERNAL_TEST;
        } else if (!strcmp(stropt, opmodes[SR_OP_LOOPBACK_TEST])) {
            devc->op_mode = SR_OP_LOOPBACK_TEST;
        } else {
            ret = SR_ERR;
        }
        sr_dbg("%s: setting pattern to %d",
            __func__, devc->op_mode);
    } else if (id == SR_CONF_THRESHOLD) {
        stropt = g_variant_get_string(data, NULL);
        if (!strcmp(stropt, thresholds[SR_TH_3V3])) {
            devc->th_level = SR_TH_3V3;
        } else if (!strcmp(stropt, thresholds[SR_TH_5V0])) {
            devc->th_level = SR_TH_5V0;
        } else {
            ret = SR_ERR;
        }
        if ((ret = command_fpga_config(usb->devhdl)) != SR_OK) {
            sr_err("Send FPGA configure command failed!");
        } else {
            /* Takes >= 10ms for the FX2 to be ready for FPGA configure. */
            g_usleep(10 * 1000);
            //char filename[256];
            //sprintf(filename,"%s%s",DS_RES_PATH,devc->profile->fpga_bit33);
            //const char *fpga_bit = filename;
            char *fpga_bit = malloc(strlen(DS_RES_PATH)+strlen(devc->profile->fpga_bit33)+1);
            if (fpga_bit == NULL)
                return SR_ERR_MALLOC;
            strcpy(fpga_bit, DS_RES_PATH);
            strcat(fpga_bit, devc->profile->fpga_bit33);
            ret = fpga_config(usb->devhdl, fpga_bit);
            if (ret != SR_OK) {
                sr_err("Configure FPGA failed!");
            }
        }
        sr_dbg("%s: setting threshold to %d",
            __func__, devc->th_level);
    }  else if (id == SR_CONF_FILTER) {
        stropt = g_variant_get_string(data, NULL);
        if (!strcmp(stropt, filters[SR_FILTER_NONE])) {
            devc->filter = SR_FILTER_NONE;
        } else if (!strcmp(stropt, filters[SR_FILTER_1T])) {
            devc->filter = SR_FILTER_1T;
        } else {
            ret = SR_ERR;
        }
        sr_dbg("%s: setting threshold to %d",
            __func__, devc->th_level);
    } else if (id == SR_CONF_EN_CH) {
        ch->enabled = g_variant_get_boolean(data);

        if (sdi->mode == DSO) {
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, ch, SR_CONF_EN_CH));
            if (en_ch_num(sdi) != 0) {
                ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, 0, SR_CONF_SAMPLERATE));
            }
        }
        if (ret == SR_OK)
            sr_dbg("%s: setting ENABLE of channel %d to %d",
                __func__, ch->index, ch->enabled);
        else
            sr_dbg("%s: setting ENABLE of channel %d to %d failed",
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
    } else if (id == SR_CONF_VPOS) {
        ch->vpos = g_variant_get_double(data);
        if (sdi->mode == DSO) {
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, ch, SR_CONF_VPOS));
        }
        if (ret == SR_OK)
            sr_dbg("%s: setting VPOS of channel %d to %lf mv",
                __func__, ch->index, ch->vpos);
        else
            sr_dbg("%s: setting VPOS of channel %d to %lf mv failed",
                __func__, ch->index, ch->vpos);
    } else if (id == SR_CONF_TIMEBASE) {
        devc->timebase = g_variant_get_uint64(data);
    } else if (id == SR_CONF_COUPLING) {
        ch->coupling = g_variant_get_byte(data);
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
        devc->trigger_source = (devc->trigger_source & 0xf0) + (g_variant_get_byte(data) & 0x0f);
        ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, NULL, SR_CONF_TRIGGER_SOURCE));
        if (ret == SR_OK)
            sr_dbg("%s: setting DSO Trigger Source to %d",
                __func__, devc->trigger_source);
        else
            sr_dbg("%s: setting DSO Trigger Source to %d failed",
                __func__, devc->trigger_source);
    } else if (id == SR_CONF_TRIGGER_CHANNEL) {
        devc->trigger_source = (g_variant_get_byte(data) << 4) + (devc->trigger_source & 0x0f);
        ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, NULL, SR_CONF_TRIGGER_SOURCE));
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
    } else if (id == SR_CONF_ZERO) {
        devc->zero = g_variant_get_boolean(data);
        if (devc->zero) {
            devc->zero_stage = -1;
            devc->zero_pcnt = 0;
            devc->zero_comb = -1;
            GList *l;
            int i;
            struct DSL_vga *vga_ptr =  get_vga_ptr(sdi);
            for(l = sdi->channels; l; l = l->next) {
                struct sr_channel *probe = (struct sr_channel *)l->data;
                probe->vpos_trans = get_default_trans(sdi);
            }
            for (i = 0; vga_ptr && (vga_ptr+i)->key; i++) {
                (vga_ptr+i)->vgain0 = get_default_vgain(sdi, i);
                (vga_ptr+i)->vgain1 = get_default_vgain(sdi, i);
                (vga_ptr+i)->voff0 = get_default_voff(sdi, 0);
                (vga_ptr+i)->voff1 = get_default_voff(sdi, 1);
            }
        }
    }  else if (id == SR_CONF_CALI) {
        devc->cali = g_variant_get_boolean(data);
    } else if (id == SR_CONF_ZERO_LOAD) {
        GSList *l;
        for(l = sdi->channels; l; l = l->next) {
            struct sr_channel *probe = (struct sr_channel *)l->data;
            if (!dso_load_eep(sdi, probe, FALSE)) {
                config_set(SR_CONF_ZERO, g_variant_new_boolean(TRUE), sdi, NULL, NULL);
                sr_info("Zero have not been setted!");
                break;
            }
        }
    } else if (id == SR_CONF_ZERO_SET) {
        GSList *l;
        struct cmd_zero_info zero_info;
        struct cmd_vga_info vga_info;
        for(l = sdi->channels; l; l = l->next) {
            struct sr_channel *probe = (struct sr_channel *)l->data;
            zero_info.zero_addr = zero_base_addr +
                                  probe->index * (sizeof(struct cmd_zero_info) + sizeof(struct cmd_vga_info));
            int i;
            struct DSL_vga *vga_ptr =  get_vga_ptr(sdi);
            uint8_t *voff_ptr = &zero_info.zero_addr + 1;
            for (i = 0; vga_ptr && (vga_ptr+i)->key; i++) {
                *(voff_ptr+2*i) = ((probe->index == 0) ? (vga_ptr+i)->voff0 : (vga_ptr+i)->voff1) & 0x00ff;
                *(voff_ptr+2*i+1) = ((probe->index == 0) ? (vga_ptr+i)->voff0 : (vga_ptr+i)->voff1) >> 8;
            }
            if (i != 0) {
                *(voff_ptr+2*i) = probe->comb_diff_top;
                *(voff_ptr+2*i+1) = probe->comb_diff_bom;
                *(voff_ptr+2*i+2) = (probe->vpos_trans&0x00FF);
                *(voff_ptr+2*i+3) = (probe->vpos_trans>>8);

                vga_info.vga_addr = zero_info.zero_addr + sizeof(struct cmd_zero_info);
                uint16_t *vgain_ptr = &vga_info.vga0;
                for (i=0; vga_ptr && (vga_ptr+i)->key; i++){
                    *(vgain_ptr+i) = ((probe->index == 0) ? (vga_ptr+i)->vgain0 : (vga_ptr+i)->vgain1) >> 8;
                }
                ret = command_wr_reg(usb->devhdl, 0, EEWP_ADDR);
                if (ret == SR_OK)
                    ret = command_wr_nvm(usb->devhdl, (unsigned char *)&zero_info, sizeof(struct cmd_zero_info));
                if (ret == SR_OK)
                    ret = command_wr_nvm(usb->devhdl, (unsigned char *)&vga_info, sizeof(struct cmd_vga_info));
                if (ret == SR_OK)
                    ret = command_wr_reg(usb->devhdl, 1, EEWP_ADDR);
                if (ret != SR_OK)
                    sr_err("DSO channel %d Set Zero command failed!", probe->index);

                const double slope = (probe->comb_diff_bom - probe->comb_diff_top)/(2.0*255.0);
                for (i = 0; i < 256; i++) {
                    ret = command_wr_reg(usb->devhdl, i, COMB_ADDR + probe->index*2);
                    int value = i+i*slope+probe->comb_diff_top*0.5+0.5;
                    value = (value < 0) ? 0 :
                            (value > 255) ? 255 : value;
                    ret = command_wr_reg(usb->devhdl, value, COMB_ADDR + probe->index*2 + 1);
                }
            }
        }
    } else if (id == SR_CONF_VOCM) {
        const uint8_t vocm = g_variant_get_byte(data);
        ret = command_wr_reg(usb->devhdl, vocm, COMB_ADDR+4);
    } else if (id == SR_CONF_VGAIN) {
        const uint64_t vgain = g_variant_get_uint64(data) << 8;
        int i;
        struct DSL_vga *vga_ptr =  get_vga_ptr(sdi);
        for (i = 0; vga_ptr && (vga_ptr+i)->key; i++) {
            if ((vga_ptr+i)->key == ch->vdiv)
                if (ch->index == 0)
                    (vga_ptr+i)->vgain0 = vgain;
                else if (ch->index == 1)
                    (vga_ptr+i)->vgain1 = vgain;
        }
        ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, ch, SR_CONF_VDIV));
        if (ret == SR_OK)
            sr_dbg("%s: setting VDIV of channel %d to %d mv",
                __func__, ch->index, ch->vdiv);
        else
            sr_dbg("%s: setting VDIV of channel %d to %d mv failed",
                __func__, ch->index, ch->vdiv);
    } else if (id == SR_CONF_VOFF) {
        uint16_t voff = g_variant_get_uint16(data);
        if (strcmp(sdi->model, "DSCope") == 0) {
            double voltage_off = (2.0 * voff / CALI_VOFF_RANGE  - 1) * ch->vdiv;
            double trans_coarse = (ch->vdiv < 500) ? (ch->vpos_trans >> 8)/DSCOPE_TRANS_CMULTI : (ch->vpos_trans >> 8);
            double trans_fine = (ch->vdiv < 500) ? (ch->vpos_trans & 0x00ff) / 1000.0 : (ch->vpos_trans & 0x00ff) / DSCOPE_TRANS_FMULTI;

            uint16_t default_voff = get_default_voff(sdi, ch->index);
            int voff_coarse = floor(voltage_off / trans_coarse + 0.5);
            int voff_fine = floor(-(voltage_off - voff_coarse*trans_coarse)/trans_fine + 0.5);
            voff_coarse = (default_voff >> 10) + voff_coarse;
            voff_fine = (default_voff&0x03ff) + voff_fine;
            voff = (voff_coarse << 10) + voff_fine;
        }
        int i;
        struct DSL_vga *vga_ptr =  get_vga_ptr(sdi);
        for (i = 0; vga_ptr && (vga_ptr+i)->key; i++) {
            if ((vga_ptr+i)->key == ch->vdiv)
                if (ch->index == 0)
                    (vga_ptr+i)->voff0 = voff;
                else if (ch->index == 1)
                    (vga_ptr+i)->voff1 = voff;
        }
        ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, ch, SR_CONF_VPOS));
        if (ret == SR_OK)
            sr_dbg("%s: setting VPOS of channel %d to %lf mv",
                __func__, ch->index, ch->vpos);
        else
            sr_dbg("%s: setting VPOS of channel %d to %lf mv failed",
                __func__, ch->index, ch->vpos);
    }else {
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
    case SR_CONF_TRIGGER_TYPE:
        *data = g_variant_new_string(TRIGGER_TYPE);
        break;
    case SR_CONF_OPERATION_MODE:
        *data = g_variant_new_strv(opmodes, opmodes_show_count);
        break;
    case SR_CONF_THRESHOLD:
        *data = g_variant_new_strv(thresholds, ARRAY_SIZE(thresholds));
        break;
    case SR_CONF_FILTER:
        *data = g_variant_new_strv(filters, ARRAY_SIZE(filters));
        break;
    default:
        return SR_ERR_NA;
    }

    return SR_OK;
}

static int dso_init(const struct sr_dev_inst *sdi)
{
    int ret, i;
    GSList *l;
    struct sr_usb_dev_inst *usb = sdi->conn;

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
        ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, probe, SR_CONF_VPOS));
        if (ret != SR_OK) {
            sr_err("Set VPOS of channel %d command failed!", probe->index);
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

static int dso_zero(struct sr_dev_inst *sdi, struct sr_status mstatus)
{
    struct DSL_context *devc = sdi->priv;
    struct sr_usb_dev_inst *usb = sdi->conn;
    GSList *l;
    int ret, i;
    static double vpos_back[2];
    static uint64_t vdiv_back[2];
    struct DSL_vga *vga_ptr = get_vga_ptr(sdi);
    struct sr_channel *probe0, *probe1;
    for(l = sdi->channels; l; l = l->next) {
        struct sr_channel *probe = (struct sr_channel *)l->data;
        if (probe->index == 0)
            probe0 = probe;
        if (probe->index == 1)
            probe1 = probe;
    }

    if (devc->zero_stage == -1) {
        // initialize before zero adjustment
        if (dso_init(sdi) == SR_OK)
            devc->zero_stage = 0;
    } else if ((vga_ptr+devc->zero_stage)->key == 0) {
        ret = SR_OK;
        if (strcmp(sdi->model, "DSCope20") == 0) {
            if (devc->zero_pcnt == 0) {
                devc->zero_comb = 0;
                vpos_back[0] = probe0->vpos;
                probe0->vpos = (vga_ptr+devc->zero_stage-1)->key * -4.8;
                vdiv_back[0] = probe0->vdiv;
                probe0->vdiv = (vga_ptr+devc->zero_stage-1)->key;
                ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, probe0, SR_CONF_VPOS));
            } else if (devc->zero_pcnt == 4) {
                const double voff = 255*0.98 - (mstatus.ch0_max + mstatus.ch0_min) / 2.0;
                if (abs(voff) < 0.5) {
                    probe0->vpos = vpos_back[0];
                } else {
                    probe0->vpos_trans += voff;
                    ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, probe0, SR_CONF_VPOS));
                    devc->zero_pcnt = 1;
                }
            } else if (devc->zero_pcnt == 5) {
                devc->zero_comb = 0;
                vpos_back[1] = probe1->vpos;
                probe1->vpos = (vga_ptr+devc->zero_stage-1)->key * -4.8;
                vdiv_back[1] = probe1->vdiv;
                probe1->vdiv = (vga_ptr+devc->zero_stage-1)->key;
                ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, probe1, SR_CONF_VPOS));
            } else if (devc->zero_pcnt == 9) {
                const double voff = 255*0.98 - (mstatus.ch1_max + mstatus.ch1_min) / 2.0;
                if (abs(voff) < 0.5) {
                    probe1->vpos = vpos_back[1];
                } else {
                    probe1->vpos_trans += voff;
                    ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, probe1, SR_CONF_VPOS));
                    devc->zero_pcnt = 6;
                }
            }
        }

        if (devc->zero_pcnt == 10) {
            ret = command_wr_reg(usb->devhdl, 0b1101, COMB_ADDR+6);
            devc->zero_comb = 0;
            vpos_back[0] = probe0->vpos;
            probe0->vpos = (vga_ptr+devc->zero_stage-1)->key * 4.5;
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, probe0, SR_CONF_VPOS));
        } else if (devc->zero_pcnt == 15) {
            probe0->comb_diff_top = (mstatus.ch0_max - mstatus.ch1_max) +
                                    (mstatus.ch0_min - mstatus.ch1_min);
            probe0->vpos = (vga_ptr+devc->zero_stage-1)->key * -4.5;
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, probe0, SR_CONF_VPOS));
        } else if (devc->zero_pcnt == 20) {
            probe0->comb_diff_bom = (mstatus.ch0_max - mstatus.ch1_max) +
                                    (mstatus.ch0_min - mstatus.ch1_min);
            probe0->vpos = vpos_back[0];
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, probe0, SR_CONF_VPOS));
        }

        if (devc->zero_pcnt == 25) {
            ret = command_wr_reg(usb->devhdl, 0b1110, COMB_ADDR+6);
            devc->zero_comb = 1;
            vpos_back[1] = probe1->vpos;
            probe1->vpos = (vga_ptr+devc->zero_stage-1)->key * 4.5;
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, probe1, SR_CONF_VPOS));
        } else if (devc->zero_pcnt == 30) {
            probe1->comb_diff_top = (mstatus.ch1_max - mstatus.ch0_max) +
                                    (mstatus.ch1_min - mstatus.ch0_min);
            probe1->vpos = (vga_ptr+devc->zero_stage-1)->key * -4.5;
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, probe1, SR_CONF_VPOS));
        } else if (devc->zero_pcnt == 35) {
            probe1->comb_diff_bom = (mstatus.ch1_max - mstatus.ch0_max) +
                                    (mstatus.ch1_min - mstatus.ch0_min);
            probe1->vpos = vpos_back[1];
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, probe1, SR_CONF_VPOS));
        }

        if (devc->zero_pcnt == 40) {
            if (strcmp(sdi->model, "DSCope20") == 0) {
                probe0->vdiv = vdiv_back[0];
                probe1->vdiv = vdiv_back[1];
            }
            ret = command_wr_reg(usb->devhdl, 0b0011, COMB_ADDR+6);
            devc->zero = FALSE;
            dso_init(sdi);
        }

        if (ret == SR_OK)
            devc->zero_pcnt++;
    } else {
        if (devc->zero_pcnt == 0) {
            for(l = sdi->channels; l; l = l->next) {
                struct sr_channel *probe = (struct sr_channel *)l->data;
                uint64_t vdiv_back = probe->vdiv;
                probe->vdiv = (vga_ptr+devc->zero_stage)->key;
                ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, probe, SR_CONF_VDIV));
                ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, probe, SR_CONF_VPOS));
                probe->vdiv = vdiv_back;
            }
        }

        if (devc->zero_pcnt == 4) {
            const double voff0 = 255/2.0 - (mstatus.ch0_max + mstatus.ch0_min)/2.0;
            const double voff1 = 255/2.0 - (mstatus.ch1_max + mstatus.ch1_min)/2.0;
            if (abs(voff0) < 0.5 && abs(voff1) < 0.5) {
                devc->zero_stage++;
            } else {
                if (strcmp(sdi->model, "DSCope") == 0) {
                    for(l = sdi->channels; l; l = l->next) {
                        struct sr_channel *probe = (struct sr_channel *)l->data;
                        double trans_coarse = ((vga_ptr+devc->zero_stage)->key < 500) ? (probe->vpos_trans >> 8)/DSCOPE_TRANS_CMULTI : (probe->vpos_trans >> 8);
                        double trans_fine = ((vga_ptr+devc->zero_stage)->key < 500) ? (probe->vpos_trans & 0x00ff) / 1000.0 : (probe->vpos_trans & 0x00ff) / DSCOPE_TRANS_FMULTI;

                        double voltage_off = ((probe->index == 0) ? voff0 : voff1) * (vga_ptr+devc->zero_stage)->key * 10 / 255.0;
                        uint16_t last_voff = ((probe->index == 0) ? (vga_ptr+devc->zero_stage)->voff0 : (vga_ptr+devc->zero_stage)->voff1);
                        int voff_coarse = floor(voltage_off / trans_coarse + 0.5);
                        int voff_fine = floor(-(voltage_off - voff_coarse*trans_coarse)/trans_fine + 0.5);
                        voff_coarse = (last_voff >> 10) + voff_coarse;
                        voff_fine = (last_voff&0x03ff) + voff_fine;
                        if (probe->index == 0)
                            (vga_ptr+devc->zero_stage)->voff0 = (voff_coarse << 10) + voff_fine;
                        else if (probe->index == 1)
                            (vga_ptr+devc->zero_stage)->voff1 = (voff_coarse << 10) + voff_fine;
                    }
                } else if (strcmp(sdi->model, "DSCope20") == 0) {
                    (vga_ptr+devc->zero_stage)->voff0 += voff0;
                    (vga_ptr+devc->zero_stage)->voff1 += voff1;
                }
            }
            devc->zero_pcnt = 0;
        } else {
            devc->zero_pcnt++;
        }
    }

    return ret;
}

static int dev_open(struct sr_dev_inst *sdi)
{
    struct sr_usb_dev_inst *usb;
    struct DSL_context *devc;
	int ret;
	int64_t timediff_us, timediff_ms;
    uint8_t fpga_done;
    GSList *l;
    gboolean zeroed;

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
        ret = DSCope_dev_open(sdi);
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
            char *fpga_bit = malloc(strlen(DS_RES_PATH)+strlen(devc->profile->fpga_bit33)+1);
            if (fpga_bit == NULL)
                return SR_ERR_MALLOC;
            strcpy(fpga_bit, DS_RES_PATH);
            strcat(fpga_bit, devc->profile->fpga_bit33);
            ret = fpga_config(usb->devhdl, fpga_bit);
            if (ret != SR_OK) {
                sr_err("%s: Configure FPGA failed!", __func__);
            }
            g_free(fpga_bit);
        }
    }

    // load zero informations
    if (sdi->mode == DSO) {
        for(l = sdi->channels; l; l = l->next) {
            struct sr_channel *probe = (struct sr_channel *)l->data;
            zeroed = dso_load_eep(sdi, probe, fpga_done);
            if (!zeroed)
                break;
        }
        if (!zeroed) {
            config_set(SR_CONF_ZERO, g_variant_new_boolean(TRUE), sdi, NULL, NULL);
            sr_info("Zero have not been setted!");
        }
        if (fpga_done == 0)
            dso_init(sdi);
    }

    return SR_OK;
}

static int dev_close(struct sr_dev_inst *sdi)
{
    struct sr_usb_dev_inst *usb;

	usb = sdi->conn;
	if (usb->devhdl == NULL)
        return SR_ERR;

    sr_info("DSCope: Closing device %d on %d.%d interface %d.",
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

static struct sr_config * new_config(int key, GVariant *data)
{
    struct sr_config *config;

    if (!(config = g_try_malloc0(sizeof(struct sr_config)))) {
        sr_err("META config malloc failed.");
        return NULL;
    }

    config->key = key;
    config->data = data;

    return config;
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
    const int sample_width = 2;
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
            analog.num_samples = (transfer->actual_length / sample_width)/g_slist_length(analog.probes);
            analog.mq = SR_MQ_VOLTAGE;
            analog.unit = SR_UNIT_VOLT;
            analog.mqflags = SR_MQFLAG_AC;
            analog.data = cur_buf + trigger_offset_bytes;
        }

        if (devc->limit_samples) {
            const uint64_t remain_length= (devc->limit_samples - devc->num_samples) * sample_width;
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
        if ((sdi->mode == LOGIC || devc->instant) &&
            devc->limit_samples &&
            (unsigned int)devc->num_samples >= devc->limit_samples) {
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
        return devc->cur_samplerate / 1000 * (devc->sample_wide ? 2 : 1);
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
    size_t total_size;
    total_size = min(devc->limit_samples * (devc->sample_wide ? 2 : 1),
                      total_buffer_time * to_bytes_per_ms(devc));
    /* Total buffer size should be able to hold about 500ms of data. */
    //n = 500 * to_bytes_per_ms(devc) / get_buffer_size(devc);
    n = ceil(total_size * 1.0 / get_buffer_size(devc));

    if (n > NUM_SIMUL_TRANSFERS)
        return NUM_SIMUL_TRANSFERS;

    return n;
    //return 1;
}

static unsigned int get_timeout(struct DSL_context *devc)
{
    size_t total_size;
    unsigned int timeout;

    total_size = get_buffer_size(devc) * get_number_of_transfers(devc);
    timeout = total_size / to_bytes_per_ms(devc);
    //return timeout + timeout / 4; /* Leave a headroom of 25% percent. */
    return timeout * 4;
}

static int dev_transfer_start(const struct sr_dev_inst *sdi)
{
    struct DSL_context *devc;
    struct sr_usb_dev_inst *usb;
    struct libusb_transfer *transfer;
    unsigned int i, timeout, num_transfers;
    int ret;
    unsigned char *buf;
    size_t size;
    int dso_buffer_size;

    devc = sdi->priv;
    usb = sdi->conn;

//    timeout = get_timeout(devc);
//    num_transfers = get_number_of_transfers(devc);
//    size = get_buffer_size(devc);
    timeout = 500;
    #ifndef _WIN32
    num_transfers = 1;
    #else
    num_transfers = buffer_cnt;
    #endif

    if (devc->instant)
        dso_buffer_size = instant_buffer_size * g_slist_length(sdi->channels);
    else
        dso_buffer_size = devc->limit_samples * en_ch_num(sdi) + 512;
    size = (sdi->mode == ANALOG) ? cons_buffer_size : ((sdi->mode == DSO) ? dso_buffer_size : buffer_size);
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

    if (devc->zero) {
        dso_zero(sdi, mstatus);
    }

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
            packet.type = SR_DF_TRIGGER;
            packet.payload = trigger_pos;
            sr_session_send(sdi, &packet);

            devc->status = DSL_TRIGGERED;
            devc->num_transfers = 0;
            devc->empty_transfer_count = 0;
        }
    }

    if (devc->status == DSL_TRIGGERED) {
        // successfull
        free_transfer(transfer);
        if ((ret = dev_transfer_start(devc->cb_data)) != SR_OK) {
            sr_err("%s: could not start data transfer"
                   "(%d)%d", __func__, ret, errno);
        }
    } else if (devc->status == DSL_START) {
        // retry
        resubmit_transfer(transfer);
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
    test_sample_value = 0;
    devc->abort = FALSE;

	/* Configures devc->trigger_* and devc->sample_wide */
    if (configure_probes(sdi) != SR_OK) {
        sr_err("%s: Failed to configure probes.", __func__);
        return SR_ERR;
	}

    /* Stop Previous GPIF acquisition */
    if ((ret = command_stop_acquisition (usb->devhdl)) != SR_OK) {
        sr_err("%s: Stop DSCope acquisition failed!", __func__);
        return ret;
    } else {
        sr_info("%s: Stop Previous DSCope acquisition!", __func__);
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

    if (devc->zero && devc->zero_stage == -1) {
        // initialize before zero adjustment
        if ((ret = dso_init(sdi)) == SR_OK) {
            devc->zero_stage = 0;
        } else {
            sr_err("%s: DSO zero initialization failed!", __func__);
            return ret;
        }
        devc->zero_stage = 0;
    }

    /*
     * settings must be updated before acquisition
     */
    if (sdi->mode == DSO) {
        devc->trigger_hpos = devc->trigger_hrate * en_ch_num(sdi) * devc->limit_samples / 200.0;
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

    struct drv_context *drvc;
    struct DSL_context *devc;
    struct sr_usb_dev_inst *usb;

    drvc = di->priv;
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
        int ret;

        usb = sdi->conn;
        ret = command_get_fw_version(usb->devhdl, &vi);
        if (ret != SR_OK) {
            sr_err("Device don't exist!");
            return SR_ERR;
        } else {
            return SR_OK;
        }
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

SR_PRIV struct sr_dev_driver DSCope_driver_info = {
    .name = "DSCope",
    .longname = "DSCope (generic driver for DScope oscilloscope)",
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
