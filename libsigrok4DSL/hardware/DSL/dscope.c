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


#include "../../libsigrok-internal.h"
#include "dsl.h"
#include "command.h"
#include "../../log.h"

#undef LOG_PREFIX 
#define LOG_PREFIX "dscope: "

static int dev_destroy(struct sr_dev_inst *sdi);

enum DSCOPE_OPERATION_MODE
{
    /** Normal */
    DS_OP_NORMAL = 0,
    /** Internal pattern test mode */
    DS_OP_INTEST = 1,
};

enum {
    BW_FULL = 0,
    BW_20M = 1,
};

static const struct sr_list_item opmode_list[] = {
    {DS_OP_NORMAL,"Normal"},
    {DS_OP_INTEST,"Internal Test"},
    {-1, NULL},
};

static const struct sr_list_item bandwidth_list[] = {
    {BW_FULL,"Full Bandwidth"},
    {BW_20M,"20MHz"},
    {-1, NULL},
};

static const struct lang_text_map_item lang_text_map[] = 
{
	{SR_CONF_OPERATION_MODE, DS_OP_NORMAL, "Normal", "正常"},
	{SR_CONF_OPERATION_MODE, DS_OP_INTEST, "Internal Test", "内部测试"},

    {SR_CONF_BANDWIDTH_LIMIT, BW_FULL, "Full Bandwidth", "全带宽"},
	{SR_CONF_BANDWIDTH_LIMIT, BW_20M, "20MHz", NULL},
};

static const int32_t hwoptions[] = {
    SR_CONF_OPERATION_MODE,
    SR_CONF_BANDWIDTH_LIMIT,
};

static const int32_t sessions_dso[] = {
    SR_CONF_OPERATION_MODE,
    SR_CONF_BANDWIDTH_LIMIT,
    SR_CONF_TIMEBASE,
    SR_CONF_TRIGGER_SLOPE,
    SR_CONF_TRIGGER_SOURCE,
    SR_CONF_TRIGGER_CHANNEL,
    SR_CONF_HORIZ_TRIGGERPOS,
    SR_CONF_TRIGGER_HOLDOFF,
    SR_CONF_TRIGGER_MARGIN,
};

static const int32_t sessions_daq[] = {
    SR_CONF_SAMPLERATE,
    SR_CONF_LIMIT_SAMPLES,
    SR_CONF_OPERATION_MODE,
    SR_CONF_BANDWIDTH_LIMIT,
    SR_CONF_TIMEBASE,
    SR_CONF_TRIGGER_SLOPE,
    SR_CONF_TRIGGER_SOURCE,
    SR_CONF_TRIGGER_CHANNEL,
    SR_CONF_HORIZ_TRIGGERPOS,
    SR_CONF_TRIGGER_HOLDOFF,
    SR_CONF_TRIGGER_MARGIN,
};

static const uint8_t zero_base_addr = 0x40;
static const uint8_t zero_big_addr = 0x20;

SR_PRIV struct sr_dev_driver DSCope_driver_info;
static struct sr_dev_driver *di = &DSCope_driver_info;

static uint16_t get_default_preoff(const struct sr_dev_inst *sdi, const struct sr_channel* ch)
{
    int i;
    struct DSL_context *devc = sdi->priv;
    for (i = 0; vga_defaults[i].id; i++) {
        if (vga_defaults[i].id == devc->profile->dev_caps.vga_id &&
            vga_defaults[i].key == ch->vdiv) {
            if (ch->index == 1)
                return vga_defaults[i].preoff_comp;
            else
                return vga_defaults[i].preoff;
        }
    }

    return 0;
}

static struct DSL_context *DSCope_dev_new(const struct DSL_profile *prof)
{
    struct DSL_context *devc;
    unsigned int i;

    assert(prof);

    if (!(devc = malloc(sizeof(struct DSL_context)))) {
        sr_err("Device context malloc failed.");
		return NULL;
	}
    memset(devc, 0, sizeof(struct DSL_context));

    for (i = 0; i < ARRAY_SIZE(channel_modes); i++){
        assert(channel_modes[i].id == i);
    }

    devc->channel = NULL;
    devc->profile = prof;
    devc->fw_updated = 0;
    devc->cur_samplerate = prof->dev_caps.default_samplerate;
    devc->limit_samples = prof->dev_caps.default_samplelimit;
    devc->clock_type = FALSE;
    devc->clock_edge = FALSE;
    devc->instant = FALSE;
    devc->op_mode = DS_OP_NORMAL;
    devc->test_mode = SR_TEST_NONE;
    devc->stream = FALSE;
    devc->ch_mode = prof->dev_caps.default_channelmode;
    devc->th_level = SR_TH_3V3;
    devc->filter = SR_FILTER_NONE;
    devc->timebase = 10000;
    devc->trigger_slope = DSO_TRIGGER_RISING;
    devc->trigger_source = DSO_TRIGGER_AUTO;
    devc->trigger_holdoff = 0;
    devc->trigger_hpos = 0x0;
    devc->trigger_hrate = 0;
    devc->zero = FALSE;
    devc->zero_branch = FALSE;
    devc->zero_comb_fgain = FALSE;
    devc->zero_comb = FALSE;
    devc->tune = FALSE;
    devc->data_lock = FALSE;
    devc->cali = FALSE;
    devc->trigger_margin = 8;
    devc->trigger_channel = 0;
    devc->rle_mode = FALSE;
    devc->status = DSL_FINISH;
    devc->bw_limit = BW_FULL;
    devc->is_loop = 0;

    dsl_adjust_samplerate(devc);
	return devc;
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
    libusb_device *device_handle = NULL;
    int ret, i, j;
	const char *conn;
    enum libusb_speed usb_speed;
    struct sr_usb_dev_inst *usb_dev_info;
    uint8_t bus;
    uint8_t address;
    int isProduct;
    int num;
    int is_speed_not_match;

	drvc = di->priv;
    num = 0;
    is_speed_not_match = 0;

    if (options != NULL)
        sr_info("Scan DSCope device with options.");
    else 
        sr_info("Scan DSCope device...");

	conn = NULL;
	for (l = options; l; l = l->next) {
		src = l->data;
		switch (src->key) {
        case SR_CONF_CONN:
            conn = g_variant_get_string(src->data, NULL);
			break;
		}
	}
	if (conn){
        sr_info("Find usb device with connect config.");
        conn_devices = sr_usb_find(drvc->sr_ctx->libusb_ctx, conn);
    }
	else
		conn_devices = NULL;

    /* Find all DSCope compatible devices and upload firmware to them. */
	devices = NULL;
    devlist = NULL;

    libusb_get_device_list(drvc->sr_ctx->libusb_ctx, &devlist);

    if (devlist == NULL){
        sr_info("%s: Failed to call libusb_get_device_list(), it returns a null list.", __func__);
        return NULL;
    }
    
	for (i = 0; devlist[i]; i++) 
    {
        device_handle = devlist[i];
        
		if (conn) {
			usb = NULL;
			for (l = conn_devices; l; l = l->next) {
				usb = l->data;
				if (usb->bus == libusb_get_bus_number(device_handle)
					&& usb->address == libusb_get_device_address(device_handle))
					break;
			}
			if (!l)
				/* This device matched none of the ones that
				 * matched the conn specification. */
				continue;
		}

		if ((ret = libusb_get_device_descriptor( device_handle, &des)) != 0) {
            sr_warn("Failed to get device descriptor: %s.",
				libusb_error_name(ret));
			continue;
		}

        // The vendor id is not right.
        if (des.idVendor != DS_VENDOR_ID)
            continue;

        usb_speed = libusb_get_device_speed(device_handle);
        if ((usb_speed != LIBUSB_SPEED_HIGH) && (usb_speed != LIBUSB_SPEED_SUPER)){   
            sr_info("scan(): The idVendor is right, but the usb speed is too low, speed type:%d", usb_speed);
            is_speed_not_match = 1;
            continue;             
        }

		prof = NULL;
        for (j = 0; supported_DSCope[j].vid; j++) 
        {
            if (des.idVendor == supported_DSCope[j].vid &&
                des.idProduct == supported_DSCope[j].pid){
                    if (usb_speed == supported_DSCope[j].usb_speed) {
                        prof = &supported_DSCope[j];
                        break;
                    }                   
			}
		}

		/* Skip if the device was not found. */
		if (prof == NULL){ 
              isProduct = 0;

               //Mybe is a dslogic device.
              for (j = 0; supported_DSLogic[j].vid; j++) 
                {
                    if (des.idVendor == supported_DSLogic[j].vid &&
                        des.idProduct == supported_DSLogic[j].pid &&
                        usb_speed == supported_DSLogic[j].usb_speed) {
                        isProduct = 1;
                        break;
                    }
                }

            if (isProduct == 0){
                sr_info("scan(): The profile is not matched, idVendor:%02X, idProduct:%02X",
                    des.idVendor,
                    des.idProduct);
            }

            continue;
        }

        if (sr_usb_device_is_exists(device_handle)){
            sr_detail("Device is exists, handle: %p", device_handle);
            continue;
        }

        bus = libusb_get_bus_number(device_handle);
        address = libusb_get_device_address(device_handle);
        sr_info("Found a new device,handle:%p", device_handle);

		if (dsl_check_conf_profile(device_handle)) {
			/* Already has the firmware, so fix the new address. */
            sr_info("Found a DSCope device,name:\"%s\",handle:%p", prof->model, device_handle);

            devc = DSCope_dev_new(prof);
            if (devc == NULL){
                break;
            }
            
            sdi = sr_dev_inst_new(channel_modes[devc->ch_mode].mode, SR_ST_INITIALIZING,
                prof->vendor, prof->model, prof->model_version);

            if (sdi == NULL) {
                free(devc);
                break;
            }

            sdi->priv = devc;
            sdi->driver = di;
            sdi->dev_type = DEV_TYPE_USB;
            sdi->handle = (ds_device_handle)device_handle;

            /* Fill in probelist according to this device's profile. */
            if (dsl_setup_probes(sdi, channel_modes[devc->ch_mode].num) != SR_OK){
                sr_err("dsl_setup_probes() error");
                dev_destroy(sdi); 
                break;
            }

            usb_dev_info = sr_usb_dev_inst_new(bus, address);
            usb_dev_info->usb_dev = device_handle;
            sdi->conn = usb_dev_info;
            sdi->status = SR_ST_INACTIVE;   

            devices = g_slist_append(devices, sdi);
            num++;       
		}
        else {
            char *firmware;
            char *res_path = DS_RES_PATH;
            if (!(firmware = malloc(strlen(res_path)+strlen(prof->firmware) + 5))) {
                sr_err("Firmware path malloc error!");
                break;
            }
            strcpy(firmware, res_path);
            strcat(firmware, "/");
            strcat(firmware, prof->firmware);

            sr_info("Install firmware bin file, device:\"%s\", file:\"%s\"", prof->model, firmware);

            if (ezusb_upload_firmware(device_handle, USB_CONFIGURATION,  firmware) != SR_OK){
                sr_err("Firmware upload failed for device %s", prof->model);
            }

            g_free(firmware);
            
            libusb_unref_device(device_handle);
#ifdef _WIN32
            libusb_unref_device(device_handle);
#endif

            sr_info("Waitting for device reconnect, name:\"%s\"", prof->model);            
		}
	}

