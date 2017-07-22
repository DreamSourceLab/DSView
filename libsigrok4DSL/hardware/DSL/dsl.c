/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2017 DreamSourceLab <dreamsourcelab@dreamsourcelab.com>
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
#include "command.h"
#include "dsl.h"

#include <math.h>
#include <assert.h>
#include <sys/stat.h>

extern struct ds_trigger *trigger;

static const unsigned int single_buffer_time = 20;
static const unsigned int total_buffer_time = 100;
static const unsigned int instant_buffer_size = 1024 * 1024;

SR_PRIV int dsl_en_ch_num(const struct sr_dev_inst *sdi)
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

/**
 * Check the USB configuration to determine if this is an dsl device.
 *
 * @return TRUE if the device's configuration profile match dsl hardware
 *         configuration, FALSE otherwise.
 */
SR_PRIV gboolean dsl_check_conf_profile(libusb_device *dev)
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
        if (strncmp((const char *)strdesc, "USB-based DSL Instrument v2", 27))
            break;

        /* If we made it here, it must be an dsl device. */
        ret = TRUE;
    }
    if (hdl)
        libusb_close(hdl);

    return ret;
}

static int hw_dev_open(struct sr_dev_driver *di, struct sr_dev_inst *sdi)
{
    libusb_device **devlist;
    struct sr_usb_dev_inst *usb;
    struct libusb_device_descriptor des;
    struct DSL_context *devc;
    struct drv_context *drvc;
    struct version_info vi;
    int ret, skip, i, device_count;
    uint8_t revid;
    struct ctl_rd_cmd rd_cmd;
    uint8_t rd_cmd_data[2];

    drvc = di->priv;
    devc = sdi->priv;
    usb = sdi->conn;

    if (sdi->status == SR_ST_ACTIVE) {
        /* Device is already in use. */
        return SR_ERR;
    }

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

        rd_cmd.header.dest = DSL_CTL_FW_VERSION;
        rd_cmd.header.size = 2;
        rd_cmd.data = rd_cmd_data;
        if ((ret = command_ctl_rd(usb->devhdl, rd_cmd)) != SR_OK) {
            sr_err("Failed to get firmware version.");
            break;
        }
        vi.major = rd_cmd_data[0];
        vi.minor = rd_cmd_data[1];

        rd_cmd.header.dest = DSL_CTL_REVID_VERSION;
        rd_cmd.header.size = 1;
        rd_cmd.data = &revid;
        if ((ret = command_ctl_rd(usb->devhdl, rd_cmd)) != SR_OK) {
            sr_err("Failed to get REVID.");
            break;
        }

        /*
         * Different versions may have incompatible issue,
         * Mark for up level process
         */
        if (vi.major != DSL_REQUIRED_VERSION_MAJOR ||
            vi.minor != DSL_REQUIRED_VERSION_MINOR) {
            sr_err("Expected firmware version %d.%d, "
                   "got %d.%d.", DSL_REQUIRED_VERSION_MAJOR, DSL_REQUIRED_VERSION_MINOR,
                   vi.major, vi.minor);
            sdi->status = SR_ST_INCOMPATIBLE;
        } else {
            sdi->status = SR_ST_ACTIVE;
        }

        sr_info("Opened device %d on %d.%d, "
            "interface %d, firmware %d.%d.",
            sdi->index, usb->bus, usb->address,
            USB_INTERFACE, vi.major, vi.minor);

        sr_info("Detected REVID=%d, it's a Cypress CY7C68013%s.",
            revid, (revid != 1) ? " (FX2)" : "A (FX2LP)");

        break;
    }
    libusb_free_device_list(devlist, 1);

    if ((sdi->status != SR_ST_ACTIVE) &&
        (sdi->status != SR_ST_INCOMPATIBLE))
        return SR_ERR;

    return SR_OK;
}

SR_PRIV int dsl_configure_probes(const struct sr_dev_inst *sdi)
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

    return SR_OK;
}

SR_PRIV uint64_t dsl_channel_depth(const struct sr_dev_inst *sdi)
{
    int ch_num = dsl_en_ch_num(sdi);
    if (strcmp(sdi->model, "DSLogic Basic") == 0)
        return DSLOGIC_BASIC_MEM_DEPTH / (ch_num ? ch_num : 1);
    else
        return DSLOGIC_MEM_DEPTH / (ch_num ? ch_num : 1);
}

SR_PRIV int dsl_wr_reg(const struct sr_dev_inst *sdi, uint8_t addr, uint8_t value)
{
    struct sr_usb_dev_inst *usb;
    struct libusb_device_handle *hdl;
    struct ctl_wr_cmd wr_cmd;
    int ret;

    usb = sdi->conn;
    hdl = usb->devhdl;

    wr_cmd.header.dest = DSL_CTL_I2C_REG;
    wr_cmd.header.offset = addr;
    wr_cmd.header.size = 1;
    wr_cmd.data[0] = value;
    if ((ret = command_ctl_wr(hdl, wr_cmd)) != SR_OK) {
        sr_err("Sent DSL_CTL_I2C_REG command failed.");
        return SR_ERR;
    }

    return SR_OK;
}

SR_PRIV int dsl_wr_dso(const struct sr_dev_inst *sdi, uint64_t cmd)
{
    struct sr_usb_dev_inst *usb;
    struct libusb_device_handle *hdl;
    struct ctl_wr_cmd wr_cmd;
    int ret;

    usb = sdi->conn;
    hdl = usb->devhdl;

    wr_cmd.header.dest = DSL_CTL_I2C_DSO;
    wr_cmd.header.offset = 0;
    wr_cmd.header.size = 8;
    wr_cmd.data[0] = (uint8_t)cmd;
    wr_cmd.data[1] = (uint8_t)(cmd >> 8);
    wr_cmd.data[2] = (uint8_t)(cmd >> 16);
    wr_cmd.data[3] = (uint8_t)(cmd >> 24);
    wr_cmd.data[4] = (uint8_t)(cmd >> 32);
    wr_cmd.data[5] = (uint8_t)(cmd >> 40);
    wr_cmd.data[6] = (uint8_t)(cmd >> 48);
    wr_cmd.data[7] = (uint8_t)(cmd >> 56);
    if ((ret = command_ctl_wr(hdl, wr_cmd)) != SR_OK) {
        sr_err("Sent DSL_CTL_I2C_DSO command failed.");
        return SR_ERR;
    }

    return SR_OK;
}

