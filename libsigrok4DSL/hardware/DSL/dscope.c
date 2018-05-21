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
    {"DAQ", ANALOG},
    {"OSC", DSO},
};

enum {
    /** Normal */
    OP_NORMAL = 0,
    /** Internal pattern test mode */
    OP_INTEST = 1,
};
static const char *opmodes[] = {
    "Normal",
    "Internal Test",
};

static const char *thresholds[] = {
    "1.8/2.5/3.3V Level",
    "5.0V Level",
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

static const int32_t probeOptions[] = {
    SR_CONF_PROBE_COUPLING,
    SR_CONF_PROBE_VDIV,
    SR_CONF_PROBE_MAP_UNIT,
    SR_CONF_PROBE_MAP_MIN,
    SR_CONF_PROBE_MAP_MAX,
};

static const int32_t probeSessions[] = {
    SR_CONF_PROBE_COUPLING,
    SR_CONF_PROBE_VDIV,
    SR_CONF_PROBE_MAP_UNIT,
    SR_CONF_PROBE_MAP_MIN,
    SR_CONF_PROBE_MAP_MAX,
};

static const uint8_t probeCoupling[] = {
    SR_DC_COUPLING,
    SR_AC_COUPLING,
};

static const uint64_t probeVdivs[] = {
    SR_mV(10),
    SR_mV(20),
    SR_mV(50),
    SR_mV(100),
    SR_mV(200),
    SR_mV(500),
    SR_V(1),
    SR_V(2),
};

static const char *probeMapUnits[] = {
    "V",
    "A",
    "°C",
    "°F",
    "g",
    "m",
    "m/s",
};

static const uint64_t samplerates[] = {
    SR_HZ(10),
    SR_HZ(20),
    SR_HZ(50),
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
    SR_MB(64),
    SR_MB(128),
};

static const uint8_t zero_base_addr = 0x40;
static const uint8_t zero_big_addr = 0x20;

SR_PRIV struct sr_dev_driver DSCope_driver_info;
static struct sr_dev_driver *di = &DSCope_driver_info;

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
    {0, 0, 0, 0, 0},
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
    {0, 0, 0, 0, 0},
};

static struct DSL_vga* get_vga_ptr(const struct sr_dev_inst *sdi)
{
    struct DSL_vga *vga_ptr = NULL;
    if (strcmp(sdi->model, "DSCope") == 0)
        vga_ptr = DSCope_vga;
    else
        vga_ptr = DSCope20_vga;

    return vga_ptr;
}

static uint16_t get_default_trans(const struct sr_dev_inst *sdi)
{
    uint16_t trans = 1;
    if (strcmp(sdi->model, "DSCope") == 0)
        trans = DSCOPE_DEFAULT_TRANS;
    else
        trans = DSCOPE20_DEFAULT_TRANS;

    return trans;
}

static uint16_t get_default_voff(const struct sr_dev_inst *sdi, int ch_index)
{
    uint16_t voff = 0;
    if (strcmp(sdi->model, "DSCope") == 0) {
        voff = DSCOPE_DEFAULT_VOFF;
    } else {
        if (ch_index == 1)
            voff = CALI_VOFF_RANGE - DSCOPE20_DEFAULT_VOFF;
        else
            voff = DSCOPE20_DEFAULT_VOFF;
    }

    return voff;
}

static uint64_t get_default_vgain(const struct sr_dev_inst *sdi, unsigned int num)
{
    uint64_t vgain = 0;
    if (strcmp(sdi->model, "DSCope") == 0) {
        assert(num < sizeof(DSCOPE_DEFAULT_VGAIN));
        vgain = DSCOPE_DEFAULT_VGAIN[num];
    } else {
        assert(num < sizeof(DSCOPE20_DEFAULT_VGAIN));
        vgain = DSCOPE20_DEFAULT_VGAIN[num];
    }

    return vgain;
}

static int counts_size(const struct sr_dev_inst *sdi)
{
    if (strcmp(sdi->model, "DSCope") == 0 ||
        strcmp(sdi->model, "DSCope20") == 0 ||
        strcmp(sdi->model, "DSCope B20") == 0) {
        if (sdi->mode == DSO)
            return 15;
        else if (sdi->mode == ANALOG)
            return ARRAY_SIZE(samplecounts);
        else
            return 0;
    } else {
        return 0;
    }
}