	libusb_free_device_list(devlist, 0);

    if (conn_devices){
        g_slist_free_full(conn_devices, (GDestroyNotify)sr_usb_dev_inst_free);
    }

    sr_info("Found new DSCope device count: %d", num);

    if (is_speed_not_match){
        post_message_callback(DS_EV_DEVICE_SPEED_NOT_MATCH);
    }

	return devices;
}

static const GSList *dev_mode_list(const struct sr_dev_inst *sdi)
{
    return dsl_mode_list(sdi);
}

static uint64_t dso_vga(const struct sr_channel* ch)
{
    int i;
    for (i = 0; ch->vga_ptr && (ch->vga_ptr+i)->id; i++) {
        if ((ch->vga_ptr+i)->key == ch->vdiv)
            return (ch->vga_ptr+i)->vgain;
    }

    return 0;
}

static uint64_t dso_preoff(const struct sr_channel* ch)
{
    int i;
    for (i = 0; ch->vga_ptr && (ch->vga_ptr+i)->id; i++) {
        if ((ch->vga_ptr+i)->key == ch->vdiv)
            return (ch->vga_ptr+i)->preoff;
    }
    return 0;
}

static uint64_t dso_offset(const struct sr_dev_inst *sdi, const struct sr_channel* ch)
{
    uint64_t pwm_off = 0;
    int offset_coarse, offset_fine;
    int trans_coarse, trans_fine;
    struct DSL_context *devc = sdi->priv;
    const double offset_mid = (1 << (ch->bits - 1));
    const double offset_max = ((1 << ch->bits) - 1.0);
    const uint64_t offset = devc->zero ? ch->zero_offset : ch->offset;
    double comb_off = (devc->profile->dev_caps.feature_caps & CAPS_FEATURE_HMCAD1511) ? 0.57 / (pow(10, 24.0*ch->comb_comp/20/4096) - 1) :
                                                                                        2.0 / (pow(10, 24.0*ch->comb_comp/20/4096) - 1);
//    const double comb_compensate = ((devc->profile->dev_caps.feature_caps & CAPS_FEATURE_HMCAD1511) &&
//                                    (dsl_en_ch_num(sdi) == 1))? (offset - offset_mid) / comb_off : 0;
    const double comb_compensate = ((ch->comb_comp != 0) && (dsl_en_ch_num(sdi) == 1)) ? (offset - offset_mid) / comb_off : 0;
    if (devc->profile->dev_caps.feature_caps & CAPS_FEATURE_PREOFF) {
        trans_coarse = (ch->vpos_trans & 0xFF00) >> 8;
        trans_fine = (ch->vpos_trans & 0x00FF);
        const double voltage = (offset_mid - offset) / offset_max * ch->vdiv * DS_CONF_DSO_VDIVS;
        if (ch->vdiv < 500) {
            offset_coarse = floor(-voltage*DSCOPE_TRANS_CMULTI/trans_coarse + 0.5);
            offset_fine = floor((voltage + offset_coarse*trans_coarse/DSCOPE_TRANS_CMULTI)*1000.0/trans_fine + 0.5);
        } else {
            offset_coarse = floor(-voltage/trans_coarse + 0.5);
            offset_fine = floor((voltage + offset_coarse*trans_coarse)*DSCOPE_TRANS_FMULTI/trans_fine + 0.5);
        }
    } else {
        pwm_off = (offset + comb_compensate) / offset_max * ch->vpos_trans;
    }

    const uint64_t preoff = dso_preoff(ch);
    if (devc->profile->dev_caps.feature_caps & CAPS_FEATURE_PREOFF)
        return (offset << 32) +
               ((offset_coarse + DSCOPE_CONSTANT_BIAS + (preoff>>10)) << 16) + offset_fine +
               (preoff & 0x03ff);
    else
        return (offset << 32) +
               pwm_off + preoff;
}