SR_PRIV int dsl_wr_nvm(const struct sr_dev_inst *sdi, unsigned char *ctx, uint16_t addr, uint8_t len)
{
    struct sr_usb_dev_inst *usb;
    struct libusb_device_handle *hdl;
    struct ctl_wr_cmd wr_cmd;
    int ret;
    int i;

    usb = sdi->conn;
    hdl = usb->devhdl;

    wr_cmd.header.dest = DSL_CTL_NVM;
    wr_cmd.header.offset = addr;
    wr_cmd.header.size = len;
    for (i = 0; i < len; i++)
        wr_cmd.data[i] = *(ctx+i);
    if ((ret = command_ctl_wr(hdl, wr_cmd)) != SR_OK) {
        sr_err("Sent DSL_CTL_NVM write command failed.");
        return SR_ERR;
    }

    return SR_OK;
}

SR_PRIV int dsl_rd_nvm(const struct sr_dev_inst *sdi, unsigned char *ctx, uint16_t addr, uint8_t len)
{
    struct sr_usb_dev_inst *usb;
    struct libusb_device_handle *hdl;
    struct ctl_rd_cmd rd_cmd;
    int ret;

    usb = sdi->conn;
    hdl = usb->devhdl;

    rd_cmd.header.dest = DSL_CTL_NVM;
    rd_cmd.header.size = len;
    rd_cmd.header.offset = addr;
    rd_cmd.data = ctx;
    if ((ret = command_ctl_rd(hdl, rd_cmd)) != SR_OK) {
        sr_err("Sent DSL_CTL_NVM read command failed.");
        return SR_ERR;
    }

    return SR_OK;
}

