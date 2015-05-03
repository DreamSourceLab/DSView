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

static const uint8_t zero_base_addr = 0x80;
static const uint8_t comb_base_addr = 0xB0;

SR_PRIV struct sr_dev_driver DSCope_driver_info;
static struct sr_dev_driver *di = &DSCope_driver_info;

extern struct ds_trigger *trigger;

gboolean mstatus_valid = FALSE;
struct sr_status mstatus;
struct cmd_zero_info zero_info;
struct cmd_comb_info comb_info;

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
        if (strncmp((const char *)strdesc, "DSCope", 6))
			break;

        /* If we made it here, it must be an DSCope. */
		ret = TRUE;
	}
	if (hdl)
		libusb_close(hdl);

	return ret;
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
    setting.trig_count0_header = 0x1c10ffff;
    setting.trig_count1_header = 0x1d10ffff;
    //setting.trig_count2_header = 0x1e10ffff;
    //setting.trig_count3_header = 0x1f10ffff;
    setting.trig_logic0_header = 0x2010ffff;
    setting.trig_logic1_header = 0x2110ffff;
    //setting.trig_logic2_header = 0x2210ffff;
    //setting.trig_logic3_header = 0x2310ffff;
    setting.end_sync = 0xfa5afa5a;

    for (l = sdi->channels; l; l = l->next) {
        struct sr_channel *probe = (struct sr_channel *)l->data;
        channel_en_cnt += probe->enabled;
        channel_cnt++;
    }
    if (channel_en_cnt == 0)
        channel_en_cnt = 1;
    //setting.mode = (test_mode ? 0x8000 : 0x0000) + trigger->trigger_en + (sdi->mode << 4);
    setting.mode = ((devc->op_mode == SR_OP_INTERNAL_TEST) << 15) +
                   ((devc->op_mode == SR_OP_EXTERNAL_TEST) << 14) +
                   ((devc->op_mode == SR_OP_LOOPBACK_TEST) << 13) +
                   trigger->trigger_en +
                   ((sdi->mode > 0) << 4) + (devc->clock_type << 1) + (devc->clock_edge << 1) +
                   (((devc->cur_samplerate == SR_MHZ(200) && sdi->mode != DSO) || (sdi->mode == ANALOG)) << 5) +
                   ((devc->cur_samplerate == SR_MHZ(400)) << 6) +
                   ((sdi->mode == ANALOG) << 7) +
                   ((devc->filter == SR_FILTER_1T) << 8) +
                   (devc->instant << 9) + (devc->zero << 10);
    setting.divider = devc->zero ? 0x1 : (uint32_t)ceil(DSCOPE_MAX_SAMPLERATE * 1.0 / devc->cur_samplerate / channel_en_cnt);
    setting.count = (uint32_t)(devc->limit_samples / (channel_cnt / channel_en_cnt));
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
    devc->op_mode = SR_OP_NORMAL;
    devc->th_level = SR_TH_3V3;
    devc->filter = SR_FILTER_NONE;
    devc->timebase = 10000;
    devc->trigger_slope = DSO_TRIGGER_RISING;
    devc->trigger_source = DSO_TRIGGER_AUTO;
    devc->trigger_hpos = 0x0;
    devc->zero = FALSE;

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

static int set_probes(struct sr_dev_inst *sdi, int num_probes)
{
    int j;
    struct sr_channel *probe;

    for (j = 0; j < num_probes; j++) {
        if (!(probe = sr_channel_new(j, (sdi->mode == LOGIC) ? SR_CHANNEL_LOGIC : ((sdi->mode == DSO) ? SR_CHANNEL_DSO : SR_CHANNEL_ANALOG),
                                   TRUE, probe_names[j])))
            return SR_ERR;
        if (sdi->mode == DSO) {
            probe->vdiv = 1000;
            probe->vfactor = 1;
            probe->vpos = 0;
            probe->coupling = SR_DC_COUPLING;
            probe->trig_value = 0x80;
        }
        sdi->channels = g_slist_append(sdi->channels, probe);
    }
    return SR_OK;
}