static uint64_t dso_cmd_gen(const struct sr_dev_inst *sdi, struct sr_channel* ch, int id)
{
    struct DSL_context *devc;
    uint64_t cmd = 0;
    uint64_t offset;
    GSList *l;
    const int ch_bit = 7;
    devc = sdi->priv;

    switch (id) {
    case SR_CONF_PROBE_EN:
    case SR_CONF_PROBE_COUPLING:
        if (sdi->mode == ANALOG || dsl_en_ch_num(sdi) == 2) {
            cmd += 0x0E00;
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
        uint64_t vgain = dso_vga(ch);
        if ((ch->comb_comp != 0) && (dsl_en_ch_num(sdi) == 1))
            vgain += (uint64_t)(ch->comb_comp) << 8;
        cmd += vgain;
        break;
    case SR_CONF_PROBE_OFFSET:
        cmd += 0x10;
        cmd += ch->index << ch_bit;
        offset = dso_offset(sdi, ch);
        cmd += (offset << 8);
        break;
    case SR_CONF_SAMPLERATE:
        cmd += 0x18;
        uint32_t divider = devc->zero ? 0x1 : (uint32_t)ceil(channel_modes[devc->ch_mode].max_samplerate * 1.0 / devc->cur_samplerate / dsl_en_ch_num(sdi));
        cmd += divider << 8;
        break;
    case SR_CONF_HORIZ_TRIGGERPOS:
        cmd += 0x20;
        cmd += ((uint64_t)devc->trigger_hpos << 8);
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
        ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe, SR_CONF_PROBE_OFFSET));
        if (ret != SR_OK) {
            sr_err("Set OFFSET of channel %d command failed!", probe->index);
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
    struct DSL_context *devc;
    int ret, i;
    uint16_t real_zero_addr;

    devc = sdi->priv;
    struct cmd_zero_info zero_info;
    uint8_t dst_addr = (zero_base_addr +
                        probe->index * (sizeof(struct cmd_zero_info) + sizeof(struct cmd_vga_info)));
    zero_info.zero_addr = dst_addr;
    if (devc->profile->dev_caps.feature_caps & CAPS_FEATURE_SEEP)
        real_zero_addr = zero_info.zero_addr;
    else if (devc->profile->dev_caps.feature_caps & CAPS_FEATURE_FLASH)
        real_zero_addr = probe->index * DSO_ZERO_PAGE;
    else
        real_zero_addr = (zero_big_addr << 8) + zero_info.zero_addr;
    if ((ret = dsl_rd_nvm(sdi, (unsigned char *)&zero_info, real_zero_addr, sizeof(struct cmd_zero_info))) != SR_OK) {
        return FALSE;
        sr_err("%s: Send Get Zero command failed!", __func__);
    } else {
        if (zero_info.zero_addr == dst_addr) {
            uint8_t* preoff_ptr = &zero_info.zero_addr + 1;
             for (i = 0; probe->vga_ptr && (probe->vga_ptr+i)->id; i++) {
                 (probe->vga_ptr+i)->preoff = (*(preoff_ptr + 2*i+1) << 8) + *(preoff_ptr + 2*i);
             }
             if (i != 0) {
                 probe->comb_diff_top = *(preoff_ptr + 2*i);
                 probe->comb_diff_bom = *(preoff_ptr + 2*i + 1);
                 probe->vpos_trans = *(preoff_ptr + 2*i + 2) + (*(preoff_ptr + 2*i + 3) << 8);
                 probe->comb_comp = *(preoff_ptr + 2*i + 4);
                 probe->digi_fgain = *(preoff_ptr + 2*i + 5) + (*(preoff_ptr + 2*i + 6) << 8);
                 probe->cali_fgain0 = dsl_adc_code2fgain(*(preoff_ptr + 2*i + 7));
                 probe->cali_fgain1 = dsl_adc_code2fgain(*(preoff_ptr + 2*i + 8));
                 probe->cali_fgain2 = dsl_adc_code2fgain(*(preoff_ptr + 2*i + 9));
                 probe->cali_fgain3 = dsl_adc_code2fgain(*(preoff_ptr + 2*i + 10));
                 probe->cali_comb_fgain0 = dsl_adc_code2fgain(*(preoff_ptr + 2*i + 11));
                 probe->cali_comb_fgain1 = dsl_adc_code2fgain(*(preoff_ptr + 2*i + 12));
                 probe->cali_comb_fgain2 = dsl_adc_code2fgain(*(preoff_ptr + 2*i + 13));
                 probe->cali_comb_fgain3 = dsl_adc_code2fgain(*(preoff_ptr + 2*i + 14));

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
    if (devc ->profile->dev_caps.feature_caps & CAPS_FEATURE_SEEP)
        real_zero_addr = vga_info.vga_addr;
    else if (devc->profile->dev_caps.feature_caps & CAPS_FEATURE_FLASH)
        real_zero_addr = probe->index * DSO_ZERO_PAGE + 1;
    else
        real_zero_addr = (zero_big_addr << 8) + vga_info.vga_addr;
    if ((ret = dsl_rd_nvm(sdi, (unsigned char *)&vga_info, real_zero_addr, sizeof(struct cmd_vga_info))) != SR_OK) {
        return FALSE;
        sr_err("%s: Send Get Zero command failed!", __func__);
    } else {
        if (vga_info.vga_addr == dst_addr + sizeof(struct cmd_zero_info)) {
            uint16_t* vgain_ptr = &vga_info.vga0;
            for (i = 0; probe->vga_ptr && (probe->vga_ptr+i)->id; i++) {
                (probe->vga_ptr+i)->vgain = *(vgain_ptr + i) << 8;
            }
        } else {
            return FALSE;
        }
    }

    return TRUE;
}

static int dso_zero(const struct sr_dev_inst *sdi, gboolean reset)
{
    struct DSL_context *devc = sdi->priv;
    GSList *l;
    int ret = SR_OK;
    struct sr_usb_dev_inst *usb;
    struct libusb_device_handle *hdl;
    struct ctl_wr_cmd wr_cmd;

    static uint64_t vdiv_back[2] = {0, 0};
    struct sr_channel *probe0 = NULL, *probe1 = NULL;
    uint16_t offset_top;
    if (devc->profile->dev_caps.feature_caps & CAPS_FEATURE_HMCAD1511)
        offset_top = 15;
    else
        offset_top = 30;
    const uint16_t offset_bom = ((1 << channel_modes[devc->ch_mode].unit_bits) - 1) - offset_top;
    const uint16_t offset_mid = (1 << (channel_modes[devc->ch_mode].unit_bits - 1));
    const uint16_t max_trans = ((1 << 10) - 1);

    #ifdef _WIN32
    const int zero_interval = (devc->profile->usb_speed == LIBUSB_SPEED_SUPER) ? 10 : 4;
    #else
    const int zero_interval = 50;
    #endif
    const double margin_pass = 0.3;
    int end_cnt = 0;
    const int branch_done_cnt = (devc->profile->usb_speed == LIBUSB_SPEED_SUPER) ? 10 : 2;
    static gboolean warm_done = FALSE;
    static gboolean trans_fix_done = FALSE;
    static gboolean mid_zero_done = FALSE;
    static double margin[2];
    //static double offset[2];
    double acc_mean = 0;
    double acc_mean0 = 0;
    double acc_mean1 = 0;
    double acc_skew[8];
    double acc_max_skew;

    if (reset) {
        warm_done = FALSE;
        trans_fix_done = FALSE;
        mid_zero_done = FALSE;
        vdiv_back[0] = 0;
        vdiv_back[1] = 0;
        return SR_OK;
    }
    if (!devc->mstatus_valid)
        return SR_ERR_ARG;

    usb = sdi->conn;
    hdl = usb->devhdl;
    for(l = sdi->channels; l; l = l->next) {
        struct sr_channel *probe = (struct sr_channel *)l->data;
        if (probe->index == 0)
            probe0 = probe;
        if (probe->index == 1)
            probe1 = probe;
        if (vdiv_back[probe->index] == 0)
            vdiv_back[probe->index] = probe->vdiv;
    }

    if (!trans_fix_done && devc->zero_stage == 0) {
        ret = SR_OK;
        if (!warm_done) {
            if (devc->zero_pcnt == 0*zero_interval) {
                for(l = sdi->channels; l; l = l->next) {
                    struct sr_channel *probe = (struct sr_channel *)l->data;
                    for (int i = 0; (probe->vga_ptr+i)->key; i++)
                        probe->vdiv = (probe->vga_ptr+i)->key;
                    ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe, SR_CONF_PROBE_VDIV));
                    probe->zero_offset = offset_mid;
                    ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe, SR_CONF_PROBE_OFFSET));
                }
            }
            if (!(devc->profile->dev_caps.feature_caps & CAPS_FEATURE_HMCAD1511) ||
                (devc->zero_pcnt == 1*zero_interval)) {
                warm_done = TRUE;
                devc->zero_pcnt = 0*zero_interval-1;
            }
        } else if (!(devc->profile->dev_caps.feature_caps & CAPS_FEATURE_PREOFF)) {
            if (devc->zero_pcnt == 0*zero_interval) {
                for(l = sdi->channels; l; l = l->next) {
                    struct sr_channel *probe = (struct sr_channel *)l->data;
                    probe->zero_offset = offset_bom;
                    ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe, SR_CONF_PROBE_OFFSET));
                }
            }
            if (devc->zero_pcnt == 1*zero_interval) {
                margin[0] = (devc->mstatus.ch0_acc_mean * 1.0 / devc->limit_samples);
                margin[1] = (devc->mstatus.ch1_acc_mean * 1.0 / devc->limit_samples);
//                if (margin[0] >= value_max || margin[1] >= value_max)
//                    ret = SR_ERR;
            }
            if (devc->zero_pcnt == 1*zero_interval+1) {
                for(l = sdi->channels; l; l = l->next) {
                    struct sr_channel *probe = (struct sr_channel *)l->data;
                    probe->zero_offset = offset_top;
                    ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe, SR_CONF_PROBE_OFFSET));
                }
            }
            if (devc->zero_pcnt == 2*zero_interval) {
                double top0 = (devc->mstatus.ch0_acc_mean * 1.0 / devc->limit_samples);
                double top1 = (devc->mstatus.ch1_acc_mean * 1.0 / devc->limit_samples);
//                if (top0 <= value_min || top1 <= value_min) {
//                    ret = SR_ERR;
                //} else {
                    margin[0] -= top0;
                    margin[1] -= top1;
                    for(l = sdi->channels; l; l = l->next) {
                        struct sr_channel *probe = (struct sr_channel *)l->data;
                        margin[probe->index] -= (offset_bom - offset_top);
                        if (fabs(margin[probe->index]) > margin_pass) {
                            margin[probe->index] = margin[probe->index] > 0 ? ceil(margin[probe->index]) : floor(margin[probe->index]);
                            probe->vpos_trans = min(probe->vpos_trans - margin[probe->index], max_trans);
                            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe0, SR_CONF_PROBE_OFFSET));
                        } else {
                            margin[probe->index] = 0;
                        }
                    }
                    trans_fix_done = (margin[0] == 0) && (margin[1] == 0);
                    devc->zero_pcnt = trans_fix_done ? 0*zero_interval : 0*zero_interval-1;
                //}
            }
        } else {
            trans_fix_done = TRUE;
        }

        if (!trans_fix_done && ret == SR_OK)
            devc->zero_pcnt++;
    } else if (!mid_zero_done) {
        if (devc->zero_pcnt == 0) {
            for(l = sdi->channels; l; l = l->next) {
                struct sr_channel *probe = (struct sr_channel *)l->data;
                probe->vdiv = (probe->vga_ptr+devc->zero_stage)->key;
                if (probe->vdiv == 0) {
                    probe->vdiv = (probe->vga_ptr+devc->zero_stage-1)->key;
                    mid_zero_done = TRUE;
                    break;
                }
                ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe, SR_CONF_PROBE_VDIV));
                probe->zero_offset = offset_mid;
                ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe, SR_CONF_PROBE_OFFSET));
                // must after offset setting
                probe->vdiv = vdiv_back[probe->index];
            }
        }

        if (devc->zero_pcnt == zero_interval) {
            margin[0] = offset_mid - (devc->mstatus.ch0_acc_mean * 1.0 / devc->limit_samples);
            margin[1] = offset_mid - (devc->mstatus.ch1_acc_mean * 1.0 / devc->limit_samples);
            if (fabs(margin[0]) < margin_pass && fabs(margin[1]) < margin_pass) {
                devc->zero_stage++;
            } else {
                if (devc->profile->dev_caps.feature_caps & CAPS_FEATURE_PREOFF) {
                    for(l = sdi->channels; l; l = l->next) {
                        struct sr_channel *probe = (struct sr_channel *)l->data;
                        double trans_coarse = ((probe->vga_ptr+devc->zero_stage)->key < 500) ? (probe->vpos_trans >> 8)/DSCOPE_TRANS_CMULTI : (probe->vpos_trans >> 8);
                        double trans_fine = ((probe->vga_ptr+devc->zero_stage)->key < 500) ? (probe->vpos_trans & 0x00ff) / 1000.0 : (probe->vpos_trans & 0x00ff) / DSCOPE_TRANS_FMULTI;

                        double voltage_margin = margin[probe->index] * (probe->vga_ptr+devc->zero_stage)->key * 10 / 255.0;
                        uint16_t last_preoff = (probe->vga_ptr+devc->zero_stage)->preoff;
                        int preoff_coarse = floor(voltage_margin / trans_coarse + 0.5);
                        int preoff_fine = floor(-(voltage_margin - preoff_coarse*trans_coarse)/trans_fine + 0.5);
                        preoff_coarse = (last_preoff >> 10) + preoff_coarse;
                        preoff_fine = (last_preoff&0x03ff) + preoff_fine;
                        (probe->vga_ptr+devc->zero_stage)->preoff = (preoff_coarse << 10) + preoff_fine;
                    }
                } else {
                    for(l = sdi->channels; l; l = l->next) {
                        struct sr_channel *probe = (struct sr_channel *)l->data;
                        (probe->vga_ptr+devc->zero_stage)->preoff += margin[probe->index] > 0 ? ceil(margin[probe->index]) : floor(margin[probe->index]);
                    }
                }
            }
            devc->zero_pcnt = 0;
        } else if (!mid_zero_done) {
            devc->zero_pcnt++;
        }
    } else {
        ret = SR_OK;
        end_cnt = 0*zero_interval + 1;
        if (devc->profile->dev_caps.feature_caps & CAPS_FEATURE_HMCAD1511) {
            if (!devc->zero_comb_fgain) {
                if (devc->zero_pcnt == 0*zero_interval+1) {
                    devc->zero_branch = TRUE;

                    probe0->zero_offset = offset_bom;
                    ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe0, SR_CONF_PROBE_OFFSET));
                    probe1->zero_offset = offset_bom;
                    ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe1, SR_CONF_PROBE_OFFSET));

                    dsl_probe_cali_fgain(devc, probe0, 1, FALSE, TRUE);
                    dsl_probe_cali_fgain(devc, probe1, 1, FALSE, TRUE);
                    dsl_config_adc_fgain(sdi, 0, probe0->cali_fgain0, probe0->cali_fgain1);
                    dsl_config_adc_fgain(sdi, 1, probe0->cali_fgain2, probe0->cali_fgain3);
                    dsl_config_adc_fgain(sdi, 2, probe1->cali_fgain0, probe1->cali_fgain1);
                    dsl_config_adc_fgain(sdi, 3, probe1->cali_fgain2, probe1->cali_fgain3);
                    acc_mean0 = 0;
                    acc_mean1 = 0;
                } else if (devc->zero_pcnt == branch_done_cnt*zero_interval) {
                    acc_mean0 = (devc->mstatus.ch0_acc_mean + devc->mstatus.ch0_acc_mean_p1 +
                                 devc->mstatus.ch0_acc_mean_p2 + devc->mstatus.ch0_acc_mean_p3) / 4.0;
                    acc_mean1 = (devc->mstatus.ch1_acc_mean + devc->mstatus.ch1_acc_mean_p1 +
                                 devc->mstatus.ch1_acc_mean_p2 + devc->mstatus.ch1_acc_mean_p3) / 4.0;

                    acc_skew[0] = devc->mstatus.ch0_acc_mean / acc_mean0 - 1;
                    acc_skew[1] = devc->mstatus.ch0_acc_mean_p1 / acc_mean0 - 1;
                    acc_skew[2] = devc->mstatus.ch0_acc_mean_p2 / acc_mean0 - 1;
                    acc_skew[3] = devc->mstatus.ch0_acc_mean_p3 / acc_mean0 - 1;
                    acc_skew[4] = devc->mstatus.ch1_acc_mean / acc_mean1 - 1;
                    acc_skew[5] = devc->mstatus.ch1_acc_mean_p1 / acc_mean1 - 1;
                    acc_skew[6] = devc->mstatus.ch1_acc_mean_p2 / acc_mean1 - 1;
                    acc_skew[7] = devc->mstatus.ch1_acc_mean_p3 / acc_mean1 - 1;
                    acc_max_skew = fabs(acc_skew[0]);
                    for (int i=1; i <8; i++)
                        acc_max_skew = max(acc_max_skew, fabs(acc_skew[i]));
                    if ((acc_max_skew > MAX_ACC_VARIANCE) && (dsl_probe_fgain_inrange(probe0, FALSE, acc_skew) ||
                                                              dsl_probe_fgain_inrange(probe1, FALSE, acc_skew))) {
                        devc->zero_pcnt = 0*zero_interval+1;

                        dsl_probe_cali_fgain(devc, probe0, acc_mean0, FALSE, FALSE);
                        dsl_probe_cali_fgain(devc, probe1, acc_mean1, FALSE, FALSE);

                        dsl_config_adc_fgain(sdi, 0, probe0->cali_fgain0, probe0->cali_fgain1);
                        dsl_config_adc_fgain(sdi, 1, probe0->cali_fgain2, probe0->cali_fgain3);
                        dsl_config_adc_fgain(sdi, 2, probe1->cali_fgain0, probe1->cali_fgain1);
                        dsl_config_adc_fgain(sdi, 3, probe1->cali_fgain2, probe1->cali_fgain3);
                    } else {
                        if (acc_max_skew <= MAX_ACC_VARIANCE) {
                            devc->zero_comb_fgain = TRUE;
                            devc->zero_pcnt = 0*zero_interval;
                        } else {
                            devc->zero_pcnt = 0*zero_interval;
                            dsl_skew_fpga_fgain(sdi, FALSE, acc_skew);
                        }
                    }
                }
            }
            if (devc->zero_comb_fgain) {
                if (devc->zero_pcnt == 0*zero_interval+1) {
                    probe0->zero_offset = offset_bom;
                    ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe0, SR_CONF_PROBE_OFFSET));
                    probe1->zero_offset = offset_bom;
                    ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe1, SR_CONF_PROBE_OFFSET));

                    acc_mean = 0;
                    devc->zero_comb = FALSE;
                    dsl_probe_cali_fgain(devc, probe0, 1, TRUE, TRUE);
                    dsl_probe_cali_fgain(devc, probe1, 1, TRUE, TRUE);
                    dsl_config_adc_fgain(sdi, 0, probe0->cali_comb_fgain0, probe0->cali_comb_fgain1);
                    dsl_config_adc_fgain(sdi, 1, probe0->cali_comb_fgain2, probe0->cali_comb_fgain3);
                    dsl_config_adc_fgain(sdi, 2, probe1->cali_comb_fgain0, probe1->cali_comb_fgain1);
                    dsl_config_adc_fgain(sdi, 3, probe1->cali_comb_fgain2, probe1->cali_comb_fgain3);
                } else if (devc->zero_pcnt == 1*zero_interval) {
                    if (!devc->zero_comb)
                        devc->zero_pcnt = 0*zero_interval+1;
                } else if (devc->zero_pcnt == branch_done_cnt*zero_interval) {
                    acc_mean = (devc->mstatus.ch0_acc_mean + devc->mstatus.ch0_acc_mean_p1 +
                                devc->mstatus.ch0_acc_mean_p2 + devc->mstatus.ch0_acc_mean_p3 +
                                devc->mstatus.ch1_acc_mean + devc->mstatus.ch1_acc_mean_p1 +
                                devc->mstatus.ch1_acc_mean_p2 + devc->mstatus.ch1_acc_mean_p3) / 8.0;

                    acc_skew[0] = devc->mstatus.ch0_acc_mean / acc_mean - 1;
                    acc_skew[1] = devc->mstatus.ch0_acc_mean_p1 / acc_mean - 1;
                    acc_skew[2] = devc->mstatus.ch0_acc_mean_p2 / acc_mean - 1;
                    acc_skew[3] = devc->mstatus.ch0_acc_mean_p3 / acc_mean - 1;
                    acc_skew[4] = devc->mstatus.ch1_acc_mean / acc_mean - 1;
                    acc_skew[5] = devc->mstatus.ch1_acc_mean_p1 / acc_mean - 1;
                    acc_skew[6] = devc->mstatus.ch1_acc_mean_p2 / acc_mean - 1;
                    acc_skew[7] = devc->mstatus.ch1_acc_mean_p3 / acc_mean - 1;
                    acc_max_skew = fabs(acc_skew[0]);
                    for (int i=1; i <8; i++)
                        acc_max_skew = max(acc_max_skew, fabs(acc_skew[i]));
                    if ((acc_max_skew > MAX_ACC_VARIANCE) && (dsl_probe_fgain_inrange(probe0, TRUE, acc_skew) ||
                                                              dsl_probe_fgain_inrange(probe1, TRUE, acc_skew))) {
                        devc->zero_pcnt = 0*zero_interval+1;

                        dsl_probe_cali_fgain(devc, probe0, acc_mean, TRUE, FALSE);
                        dsl_probe_cali_fgain(devc, probe1, acc_mean, TRUE, FALSE);

                        dsl_config_adc_fgain(sdi, 0, probe0->cali_comb_fgain0, probe0->cali_comb_fgain1);
                        dsl_config_adc_fgain(sdi, 1, probe0->cali_comb_fgain2, probe0->cali_comb_fgain3);
                        dsl_config_adc_fgain(sdi, 2, probe1->cali_comb_fgain0, probe1->cali_comb_fgain1);
                        dsl_config_adc_fgain(sdi, 3, probe1->cali_comb_fgain2, probe1->cali_comb_fgain3);
                    } else {
                        if (acc_max_skew <= MAX_ACC_VARIANCE) {
                            devc->zero_comb_fgain = FALSE;
                            devc->zero_branch = FALSE;
                        } else {
                            devc->zero_pcnt = 0*zero_interval;
                            dsl_skew_fpga_fgain(sdi, TRUE, acc_skew);
                        }
                    }
                }
            }
            end_cnt = branch_done_cnt*zero_interval + 1;
        } else {
            if (devc->zero_pcnt == 0*zero_interval+1) {
                ret = dsl_wr_reg(sdi, COMB_ADDR+6, 0b1101);
                wr_cmd.header.dest = DSL_CTL_DSO_EN1;
                wr_cmd.data[0] = (uint8_t)~bmCH_CH1;
                wr_cmd.header.size = 1;
                ret = command_ctl_wr(hdl, wr_cmd);

                probe0->zero_offset = offset_top;
                ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe0, SR_CONF_PROBE_OFFSET));
            } else if (devc->zero_pcnt == 1*zero_interval) {
                probe0->comb_diff_top = ((devc->mstatus.ch0_acc_mean * 2.0 - devc->mstatus.ch1_acc_mean * 2.0) / devc->limit_samples);
                probe0->zero_offset = offset_bom;
                ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe0, SR_CONF_PROBE_OFFSET));
            } else if (devc->zero_pcnt == 2*zero_interval) {
                probe0->comb_diff_bom = ((devc->mstatus.ch0_acc_mean * 2.0 - devc->mstatus.ch1_acc_mean * 2.0) / devc->limit_samples);
            }

            if (devc->zero_pcnt == 2*zero_interval+1) {
                ret = dsl_wr_reg(sdi, COMB_ADDR+6, 0b1110);
                wr_cmd.header.dest = DSL_CTL_DSO_EN1;
                wr_cmd.data[0] = bmCH_CH1;
                wr_cmd.header.size = 1;
                ret = command_ctl_wr(hdl, wr_cmd);
                wr_cmd.header.dest = DSL_CTL_DSO_EN0;
                wr_cmd.data[0] = (uint8_t)~bmCH_CH0;
                wr_cmd.header.size = 1;
                ret = command_ctl_wr(hdl, wr_cmd);

                probe1->zero_offset = offset_top;
                ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe1, SR_CONF_PROBE_OFFSET));
            } else if (devc->zero_pcnt == 3*zero_interval) {
                probe1->comb_diff_top = ((devc->mstatus.ch1_acc_mean * 2.0 - devc->mstatus.ch0_acc_mean * 2.0) / devc->limit_samples);
                probe1->zero_offset = offset_bom;
                ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe1, SR_CONF_PROBE_OFFSET));
            } else if (devc->zero_pcnt == 4*zero_interval) {
                probe1->comb_diff_bom = ((devc->mstatus.ch1_acc_mean * 2.0 - devc->mstatus.ch0_acc_mean * 2.0) / devc->limit_samples);
            }

            end_cnt = 4*zero_interval+1;
        }

        if (ret == SR_OK)
            devc->zero_pcnt++;

        if (devc->zero_pcnt == end_cnt) {
            for(l = sdi->channels; l; l = l->next) {
                struct sr_channel *probe = (struct sr_channel *)l->data;
                probe->vdiv = vdiv_back[probe->index];
                ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe, SR_CONF_PROBE_VDIV));

                // vgain tunning
                if (devc->profile->dev_caps.feature_caps & CAPS_FEATURE_AUTO_VGAIN) {
                    if (probe->vga_ptr != NULL) {
                        for (uint16_t i = 0; devc->profile->dev_caps.vdivs[i]; i++) {
                            for (uint16_t j = 0; j < ARRAY_SIZE(vga_defaults); j++) {
                                if (vga_defaults[j].id == devc->profile->dev_caps.vga_id &&
                                    vga_defaults[j].key == devc->profile->dev_caps.vdivs[i]) {
                                    const int64_t cur_trans = probe->vpos_trans;
                                    const int64_t def_trans = devc->profile->dev_caps.default_pwmtrans;
                                    const int64_t vgain_delta = (cur_trans > def_trans) ? ((cur_trans - def_trans) << 8) :
                                                                                          (((cur_trans - def_trans) << 7) & ~0xFFLL);
                                    (probe->vga_ptr+i)->vgain = vga_defaults[j].vgain + vgain_delta;
									break;
                                }
                            }
                        }
                    }
                }
            }

            ret = dsl_wr_reg(sdi, COMB_ADDR+6, 0b0011);
            wr_cmd.header.dest = DSL_CTL_DSO_EN0;
            wr_cmd.data[0] = bmCH_CH0;
            wr_cmd.header.size = 1;
            ret = command_ctl_wr(hdl, wr_cmd);
            wr_cmd.header.dest = DSL_CTL_DSO_EN1;
            wr_cmd.data[0] = bmCH_CH1;
            wr_cmd.header.size = 1;
            ret = command_ctl_wr(hdl, wr_cmd);

            devc->zero = FALSE;
            warm_done = FALSE;
            trans_fix_done = FALSE;
            mid_zero_done = FALSE;
            vdiv_back[0] = 0;
            vdiv_back[1] = 0;
            dso_init(sdi);
        }
    }

    return ret;
}