SR_PRIV int dsl_fpga_arm(const struct sr_dev_inst *sdi)
{
    struct DSL_context *devc;
    struct sr_usb_dev_inst *usb;
    struct libusb_device_handle *hdl;
    struct DSL_setting setting;
    int ret;
    int transferred;
    int i;
    GSList *l;
    uint32_t tmp_u32;
    uint64_t tmp_u64;
    const int ch_num = dsl_en_ch_num(sdi);
    uint32_t arm_size;
    struct ctl_wr_cmd wr_cmd;
    struct ctl_rd_cmd rd_cmd;
    uint8_t rd_cmd_data;

    devc = sdi->priv;
    usb = sdi->conn;
    hdl = usb->devhdl;

    setting.sync = 0xf5a5f5a5;
    setting.mode_header = 0x0001;
    setting.divider_header = 0x0102;
    setting.count_header = 0x0302;
    setting.trig_pos_header = 0x0502;
    setting.trig_glb_header = 0x0701;
    setting.ch_en_header = 0x0801;
    setting.trig_header = 0x40a0;
    setting.end_sync = 0xfa5afa5a;

    // basic configuration
    setting.mode = (trigger->trigger_en << TRIG_EN_BIT) +
                   (devc->clock_type << CLK_TYPE_BIT) +
                   (devc->clock_edge << CLK_EDGE_BIT) +
                   (devc->rle_mode << RLE_MODE_BIT) +
                   ((sdi->mode == DSO) << DSO_MODE_BIT) +
                   ((((devc->cur_samplerate == (2 * DSLOGIC_MAX_LOGIC_SAMPLERATE)) && sdi->mode != DSO) || (sdi->mode == ANALOG)) << HALF_MODE_BIT) +
                   ((devc->cur_samplerate == (4 * DSLOGIC_MAX_LOGIC_SAMPLERATE)) << QUAR_MODE_BIT) +
                   ((sdi->mode == ANALOG) << ANALOG_MODE_BIT) +
                   ((devc->filter == SR_FILTER_1T) << FILTER_BIT) +
                   (devc->instant << INSTANT_BIT) +
                   ((trigger->trigger_mode == SERIAL_TRIGGER) << STRIG_MODE_BIT) +
                   ((devc->stream) << STREAM_MODE_BIT) +
                   ((devc->op_mode == SR_OP_LOOPBACK_TEST) << LPB_TEST_BIT) +
                   ((devc->op_mode == SR_OP_EXTERNAL_TEST) << EXT_TEST_BIT) +
                   ((devc->op_mode == SR_OP_INTERNAL_TEST) << INT_TEST_BIT);

    // sample rate divider
    tmp_u32  = (sdi->mode == DSO) ? (uint32_t)ceil(DSLOGIC_MAX_DSO_SAMPLERATE * 1.0 / devc->cur_samplerate / ch_num) :
                                    (uint32_t)ceil(DSLOGIC_MAX_LOGIC_SAMPLERATE * 1.0 / devc->cur_samplerate);
    setting.div_l = tmp_u32 & 0x0000ffff;
    setting.div_h = tmp_u32 >> 16;

    // capture counter
    // analog: 16bits, but sample with half mode(0-7 valid only)
    tmp_u64 = (sdi->mode == DSO) ?    (devc->limit_samples / (g_slist_length(sdi->channels) / ch_num)) :
              (sdi->mode == ANALOG) ? (devc->limit_samples * g_slist_length(sdi->channels) * 4) :
                                      (devc->limit_samples);
    tmp_u64 >>= 4; // hardware minimum unit 64
    setting.cnt_l = tmp_u64 & 0x0000ffff;
    setting.cnt_h = tmp_u64 >> 16;

    // trigger position
    // must be align to minimum parallel bits
    tmp_u32 = max((uint32_t)(trigger->trigger_pos / 100.0 * devc->limit_samples), DSLOGIC_ATOMIC_SAMPLES);
    if (devc->stream)
        tmp_u32 = min(tmp_u32, dsl_channel_depth(sdi) * 10 / 100);
    else
        tmp_u32 = min(tmp_u32, dsl_channel_depth(sdi) * DS_MAX_TRIG_PERCENT / 100);
    setting.tpos_l = tmp_u32 & DSLOGIC_ATOMIC_MASK;
    setting.tpos_h = tmp_u32 >> 16;

    // trigger global settings
    setting.trig_glb = ((ch_num & 0xf) << 4) +
                       trigger->trigger_stages;

    // channel enable mapping
    setting.ch_en = 0;
    for (l = sdi->channels; l; l = l->next) {
        struct sr_channel *probe = (struct sr_channel *)l->data;
        setting.ch_en += probe->enabled << probe->index;
    }

    // trigger advanced configuration
    if (trigger->trigger_mode == SIMPLE_TRIGGER) {
        setting.trig_mask0[0] = ds_trigger_get_mask0(TriggerStages);
        setting.trig_mask1[0] = ds_trigger_get_mask1(TriggerStages);

        setting.trig_value0[0] = ds_trigger_get_value0(TriggerStages);
        setting.trig_value1[0] = ds_trigger_get_value1(TriggerStages);

        setting.trig_edge0[0] = ds_trigger_get_edge0(TriggerStages);
        setting.trig_edge1[0] = ds_trigger_get_edge1(TriggerStages);

        if (setting.mode & (1 << QUAR_MODE_BIT)) {
            setting.trig_mask0[0] = ((setting.trig_mask0[0] & 0x0f) << 12) +
                                    ((setting.trig_mask0[0] & 0x0f) << 8) +
                                    ((setting.trig_mask0[0] & 0x0f) << 4) +
                                    ((setting.trig_mask0[0] & 0x0f) << 0);
            setting.trig_mask1[0] = ((setting.trig_mask1[0] & 0x0f) << 12) +
                                    ((setting.trig_mask1[0] & 0x0f) << 8) +
                                    ((setting.trig_mask1[0] & 0x0f) << 4) +
                                    ((setting.trig_mask1[0] & 0x0f) << 0);
            setting.trig_value0[0] = ((setting.trig_value0[0] & 0x0f) << 12) +
                                     ((setting.trig_value0[0] & 0x0f) << 8) +
                                     ((setting.trig_value0[0] & 0x0f) << 4) +
                                     ((setting.trig_value0[0] & 0x0f) << 0);
            setting.trig_value1[0] = ((setting.trig_value1[0] & 0x0f) << 12) +
                                     ((setting.trig_value1[0] & 0x0f) << 8) +
                                     ((setting.trig_value1[0] & 0x0f) << 4) +
                                     ((setting.trig_value1[0] & 0x0f) << 0);
            setting.trig_edge0[0] = ((setting.trig_edge0[0] & 0x0f) << 12) +
                                    ((setting.trig_edge0[0] & 0x0f) << 8) +
                                    ((setting.trig_edge0[0] & 0x0f) << 4) +
                                    ((setting.trig_edge0[0] & 0x0f) << 0);
            setting.trig_edge1[0] = ((setting.trig_edge1[0] & 0x0f) << 12) +
                                    ((setting.trig_edge1[0] & 0x0f) << 8) +
                                    ((setting.trig_edge1[0] & 0x0f) << 4) +
                                    ((setting.trig_edge1[0] & 0x0f) << 0);
        } else if (setting.mode & (1 << HALF_MODE_BIT)) {
            setting.trig_mask0[0] = ((setting.trig_mask0[0] & 0xff) << 8) +
                                    ((setting.trig_mask0[0] & 0xff) << 0);
            setting.trig_mask1[0] = ((setting.trig_mask1[0] & 0xff) << 8) +
                                    ((setting.trig_mask1[0] & 0xff) << 0);
            setting.trig_value0[0] = ((setting.trig_value0[0] & 0xff) << 8) +
                                     ((setting.trig_value0[0] & 0xff) << 0);
            setting.trig_value1[0] = ((setting.trig_value1[0] & 0xff) << 8) +
                                     ((setting.trig_value1[0] & 0xff) << 0);
            setting.trig_edge0[0] = ((setting.trig_edge0[0] & 0xff) << 8) +
                                    ((setting.trig_edge0[0] & 0xff) << 0);
            setting.trig_edge1[0] = ((setting.trig_edge1[0] & 0xff) << 8) +
                                    ((setting.trig_edge1[0] & 0xff) << 0);
        }

        setting.trig_logic0[0] = (trigger->trigger_logic[TriggerStages] << 1) + trigger->trigger0_inv[TriggerStages];
        setting.trig_logic1[0] = (trigger->trigger_logic[TriggerStages] << 1) + trigger->trigger1_inv[TriggerStages];

        setting.trig_count[0] = trigger->trigger0_count[TriggerStages];

        for (i = 1; i < NUM_TRIGGER_STAGES; i++) {
            setting.trig_mask0[i] = 0xffff;
            setting.trig_mask1[i] = 0xffff;

            setting.trig_value0[i] = 0;
            setting.trig_value1[i] = 0;

            setting.trig_edge0[i] = 0;
            setting.trig_edge1[i] = 0;

            setting.trig_logic0[i] = 2;
            setting.trig_logic1[i] = 2;

            setting.trig_count[i] = 0;
        }
    } else {
        for (i = 0; i < NUM_TRIGGER_STAGES; i++) {
            setting.trig_mask0[i] = ds_trigger_get_mask0(i);
            setting.trig_mask1[i] = ds_trigger_get_mask1(i);

            setting.trig_value0[i] = ds_trigger_get_value0(i);
            setting.trig_value1[i] = ds_trigger_get_value1(i);

            setting.trig_edge0[i] = ds_trigger_get_edge0(i);
            setting.trig_edge1[i] = ds_trigger_get_edge1(i);

            if (setting.mode & (1 << STRIG_MODE_BIT) && i == STriggerDataStage) {
                // serial trigger, data mask/value should not be duplicated
            } else {
                if (setting.mode & (1 << QUAR_MODE_BIT)) {
                    setting.trig_mask0[i] = ((setting.trig_mask0[i] & 0x0f) << 12) +
                                            ((setting.trig_mask0[i] & 0x0f) << 8) +
                                            ((setting.trig_mask0[i] & 0x0f) << 4) +
                                            ((setting.trig_mask0[i] & 0x0f) << 0);
                    setting.trig_mask1[i] = ((setting.trig_mask1[i] & 0x0f) << 12) +
                                            ((setting.trig_mask1[i] & 0x0f) << 8) +
                                            ((setting.trig_mask1[i] & 0x0f) << 4) +
                                            ((setting.trig_mask1[i] & 0x0f) << 0);
                    setting.trig_value0[i] = ((setting.trig_value0[i] & 0x0f) << 12) +
                                             ((setting.trig_value0[i] & 0x0f) << 8) +
                                             ((setting.trig_value0[i] & 0x0f) << 4) +
                                             ((setting.trig_value0[i] & 0x0f) << 0);
                    setting.trig_value1[i] = ((setting.trig_value1[i] & 0x0f) << 12) +
                                             ((setting.trig_value1[i] & 0x0f) << 8) +
                                             ((setting.trig_value1[i] & 0x0f) << 4) +
                                             ((setting.trig_value1[i] & 0x0f) << 0);
                    setting.trig_edge0[i] = ((setting.trig_edge0[i] & 0x0f) << 12) +
                                            ((setting.trig_edge0[i] & 0x0f) << 8) +
                                            ((setting.trig_edge0[i] & 0x0f) << 4) +
                                            ((setting.trig_edge0[i] & 0x0f) << 0);
                    setting.trig_edge1[i] = ((setting.trig_edge1[i] & 0x0f) << 12) +
                                            ((setting.trig_edge1[i] & 0x0f) << 8) +
                                            ((setting.trig_edge1[i] & 0x0f) << 4) +
                                            ((setting.trig_edge1[i] & 0x0f) << 0);
                } else if (setting.mode & (1 << HALF_MODE_BIT)) {
                    setting.trig_mask0[i] = ((setting.trig_mask0[i] & 0xff) << 8) +
                                            ((setting.trig_mask0[i] & 0xff) << 0);
                    setting.trig_mask1[i] = ((setting.trig_mask1[i] & 0xff) << 8) +
                                            ((setting.trig_mask1[i] & 0xff) << 0);
                    setting.trig_value0[i] = ((setting.trig_value0[i] & 0xff) << 8) +
                                             ((setting.trig_value0[i] & 0xff) << 0);
                    setting.trig_value1[i] = ((setting.trig_value1[i] & 0xff) << 8) +
                                             ((setting.trig_value1[i] & 0xff) << 0);
                    setting.trig_edge0[i] = ((setting.trig_edge0[i] & 0xff) << 8) +
                                            ((setting.trig_edge0[i] & 0xff) << 0);
                    setting.trig_edge1[i] = ((setting.trig_edge1[i] & 0xff) << 8) +
                                            ((setting.trig_edge1[i] & 0xff) << 0);
                }
            }

            setting.trig_logic0[i] = (trigger->trigger_logic[i] << 1) + trigger->trigger0_inv[i];
            setting.trig_logic1[i] = (trigger->trigger_logic[i] << 1) + trigger->trigger1_inv[i];

            setting.trig_count[i] = trigger->trigger0_count[i];
        }
    }

    // set GPIF to be wordwide
    wr_cmd.header.dest = DSL_CTL_WORDWIDE;
    wr_cmd.header.size = 1;
    wr_cmd.data[0] = bmWR_WORDWIDE;
    if ((ret = command_ctl_wr(hdl, wr_cmd)) != SR_OK) {
        sr_err("Sent DSL_CTL_WORDWIDE command failed.");
        return SR_ERR;
    }

    // send bulk write control command
    arm_size = sizeof(struct DSL_setting) / sizeof(uint16_t);
    wr_cmd.header.dest = DSL_CTL_BULK_WR;
    wr_cmd.header.size = 3;
    wr_cmd.data[0] = (uint8_t)arm_size;
    wr_cmd.data[1] = (uint8_t)(arm_size >> 8);
    wr_cmd.data[2] = (uint8_t)(arm_size >> 16);
    if ((ret = command_ctl_wr(hdl, wr_cmd)) != SR_OK) {
        sr_err("Sent bulk write command of arm FPGA failed.");
        return SR_ERR;
    }
    //command_fpga_setting(hdl, arm_size);

    // send bulk data
    ret = libusb_bulk_transfer(hdl, 2 | LIBUSB_ENDPOINT_OUT,
                               (unsigned char *)&setting,
                               sizeof(struct DSL_setting),
                               &transferred, 1000);
    if (ret < 0) {
        sr_err("Unable to arm FPGA of dsl device: %s.",
                libusb_error_name(ret));
        return SR_ERR;
    } else if (transferred != sizeof(struct DSL_setting)) {
        sr_err("Arm FPGA error: expacted transfer size %d; actually %d",
                sizeof(struct DSL_setting), transferred);
        return SR_ERR;
    }

    // assert INTRDY high (indicate data end)
    wr_cmd.header.dest = DSL_CTL_INTRDY;
    wr_cmd.header.size = 1;
    wr_cmd.data[0] = bmWR_INTRDY;
    if ((ret = command_ctl_wr(hdl, wr_cmd)) != SR_OK)
        return SR_ERR;


    // check FPGA_DONE bit
    rd_cmd.header.dest = DSL_CTL_HW_STATUS;
    rd_cmd.header.size = 1;
    rd_cmd_data = 0;
    rd_cmd.data = &rd_cmd_data;
    if ((ret = command_ctl_rd(hdl, rd_cmd)) != SR_OK)
        return SR_ERR;
    if (rd_cmd_data & bmGPIF_DONE) {
        sr_info("Arm FPGA done");
        return SR_OK;
    } else {
        return SR_ERR;
    }
}