static void probe_init(struct sr_dev_inst *sdi)
{
    int i;
    GSList *l;
    for (l = sdi->channels; l; l = l->next) {
        struct sr_channel *probe = (struct sr_channel *)l->data;
        probe->vdiv = 1000;
        probe->vfactor = 1;
        probe->vpos = 0;
        probe->coupling = SR_DC_COUPLING;
        probe->trig_value = 0x80;
        probe->vpos_trans = get_default_trans(sdi);
        probe->ms_show = TRUE;
        for (i = DSO_MS_BEGIN; i < DSO_MS_END; i++)
            probe->ms_en[i] = default_ms_en[i];
        probe->map_unit = probeMapUnits[0];
        probe->map_min = -1;
        probe->map_max = 1;
    }
}

static int setup_probes(struct sr_dev_inst *sdi, int num_probes)
{
    uint16_t j;
    struct sr_channel *probe;

    for (j = 0; j < num_probes; j++) {
        if (!(probe = sr_channel_new(j, (sdi->mode == LOGIC) ? SR_CHANNEL_LOGIC :
                                        ((sdi->mode == DSO) ? SR_CHANNEL_DSO : SR_CHANNEL_ANALOG),
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

static struct DSL_context *DSCope_dev_new(const struct sr_dev_inst *sdi)
{
    struct DSL_context *devc;

    if (!(devc = g_try_malloc(sizeof(struct DSL_context)))) {
        sr_err("Device context malloc failed.");
		return NULL;
	}

    devc->channel = NULL;
    devc->profile = NULL;
    devc->fw_updated = 0;
    devc->cur_samplerate = DSCOPE_MAX_SAMPLERATE / MAX_DSO_PROBES_NUM;
    devc->limit_samples = DSCOPE_MAX_DEPTH / MAX_DSO_PROBES_NUM;
    devc->sample_wide = TRUE;
    devc->clock_type = FALSE;
    devc->clock_edge = FALSE;
    devc->instant = FALSE;
    devc->op_mode = OP_NORMAL;
    devc->test_mode = SR_TEST_NONE;
    devc->stream = FALSE;
    devc->samplerates_size = ARRAY_SIZE(samplerates);
    devc->samplecounts_size = counts_size(sdi);
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
    devc->unit_bits = 8;
    devc->trigger_margin = 8;
    devc->trigger_channel = 0;
    devc->rle_mode = FALSE;
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
        if (setup_probes(sdi, 2) != SR_OK)
            return NULL;

        devc = DSCope_dev_new(sdi);
        devc->profile = prof;
        sdi->priv = devc;
        drvc->instances = g_slist_append(drvc->instances, sdi);
        //devices = g_slist_append(devices, sdi);

		if (dsl_check_conf_profile(devlist[i])) {
			/* Already has the firmware, so fix the new address. */
            sr_dbg("Found an DSCope device.");
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
    (void)sdi;
    GSList *l = NULL;
    unsigned int i;

    for(i = 0; i < ARRAY_SIZE(mode_list); i++) {
        l = g_slist_append(l, &mode_list[i]);
    }

    return l;
}

static uint64_t dso_vga(const struct sr_dev_inst *sdi, const struct sr_channel* ch)
{
    int i;
    struct DSL_vga *vga_ptr = get_vga_ptr(sdi);
    for (i = 0; vga_ptr && (vga_ptr+i)->key; i++) {
        if ((vga_ptr+i)->key == ch->vdiv)
            return (ch->index == 0) ? (vga_ptr+i)->vgain0 : (vga_ptr+i)->vgain1;
    }

    return 0;
}

static uint64_t dso_voff(const struct sr_dev_inst *sdi, const struct sr_channel* ch)
{
    int i;
    struct DSL_vga *vga_ptr =  get_vga_ptr(sdi);
    for (i = 0; vga_ptr && (vga_ptr+i)->key; i++) {
        if ((vga_ptr+i)->key == ch->vdiv)
            return (ch->index == 0) ? (vga_ptr+i)->voff0 : (vga_ptr+i)->voff1;
    }
    return 0;
}

static uint64_t dso_vpos(const struct sr_dev_inst *sdi, const struct sr_channel* ch)
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
    } else {
        vpos = ((ch->vdiv*5.0) - voltage)/(ch->vdiv*10.0)*ch->vpos_trans;
    }

    const uint64_t voff = dso_voff(sdi, ch);
    if (strcmp(sdi->model, "DSCope") == 0)
        return ((vpos_coarse+DSCOPE_CONSTANT_BIAS+(voff>>10)) << 16)+vpos_fine+(voff&0x03ff);
    else
        return vpos+voff;
}

static uint64_t dso_cmd_gen(const struct sr_dev_inst *sdi, struct sr_channel* ch, int id)
{
    struct DSL_context *devc;
    uint64_t cmd = 0;
    uint64_t vpos;
    GSList *l;
    const int ch_bit = 7;
    devc = sdi->priv;

    switch (id) {
    case SR_CONF_PROBE_EN:
    case SR_CONF_PROBE_COUPLING:
        if (devc->zero || dsl_en_ch_num(sdi) == 2) {
            cmd += 0x0E00;
            //cmd += 0x000;
        } else if (dsl_en_ch_num(sdi) == 1) {
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
    case SR_CONF_PROBE_VDIV:
    case SR_CONF_TIMEBASE:
        cmd += 0x8;
        cmd += ch->index << ch_bit;
        //  --VGAIN
        cmd += dso_vga(sdi, ch);
        break;
    case SR_CONF_PROBE_VPOS:
        cmd += 0x10;
        cmd += ch->index << ch_bit;
        vpos = dso_vpos(sdi, ch);
        cmd += (vpos << 8);
        break;
    case SR_CONF_SAMPLERATE:
        cmd += 0x18;
        uint32_t divider = devc->zero ? 0x1 : (uint32_t)ceil(DSCOPE_MAX_SAMPLERATE * 1.0 / devc->cur_samplerate / dsl_en_ch_num(sdi));
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
        ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe, SR_CONF_PROBE_VPOS));
        if (ret != SR_OK) {
            sr_err("Set VPOS of channel %d command failed!", probe->index);
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

static gboolean dso_load_eep(struct sr_dev_inst *sdi, struct sr_channel *probe, gboolean fpga_done)
{
    int ret, i;
    uint16_t real_zero_addr;

    struct cmd_zero_info zero_info;
    uint8_t dst_addr = (zero_base_addr +
                        probe->index * (sizeof(struct cmd_zero_info) + sizeof(struct cmd_vga_info)));
    zero_info.zero_addr = dst_addr;
    if (strcmp(sdi->model, "DSCope20") == 0 ||
        strcmp(sdi->model, "DSCope") == 0)
        real_zero_addr = zero_info.zero_addr;
    else
        real_zero_addr = (zero_big_addr << 8) + zero_info.zero_addr;
    if ((ret = dsl_rd_nvm(sdi, (unsigned char *)&zero_info, real_zero_addr, sizeof(struct cmd_zero_info))) != SR_OK) {
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
                         ret = dsl_wr_reg(sdi, COMB_ADDR + probe->index*2, i);
                         int value = i+i*slope+probe->comb_diff_top*0.5+0.5;
                         value = (value < 0) ? 0 :
                                 (value > 255) ? 255 : value;
                         ret = dsl_wr_reg(sdi, COMB_ADDR + probe->index*2 + 1, value);
                     }
                 }
             }
        } else {
            return FALSE;
        }
    }

    struct cmd_vga_info vga_info;
    vga_info.vga_addr = dst_addr + sizeof(struct cmd_zero_info);
    if (strcmp(sdi->model, "DSCope20") == 0 ||
        strcmp(sdi->model, "DSCope") == 0)
        real_zero_addr = vga_info.vga_addr;
    else
        real_zero_addr = (zero_big_addr << 8) + vga_info.vga_addr;
    if ((ret = dsl_rd_nvm(sdi, (unsigned char *)&vga_info, real_zero_addr, sizeof(struct cmd_vga_info))) != SR_OK) {
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
    unsigned int i;
    struct DSL_vga *vga_ptr;
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
        case SR_CONF_CALI:
            if (!sdi)
                return SR_ERR;
            devc = sdi->priv;
            *data = g_variant_new_boolean(devc->cali);
            break;
        case SR_CONF_TEST:
            if (!sdi)
                return SR_ERR;
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
            *data = g_variant_new_uint64(DSCOPE_MAX_SAMPLERATE);
            break;
        case SR_CONF_MAX_DSO_SAMPLELIMITS:
            if (!sdi)
                return SR_ERR;
            *data = g_variant_new_uint64(DSCOPE_MAX_DEPTH);
            break;
        case SR_CONF_HW_DEPTH:
            if (!sdi)
                return SR_ERR;
            *data = g_variant_new_uint64(DSCOPE_INSTANT_DEPTH);
            break;
        case SR_CONF_PROBE_VGAIN:
            if (!sdi || !ch)
                return SR_ERR;
            *data = g_variant_new_uint64(dso_vga(sdi, ch)>>8);
            break;
        case SR_CONF_PROBE_VGAIN_DEFAULT:
            if (!sdi || !ch)
                return SR_ERR;
            vga_ptr =  get_vga_ptr(sdi);
            for (i = 0; vga_ptr && (vga_ptr+i)->key; i++) {
                if ((vga_ptr+i)->key == ch->vdiv)
                    break;
            }
            *data = g_variant_new_uint64(get_default_vgain(sdi, i)>>8);
            break;
        case SR_CONF_PROBE_VGAIN_RANGE:
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
        case SR_CONF_PROBE_VOFF:
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
        case SR_CONF_PROBE_VOFF_DEFAULT:
            if (!sdi || !ch)
                return SR_ERR;
            *data = g_variant_new_uint16(get_default_voff(sdi, ch->index));
            break;
        case SR_CONF_PROBE_VOFF_RANGE:
            if (!sdi)
                return SR_ERR;
            *data = g_variant_new_uint16(CALI_VOFF_RANGE);
            break;
        case SR_CONF_PROBE_MAP_UNIT:
            if (!sdi || !ch)
                return SR_ERR;
            *data = g_variant_new_string(ch->map_unit);
            break;
        case SR_CONF_PROBE_MAP_MIN:
            if (!sdi || !ch)
                return SR_ERR;
            *data = g_variant_new_double(ch->map_min);
            break;
        case SR_CONF_PROBE_MAP_MAX:
            if (!sdi || !ch)
                return SR_ERR;
            *data = g_variant_new_double(ch->map_max);
            break;
        case SR_CONF_VLD_CH_NUM:
            if (!sdi)
                return SR_ERR;
            devc = sdi->priv;
            *data = g_variant_new_int16(DSCOPE_VLD_CH_NUM);
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
    struct drv_context *drvc;

    (void)cg;

    if (sdi->status != SR_ST_ACTIVE)
        return SR_ERR;

    drvc = di->priv;
	devc = sdi->priv;

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
        ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, ch, SR_CONF_PROBE_VDIV));
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
        ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, ch, SR_CONF_PROBE_COUPLING));
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
        devc->cur_samplerate = g_variant_get_uint64(data);
        if (sdi->mode == LOGIC) {
            if (devc->cur_samplerate >= SR_MHZ(200)) {
                adjust_probes(sdi, SR_MHZ(1600)/devc->cur_samplerate);
            } else {
                adjust_probes(sdi, 16);
            }
        } else if(sdi->mode == DSO) {
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, 0, SR_CONF_SAMPLERATE));
        }
    } else if (id == SR_CONF_INSTANT) {
        devc->instant = g_variant_get_boolean(data);
        if (sdi->mode == DSO && dsl_en_ch_num(sdi) != 0) {
            if (devc->instant)
                devc->limit_samples = DSCOPE_INSTANT_DEPTH / dsl_en_ch_num(sdi);
            else
                devc->limit_samples = DSCOPE_MAX_DEPTH / dsl_en_ch_num(sdi);
        }
    } else if (id == SR_CONF_DEVICE_MODE) {
        sdi->mode = g_variant_get_int16(data);
        if (sdi->mode == DSO) {
            num_probes = devc->profile->dev_caps & DEV_CAPS_16BIT ? MAX_DSO_PROBES_NUM : 1;
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, NULL, SR_CONF_DSO_SYNC));
            if (ret != SR_OK)
                sr_dbg("%s: DSO configuration sync failed", __func__);
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, sdi->channels->data, SR_CONF_PROBE_VDIV));
            if (ret == SR_OK)
                sr_dbg("%s: Initial setting for DSO mode", __func__);
            else
                sr_dbg("%s: Initial setting for DSO mode failed", __func__);
            devc->op_mode = OP_NORMAL;
            devc->test_mode = SR_TEST_NONE;
            devc->stream = FALSE;
            devc->instant = FALSE;
            devc->samplerates_size = ARRAY_SIZE(samplerates);
            devc->cur_samplerate = DSCOPE_MAX_SAMPLERATE / num_probes;
            devc->limit_samples = DSCOPE_MAX_DEPTH / num_probes;
        } else if (sdi->mode == ANALOG) {
            num_probes = devc->profile->dev_caps & DEV_CAPS_16BIT ? MAX_DSO_PROBES_NUM : 1;
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, sdi->channels->data, SR_CONF_PROBE_VDIV));
            if (ret == SR_OK)
                sr_dbg("%s: Initial setting for DSO mode", __func__);
            else
                sr_dbg("%s: Initial setting for DSO mode failed", __func__);
            devc->op_mode = OP_NORMAL;
            devc->test_mode = SR_TEST_NONE;
            devc->stream = TRUE;
            devc->instant = TRUE;
            devc->samplerates_size = 19;
            devc->cur_samplerate = SR_MHZ(1);
            devc->limit_samples = SR_MB(16);
        } else {
            num_probes = 0;
        }
        devc->samplecounts_size = counts_size(sdi);
        sr_dev_probes_free(sdi);
        setup_probes(sdi, num_probes);
        sr_dbg("%s: setting mode to %d", __func__, sdi->mode);
    } else if (id == SR_CONF_OPERATION_MODE) {
        stropt = g_variant_get_string(data, NULL);
        if (!strcmp(stropt, opmodes[OP_NORMAL])) {
            devc->op_mode = OP_NORMAL;
            devc->test_mode = SR_TEST_NONE;
        } else if (!strcmp(stropt, opmodes[OP_INTEST])) {
            devc->op_mode = OP_INTEST;
            devc->test_mode = SR_TEST_INTERNAL;
        } else {
            ret = SR_ERR;
        }
        sr_dbg("%s: setting pattern to %d",
            __func__, devc->op_mode);
    } else if (id == SR_CONF_PROBE_EN) {
        ch->enabled = g_variant_get_boolean(data);

        if (sdi->mode == DSO) {
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, ch, SR_CONF_PROBE_EN));
            if (dsl_en_ch_num(sdi) != 0) {
                ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, 0, SR_CONF_SAMPLERATE));
                devc->limit_samples = DSCOPE_MAX_DEPTH / dsl_en_ch_num(sdi);
            }
        }
        if (ret == SR_OK)
            sr_dbg("%s: setting ENABLE of channel %d to %d",
                __func__, ch->index, ch->enabled);
        else
            sr_dbg("%s: setting ENABLE of channel %d to %d failed",
                __func__, ch->index, ch->enabled);
    } else if (id == SR_CONF_PROBE_VPOS) {
        ch->vpos = g_variant_get_double(data);
        ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, ch, SR_CONF_PROBE_VPOS));
        if (ret == SR_OK)
            sr_dbg("%s: setting VPOS of channel %d to %lf mv",
                __func__, ch->index, ch->vpos);
        else
            sr_dbg("%s: setting VPOS of channel %d to %lf mv failed",
                __func__, ch->index, ch->vpos);
    } else if (id == SR_CONF_TRIGGER_SOURCE) {
        devc->trigger_source = (devc->trigger_source & 0xf0) + (g_variant_get_byte(data) & 0x0f);
        ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, NULL, SR_CONF_TRIGGER_SOURCE));
        if (ret == SR_OK)
            sr_dbg("%s: setting DSO Trigger Source to %d",
                __func__, devc->trigger_source);
        else
            sr_dbg("%s: setting DSO Trigger Source to %d failed",
                __func__, devc->trigger_source);
    } else if (id == SR_CONF_TRIGGER_CHANNEL) {
        devc->trigger_source = (g_variant_get_byte(data) << 4) + (devc->trigger_source & 0x0f);
        ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, NULL, SR_CONF_TRIGGER_SOURCE));
        if (ret == SR_OK)
            sr_dbg("%s: setting DSO Trigger Source to %d",
                __func__, devc->trigger_source);
        else
            sr_dbg("%s: setting DSO Trigger Source to %d failed",
                __func__, devc->trigger_source);
    } else if (id == SR_CONF_ZERO) {
        devc->zero = g_variant_get_boolean(data);
        if (devc->zero) {
            devc->zero_stage = -1;
            devc->zero_pcnt = 0;
            devc->zero_comb = -1;
            GSList *l;
            unsigned int i;
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
            uint16_t real_zero_addr;
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
                ret = dsl_wr_reg(sdi, EEWP_ADDR, bmEEWP);
                if (ret == SR_OK) {
                    if (strcmp(sdi->model, "DSCope20") == 0 ||
                        strcmp(sdi->model, "DSCope") == 0)
                        real_zero_addr = zero_info.zero_addr;
                    else
                        real_zero_addr = (zero_big_addr << 8) + zero_info.zero_addr;
                    ret = dsl_wr_nvm(sdi, (unsigned char *)&zero_info, real_zero_addr, sizeof(struct cmd_zero_info));
                }
                if (ret == SR_OK) {
                    if (strcmp(sdi->model, "DSCope20") == 0 ||
                        strcmp(sdi->model, "DSCope") == 0)
                        real_zero_addr = vga_info.vga_addr;
                    else
                        real_zero_addr = (zero_big_addr << 8) + vga_info.vga_addr;
                    ret = dsl_wr_nvm(sdi, (unsigned char *)&vga_info, real_zero_addr, sizeof(struct cmd_vga_info));
                }
                if (ret == SR_OK)
                    ret = dsl_wr_reg(sdi, EEWP_ADDR, bmZERO);
                if (ret != SR_OK)
                    sr_err("DSO channel %d Set Zero command failed!", probe->index);

                const double slope = (probe->comb_diff_bom - probe->comb_diff_top)/(2.0*255.0);
                for (i = 0; i < 256; i++) {
                    ret = dsl_wr_reg(sdi, COMB_ADDR + probe->index*2, i);
                    int value = i+i*slope+probe->comb_diff_top*0.5+0.5;
                    value = (value < 0) ? 0 :
                            (value > 255) ? 255 : value;
                    ret = dsl_wr_reg(sdi, COMB_ADDR + probe->index*2 + 1, value);
                }
            }
        }
    } else if (id == SR_CONF_VOCM) {
        const uint8_t vocm = g_variant_get_byte(data);
        ret = dsl_wr_reg(sdi, COMB_ADDR+4, vocm);
    } else if (id == SR_CONF_PROBE_VGAIN) {
        const uint64_t vgain = g_variant_get_uint64(data) << 8;
        int i;
        struct DSL_vga *vga_ptr =  get_vga_ptr(sdi);
        for (i = 0; vga_ptr && (vga_ptr+i)->key; i++) {
            if ((vga_ptr+i)->key == ch->vdiv) {
                if (ch->index == 0)
                    (vga_ptr+i)->vgain0 = vgain;
                else if (ch->index == 1)
                    (vga_ptr+i)->vgain1 = vgain;
            }
        }
        ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, ch, SR_CONF_PROBE_VDIV));
        if (ret == SR_OK)
            sr_dbg("%s: setting VDIV of channel %d to %d mv",
                __func__, ch->index, ch->vdiv);
        else
            sr_dbg("%s: setting VDIV of channel %d to %d mv failed",
                __func__, ch->index, ch->vdiv);
    } else if (id == SR_CONF_PROBE_VOFF) {
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
            if ((vga_ptr+i)->key == ch->vdiv) {
                if (ch->index == 0)
                    (vga_ptr+i)->voff0 = voff;
                else if (ch->index == 1)
                    (vga_ptr+i)->voff1 = voff;
            }
        }
        ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, ch, SR_CONF_PROBE_VPOS));
        if (ret == SR_OK)
            sr_dbg("%s: setting VPOS of channel %d to %lf mv",
                __func__, ch->index, ch->vpos);
        else
            sr_dbg("%s: setting VPOS of channel %d to %lf mv failed",
                __func__, ch->index, ch->vpos);
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
        *data = g_variant_new_strv(opmodes, ARRAY_SIZE(opmodes));
        break;
    case SR_CONF_THRESHOLD:
        *data = g_variant_new_strv(thresholds, ARRAY_SIZE(thresholds));
        break;

    case SR_CONF_PROBE_CONFIGS:
        *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                probeOptions, ARRAY_SIZE(probeOptions)*sizeof(int32_t), TRUE, NULL, NULL);
        break;
    case SR_CONF_PROBE_SESSIONS:
        *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                probeSessions, ARRAY_SIZE(probeSessions)*sizeof(int32_t), TRUE, NULL, NULL);
        break;
    case SR_CONF_PROBE_VDIV:
        g_variant_builder_init(&gvb, G_VARIANT_TYPE("a{sv}"));
        gvar = g_variant_new_from_data(G_VARIANT_TYPE("at"),
                probeVdivs, ARRAY_SIZE(probeVdivs)*sizeof(uint64_t), TRUE, NULL, NULL);
        g_variant_builder_add(&gvb, "{sv}", "vdivs", gvar);
        *data = g_variant_builder_end(&gvb);
        break;
    case SR_CONF_PROBE_COUPLING:
        g_variant_builder_init(&gvb, G_VARIANT_TYPE("a{sv}"));
        gvar = g_variant_new_from_data(G_VARIANT_TYPE("ay"),
                probeCoupling, ARRAY_SIZE(probeCoupling)*sizeof(uint8_t), TRUE, NULL, NULL);
        g_variant_builder_add(&gvb, "{sv}", "coupling", gvar);
        *data = g_variant_builder_end(&gvb);
        break;
    case SR_CONF_PROBE_MAP_UNIT:
        *data = g_variant_new_strv(probeMapUnits, ARRAY_SIZE(probeMapUnits));
        break;
    default:
        return SR_ERR_NA;
    }

    return SR_OK;
}