static int config_get(int id, GVariant **data, const struct sr_dev_inst *sdi,
                      const struct sr_channel *ch,
                      const struct sr_channel_group *cg)
{
    unsigned int i;
    int ret;
    struct DSL_context *devc;
    uint64_t vgain_default;

    assert(sdi);
    assert(sdi->priv);
    
    devc = sdi->priv;

    ret = dsl_config_get(id, data, sdi, ch, cg);
    if (ret != SR_OK) {
        switch (id) {
        case SR_CONF_OPERATION_MODE:
            *data = g_variant_new_int16(devc->op_mode);
            break;
        case SR_CONF_BANDWIDTH_LIMIT:
            *data = g_variant_new_int16(devc->bw_limit);
            break;
        case SR_CONF_CALI:
            *data = g_variant_new_boolean(devc->cali);
            break;
        case SR_CONF_TEST:
            *data = g_variant_new_boolean(FALSE);
            break;
        case SR_CONF_STREAM:
            *data = g_variant_new_boolean(devc->stream);
            break;
        case SR_CONF_MAX_DSO_SAMPLERATE:
            *data = g_variant_new_uint64(channel_modes[devc->ch_mode].max_samplerate);
            break;
        case SR_CONF_MAX_DSO_SAMPLELIMITS:
            *data = g_variant_new_uint64(devc->profile->dev_caps.dso_depth);
            break;
        case SR_CONF_HW_DEPTH:
            *data = g_variant_new_uint64(devc->profile->dev_caps.hw_depth / channel_modes[devc->ch_mode].unit_bits);
            break;
        case SR_CONF_PROBE_VGAIN:
            if (!sdi || !ch)
                return SR_ERR;
            *data = g_variant_new_uint64(dso_vga(ch)>>8);
            break;
        case SR_CONF_PROBE_COMB_COMP_EN:
            *data = g_variant_new_boolean((devc->profile->dev_caps.feature_caps & CAPS_FEATURE_HMCAD1511) != 0);
            break;
        case SR_CONF_PROBE_COMB_COMP:
            if (!sdi || !ch)
                return SR_ERR;
            *data = g_variant_new_int16(ch->comb_comp);
            break;
        case SR_CONF_PROBE_VGAIN_DEFAULT:
            if (!sdi || !ch)
                return SR_ERR;
            for (i = 0; vga_defaults[i].id; i++) {
                if (vga_defaults[i].id == devc->profile->dev_caps.vga_id &&
                    vga_defaults[i].key == ch->vdiv) {
                    *data = g_variant_new_uint64(vga_defaults[i].vgain >> 8);
                    break;
                }
            }
            break;
        case SR_CONF_PROBE_VGAIN_RANGE:
            vgain_default = 0;
            for (i = 0; vga_defaults[i].id; i++) {
                if (vga_defaults[i].id == devc->profile->dev_caps.vga_id &&
                    vga_defaults[i].key == ch->vdiv) {
                    vgain_default = vga_defaults[i].vgain;
                    break;
                }
            }
            vgain_default = (vgain_default>>8) & 0x0FFF;
            *data = g_variant_new_uint16(min(CALI_VGAIN_RANGE, vgain_default*2));
            break;
        case SR_CONF_PROBE_PREOFF:
            if (!sdi || !ch)
                return SR_ERR;
            uint16_t preoff = dso_preoff(ch);
            uint16_t preoff_default = get_default_preoff(sdi, ch);
            if (devc->profile->dev_caps.feature_caps & CAPS_FEATURE_PREOFF) {
                int preoff_skew_coarse = (preoff >> 10) - (preoff_default >> 10);
                int preoff_skew_fine = (preoff & 0x03ff) - (preoff_default & 0x03ff);
                double trans_coarse = (ch->vdiv < 500) ? (ch->vpos_trans >> 8)/DSCOPE_TRANS_CMULTI : (ch->vpos_trans >> 8);
                double trans_fine = (ch->vdiv < 500) ? (ch->vpos_trans & 0x00ff) / 1000.0 : (ch->vpos_trans & 0x00ff) / DSCOPE_TRANS_FMULTI;
                double preoff_rate = (preoff_skew_coarse*trans_coarse - preoff_skew_fine*trans_fine) / ch->vdiv;
                preoff = (preoff_rate * 0.5 + 0.5) * devc->profile->dev_caps.default_pwmmargin;
            }
            *data = g_variant_new_uint16(preoff);
            break;
        case SR_CONF_PROBE_PREOFF_DEFAULT:
            if (!sdi || !ch)
                return SR_ERR;
            *data = g_variant_new_uint16(get_default_preoff(sdi, ch));
            break;
        case SR_CONF_PROBE_PREOFF_MARGIN:
            *data = g_variant_new_uint16(devc->profile->dev_caps.default_pwmmargin);
            break;
        case SR_CONF_PROBE_MAP_DEFAULT:
            if (!sdi || !ch)
                return SR_ERR;
            *data = g_variant_new_boolean(ch->map_default);
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
            *data = g_variant_new_int16(channel_modes[devc->ch_mode].vld_num);
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
    int ret, num_probes;
    struct sr_usb_dev_inst *usb;
    struct libusb_device_handle *hdl;
    struct ctl_wr_cmd wr_cmd;
    unsigned int i;
    GSList *l;
    int nv;

    assert(sdi);
    assert(sdi->priv);

    (void)cg;

    if (sdi->status != SR_ST_ACTIVE) {
        return SR_ERR;
    }

	devc = sdi->priv;
    usb = sdi->conn;
    hdl = usb->devhdl;

    ret = dsl_config_set(id, data, sdi, ch, cg);
    if (ret == SR_OK)
        return ret;

    ret = SR_OK;
    if (id == SR_CONF_CLOCK_TYPE) {
        devc->clock_type = g_variant_get_boolean(data);
    } else if (id == SR_CONF_CLOCK_EDGE) {
        devc->clock_edge = g_variant_get_boolean(data);
    } else if (id == SR_CONF_LIMIT_SAMPLES) {
        devc->limit_samples = g_variant_get_uint64(data);
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
        if(sdi->mode == DSO) {
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, 0, SR_CONF_SAMPLERATE));
        }
    } else if (id == SR_CONF_INSTANT) {
        devc->instant = g_variant_get_boolean(data);
        if (sdi->mode == DSO && dsl_en_ch_num(sdi) != 0) {
            if (devc->instant)
                devc->limit_samples = devc->profile->dev_caps.hw_depth / channel_modes[devc->ch_mode].unit_bits /  dsl_en_ch_num(sdi);
            else
                devc->limit_samples = devc->profile->dev_caps.dso_depth / dsl_en_ch_num(sdi);
        }
    } else if (id == SR_CONF_DEVICE_MODE) {
        sdi->mode = g_variant_get_int16(data);
        num_probes = 0;
        if (sdi->mode == DSO) {
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, NULL, SR_CONF_DSO_SYNC));
            if (ret != SR_OK)
                sr_dbg("%s: DSO configuration sync failed", __func__);
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, sdi->channels->data, SR_CONF_PROBE_VDIV));
            if (ret == SR_OK)
                sr_dbg("%s: Initial setting for DSO mode", __func__);
            else
                sr_dbg("%s: Initial setting for DSO mode failed", __func__);
            devc->op_mode = DS_OP_NORMAL;
            devc->test_mode = SR_TEST_NONE;
            devc->instant = FALSE;
            for (i = 0; i < ARRAY_SIZE(channel_modes); i++) {
                if (channel_modes[i].mode == DSO &&
                    devc->profile->dev_caps.channels & (1 << i)) {
                    devc->ch_mode = channel_modes[i].id;
                    num_probes = channel_modes[i].num;
                    devc->stream = channel_modes[i].stream;
                    devc->cur_samplerate = channel_modes[i].max_samplerate / num_probes;
                    break;
                }
            }
            devc->limit_samples = devc->profile->dev_caps.dso_depth / num_probes;
        } else if (sdi->mode == ANALOG) {
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, sdi->channels->data, SR_CONF_PROBE_VDIV));
            if (ret == SR_OK)
                sr_dbg("%s: Initial setting for DSO mode", __func__);
            else
                sr_dbg("%s: Initial setting for DSO mode failed", __func__);
            devc->op_mode = DS_OP_NORMAL;
            devc->test_mode = SR_TEST_NONE;
            devc->instant = TRUE;
            for (i = 0; i < ARRAY_SIZE(channel_modes); i++) {
                if (channel_modes[i].mode == ANALOG &&
                    devc->profile->dev_caps.channels & (1 << i)) {
                    devc->ch_mode = channel_modes[i].id;
                    num_probes = channel_modes[i].num;
                    devc->stream = channel_modes[i].stream;
                    devc->cur_samplerate = channel_modes[i].max_samplerate;
                    break;
                }
            }
            devc->limit_samples = devc->cur_samplerate;
        }
        assert(num_probes != 0);
        dsl_adjust_probes(sdi, num_probes);
        dsl_adjust_samplerate(devc);
        sr_dbg("%s: setting mode to %d", __func__, sdi->mode);
    } 
    else if (id == SR_CONF_OPERATION_MODE) {
        nv = g_variant_get_int16(data);
        if (nv == DS_OP_NORMAL) {
            devc->op_mode = DS_OP_NORMAL;
            devc->test_mode = SR_TEST_NONE;
        }
        else if (nv == DS_OP_INTEST) {
            devc->op_mode = DS_OP_INTEST;
            devc->test_mode = SR_TEST_INTERNAL;
        } 
        else {
            ret = SR_ERR;
        }
        sr_dbg("%s: setting pattern to %d",
            __func__, devc->op_mode);
    } 
    else if (id == SR_CONF_BANDWIDTH_LIMIT) {
        nv = g_variant_get_int16(data);
        if (nv == BW_FULL) {
            devc->bw_limit = BW_FULL;
            dsl_wr_reg(sdi, CTR0_ADDR, bmBW20M_CLR);
        } 
        else if (nv == BW_20M) {
            devc->bw_limit = BW_20M;
            dsl_wr_reg(sdi, CTR0_ADDR, bmBW20M_SET);
        } 
        else {
            ret = SR_ERR;
        }
        sr_dbg("%s: setting bandwidth limit to %d",
            __func__, devc->bw_limit);
    } 
    else if (id == SR_CONF_PROBE_EN) {
        ch->enabled = g_variant_get_boolean(data);

        if (sdi->mode == DSO) {
            if (devc->status == DSL_DATA &&
                devc->profile->dev_caps.feature_caps & CAPS_FEATURE_HMCAD1511) {
                if (dsl_en_ch_num(sdi) == 2) {
                    dsl_config_adc(sdi, adc_dual_ch03);
                } else if (dsl_en_ch_num(sdi) == 1) {
                    for (l = sdi->channels; l; l = l->next) {
                        struct sr_channel *probe = (struct sr_channel *)l->data;
                        if (probe->enabled && probe->index == 0) {
                            dsl_config_adc(sdi, adc_single_ch0);
                            break;
                        } else if (probe->enabled && probe->index == 1) {
                            dsl_config_adc(sdi, adc_single_ch3);
                            break;
                        }
                    }
                }
            }
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, ch, SR_CONF_PROBE_EN));
            if (ch->index == 0) {
                wr_cmd.header.dest = DSL_CTL_DSO_EN0;
                wr_cmd.data[0] = ch->enabled ? bmCH_CH0 : (uint8_t)~bmCH_CH0;
            } else {
                wr_cmd.header.dest = DSL_CTL_DSO_EN1;
                wr_cmd.data[0] = ch->enabled ? bmCH_CH1 : (uint8_t)~bmCH_CH1;
            }
            wr_cmd.header.size = 1;
            ret = command_ctl_wr(hdl, wr_cmd);
            if (dsl_en_ch_num(sdi) != 0) {
                ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, 0, SR_CONF_SAMPLERATE));
                devc->limit_samples = devc->profile->dev_caps.dso_depth / dsl_en_ch_num(sdi);
            }
        } else if (sdi->mode == ANALOG) {
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, ch, SR_CONF_PROBE_EN));
            if (ch->index == 0) {
                wr_cmd.header.dest = DSL_CTL_DSO_EN0;
                wr_cmd.data[0] = bmCH_CH0;
            } else {
                wr_cmd.header.dest = DSL_CTL_DSO_EN1;
                wr_cmd.data[0] = bmCH_CH1;
            }
            wr_cmd.header.size = 1;
            ret = command_ctl_wr(hdl, wr_cmd);
        }
        if (ret == SR_OK)
            sr_dbg("%s: setting ENABLE of channel %d to %d",
                __func__, ch->index, ch->enabled);
        else
            sr_dbg("%s: setting ENABLE of channel %d to %d failed",
                __func__, ch->index, ch->enabled);
    } else if (id == SR_CONF_PROBE_OFFSET) {
        ch->offset = g_variant_get_uint16(data);

        if (ch->offset > devc->profile->dev_caps.ref_max){
            ch->offset = devc->profile->dev_caps.ref_max;
        }
        if (ch->offset < devc->profile->dev_caps.ref_min){
            ch->offset = devc->profile->dev_caps.ref_min;
        }

        if (devc->status != DSL_FINISH)
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, ch, SR_CONF_PROBE_OFFSET));
        else
            ret = SR_OK;
        if (ret == SR_OK)
            sr_dbg("%s: setting OFFSET of channel %d to %d",
                __func__, ch->index, ch->offset);
        else
            sr_dbg("%s: setting OFFSET of channel %d to %d failed",
                __func__, ch->index, ch->offset);
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
            GSList *l;
            unsigned int i, j;
            for(l = sdi->channels; l; l = l->next) {
                struct sr_channel *probe = (struct sr_channel *)l->data;
                probe->vpos_trans = devc->profile->dev_caps.default_pwmtrans;
                //probe->comb_comp = devc->profile->dev_caps.default_comb_comp;
                //probe->digi_fgain = 0;
                if (probe->vga_ptr != NULL) {
                    for (i = 0; devc->profile->dev_caps.vdivs[i]; i++) {
                        for (j = 0; j < ARRAY_SIZE(vga_defaults); j++) {
                            if (vga_defaults[j].id == devc->profile->dev_caps.vga_id &&
                                vga_defaults[j].key == devc->profile->dev_caps.vdivs[i]) {
                                (probe->vga_ptr+i)->id = vga_defaults[j].id;
                                (probe->vga_ptr+i)->key = vga_defaults[j].key;
                                //(probe->vga_ptr+i)->vgain = vga_defaults[j].vgain;
                                (probe->vga_ptr+i)->preoff = vga_defaults[j].preoff;
                                break;
                            }
                        }
                    }
                }
            }  
        } else {
            dso_zero(sdi, TRUE);
        }
    } else if (id == SR_CONF_ZERO_DEFAULT) {
        unsigned int i, j;
        for(l = sdi->channels; l; l = l->next) {
            struct sr_channel *probe = (struct sr_channel *)l->data;
            probe->vpos_trans = devc->profile->dev_caps.default_pwmtrans;
            probe->comb_comp = devc->profile->dev_caps.default_comb_comp;
            probe->digi_fgain = 0;
            probe->cali_fgain0 = 1;
            probe->cali_fgain1 = 1;
            probe->cali_fgain2 = 1;
            probe->cali_fgain3 = 1;
            probe->cali_comb_fgain0 = 1;
            probe->cali_comb_fgain1 = 1;
            probe->cali_comb_fgain2 = 1;
            probe->cali_comb_fgain3 = 1;
            if (probe->vga_ptr != NULL) {
                for (i = 0; devc->profile->dev_caps.vdivs[i]; i++) {
                    for (j = 0; j < ARRAY_SIZE(vga_defaults); j++) {
                        if (vga_defaults[j].id == devc->profile->dev_caps.vga_id &&
                            vga_defaults[j].key == devc->profile->dev_caps.vdivs[i]) {
                            (probe->vga_ptr+i)->id = vga_defaults[j].id;
                            (probe->vga_ptr+i)->key = vga_defaults[j].key;
                            (probe->vga_ptr+i)->vgain = vga_defaults[j].vgain;
                            (probe->vga_ptr+i)->preoff = vga_defaults[j].preoff;
                            break;
                        }
                    }
                }
            }
        }
    } else if (id == SR_CONF_CALI) {
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
            uint8_t *preoff_ptr = &zero_info.zero_addr + 1;
            for (i = 0; probe->vga_ptr && (probe->vga_ptr+i)->id; i++) {
                *(preoff_ptr+2*i) = (probe->vga_ptr+i)->preoff & 0x00ff;
                *(preoff_ptr+2*i+1) = (probe->vga_ptr+i)->preoff >> 8;
            }
            if (i != 0) {
                *(preoff_ptr+2*i) = probe->comb_diff_top;
                *(preoff_ptr+2*i+1) = probe->comb_diff_bom;
                *(preoff_ptr+2*i+2) = (probe->vpos_trans&0x00FF);
                *(preoff_ptr+2*i+3) = (probe->vpos_trans>>8);
                *(preoff_ptr+2*i+4) = probe->comb_comp;
                *(preoff_ptr+2*i+5) = (probe->digi_fgain&0x00FF);
                *(preoff_ptr+2*i+6) = (probe->digi_fgain>>8);
                *(preoff_ptr+2*i+7) = dsl_adc_fgain2code(probe->cali_fgain0);
                *(preoff_ptr+2*i+8) = dsl_adc_fgain2code(probe->cali_fgain1);
                *(preoff_ptr+2*i+9) = dsl_adc_fgain2code(probe->cali_fgain2);
                *(preoff_ptr+2*i+10) = dsl_adc_fgain2code(probe->cali_fgain3);
                *(preoff_ptr+2*i+11) = dsl_adc_fgain2code(probe->cali_comb_fgain0);
                *(preoff_ptr+2*i+12) = dsl_adc_fgain2code(probe->cali_comb_fgain1);
                *(preoff_ptr+2*i+13) = dsl_adc_fgain2code(probe->cali_comb_fgain2);
                *(preoff_ptr+2*i+14) = dsl_adc_fgain2code(probe->cali_comb_fgain3);

                vga_info.vga_addr = zero_info.zero_addr + sizeof(struct cmd_zero_info);
                uint16_t *vgain_ptr = &vga_info.vga0;
                for (i=0; probe->vga_ptr && (probe->vga_ptr+i)->id; i++){
                    *(vgain_ptr+i) = (probe->vga_ptr+i)->vgain >> 8;
                }
                ret = dsl_wr_reg(sdi, CTR0_ADDR, bmEEWP);
                if (ret == SR_OK) {
                    if (devc->profile->dev_caps.feature_caps & CAPS_FEATURE_SEEP)
                        real_zero_addr = zero_info.zero_addr;
                    else if (devc->profile->dev_caps.feature_caps & CAPS_FEATURE_FLASH)
                        real_zero_addr = probe->index * DSO_ZERO_PAGE;
                    else
                        real_zero_addr = (zero_big_addr << 8) + zero_info.zero_addr;
                    ret = dsl_wr_nvm(sdi, (unsigned char *)&zero_info, real_zero_addr, sizeof(struct cmd_zero_info));
                }
                if (ret == SR_OK) {
                    if (devc->profile->dev_caps.feature_caps & CAPS_FEATURE_SEEP)
                        real_zero_addr = vga_info.vga_addr;
                    else if (devc->profile->dev_caps.feature_caps & CAPS_FEATURE_FLASH)
                        real_zero_addr = probe->index * DSO_ZERO_PAGE + 1;
                    else
                        real_zero_addr = (zero_big_addr << 8) + vga_info.vga_addr;
                    ret = dsl_wr_nvm(sdi, (unsigned char *)&vga_info, real_zero_addr, sizeof(struct cmd_vga_info));
                }
                ret = dsl_wr_reg(sdi, CTR0_ADDR, bmNONE);

                if (!(devc->profile->dev_caps.feature_caps & CAPS_FEATURE_HMCAD1511)) {
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
        }
    } else if (id == SR_CONF_VOCM) {
        const uint8_t vocm = g_variant_get_byte(data);
        ret = dsl_wr_reg(sdi, COMB_ADDR+4, vocm);
    } else if (id == SR_CONF_PROBE_VGAIN) {
        const uint64_t vgain = g_variant_get_uint64(data) << 8;
        int i;
        for (i = 0; ch->vga_ptr && (ch->vga_ptr+i)->id; i++) {
            if ((ch->vga_ptr+i)->key == ch->vdiv) {
                (ch->vga_ptr+i)->vgain = vgain;
            }
        }
        ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, ch, SR_CONF_PROBE_VDIV));
        if (ret == SR_OK)
            sr_dbg("%s: setting VDIV of channel %d to %d mv",
                __func__, ch->index, ch->vdiv);
        else
            sr_dbg("%s: setting VDIV of channel %d to %d mv failed",
                __func__, ch->index, ch->vdiv);
    } else if (id == SR_CONF_PROBE_PREOFF) {
        uint16_t preoff = g_variant_get_uint16(data);
        if (devc->profile->dev_caps.feature_caps & CAPS_FEATURE_PREOFF) {
            double voltage_off = (2.0 * preoff / devc->profile->dev_caps.default_pwmmargin  - 1) * ch->vdiv;
            double trans_coarse = (ch->vdiv < 500) ? (ch->vpos_trans >> 8)/DSCOPE_TRANS_CMULTI : (ch->vpos_trans >> 8);
            double trans_fine = (ch->vdiv < 500) ? (ch->vpos_trans & 0x00ff) / 1000.0 : (ch->vpos_trans & 0x00ff) / DSCOPE_TRANS_FMULTI;

            uint16_t default_preoff = get_default_preoff(sdi, ch);
            int preoff_coarse = floor(voltage_off / trans_coarse + 0.5);
            int preoff_fine = floor(-(voltage_off - preoff_coarse*trans_coarse)/trans_fine + 0.5);
            preoff_coarse = (default_preoff >> 10) + preoff_coarse;
            preoff_fine = (default_preoff&0x03ff) + preoff_fine;
            preoff = (preoff_coarse << 10) + preoff_fine;
        }
        int i;
        for (i = 0; ch->vga_ptr && (ch->vga_ptr+i)->id; i++) {
            if ((ch->vga_ptr+i)->key == ch->vdiv) {
                (ch->vga_ptr+i)->preoff = preoff;
            }
        }
        if (devc->status != DSL_FINISH)
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, ch, SR_CONF_PROBE_OFFSET));
        else
            ret = SR_OK;

        if (ret == SR_OK)
            sr_dbg("%s: setting OFFSET of channel %d to %d",
                __func__, ch->index, ch->offset);
        else
            sr_dbg("%s: setting OFFSET of channel %d to %d failed",
                __func__, ch->index, ch->offset);
    } else if (id == SR_CONF_PROBE_COMB_COMP) {
        ch->comb_comp = g_variant_get_int16(data);
        ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, ch, SR_CONF_PROBE_VDIV));
        ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, ch, SR_CONF_PROBE_OFFSET));
        if (ret == SR_OK)
            sr_dbg("%s: setting COMB_COMP of channel %d to %d mv",
                __func__, ch->index, ch->comb_comp);
        else
            sr_dbg("%s: setting COMB_COMP of channel %d to %d mv failed",
                __func__, ch->index, ch->comb_comp);
    } else {
        ret = SR_ERR_NA;
	}

	return ret;
}