SR_PRIV int dsl_fpga_config(struct libusb_device_handle *hdl, const char *filename)
{
    FILE *fw;
    int chunksize, ret;
    unsigned char *buf;
    int transferred;
    uint64_t filesize;
    struct ctl_wr_cmd wr_cmd;
    struct ctl_rd_cmd rd_cmd;
    uint8_t rd_cmd_data;
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
        sr_err("FPGA configure buf malloc failed.");
        return SR_ERR;
    }

	// step0: assert PROG_B low
    wr_cmd.header.dest = DSL_CTL_PROG_B;
    wr_cmd.header.size = 1;
    wr_cmd.data[0] = ~bmWR_PROG_B;
    if ((ret = command_ctl_wr(hdl, wr_cmd)) != SR_OK)
		return SR_ERR;

	// step1: turn off GREEN/RED led
    wr_cmd.header.dest = DSL_CTL_LED;
    wr_cmd.header.size = 1;
    wr_cmd.data[0] = ~bmLED_GREEN & ~bmLED_RED;
    if ((ret = command_ctl_wr(hdl, wr_cmd)) != SR_OK)
		return SR_ERR;

	// step2: assert PORG_B high
    wr_cmd.header.dest = DSL_CTL_PROG_B;
    wr_cmd.header.size = 1;
    wr_cmd.data[0] = bmWR_PROG_B;
    if ((ret = command_ctl_wr(hdl, wr_cmd)) != SR_OK)
		return SR_ERR;

	// step3: wait INIT_B go high
    rd_cmd.header.dest = DSL_CTL_HW_STATUS;
    rd_cmd.header.size = 1;
	rd_cmd_data = 0;
    rd_cmd.data = &rd_cmd_data;
    while(1) {
        if ((ret = command_ctl_rd(hdl, rd_cmd)) != SR_OK)
			return SR_ERR;
        if (rd_cmd_data & bmFPGA_INIT_B)
            break;
    }

	// step4: send config ctl command
    wr_cmd.header.dest = DSL_CTL_WORDWIDE;
    wr_cmd.header.size = 1;
    wr_cmd.data[0] = ~bmWR_WORDWIDE;
    if ((ret = command_ctl_wr(hdl, wr_cmd)) != SR_OK) {
        sr_err("Sent DSL_CTL_WORDWIDE command failed.");
        return SR_ERR;
    }
    wr_cmd.header.dest = DSL_CTL_INTRDY;
    wr_cmd.header.size = 1;
    wr_cmd.data[0] = (uint8_t)~bmWR_INTRDY;
    if ((ret = command_ctl_wr(hdl, wr_cmd)) != SR_OK)
        return SR_ERR;

    wr_cmd.header.dest = DSL_CTL_BULK_WR;
    wr_cmd.header.size = 3;
    wr_cmd.data[0] = (uint8_t)filesize;
    wr_cmd.data[1] = (uint8_t)(filesize >> 8);
    wr_cmd.data[2] = (uint8_t)(filesize >> 16);
    if ((ret = command_ctl_wr(hdl, wr_cmd)) != SR_OK) {
        sr_err("Configure FPGA error: send command fpga_config failed.");
		return SR_ERR;
    }

	// step5: send config data
    chunksize = fread(buf, 1, filesize, fw);
    if (chunksize == 0)
		return SR_ERR;

    ret = libusb_bulk_transfer(hdl, 2 | LIBUSB_ENDPOINT_OUT,
                               buf, chunksize,
                               &transferred, 1000);
    fclose(fw);
    g_free(buf);

    if (ret < 0) {
        sr_err("Unable to configure FPGA of dsl device: %s.",
                libusb_error_name(ret));
		return SR_ERR;
    } else if (transferred != chunksize) {
        sr_err("Configure FPGA error: expacted transfer size %d; actually %d.",
                chunksize, transferred);
		return SR_ERR;
    }

	// step6: assert INTRDY high (indicate data end)
    wr_cmd.header.dest = DSL_CTL_INTRDY;
    wr_cmd.header.size = 1;
    wr_cmd.data[0] = bmWR_INTRDY;
    if ((ret = command_ctl_wr(hdl, wr_cmd)) != SR_OK)
		return SR_ERR;

    // step7: check GPIF_DONE
    rd_cmd.header.dest = DSL_CTL_HW_STATUS;
    rd_cmd.header.size = 1;
    rd_cmd_data = 0;
    rd_cmd.data = &rd_cmd_data;
    while ((ret = command_ctl_rd(hdl, rd_cmd)) == SR_OK) {
        if (rd_cmd_data & bmGPIF_DONE) {
            break;
        }
    }

    // step8: assert INTRDY low
    wr_cmd.header.dest = DSL_CTL_INTRDY;
    wr_cmd.header.size = 1;
    wr_cmd.data[0] = (uint8_t)~bmWR_INTRDY;
    if ((ret = command_ctl_wr(hdl, wr_cmd)) != SR_OK)
        return SR_ERR;

    // step9: check FPGA_DONE bit
    rd_cmd.header.dest = DSL_CTL_HW_STATUS;
    rd_cmd.header.size = 1;
	rd_cmd_data = 0;
    rd_cmd.data = &rd_cmd_data;
    if ((ret = command_ctl_rd(hdl, rd_cmd)) != SR_OK)
		return SR_ERR;
    if (rd_cmd_data & bmFPGA_DONE) {
        // step10: turn on GREEN led
        wr_cmd.header.dest = DSL_CTL_LED;
        wr_cmd.data[0] = bmLED_GREEN;
        if ((ret = command_ctl_wr(hdl, wr_cmd)) != SR_OK)
			return SR_ERR;
	} else {
		return SR_ERR;
	}

    sr_info("FPGA configure done: %d bytes.", chunksize);
    return SR_OK;
}