static int adjust_probes(struct sr_dev_inst *sdi, int num_probes)
{
    int j;
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
		devices = g_slist_append(devices, sdi);

		if (check_conf_profile(devlist[i])) {
			/* Already has the firmware, so fix the new address. */
            sr_dbg("Found an DSCope device.");
            sdi->status = SR_ST_INACTIVE;
            sdi->inst_type = SR_INST_USB;
            sdi->conn = sr_usb_dev_inst_new(libusb_get_bus_number(devlist[i]),
					libusb_get_device_address(devlist[i]), NULL);
		} else {
            char filename[256];
            sprintf(filename,"%s%s",config_path,prof->firmware);
            const char *firmware = filename;
            if (ezusb_upload_firmware(devlist[i], USB_CONFIGURATION,
                firmware) == SR_OK)
				/* Store when this device's FW was updated. */
				devc->fw_updated = g_get_monotonic_time();
			else
                sr_err("Firmware upload failed for "
				       "device %d.", devcnt);
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

static GSList *dev_mode_list(void)
{
    GSList *l = NULL;
    int i;

    for(i = 0; i < ARRAY_SIZE(mode_list); i++) {
        l = g_slist_append(l, &mode_list[i]);
    }

    return l;
}

static uint64_t dso_cmd_gen(struct sr_dev_inst *sdi, struct sr_channel* ch, int id)
{
    struct DSL_context *devc;
    uint64_t cmd = 0;
    int channel_cnt = 0;
    uint16_t vpos_coarse;
    uint16_t vpos_fine;
    gboolean vpos_coarse_neg;
    gboolean vpos_fine_neg;
    GSList *l;
    const int ch_bit = 7;

    devc = sdi->priv;

    switch (id) {
    case SR_CONF_EN_CH:
    case SR_CONF_COUPLING:
        for (l = sdi->channels; l; l = l->next) {
            struct sr_channel *probe = (struct sr_channel *)l->data;
            channel_cnt += probe->enabled;
        }
        if (channel_cnt == 1) {
            if (((ch->index == 0) && ch->enabled) || ((ch->index == 1) && !ch->enabled))
                cmd += 0x1600;
            else if (((ch->index == 1) && ch->enabled) || ((ch->index == 0) && !ch->enabled))
                cmd += 0x1A00;
        } else if (channel_cnt == 2) {
            cmd += 0x0E00;
            //cmd += 0x000;
        } else {
            return 0x0;
        }

        cmd += ch->index << ch_bit;
        if (ch->coupling == SR_GND_COUPLING)
            cmd &= 0xFFFFFDFF;
        else if (ch->coupling == SR_DC_COUPLING)
            cmd += 0x100;
        break;
    case SR_CONF_VDIV:
    case SR_CONF_TIMEBASE:
        cmd += 0x8;
        cmd += ch->index << ch_bit;
        //  --VDBS
        switch(ch->vdiv){
        case 5:     cmd += 0x170000; break;
        case 10:    cmd += 0x162800; break;
        case 20:    cmd += 0x14D000; break;
        case 50:    cmd += 0x12E800; break;
        case 100:   cmd += 0x118000; break;
        case 200:   cmd += 0x101800; break;
        case 500:   cmd += 0x2E800; break;
        case 1000:  cmd += 0x18000; break;
        case 2000:  cmd += 0x01800; break;
        case 5000:  cmd += 0x00000; break;
        default: cmd += 0x0; break;
        }
        break;
    case SR_CONF_VPOS:
        cmd += 0x10;
        cmd += ch->index << ch_bit;
        if (ch->vdiv < 500) {
            vpos_coarse_neg = (ch->vpos < 0);
            vpos_coarse = (uint16_t)(abs(ch->vpos)/(2*VPOS_STEP) + 0.5) * 4;
            vpos_fine_neg = vpos_coarse_neg ^ ((abs(ch->vpos) < vpos_coarse*0.5*VPOS_STEP));
            vpos_fine = (uint16_t)(abs((abs(ch->vpos) - vpos_coarse*0.5*VPOS_STEP))/(2*VPOS_MINISTEP) + 0.5);
        } else {
            vpos_coarse_neg = (ch->vpos < 0);
            vpos_coarse = (uint16_t)(abs(ch->vpos)/(20*VPOS_STEP) + 0.5) * 4;
            vpos_fine_neg = vpos_coarse_neg ^ ((abs(ch->vpos) < vpos_coarse*5*VPOS_STEP));
            vpos_fine = (uint16_t)(abs((abs(ch->vpos) - vpos_coarse*5*VPOS_STEP))/(20*VPOS_MINISTEP) + 0.5);
        }
        cmd += (vpos_fine_neg << 31) + (vpos_fine << 20) +
               (vpos_coarse_neg << 19) + (vpos_coarse << 8);
        break;
    case SR_CONF_SAMPLERATE:
        for (l = sdi->channels; l; l = l->next) {
            struct sr_channel *probe = (struct sr_channel *)l->data;
            channel_cnt += probe->enabled;
        }
        cmd += 0x18;
        uint32_t divider = devc->zero ? 0x1 : (uint32_t)ceil(DSCOPE_MAX_SAMPLERATE * 1.0 / devc->cur_samplerate / channel_cnt);
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
    case SR_CONF_ZERO_SET:
        cmd += 0x40;
        cmd += ch->index << ch_bit;
        cmd += ((uint64_t)zero_info.vpos_l << 8);
        cmd += ((uint64_t)(zero_info.vpos_h & 0x3) << 16);
        cmd += ((uint64_t)zero_info.voff_l << 24);
        cmd += ((uint64_t)(zero_info.voff_h & 0x3) << 32);
        cmd += ((uint64_t)zero_info.vcntr_l << 40);
        cmd += ((uint64_t)(zero_info.vcntr_h & 0x3) << 48);
        cmd += ((uint64_t)zero_info.adc_off << 56);
        break;
    case SR_CONF_COMB_SET:
        cmd += 0x48;
        cmd += ((uint64_t)comb_info.comb0_low_off << 8);
        cmd += ((uint64_t)comb_info.comb0_hig_off << 16);
        cmd += ((uint64_t)comb_info.comb1_low_off << 24);
        cmd += ((uint64_t)comb_info.comb1_hig_off << 32);
        cmd += ((uint64_t)comb_info.comb_sign << 40);
        break;
    case SR_CONF_ZERO_OVER:
        cmd += 0x50;
        break;
    case SR_CONF_DSO_SYNC:
        cmd = 0xa5a5a500;
        break;
    default:
        cmd = 0x00000000;
    }

    return cmd;
}

static int dev_open(struct sr_dev_inst *sdi)
{
    struct sr_usb_dev_inst *usb;
    struct DSL_context *devc;
	int ret;
	int64_t timediff_us, timediff_ms;

	devc = sdi->priv;
	usb = sdi->conn;

	/*
	 * If the firmware was recently uploaded, wait up to MAX_RENUM_DELAY_MS
	 * milliseconds for the FX2 to renumerate.
	 */
    ret = SR_ERR;
	if (devc->fw_updated > 0) {
        sr_info("Waiting for device to reset.");
		/* Takes >= 300ms for the FX2 to be gone from the USB bus. */
        g_usleep(300 * 1000);
		timediff_ms = 0;
		while (timediff_ms < MAX_RENUM_DELAY_MS) {
            if ((ret = DSCope_dev_open(sdi)) == SR_OK)
				break;
			g_usleep(100 * 1000);

			timediff_us = g_get_monotonic_time() - devc->fw_updated;
			timediff_ms = timediff_us / 1000;
            sr_spew("Waited %" PRIi64 "ms.", timediff_ms);
		}
        if (ret != SR_OK) {
            sr_err("Device failed to renumerate.");
            return SR_ERR;
		}
        sr_info("Device came back after %" PRIi64 "ms.", timediff_ms);
	} else {
        sr_info("Firmware upload was not needed.");
        ret = DSCope_dev_open(sdi);
	}

    if (ret != SR_OK) {
        sr_err("Unable to open device.");
        return SR_ERR;
	}

	ret = libusb_claim_interface(usb->devhdl, USB_INTERFACE);
	if (ret != 0) {
		switch(ret) {
		case LIBUSB_ERROR_BUSY:
            sr_err("Unable to claim USB interface. Another "
			       "program or driver has already claimed it.");
			break;
		case LIBUSB_ERROR_NO_DEVICE:
            sr_err("Device has been disconnected.");
			break;
		default:
            sr_err("Unable to claim interface: %s.",
			       libusb_error_name(ret));
			break;
		}

        return SR_ERR;
	}

    if ((ret = command_fpga_config(usb->devhdl)) != SR_OK) {
        sr_err("Send FPGA configure command failed!");
    } else {
        /* Takes >= 10ms for the FX2 to be ready for FPGA configure. */
        g_usleep(10 * 1000);
        char filename[256];
        sprintf(filename,"%s%s",config_path,devc->profile->fpga_bit33);

        const char *fpga_bit = filename;
        ret = fpga_config(usb->devhdl, fpga_bit);
        if (ret != SR_OK) {
            sr_err("Configure FPGA failed!");
        }
    }

    if (sdi->mode == DSO) {
        GSList *l;
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
    }

    GSList *l;
        for(l = sdi->channels; l; l = l->next) {
        struct sr_channel *probe = (struct sr_channel *)l->data;
        zero_info.zero_addr = (zero_base_addr + probe->index * sizeof(struct cmd_zero_info));
        if ((ret = command_rd_nvm(usb->devhdl, (unsigned char *)&zero_info, zero_info.zero_addr, sizeof(struct cmd_zero_info))) != SR_OK) {
            sr_err("Send Get Zero command failed!");
        } else {
            if (zero_info.zero_addr == (zero_base_addr + probe->index * sizeof(struct cmd_zero_info))) {
                ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, probe, SR_CONF_ZERO_SET));
                if (ret != SR_OK) {
                    sr_err("Set Zero command failed!");
                    return ret;
                }
            } else {
                devc->zero = TRUE;
                sr_info("Zero have not been setted!");
            }
        }
    }

    comb_info.comb_addr = comb_base_addr;
    if ((ret = command_rd_nvm(usb->devhdl, (unsigned char *)&comb_info, comb_info.comb_addr, sizeof(struct cmd_comb_info))) != SR_OK) {
        sr_err("Send Get Comb Command Failed!");
    } else {
        if (comb_info.comb_addr == comb_base_addr) {
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, NULL, SR_CONF_COMB_SET));
            if (ret != SR_OK) {
                sr_err("Set Comb command failed!");
                return ret;
            }
        } else {
            devc->zero = TRUE;
            sr_info("Comb have not been setted!");
        }
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
        *data = g_variant_new_byte(devc->trigger_source);
        break;
    case SR_CONF_TRIGGER_VALUE:
        if (!ch)
            return SR_ERR;
        *data = g_variant_new_uint16(ch->trig_value);
        break;
    case SR_CONF_HORIZ_TRIGGERPOS:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        uint16_t channel_cnt = 0;
        GSList *l;
        for (l = sdi->channels; l; l = l->next) {
            struct sr_channel *probe = (struct sr_channel *)l->data;
            channel_cnt += probe->enabled;
        }
        uint16_t pos = devc->trigger_hpos * channel_cnt * 100 / devc->limit_samples;
        *data = g_variant_new_uint16(pos);
        break;
    case SR_CONF_ZERO:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_boolean(devc->zero);
        break;
    case SR_CONF_STREAM:
        if (!sdi)
            return SR_ERR;
        *data = g_variant_new_boolean(FALSE);
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

    (void)cg;

    if (sdi->status != SR_ST_ACTIVE)
        return SR_ERR;

	devc = sdi->priv;
    usb = sdi->conn;

    if (id == SR_CONF_SAMPLERATE) {
		devc->cur_samplerate = g_variant_get_uint64(data);
        if (sdi->mode == LOGIC) {
            if (devc->cur_samplerate >= SR_MHZ(200)) {
                adjust_probes(sdi, SR_MHZ(1600)/devc->cur_samplerate);
            } else {
                adjust_probes(sdi, 16);
            }
            ret = SR_OK;
        } else if(sdi->mode == DSO) {
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, 0, SR_CONF_SAMPLERATE));
        }

    } else if (id == SR_CONF_CLOCK_TYPE) {
        devc->clock_type = g_variant_get_boolean(data);
        ret = SR_OK;
    } else if (id == SR_CONF_CLOCK_EDGE) {
        devc->clock_edge = g_variant_get_boolean(data);
        ret = SR_OK;
    } else if (id == SR_CONF_INSTANT) {
        devc->instant = g_variant_get_boolean(data);
        int num_probes = 0;
        GSList *l;
        for (l = sdi->channels; l; l = l->next) {
            struct sr_channel *probe = (struct sr_channel *)l->data;
            num_probes += probe->enabled;
        }
        if (num_probes != 0) {
            if (devc->instant)
                devc->limit_samples = DSCOPE_INSTANT_DEPTH / num_probes;
            else
                devc->limit_samples = DSCOPE_MAX_DEPTH / num_probes;
        }
        ret = SR_OK;
    } else if (id == SR_CONF_LIMIT_SAMPLES) {
		devc->limit_samples = g_variant_get_uint64(data);
        ret = SR_OK;
    } else if (id == SR_CONF_DEVICE_MODE) {
        sdi->mode = g_variant_get_int16(data);
        ret = SR_OK;
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
        ret = SR_OK;
        if (!strcmp(stropt, opmodes[SR_OP_NORMAL])) {
            devc->op_mode = SR_OP_NORMAL;
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
        ret = SR_OK;
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
            char filename[256];
            sprintf(filename,"%s%s",config_path,devc->profile->fpga_bit33);
            const char *fpga_bit = filename;
            ret = fpga_config(usb->devhdl, fpga_bit);
            if (ret != SR_OK) {
                sr_err("Configure FPGA failed!");
            }
        }
        sr_dbg("%s: setting threshold to %d",
            __func__, devc->th_level);
    }  else if (id == SR_CONF_FILTER) {
        stropt = g_variant_get_string(data, NULL);
        ret = SR_OK;
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
        }
        if (ret == SR_OK)
            sr_dbg("%s: setting ENABLE of channel %d to %d",
                __func__, ch->index, ch->enabled);
        else
            sr_dbg("%s: setting ENABLE of channel %d to %d",
                __func__, ch->index, ch->enabled);
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
            sr_dbg("%s: setting VPOS of channel %d to %d mv",
                __func__, ch->index, ch->vpos);
        else
            sr_dbg("%s: setting VPOS of channel %d to %d mv failed",
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
        ch->trig_value = g_variant_get_uint16(data);
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
        uint16_t channel_cnt = 0;
        GSList *l;
        for (l = sdi->channels; l; l = l->next) {
            struct sr_channel *probe = (struct sr_channel *)l->data;
            channel_cnt += probe->enabled;
        }
        devc->trigger_hpos = g_variant_get_uint16(data) * channel_cnt * devc->limit_samples / 200.0;
        if (sdi->mode == DSO) {
            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, 1, SR_CONF_HORIZ_TRIGGERPOS));
        }
        if (ret == SR_OK)
            sr_dbg("%s: setting DSO Horiz Trigger Position to %d",
                __func__, devc->trigger_hpos);
        else
            sr_dbg("%s: setting DSO Horiz Trigger Position to %d failed",
                __func__, devc->trigger_hpos);
    } else if (id == SR_CONF_ZERO) {
        devc->zero = g_variant_get_boolean(data);
    } else if (id == SR_CONF_ZERO_SET) {
        GSList *l;
        for(l = sdi->channels; l; l = l->next) {
            struct sr_channel *probe = (struct sr_channel *)l->data;
            zero_info.zero_addr = zero_base_addr + probe->index * sizeof(struct cmd_zero_info);
            zero_info.vpos_l = (probe->index == 0) ? mstatus.ch0_vpos_mid : mstatus.ch1_vpos_mid;
            zero_info.vpos_h = (probe->index == 0) ? mstatus.ch0_vpos_mid >> 8 : mstatus.ch1_vpos_mid >> 8;
            zero_info.voff_l = (probe->index == 0) ? mstatus.ch0_voff_mid : mstatus.ch1_voff_mid;
            zero_info.voff_h = (probe->index == 0) ? mstatus.ch0_voff_mid >> 8 : mstatus.ch1_voff_mid >> 8;
            zero_info.vcntr_l = (probe->index == 0) ? mstatus.ch0_vcntr : mstatus.ch1_vcntr;
            zero_info.vcntr_h = (probe->index == 0) ? mstatus.ch0_vcntr >> 8 : mstatus.ch1_vcntr >> 8;
            zero_info.adc_off = (probe->index == 0) ? mstatus.ch0_adc_off + (mstatus.ch0_adc_sign << 7) : mstatus.ch1_adc_off + (mstatus.ch1_adc_sign << 7);
            ret = command_wr_nvm(usb->devhdl, (unsigned char *)&zero_info, sizeof(struct cmd_zero_info));
            if (ret != SR_OK)
                sr_err("DSO channel %d Set Zero command failed!", probe->index);
        }
        comb_info.comb_addr = comb_base_addr;
        comb_info.comb0_low_off = mstatus.comb0_off;
        comb_info.comb0_hig_off = mstatus.comb0_off >> 8;
        comb_info.comb1_low_off = mstatus.comb1_off;
        comb_info.comb1_hig_off = mstatus.comb1_off >> 8;
        comb_info.comb_sign = mstatus.comb_sign;
        ret = command_wr_nvm(usb->devhdl, (unsigned char *)&comb_info, sizeof(struct cmd_comb_info));
        if (ret != SR_OK)
            sr_err("DSO Set Comb command failed!");
        else
            devc->zero = FALSE;
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
        *data = g_variant_new_strv(opmodes, ARRAY_SIZE(opmodes));
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

static void abort_acquisition(struct DSL_context *devc)
{
    int i;
	int ret;
	struct sr_usb_dev_inst *usb;

    devc->num_samples = -1;

	sr_info("%s: Stopping", __func__);

    /* Stop GPIF acquisition */
    usb = ((struct sr_dev_inst *)devc->cb_data)->conn;
    if ((ret = command_stop_acquisition (usb->devhdl)) != SR_OK)
        sr_err("Stop DSCope acquisition failed!");
    else
        sr_info("Stop DSCope acquisition!");

    ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen((struct sr_dev_inst *)devc->cb_data, NULL, SR_CONF_ZERO_OVER));
    if (ret != SR_OK)
        sr_err("DSO zero over command failed!");

    /* Cancel exist transfers */
    if (devc->num_transfers)
        for (i = devc->num_transfers - 1; i >= 0; i--) {
            if (devc->transfers[i])
                libusb_cancel_transfer(devc->transfers[i]);
        }
}