static int config_list(int key, GVariant **data, const struct sr_dev_inst *sdi,
                       const struct sr_channel_group *cg)
{
    assert(sdi);
    assert(sdi->priv);

    (void)cg;

    if (dsl_config_list(key, data, sdi, cg) == SR_OK)
        return SR_OK;

    switch (key) {
    case SR_CONF_DEVICE_OPTIONS:
        *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                hwoptions, ARRAY_SIZE(hwoptions)*sizeof(int32_t), TRUE, NULL, NULL);
        break;
    case SR_CONF_DEVICE_SESSIONS:
        if (sdi->mode == DSO)
            *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                    sessions_dso, ARRAY_SIZE(sessions_dso)*sizeof(int32_t), TRUE, NULL, NULL);
        else if (sdi->mode == ANALOG)
            *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                    sessions_daq, ARRAY_SIZE(sessions_daq)*sizeof(int32_t), TRUE, NULL, NULL);
        break;
    case SR_CONF_OPERATION_MODE: 
        *data = g_variant_new_uint64((uint64_t)&opmode_list);
        break;
    case SR_CONF_BANDWIDTH_LIMIT: 
        *data = g_variant_new_uint64((uint64_t)&bandwidth_list);
        break;
    default:
        return SR_ERR_NA;
    }

    return SR_OK;
}