SR_PRIV int dsl_config_get(int id, GVariant **data, const struct sr_dev_inst *sdi,
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
    case SR_CONF_ROLL:
        if (!sdi)
            return SR_ERR;
        devc = sdi->priv;
        *data = g_variant_new_boolean(devc->roll);
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

SR_PRIV int dsl_dev_open(struct sr_dev_driver *di, struct sr_dev_inst *sdi, gboolean *fpga_done)
{
    struct sr_usb_dev_inst *usb;
    struct DSL_context *devc;
    int ret;
    uint8_t hw_info;
    struct ctl_rd_cmd rd_cmd;

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
        ret = hw_dev_open(di, sdi);
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

    rd_cmd.header.dest = DSL_CTL_HW_STATUS;
    rd_cmd.header.size = 1;
    hw_info = 0;
    rd_cmd.data = &hw_info;
    if ((ret = command_ctl_rd(usb->devhdl, rd_cmd)) != SR_OK) {
        sr_err("Failed to get hardware infos.");
        return SR_ERR;
    }
    *fpga_done = (hw_info & bmFPGA_DONE) != 0;

    if ((sdi->status == SR_ST_ACTIVE) && !(*fpga_done)) {
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
            sr_err("%s: Configure FPGA failed!", __func__);
            return SR_ERR;
        }
    }


    return SR_OK;
}