static void finish_acquisition(struct DSL_context *devc)
{
    struct sr_datafeed_packet packet;
    int i, ret;
    struct sr_usb_dev_inst *usb;

    sr_err("finish acquisition: send SR_DF_END packet");
    /* Terminate session. */
    packet.type = SR_DF_END;
    sr_session_send(devc->cb_data, &packet);

    sr_err("finish acquisition: remove fds from polling");
    /* Remove fds from polling. */
    for (i = 0; devc->usbfd[i] != -1; i++)
        sr_source_remove(devc->usbfd[i]);
    g_free(devc->usbfd);
	
    if (devc->num_transfers != 0) {
        devc->num_transfers = 0;
        g_free(devc->transfers);
    }
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
    struct sr_datafeed_meta meta;
    struct DSL_context *devc;
	int trigger_offset, i, sample_width, cur_sample_count;
	int trigger_offset_bytes;
	uint8_t *cur_buf;
    //GTimeVal cur_time;

    //g_get_current_time(&cur_time);
    //sr_info("receive_transfer: current time %d sec %d usec", cur_time.tv_sec, cur_time.tv_usec);


	devc = transfer->user_data;

	/*
	 * If acquisition has already ended, just free any queued up
	 * transfer that come in.
	 */
    if (devc->num_samples == -1) {
        free_transfer(transfer);
        return;
    }

    sr_info("receive_transfer(): status %d; timeout %d; received %d bytes.",
        transfer->status, transfer->timeout, transfer->actual_length);

    /* Save incoming transfer before reusing the transfer struct. */
    cur_buf = transfer->buffer;
    sample_width = 2;
    cur_sample_count = transfer->actual_length / sample_width;

    switch (transfer->status) {
    case LIBUSB_TRANSFER_NO_DEVICE:
        //abort_acquisition(devc);
        free_transfer(transfer);
        devc->status = DSL_ERROR;
        return;
    case LIBUSB_TRANSFER_COMPLETED:
    case LIBUSB_TRANSFER_TIMED_OUT: /* We may have received some data though. */
        break;
    default:
        packet_has_error = TRUE;
        break;
    }

    if (transfer->actual_length == 0 || packet_has_error) {
        devc->empty_transfer_count++;
        if (devc->empty_transfer_count > MAX_EMPTY_TRANSFERS) {
            /*
             * The FX2 gave up. End the acquisition, the frontend
             * will work out that the samplecount is short.
             */
            //abort_acquisition(devc);
            free_transfer(transfer);
            devc->status = DSL_ERROR;
        } else {
            resubmit_transfer(transfer);
        }
        return;
    } else {
        devc->empty_transfer_count = 0;
    }

    trigger_offset = 0;
    if (devc->trigger_stage >= 0) {
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
                    sr_session_send(devc->cb_data, &packet);

                    /*
                     * Send the samples that triggered it,
                     * since we're skipping past them.
                     */
                    packet.type = SR_DF_LOGIC;
                    packet.payload = &logic;
                    logic.unitsize = sizeof(*devc->trigger_buffer);
                    logic.length = devc->trigger_stage * logic.unitsize;
                    logic.data = devc->trigger_buffer;
                    sr_session_send(devc->cb_data, &packet);

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

    if (devc->trigger_stage == TRIGGER_FIRED) {
        /* Send the incoming transfer to the session bus. */
        trigger_offset_bytes = trigger_offset * sample_width;
        // check packet type
        if ((*(struct sr_dev_inst *)(devc->cb_data)).mode == LOGIC) {
            packet.type = SR_DF_LOGIC;
            packet.payload = &logic;
            logic.length = transfer->actual_length - trigger_offset_bytes;
            logic.unitsize = sample_width;
            logic.data_error = 0;
            logic.data = cur_buf + trigger_offset_bytes;
        } else if ((*(struct sr_dev_inst *)(devc->cb_data)).mode == DSO) {
            uint16_t channel_cnt = 0;
            uint16_t channel_en_cnt = 0;
            GSList *l;
            struct sr_dev_inst *sdi = devc->cb_data;
            for (l = sdi->channels; l; l = l->next) {
                struct sr_channel *probe = (struct sr_channel *)l->data;
                channel_cnt ++;
                channel_en_cnt += probe->enabled;
            }
            if (channel_en_cnt == 0)
                channel_en_cnt = 1;

            if (!devc->instant) {
                const uint32_t mstatus_offset = devc->limit_samples / (channel_cnt/channel_en_cnt);
                mstatus.ch0_max = *((const uint8_t*)cur_buf + mstatus_offset*2 + 1*2);
                mstatus.ch0_min = *((const uint8_t*)cur_buf + mstatus_offset*2 + 3);
                mstatus.ch0_period = *((const uint32_t*)cur_buf + mstatus_offset/2 + 2/2);
                mstatus.ch0_period += ((uint64_t)*((const uint32_t*)cur_buf + mstatus_offset/2 + 4/2)) << 32;
                mstatus.ch0_pcnt = *((const uint32_t*)cur_buf + mstatus_offset/2 + 6/2);
                mstatus.ch1_max = *((const uint8_t*)cur_buf + mstatus_offset*2 + 9*2);
                mstatus.ch1_min = *((const uint8_t*)cur_buf + mstatus_offset*2 + 19);
                mstatus.ch1_period = *((const uint32_t*)cur_buf + mstatus_offset/2 + 10/2);
                mstatus.ch1_period += ((uint64_t)*((const uint32_t*)cur_buf + mstatus_offset/2 + 12/2)) << 32;
                mstatus.vlen = *((const uint32_t*)cur_buf + mstatus_offset/2 + 16/2) & 0x7fffffff;
                mstatus.stream_mode = *((const uint32_t*)cur_buf + mstatus_offset/2 + 16/2) & 0x80000000;
                mstatus.sample_divider = *((const uint32_t*)cur_buf + mstatus_offset/2 + 18/2);
                mstatus.zeroing = (*((const uint16_t*)cur_buf + mstatus_offset + 128) & 0x8000) != 0;
                mstatus.ch0_vpos_mid = *((const uint16_t*)cur_buf + mstatus_offset + 128) & 0x7fff;
                mstatus.ch0_voff_mid = *((const uint16_t*)cur_buf + mstatus_offset + 129);
                mstatus.ch0_vcntr = *((const uint16_t*)cur_buf + mstatus_offset + 130);
                mstatus.ch0_adc_off = *((const uint8_t*)cur_buf + mstatus_offset*2 + 131*2);
                mstatus.ch0_adc_sign = *((const uint8_t*)cur_buf + mstatus_offset*2 + 131*2+1);
                mstatus.ch1_vpos_mid = *((const uint16_t*)cur_buf + mstatus_offset + 132);
                mstatus.ch1_voff_mid = *((const uint16_t*)cur_buf + mstatus_offset + 133);
                mstatus.ch1_vcntr = *((const uint16_t*)cur_buf + mstatus_offset + 134);
                mstatus.ch1_adc_off = *((const uint8_t*)cur_buf + mstatus_offset*2 + 135*2);
                mstatus.ch1_adc_sign = *((const uint8_t*)cur_buf + mstatus_offset*2 + 135*2+1);
                mstatus.comb0_off = *((const uint16_t*)cur_buf + mstatus_offset + 136);
                mstatus.comb1_off = *((const uint16_t*)cur_buf + mstatus_offset + 137);
                mstatus.comb_sign = *((const uint8_t*)cur_buf + mstatus_offset*2 + 138*2);
            } else {
                mstatus.vlen = instant_buffer_size;
            }

            const uint32_t divider = devc->zero ? 0x1 : (uint32_t)ceil(DSCOPE_MAX_SAMPLERATE * 1.0 / devc->cur_samplerate / channel_en_cnt);
            if ((mstatus.sample_divider == divider &&
                mstatus.vlen != 0 &&
                mstatus.vlen <= (transfer->actual_length - 512) / sample_width) ||
                devc->instant) {
                mstatus_valid = devc->instant ? FALSE : TRUE;
                packet.type = SR_DF_DSO;
                packet.payload = &dso;
                dso.probes = (*(struct sr_dev_inst *)(devc->cb_data)).channels;
                //dso.num_samples = (transfer->actual_length - 512) / sample_width;
                cur_sample_count = 2 * mstatus.vlen / channel_en_cnt ;
                dso.num_samples = cur_sample_count;
                dso.mq = SR_MQ_VOLTAGE;
                dso.unit = SR_UNIT_VOLT;
                dso.mqflags = SR_MQFLAG_AC;
                dso.data = cur_buf + trigger_offset_bytes;
            } else {
                mstatus_valid = FALSE;
            }
        } else {
            packet.type = SR_DF_ANALOG;
            packet.payload = &analog;
            analog.probes = (*(struct sr_dev_inst *)(devc->cb_data)).channels;
            analog.num_samples = transfer->actual_length / sample_width;
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
            sr_session_send(devc->cb_data, &packet);
        }

        devc->num_samples += cur_sample_count;
        if (((*(struct sr_dev_inst *)(devc->cb_data)).mode == LOGIC || devc->instant) &&
            devc->limit_samples &&
            (unsigned int)devc->num_samples >= devc->limit_samples) {
            //abort_acquisition(devc);
            free_transfer(transfer);
            devc->status = DSL_STOP;
            return;
        }
    } else {
        /*
         * TODO: Buffer pre-trigger data in capture
         * ratio-sized buffer.
         */
    }

    resubmit_transfer(transfer);
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
    num_transfers = buffer_cnt;
    uint16_t channel_en_cnt = 0;
    uint16_t channel_cnt = 0;
    GSList *l;
    for (l = sdi->channels; l; l = l->next) {
        struct sr_channel *probe = (struct sr_channel *)l->data;
        channel_en_cnt += probe->enabled;
        channel_cnt++;
    }
    if (devc->instant)
        dso_buffer_size = instant_buffer_size * channel_cnt;
    else
        dso_buffer_size = devc->limit_samples * channel_en_cnt + 512;
    size = (sdi->mode == ANALOG) ? cons_buffer_size : ((sdi->mode == DSO) ? dso_buffer_size : buffer_size);
    devc->submitted_transfers = 0;

    devc->transfers = g_try_malloc0(sizeof(*devc->transfers) * num_transfers);
    if (!devc->transfers) {
        sr_err("USB transfers malloc failed.");
        return SR_ERR_MALLOC;
    }

    devc->num_transfers = num_transfers;
    for (i = 0; i < num_transfers; i++) {
        if (!(buf = g_try_malloc(size))) {
            sr_err("USB transfer buffer malloc failed.");
            return SR_ERR_MALLOC;
        }
        transfer = libusb_alloc_transfer(0);
        libusb_fill_bulk_transfer(transfer, usb->devhdl,
                6 | LIBUSB_ENDPOINT_IN, buf, size,
                receive_transfer, devc, 0);
        if ((ret = libusb_submit_transfer(transfer)) != 0) {
            sr_err("Failed to submit transfer: %s.",
                   libusb_error_name(ret));
            libusb_free_transfer(transfer);
            g_free(buf);
            abort_acquisition(devc);
            return SR_ERR;
        }
        devc->transfers[i] = transfer;
        devc->submitted_transfers++;
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

    (void)fd;
    (void)revents;

    drvc = di->priv;
    devc = sdi->priv;

    if (devc->num_samples != -1 &&
        (devc->status == DSL_STOP || devc->status == DSL_ERROR)) {
        sr_info("%s: Stopping", __func__);
        abort_acquisition(devc);
    }

    tv.tv_sec = tv.tv_usec = 0;
    libusb_handle_events_timeout_completed(drvc->sr_ctx->libusb_ctx, &tv, &completed);

    return TRUE;
}

static void receive_trigger_pos(struct libusb_transfer *transfer)
{
    struct DSL_context *devc;
    struct sr_datafeed_packet packet;
    struct sr_datafeed_logic logic;
    struct sr_datafeed_dso dso;
    struct sr_datafeed_analog analog;
    struct ds_trigger_pos *trigger_pos;
    int ret;

    devc = transfer->user_data;
    sr_info("receive_trigger_pos(): status %d; timeout %d; received %d bytes.",
        transfer->status, transfer->timeout, transfer->actual_length);

    if (devc->num_samples == -1) {
        free_transfer(transfer);
        return;
    }

    trigger_pos = (struct ds_trigger_pos *)transfer->buffer;
    switch (transfer->status) {
    case LIBUSB_TRANSFER_COMPLETED:
        if (transfer->actual_length == sizeof(struct ds_trigger_pos)) {
            packet.type = SR_DF_TRIGGER;
            packet.payload = trigger_pos;
            sr_session_send(devc->cb_data, &packet);

            devc->status = DSL_TRIGGERED;
            free_transfer(transfer);
            devc->num_transfers = 0;
            devc->empty_transfer_count = 0;
        } else {
            free_transfer(transfer);
            devc->status = DSL_ERROR;
        }
        break;
    case LIBUSB_TRANSFER_TIMED_OUT:
        devc->empty_transfer_count++;
        if (devc->empty_transfer_count > MAX_EMPTY_TRANSFERS) {
            /*
             * The FX2 gave up. End the acquisition, the frontend
             * will work out that the samplecount is short.
             */
            //abort_acquisition(devc);
            free_transfer(transfer);
            devc->status = DSL_ERROR;
        } else {
            resubmit_transfer(transfer);
        }
        break;
    case LIBUSB_TRANSFER_CANCELLED:
        resubmit_transfer(transfer);
        break;
    default:
        //abort_acquisition(devc);
        free_transfer(transfer);
        devc->status = DSL_ERROR;
        break;
    }

    if (devc->status == DSL_TRIGGERED) {
        if ((ret = dev_transfer_start(devc->cb_data)) != SR_OK) {
            sr_err("%s: could not start data transfer"
                   "(%d)%d", __func__, ret, errno);
        }
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

	/* Configures devc->trigger_* and devc->sample_wide */
    if (configure_probes(sdi) != SR_OK) {
        sr_err("Failed to configure probes.");
        return SR_ERR;
	}

    /* Stop Previous GPIF acquisition */
    if ((ret = command_stop_acquisition (usb->devhdl)) != SR_OK) {
        sr_err("Stop DSCope acquisition failed!");
        abort_acquisition(devc);
        return ret;
    } else {
        sr_info("Stop Previous DSCope acquisition!");
    }

    /* Setting FPGA before acquisition start*/
    if ((ret = command_fpga_setting(usb->devhdl, sizeof(struct DSL_setting) / sizeof(uint16_t))) != SR_OK) {
        sr_err("Send FPGA setting command failed!");
    } else {
        if ((ret = fpga_setting(sdi)) != SR_OK) {
            sr_err("Configure FPGA failed!");
            abort_acquisition(devc);
            return ret;
        }
    }

//    if (sdi->mode == DSO) {
//        GSList *l;
//        for(l = sdi->channels; l; l = l->next) {
//            struct sr_channel *probe = (struct sr_channel *)l->data;
//            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, probe, SR_CONF_COUPLING));
//            if (ret != SR_OK) {
//                sr_err("Set COUPLING of channel %d command failed!", probe->index);
//                return ret;
//            }
//            ret = command_dso_ctrl(usb->devhdl, dso_cmd_gen(sdi, probe, SR_CONF_VPOS));
//            if (ret != SR_OK) {
//                sr_err("Set VDIV of channel %d command failed!", probe->index);
//                return ret;
//            }
//        }
//    }

    if ((ret = command_start_acquisition (usb->devhdl,
        devc->cur_samplerate, devc->sample_wide, (sdi->mode == LOGIC))) != SR_OK) {
        abort_acquisition(devc);
        return ret;
    }

    test_sample_value = 0;

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
	
    /* poll trigger status transfer*/
    if (!(trigger_pos = g_try_malloc0(sizeof(struct ds_trigger_pos)))) {
        sr_err("USB trigger_pos buffer malloc failed.");
        return SR_ERR_MALLOC;
    }
    devc->transfers = g_try_malloc0(sizeof(*devc->transfers));
    if (!devc->transfers) {
        sr_err("USB trigger_pos transfer malloc failed.");
        return SR_ERR_MALLOC;
    }
    devc->num_transfers = 1;
    transfer = libusb_alloc_transfer(0);
    libusb_fill_bulk_transfer(transfer, usb->devhdl,
            6 | LIBUSB_ENDPOINT_IN, trigger_pos, sizeof(struct ds_trigger_pos),
            receive_trigger_pos, devc, 0);
    if ((ret = libusb_submit_transfer(transfer)) != 0) {
        sr_err("Failed to submit trigger_pos transfer: %s.",
               libusb_error_name(ret));
        libusb_free_transfer(transfer);
        g_free(trigger_pos);
        abort_acquisition(devc);
        return SR_ERR;
    }
    devc->transfers[0] = transfer;
    devc->submitted_transfers++;

    devc->status = DSL_START;
    mstatus_valid = FALSE;
    mstatus.zeroing = devc->zero;
	/* Send header packet to the session bus. */
    //std_session_send_df_header(cb_data, LOG_PREFIX);
    std_session_send_df_header(sdi, LOG_PREFIX);

    return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi, void *cb_data)
{
	(void)cb_data;

    struct DSL_context *devc;

    devc = sdi->priv;
    devc->status = DSL_STOP;

    //abort_acquisition(sdi->priv);

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
    (void)begin;
    (void)end;
    if (sdi) {
        struct DSL_context *devc;
        devc = sdi->priv;
        if (mstatus_valid) {
            *status = mstatus;
            if (devc->zero)
                return SR_ERR;
            else
                return SR_OK;
        } else {
            return SR_ERR;
        }
    } else {
        return SR_ERR;
    }
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