static int dso_tune(const struct sr_dev_inst *sdi)
{
    struct DSL_context *devc = sdi->priv;
    int ret = SR_OK;
    double margin;
    static uint64_t vdiv_back = 0;
    static uint16_t offset_back = 0;
    static int coupling_back = SR_DC_COUPLING;
    const uint8_t mux0[8] = {0x09, 0x0f, 0x0b, 0x0d, 0x07, 0x05, 0x01, 0x03};
    const uint8_t mux1[8] = {0x09, 0x0f, 0x0b, 0x0d, 0x0e, 0x0c, 0x08, 0x0a};
    const uint8_t *mux = (devc->profile->dev_caps.feature_caps & CAPS_FEATURE_POGOPIN) ? mux1 : mux0;

    if (devc->tune_probe && devc->tune_stage == -1) {
        vdiv_back = devc->tune_probe->vdiv;
        offset_back = devc->tune_probe->offset;
        coupling_back = devc->tune_probe->coupling;

        devc->tune_stage = 0;
        ret = dsl_wr_ext(sdi, 0x03, 0x00);
        ret = dsl_wr_ext(sdi, 0x01, mux[devc->tune_stage]);
        devc->tune_probe->vdiv = (devc->tune_probe->vga_ptr + devc->tune_stage)->key;
        devc->tune_probe->offset = (1 << (channel_modes[devc->ch_mode].unit_bits - 1));
        devc->tune_probe->coupling = SR_AC_COUPLING;
        ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, devc->tune_probe, SR_CONF_PROBE_VDIV));
        ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, devc->tune_probe, SR_CONF_PROBE_OFFSET));
        ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, devc->tune_probe, SR_CONF_PROBE_COUPLING));
    } else if (devc->tune_probe && devc->profile->dev_caps.vdivs[devc->tune_stage] != 0) {
        if (devc->tune_pcnt == 10) {
            devc->tune_pcnt = 0;
            margin = (devc->tune_probe->coupling == SR_AC_COUPLING) ? 127.5 : 25.5;
            if (devc->tune_probe->index == 0)
                margin -= (devc->mstatus.ch0_acc_mean * 1.0 / devc->limit_samples);
            else
                margin -= (devc->mstatus.ch1_acc_mean * 1.0 / devc->limit_samples);

            if ((devc->tune_probe->coupling == SR_AC_COUPLING) && (fabs(margin) < 0.5)) {
                devc->tune_probe->coupling = SR_DC_COUPLING;
                ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, devc->tune_probe, SR_CONF_PROBE_COUPLING));
            } else if (devc->tune_probe->coupling == SR_AC_COUPLING){
                (devc->tune_probe->vga_ptr+devc->tune_stage)->preoff += margin;
                ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, devc->tune_probe, SR_CONF_PROBE_OFFSET));
            } else if ((devc->tune_probe->coupling == SR_DC_COUPLING) && (fabs(margin) < 0.5)) {
                devc->tune_stage++;
                if (devc->profile->dev_caps.vdivs[devc->tune_stage] != 0) {
                    ret = dsl_wr_ext(sdi, 0x01, mux[devc->tune_stage]);
                    devc->tune_probe->vdiv = (devc->tune_probe->vga_ptr + devc->tune_stage)->key;
                    devc->tune_probe->coupling = SR_AC_COUPLING;
                    ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, devc->tune_probe, SR_CONF_PROBE_VDIV));
                    ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, devc->tune_probe, SR_CONF_PROBE_OFFSET));
                    ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, devc->tune_probe, SR_CONF_PROBE_COUPLING));
                }else {
                    ret = dsl_wr_ext(sdi, 0x01, mux[0]);
                    devc->tune_probe->vdiv = vdiv_back;
                    devc->tune_probe->offset = offset_back;
                    devc->tune_probe->coupling = coupling_back;
                    ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, devc->tune_probe, SR_CONF_PROBE_VDIV));
                    ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, devc->tune_probe, SR_CONF_PROBE_OFFSET));
                    ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, devc->tune_probe, SR_CONF_PROBE_COUPLING));
                    devc->tune = FALSE;
                }
            } else if (devc->tune_probe->coupling == SR_DC_COUPLING){
                (devc->tune_probe->vga_ptr + devc->tune_stage)->vgain -= ceil(margin*1024);
                ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, devc->tune_probe, SR_CONF_PROBE_VDIV));
            }
        }
        if (ret == SR_OK)
            devc->tune_pcnt++;
    }

    return ret;
}