SR_PRIV int dsl_dev_close(struct sr_dev_inst *sdi)
{
    struct sr_usb_dev_inst *usb;

    usb = sdi->conn;
    if (usb->devhdl == NULL)
        return SR_ERR;

    sr_info("%s: Closing device %d on %d.%d interface %d.",
        sdi->driver->name, sdi->index, usb->bus, usb->address, USB_INTERFACE);
    libusb_release_interface(usb->devhdl, USB_INTERFACE);
    libusb_close(usb->devhdl);
    usb->devhdl = NULL;
    sdi->status = SR_ST_INACTIVE;

    return SR_OK;
}

SR_PRIV int dsl_dev_acquisition_stop(const struct sr_dev_inst *sdi, void *cb_data)
{
    (void)cb_data;

    struct DSL_context *devc;
    struct sr_usb_dev_inst *usb;
    int ret;
    struct ctl_wr_cmd wr_cmd;

    devc = sdi->priv;
    usb = sdi->conn;

    if (!devc->abort) {
        devc->abort = TRUE;
        dsl_wr_reg(sdi, EEWP_ADDR, bmFORCE_RDY);
    } else if (devc->status == DSL_FINISH) {
        /* Stop GPIF acquisition */
        wr_cmd.header.dest = DSL_CTL_STOP;
        wr_cmd.header.size = 0;
        if ((ret = command_ctl_wr(usb->devhdl, wr_cmd)) != SR_OK)
            sr_err("%s: Sent acquisition stop command failed!", __func__);
        else
            sr_info("%s: Sent acquisition stop command!", __func__);
    }

    return SR_OK;
}

SR_PRIV int dsl_dev_status_get(const struct sr_dev_inst *sdi, struct sr_status *status, int begin, int end)
{
    int ret = SR_ERR;
    struct ctl_rd_cmd rd_cmd;

    if (sdi) {
        struct DSL_context *devc;
        struct sr_usb_dev_inst *usb;

        devc = sdi->priv;
        usb = sdi->conn;
        if (devc->status == DSL_START) {
            rd_cmd.header.dest = DSL_CTL_DSO_MEASURE;
            rd_cmd.header.offset = begin;
            rd_cmd.header.size = end - begin + 1;
            rd_cmd.data = (unsigned char*)status;
            ret = command_ctl_rd(usb->devhdl, rd_cmd);
        } else if (devc->mstatus_valid) {
            *status = devc->mstatus;
            ret = SR_OK;
        }
    }

    return ret;
}

static unsigned int to_bytes_per_ms(struct DSL_context *devc)
{
    struct sr_dev_inst *sdi = devc->cb_data;
    if (devc->cur_samplerate > SR_MHZ(100))
        return SR_MHZ(100) / 1000 * dsl_en_ch_num(sdi) / 8;
    else
        return devc->cur_samplerate / 1000 * dsl_en_ch_num(sdi) / 8;
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
    n = ceil(total_buffer_time * 1.0f * to_bytes_per_ms(devc) / get_buffer_size(devc));

    if (n > NUM_SIMUL_TRANSFERS)
        return NUM_SIMUL_TRANSFERS;

    return n;
}

SR_PRIV unsigned int dsl_get_timeout(struct DSL_context *devc)
{
    size_t total_size;
    unsigned int timeout;

    total_size = get_buffer_size(devc) * get_number_of_transfers(devc);
    timeout = total_size / to_bytes_per_ms(devc);

    if (devc->op_mode == SR_OP_STREAM)
        return timeout + timeout / 4; /* Leave a headroom of 25% percent. */
    else
        return 1000;
}