static int dso_zero(const struct sr_dev_inst *sdi)
{
    struct DSL_context *devc = sdi->priv;
    GSList *l;
    int ret;
    static double vpos_back[2];
    static uint64_t vdiv_back[2];
    struct DSL_vga *vga_ptr = get_vga_ptr(sdi);
    struct sr_channel *probe0 = NULL, *probe1 = NULL;
    for(l = sdi->channels; l; l = l->next) {
        struct sr_channel *probe = (struct sr_channel *)l->data;
        if (probe->index == 0)
            probe0 = probe;
        if (probe->index == 1)
            probe1 = probe;
    }

    if (devc->zero_stage == -1) {
        // initialize before Auto Calibration
        if (dso_init(sdi) == SR_OK)
            devc->zero_stage = 0;
    } else if ((vga_ptr+devc->zero_stage)->key == 0) {
        ret = SR_OK;
        if (strcmp(sdi->model, "DSCope") != 0) {
            if (devc->zero_pcnt == 0) {
                devc->zero_comb = 0;
                vpos_back[0] = probe0->vpos;
                probe0->vpos = (vga_ptr+devc->zero_stage-1)->key * -4.8;
                vdiv_back[0] = probe0->vdiv;
                probe0->vdiv = (vga_ptr+devc->zero_stage-1)->key;
                ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe0, SR_CONF_PROBE_VPOS));
            } else if (devc->zero_pcnt == 4) {
                const double voff = 255*0.98 - (devc->mstatus.ch0_max + devc->mstatus.ch0_min) / 2.0;
                if (abs(voff) < 0.5) {
                    probe0->vpos = vpos_back[0];
                } else {
                    probe0->vpos_trans += voff;
                    ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe0, SR_CONF_PROBE_VPOS));
                    devc->zero_pcnt = 1;
                }
            } else if (devc->zero_pcnt == 5) {
                devc->zero_comb = 0;
                vpos_back[1] = probe1->vpos;
                probe1->vpos = (vga_ptr+devc->zero_stage-1)->key * -4.8;
                vdiv_back[1] = probe1->vdiv;
                probe1->vdiv = (vga_ptr+devc->zero_stage-1)->key;
                ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe1, SR_CONF_PROBE_VPOS));
            } else if (devc->zero_pcnt == 9) {
                const double voff = 255*0.98 - (devc->mstatus.ch1_max + devc->mstatus.ch1_min) / 2.0;
                if (abs(voff) < 0.5) {
                    probe1->vpos = vpos_back[1];
                } else {
                    probe1->vpos_trans += voff;
                    ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe1, SR_CONF_PROBE_VPOS));
                    devc->zero_pcnt = 6;
                }
            }
        }

        if (devc->zero_pcnt == 10) {
            ret = dsl_wr_reg(sdi, COMB_ADDR+6, 0b1101);
            devc->zero_comb = 0;
            vpos_back[0] = probe0->vpos;
            probe0->vpos = (vga_ptr+devc->zero_stage-1)->key * 4.5;
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe0, SR_CONF_PROBE_VPOS));
        } else if (devc->zero_pcnt == 15) {
            probe0->comb_diff_top = (devc->mstatus.ch0_max - devc->mstatus.ch1_max) +
                                    (devc->mstatus.ch0_min - devc->mstatus.ch1_min);
            probe0->vpos = (vga_ptr+devc->zero_stage-1)->key * -4.5;
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe0, SR_CONF_PROBE_VPOS));
        } else if (devc->zero_pcnt == 20) {
            probe0->comb_diff_bom = (devc->mstatus.ch0_max - devc->mstatus.ch1_max) +
                                    (devc->mstatus.ch0_min - devc->mstatus.ch1_min);
            probe0->vpos = vpos_back[0];
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe0, SR_CONF_PROBE_VPOS));
        }

        if (devc->zero_pcnt == 25) {
            ret = dsl_wr_reg(sdi, COMB_ADDR+6, 0b1110);
            devc->zero_comb = 1;
            vpos_back[1] = probe1->vpos;
            probe1->vpos = (vga_ptr+devc->zero_stage-1)->key * 4.5;
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe1, SR_CONF_PROBE_VPOS));
        } else if (devc->zero_pcnt == 30) {
            probe1->comb_diff_top = (devc->mstatus.ch1_max - devc->mstatus.ch0_max) +
                                    (devc->mstatus.ch1_min - devc->mstatus.ch0_min);
            probe1->vpos = (vga_ptr+devc->zero_stage-1)->key * -4.5;
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe1, SR_CONF_PROBE_VPOS));
        } else if (devc->zero_pcnt == 35) {
            probe1->comb_diff_bom = (devc->mstatus.ch1_max - devc->mstatus.ch0_max) +
                                    (devc->mstatus.ch1_min - devc->mstatus.ch0_min);
            probe1->vpos = vpos_back[1];
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe1, SR_CONF_PROBE_VPOS));
        }

        if (devc->zero_pcnt == 40) {
            if (strcmp(sdi->model, "DSCope") != 0) {
                probe0->vdiv = vdiv_back[0];
                probe1->vdiv = vdiv_back[1];
            }
            ret = dsl_wr_reg(sdi, COMB_ADDR+6, 0b0011);
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
                ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe, SR_CONF_PROBE_VDIV));
                ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe, SR_CONF_PROBE_VPOS));
                probe->vdiv = vdiv_back;
            }
        }

        if (devc->zero_pcnt == 4) {
            const double voff0 = 255/2.0 - (devc->mstatus.ch0_max + devc->mstatus.ch0_min)/2.0;
            const double voff1 = 255/2.0 - (devc->mstatus.ch1_max + devc->mstatus.ch1_min)/2.0;
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
                } else {
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
    gboolean fpga_done;
    int ret;
    GSList *l;
    gboolean zeroed;

    if ((ret = dsl_dev_open(di, sdi, &fpga_done)) == SR_OK) {
        // load zero informations
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
        if (!fpga_done)
            dso_init(sdi);
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

    (void)fd;
    (void)revents;

    drvc = di->priv;
    devc = sdi->priv;

    tv.tv_sec = tv.tv_usec = 0;
    libusb_handle_events_timeout_completed(drvc->sr_ctx->libusb_ctx, &tv, &completed);

    if (devc->zero && devc->trf_completed) {
        dso_zero(sdi);
    }

    if (devc->status == DSL_FINISH) {
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
    devc->empty_transfer_count = 0;
    devc->status = DSL_INIT;
    devc->num_transfers = 0;
    devc->submitted_transfers = 0;
    devc->actual_samples = devc->limit_samples;
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
        sr_err("%s: Stop DSCope acquisition failed!", __func__);
        return ret;
    } else {
        sr_info("%s: Stop Previous DSCope acquisition!", __func__);
    }

    /* Arm FPGA before acquisition start*/
    if ((ret = dsl_fpga_arm(sdi)) != SR_OK) {
        sr_err("%s: Arm FPGA failed!", __func__);
        return ret;
    }

    if (devc->zero && devc->zero_stage == -1) {
        // initialize before Auto Calibration
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
    .dev_status_get = dev_status_get,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.priv = NULL,
};