static int dev_open(struct sr_dev_inst *sdi)
{
    gboolean fpga_done;
    int ret;
    GSList *l;
    gboolean zeroed;
    struct DSL_context *devc = sdi->priv;

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
        if (!fpga_done) {
            if (devc->profile->dev_caps.feature_caps & CAPS_FEATURE_HMCAD1511) {
                dsl_config_adc(sdi, adc_init_fix);
                dsl_config_adc(sdi, adc_clk_init_1g);
                dsl_config_adc(sdi, adc_power_down);
            }
            dso_init(sdi);
        }
    }

    return ret;
}

static int dev_close(struct sr_dev_inst *sdi)
{
    int ret;
    dso_zero(sdi, TRUE);
    ret = dsl_dev_close(sdi);
    return ret;
}

static int dev_destroy(struct sr_dev_inst *sdi)
{
    return dsl_destroy_device(sdi);
}

static int cleanup(void)
{
    safe_free(di->priv);

    return SR_OK;
}

static int receive_data(int fd, int revents, const struct sr_dev_inst *sdi)
{
    int completed = 0;
    struct timeval tv;
    struct drv_context *drvc;
    struct DSL_context *devc;
    struct ctl_rd_cmd rd_cmd;
    struct sr_usb_dev_inst *usb;
    int ret;

    (void)fd;
    (void)revents;

    drvc = di->priv;
    devc = sdi->priv;
    usb = sdi->conn;

    tv.tv_sec = tv.tv_usec = 0;
    libusb_handle_events_timeout_completed(drvc->sr_ctx->libusb_ctx, &tv, &completed);

    if (devc->trf_completed)
        devc->empty_poll_count = 0;
    else
        devc->empty_poll_count++;

    if (devc->zero && devc->trf_completed) {
        dso_zero(sdi, FALSE);
    }
    if (devc->tune && devc->trf_completed) {
        dso_tune(sdi);
    }

    // progress check
    if ((devc->empty_poll_count > MAX_EMPTY_POLL) && (devc->status == DSL_START)) {
        devc->mstatus.captured_cnt0 = 0;
        rd_cmd.header.dest = DSL_CTL_I2C_STATUS;
        rd_cmd.header.offset = 0;
        rd_cmd.header.size = 4;
        rd_cmd.data = (unsigned char*)&devc->mstatus;
        if ((ret = command_ctl_rd(usb->devhdl, rd_cmd)) != SR_OK)
            sr_err("Failed to get progress infos.");

        devc->empty_poll_count = 0;
    }

    if (devc->status == DSL_FINISH) {
        sr_session_source_remove((gintptr) devc->channel);
    }

    devc->trf_completed = 0;
    return TRUE;
}