static void finish_acquisition(struct DSL_context *devc)
{
    struct sr_datafeed_packet packet;

    sr_err("%s: send SR_DF_END packet", __func__);
    /* Terminate session. */
    packet.type = SR_DF_END;
    packet.status = SR_PKT_OK;
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
    if (devc->submitted_transfers == 0)
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
    struct sr_datafeed_packet packet;
    struct sr_datafeed_logic logic;
    struct sr_datafeed_dso dso;
    struct sr_datafeed_analog analog;
    uint64_t cur_sample_count = 0;

    uint8_t *cur_buf = transfer->buffer;
    struct DSL_context *devc = transfer->user_data;
    struct sr_dev_inst *sdi = devc->cb_data;
    const int sample_width = (devc->sample_wide) ? 2 : 1;

    if (devc->status == DSL_START)
        devc->status = DSL_DATA;
    if (devc->data_lock) {
        resubmit_transfer(transfer);
        devc->trf_completed = 1;
        return;
    }

    if (devc->abort)
        devc->status = DSL_STOP;

    sr_info("%" PRIu64 ": receive_transfer(): status %d; timeout %d; received %d bytes.",
        g_get_monotonic_time(), transfer->status, transfer->timeout, transfer->actual_length);

    switch (transfer->status) {
    case LIBUSB_TRANSFER_COMPLETED:
    case LIBUSB_TRANSFER_TIMED_OUT: /* We may have received some data though. */
        break;
    default:
        devc->status = DSL_ERROR;
        break;
    }

    packet.status = SR_PKT_OK;
    if (devc->status == DSL_DATA &&
        transfer->actual_length != 0) {
        /* Send the incoming transfer to the session bus. */
        // check packet type
        if (sdi->mode == LOGIC) {
            packet.type = SR_DF_LOGIC;
            packet.payload = &logic;
            cur_sample_count = transfer->actual_length * 8 / dsl_en_ch_num(sdi) ;
            logic.length = transfer->actual_length;
            logic.format = LA_CROSS_DATA;
            logic.data_error = 0;
            logic.data = cur_buf;
        } else if (sdi->mode == DSO) {
            if (!devc->instant) {
                const uint32_t mstatus_offset = devc->limit_samples / (g_slist_length(sdi->channels)/dsl_en_ch_num(sdi));
                devc->mstatus.pkt_id = *((const uint16_t*)cur_buf + mstatus_offset);
                devc->mstatus.ch0_max = *((const uint8_t*)cur_buf + mstatus_offset*2 + 1*2);
                devc->mstatus.ch0_min = *((const uint8_t*)cur_buf + mstatus_offset*2 + 3);
                devc->mstatus.ch0_period = *((const uint32_t*)cur_buf + mstatus_offset/2 + 2/2);
                devc->mstatus.ch0_period += ((uint64_t)*((const uint32_t*)cur_buf + mstatus_offset/2 + 4/2)) << 32;
                devc->mstatus.ch0_pcnt = *((const uint32_t*)cur_buf + mstatus_offset/2 + 6/2);
                devc->mstatus.ch1_max = *((const uint8_t*)cur_buf + mstatus_offset*2 + 9*2);
                devc->mstatus.ch1_min = *((const uint8_t*)cur_buf + mstatus_offset*2 + 19);
                devc->mstatus.ch1_period = *((const uint32_t*)cur_buf + mstatus_offset/2 + 10/2);
                devc->mstatus.ch1_period += ((uint64_t)*((const uint32_t*)cur_buf + mstatus_offset/2 + 12/2)) << 32;
                devc->mstatus.ch1_pcnt = *((const uint32_t*)cur_buf + mstatus_offset/2 + 14/2);
                devc->mstatus.vlen = *((const uint32_t*)cur_buf + mstatus_offset/2 + 16/2) & 0x7fffffff;
                devc->mstatus.stream_mode = *((const uint32_t*)cur_buf + mstatus_offset/2 + 16/2) & 0x80000000;
                devc->mstatus.sample_divider = *((const uint32_t*)cur_buf + mstatus_offset/2 + 18/2) & 0x0fffffff;
                devc->mstatus.sample_divider_tog = *((const uint32_t*)cur_buf + mstatus_offset/2 + 18/2) & 0x80000000;
                devc->mstatus.trig_flag = *((const uint32_t*)cur_buf + mstatus_offset/2 + 18/2) & 0x40000000;
            } else {
                devc->mstatus.vlen = instant_buffer_size;
            }

            const uint32_t divider = devc->zero ? 0x1 : (uint32_t)ceil(DSCOPE_MAX_SAMPLERATE * 1.0 / devc->cur_samplerate / dsl_en_ch_num(sdi));
            if ((devc->mstatus.pkt_id == DSO_PKTID &&
                 devc->mstatus.sample_divider == divider &&
                 devc->mstatus.vlen != 0 &&
                 devc->mstatus.vlen <= (transfer->actual_length - 512) / sample_width) ||
                devc->instant) {
                devc->roll = (devc->mstatus.stream_mode != 0);
                devc->mstatus_valid = devc->instant ? FALSE : TRUE;
                packet.type = SR_DF_DSO;
                packet.payload = &dso;
                dso.probes = sdi->channels;
                //dso.num_samples = (transfer->actual_length - 512) / sample_width;
                cur_sample_count = 2 * devc->mstatus.vlen / dsl_en_ch_num(sdi) ;
                dso.num_samples = cur_sample_count;
                dso.mq = SR_MQ_VOLTAGE;
                dso.unit = SR_UNIT_VOLT;
                dso.mqflags = SR_MQFLAG_AC;
                dso.samplerate_tog = (devc->mstatus.sample_divider_tog != 0);
                dso.trig_flag = (devc->mstatus.trig_flag != 0);
                dso.data = cur_buf;
            } else {
                packet.type = SR_DF_DSO;
                packet.status = SR_PKT_DATA_ERROR;
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
            analog.data = (float *)cur_buf;
        }

        if ((devc->limit_samples && devc->num_bytes < devc->actual_bytes) ||
            sdi->mode != LOGIC ) {
            const uint64_t remain_length= devc->actual_bytes - devc->num_bytes;
            logic.length = min(logic.length, remain_length);

            /* send data to session bus */
            if (!devc->overflow) {
                if (packet.status == SR_PKT_OK)
                    sr_session_send(sdi, &packet);
            } else {
                packet.type = SR_DF_OVERFLOW;
                packet.payload = NULL;
                sr_session_send(sdi, &packet);
            }
        }

        devc->num_samples += cur_sample_count;
        devc->num_bytes += logic.length;
        if (sdi->mode == LOGIC &&
            devc->limit_samples &&
            devc->num_bytes >= devc->actual_bytes) {
            devc->status = DSL_STOP;
        } else if ((sdi->mode != DSO || devc->instant) &&
                   devc->limit_samples &&
                   devc->num_samples >= devc->actual_samples) {
            devc->status = DSL_STOP;
        }
    }

    if (devc->status == DSL_DATA)
        resubmit_transfer(transfer);
    else
        free_transfer(transfer);

    devc->trf_completed = 1;
}

static void receive_trigger_pos(struct libusb_transfer *transfer)
{
    struct DSL_context *devc;
    struct sr_datafeed_packet packet;
    struct ds_trigger_pos *trigger_pos;
    const struct sr_dev_inst *sdi;
    uint64_t remain_cnt;

    packet.status = SR_PKT_OK;
    devc = transfer->user_data;
    sdi = devc->cb_data;
    trigger_pos = (struct ds_trigger_pos *)transfer->buffer;
    if (devc->status != DSL_ABORT)
        devc->status = DSL_ERROR;
    if (!devc->abort && transfer->status == LIBUSB_TRANSFER_COMPLETED &&
        trigger_pos->check_id == TRIG_CHECKID) {
        sr_info("%" PRIu64 ": receive_trigger_pos(): status %d; timeout %d; received %d bytes.",
            g_get_monotonic_time(), transfer->status, transfer->timeout, transfer->actual_length);
        remain_cnt = trigger_pos->remain_cnt_h;
        remain_cnt = (remain_cnt << 32) + trigger_pos->remain_cnt_l;
        if (transfer->actual_length == sizeof(struct ds_trigger_pos)) {
            if (sdi->mode != LOGIC ||
                devc->stream ||
                remain_cnt < devc->limit_samples) {
                if (sdi->mode == LOGIC && (!devc->stream || (devc->status == DSL_ABORT))) {
                    devc->actual_samples = devc->limit_samples - remain_cnt;
                    devc->actual_bytes = devc->actual_samples / DSLOGIC_ATOMIC_SAMPLES * dsl_en_ch_num(sdi) * DSLOGIC_ATOMIC_SIZE;
                    devc->actual_samples = devc->actual_bytes / dsl_en_ch_num(sdi) * 8;
                }

                packet.type = SR_DF_TRIGGER;
                packet.payload = trigger_pos;
                sr_session_send(sdi, &packet);

                devc->status = DSL_DATA;
            }
        }
    } else if (!devc->abort) {
        sr_err("%s: trigger packet data error.", __func__);
        packet.type = SR_DF_TRIGGER;
        packet.payload = trigger_pos;
        packet.status = SR_PKT_DATA_ERROR;
        sr_session_send(sdi, &packet);
    }

    free_transfer(transfer);
}

SR_PRIV int dsl_start_transfers(const struct sr_dev_inst *sdi)
{
    struct DSL_context *devc;
    struct sr_usb_dev_inst *usb;
    struct libusb_transfer *transfer;
    unsigned int i, num_transfers;
    int ret;
    unsigned char *buf;
    size_t size;
    unsigned int dso_buffer_size;
    struct ds_trigger_pos *trigger_pos;

    devc = sdi->priv;
    usb = sdi->conn;

    if (devc->instant)
        dso_buffer_size = instant_buffer_size * g_slist_length(sdi->channels);
    else
        dso_buffer_size = devc->limit_samples * dsl_en_ch_num(sdi) + 512;

    num_transfers = (devc->stream) ? get_number_of_transfers(devc) : 1;
    size = (sdi->mode == DSO) ? dso_buffer_size :
           (devc->stream) ? get_buffer_size(devc) : instant_buffer_size;

    /* trigger packet transfer */
    if (!(trigger_pos = g_try_malloc0(sizeof(struct ds_trigger_pos)))) {
        sr_err("%s: USB trigger_pos buffer malloc failed.", __func__);
        return SR_ERR_MALLOC;
    }
    devc->transfers = g_try_malloc0(sizeof(*devc->transfers) * (num_transfers + 1));
    if (!devc->transfers) {
        sr_err("%s: USB transfer malloc failed.", __func__);
        return SR_ERR_MALLOC;
    }
    transfer = libusb_alloc_transfer(0);
    libusb_fill_bulk_transfer(transfer, usb->devhdl,
            6 | LIBUSB_ENDPOINT_IN, (unsigned char *)trigger_pos, sizeof(struct ds_trigger_pos),
            (libusb_transfer_cb_fn)receive_trigger_pos, devc, 0);
    if ((ret = libusb_submit_transfer(transfer)) != 0) {
        sr_err("%s: Failed to submit trigger_pos transfer: %s.",
               __func__, libusb_error_name(ret));
        libusb_free_transfer(transfer);
        g_free(trigger_pos);
        devc->status = DSL_ERROR;
        return SR_ERR;
    } else {
        devc->num_transfers++;
        devc->transfers[0] = transfer;
        devc->submitted_transfers++;
    }

    /* data packet transfer */
    for (i = 1; i <= num_transfers; i++) {
        if (!(buf = g_try_malloc(size))) {
            sr_err("%s: USB transfer buffer malloc failed.", __func__);
            return SR_ERR_MALLOC;
        }
        transfer = libusb_alloc_transfer(0);
        libusb_fill_bulk_transfer(transfer, usb->devhdl,
                6 | LIBUSB_ENDPOINT_IN, buf, size,
                (libusb_transfer_cb_fn)receive_transfer, devc, 0);
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

    return SR_OK;
}