static int dev_acquisition_start(struct sr_dev_inst *sdi, void *cb_data)
{
    (void)cb_data;

    struct DSL_context *devc;
    struct sr_usb_dev_inst *usb;
    int ret;
    struct ctl_wr_cmd wr_cmd;
    GSList *l;

    if (sdi->status != SR_ST_ACTIVE){
        ds_set_last_error(SR_ERR_DEVICE_CLOSED);
        return SR_ERR_DEVICE_CLOSED;
    }

    devc = sdi->priv;
    usb = sdi->conn;

    //devc->cb_data = cb_data;
    devc->cb_data = sdi;
    devc->num_samples = 0;
    devc->empty_transfer_count = 0;
    devc->empty_poll_count = 0;
    devc->status = DSL_INIT;
    devc->num_transfers = 0;
    devc->submitted_transfers = 0;
    devc->actual_samples = (devc->limit_samples + SAMPLES_ALIGN) & ~SAMPLES_ALIGN;
	devc->abort = FALSE;
    devc->mstatus_valid = FALSE;
    devc->mstatus.captured_cnt0 = 0;
    devc->mstatus.captured_cnt1 = 0;
    devc->mstatus.captured_cnt2 = 0;
    devc->mstatus.captured_cnt3 = 0;
    devc->mstatus.trig_hit = 0;
    devc->overflow = FALSE;
    devc->instant_tail_bytes = dsl_header_size(devc);

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
    if (devc->profile->dev_caps.feature_caps & CAPS_FEATURE_HMCAD1511) {
        if (dsl_en_ch_num(sdi) == 2) {
            dsl_config_adc(sdi, adc_dual_ch03);
            for (l = sdi->channels; l; l = l->next) {
                struct sr_channel *probe = (struct sr_channel *)l->data;
                dsl_config_adc_fgain(sdi, probe->index*2 + 0, probe->cali_fgain0, probe->cali_fgain1);
                dsl_config_adc_fgain(sdi, probe->index*2 + 1, probe->cali_fgain2, probe->cali_fgain3);
            }
        } else if (dsl_en_ch_num(sdi) == 1) {
            for (l = sdi->channels; l; l = l->next) {
                struct sr_channel *probe = (struct sr_channel *)l->data;
                if (probe->enabled && probe->index == 0) {
                    dsl_config_adc(sdi, adc_single_ch0);
                } else if (probe->enabled && probe->index == 1) {
                    dsl_config_adc(sdi, adc_single_ch3);
                }
            }
            for (l = sdi->channels; l; l = l->next) {
                struct sr_channel *probe = (struct sr_channel *)l->data;
                dsl_config_adc_fgain(sdi, probe->index*2 + 0, probe->cali_comb_fgain0, probe->cali_comb_fgain1);
                dsl_config_adc_fgain(sdi, probe->index*2 + 1, probe->cali_comb_fgain2, probe->cali_comb_fgain3);
            }
        }
        dsl_config_fpga_fgain(sdi);
    }
    if ((ret = dsl_fpga_arm(sdi)) != SR_OK) {
        sr_err("%s: Arm FPGA failed!", __func__);
        return ret;
    }

    if (devc->zero && devc->zero_stage == -1) {
        // initialize before Auto Calibration
        if ((ret = dso_init(sdi)) != SR_OK) {
            sr_err("%s: DSO zero initialization failed!", __func__);
            return ret;
        }
        devc->zero_stage = 0;
        devc->zero_comb_fgain = FALSE;
        devc->zero_branch = FALSE;
    }

    /*
     * settings must be updated before acquisition
     */
    if (sdi->mode != LOGIC) {
        devc->trigger_hpos =  devc->trigger_hrate * dsl_en_ch_num(sdi) * devc->limit_samples / 200.0;
        ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, NULL, SR_CONF_HORIZ_TRIGGERPOS));
        if (ret != SR_OK)
            sr_dbg("%s: setting DSO Horiz Trigger Position to %d failed", __func__, devc->trigger_hpos);

        for(l = sdi->channels; l; l = l->next) {
            struct sr_channel *probe = (struct sr_channel *)l->data;
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe, SR_CONF_PROBE_VDIV));
            if (ret != SR_OK)
                sr_err("%s: Set VDIV of channel %d command failed!", __func__, probe->index);
            ret = dsl_wr_dso(sdi, dso_cmd_gen(sdi, probe, SR_CONF_PROBE_OFFSET));
            if (ret != SR_OK)
                sr_err("%s: Set OFFSET of channel %d command failed!", __func__, probe->index);
            probe->hw_offset = probe->offset;
        }
    }

    /* setup and submit usb transfer */
    if ((ret = dsl_start_transfers(devc->cb_data)) != SR_OK) {
        sr_err("%s: Could not submit usb transfer"
               "(%d)%d", __func__, ret, errno);
        return ret;
    }

    sr_session_source_add ((gintptr) devc->channel, G_IO_IN | G_IO_ERR,
                           (int) dsl_get_timeout(sdi), receive_data, sdi);

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

static int dev_status_get(const struct sr_dev_inst *sdi, struct sr_status *status, gboolean prg)
{
    int ret = dsl_dev_status_get(sdi, status, prg);
    return ret;
}

SR_PRIV int sr_dscope_option_value_to_code(const struct sr_dev_inst *sdi, int config_id, const char *value)
{
    int num;
    
    (void)sdi;
    assert(sdi);
    
    num = sizeof(lang_text_map) / sizeof(lang_text_map[0]);
    return sr_option_value_to_code(config_id, value, 
                (const struct lang_text_map_item*)lang_text_map, num);
}

SR_PRIV struct sr_dev_driver DSCope_driver_info = {
    .name = "DSCope",
    .longname = "DSCope (generic driver for DScope oscilloscope)",
	.api_version = 1,
    .driver_type = DRIVER_TYPE_HARDWARE,
	.init = init,
	.cleanup = cleanup,
	.scan = scan, 
    .dev_mode_list = dev_mode_list,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
    .dev_destroy = dev_destroy,
    .dev_status_get = dev_status_get,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.priv = NULL,
};
