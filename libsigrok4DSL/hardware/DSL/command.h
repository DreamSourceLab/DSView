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

#ifndef LIBDSL_HARDWARE_COMMAND_H
#define LIBDSL_HARDWARE_COMMAND_H

#include <glib.h>
#include "libsigrok.h"
#include "libsigrok-internal.h"

/* Protocol commands */
#define CMD_GET_FW_VERSION		0xb0
#define CMD_GET_REVID_VERSION	0xb1
#define CMD_START               0xb2
#define CMD_CONFIG              0xb3
#define CMD_SETTING             0xb4
#define CMD_CONTROL             0xb5
#define CMD_STATUS              0xb6
#define CMD_STATUS_INFO         0xb7
#define CMD_VTH                 0xb8
#define CMD_WR_NVM              0xb9
#define CMD_RD_NVM              0xba
#define CMD_RD_NVM_PRE          0xbb

#define CMD_START_FLAGS_MODE_POS    4
#define CMD_START_FLAGS_WIDE_POS	5
#define CMD_START_FLAGS_CLK_SRC_POS	6
#define CMD_START_FLAGS_STOP_POS	7

#define CMD_START_FLAGS_MODE_LA         (1 << CMD_START_FLAGS_MODE_POS)

#define CMD_START_FLAGS_SAMPLE_8BIT     (0 << CMD_START_FLAGS_WIDE_POS)
#define CMD_START_FLAGS_SAMPLE_16BIT	(1 << CMD_START_FLAGS_WIDE_POS)

#define CMD_START_FLAGS_CLK_30MHZ	(0 << CMD_START_FLAGS_CLK_SRC_POS)
#define CMD_START_FLAGS_CLK_48MHZ	(1 << CMD_START_FLAGS_CLK_SRC_POS)

#define CMD_START_FLAGS_STOP        (1 << CMD_START_FLAGS_STOP_POS)

#define CMD_STATUS_CNT	32

#pragma pack(push, 1)

struct version_info {
	uint8_t major;
	uint8_t minor;
};

struct cmd_start_acquisition {
	uint8_t flags;
	uint8_t sample_delay_h;
	uint8_t sample_delay_l;
};

struct cmd_setting_count {
    uint8_t byte0;
    uint8_t byte1;
    uint8_t byte2;
};

struct cmd_cfg_count {
    uint8_t byte0;
    uint8_t byte1;
    uint8_t byte2;
};

struct cmd_control {
    uint8_t byte0;
    uint8_t byte1;
    uint8_t byte2;
    uint8_t byte3;
    uint8_t byte4;
    uint8_t byte5;
    uint8_t byte6;
    uint8_t byte7;
};

struct cmd_status_info {
    uint8_t begin;
    uint8_t end;
};

struct cmd_zero_info {
    uint8_t zero_addr;
    uint8_t vpos_l;
    uint8_t vpos_h;
    uint8_t voff_l;
    uint8_t voff_h;
    uint8_t vcntr_l;
    uint8_t vcntr_h;
    uint8_t adc_off;
};

struct cmd_comb_info {
    uint8_t comb_addr;
    uint8_t comb0_low_off;
    uint8_t comb0_hig_off;
    uint8_t comb1_low_off;
    uint8_t comb1_hig_off;
    uint8_t comb_sign;
};

struct cmd_nvm_info {
    uint8_t addr;
    uint8_t len;
};

#pragma pack(pop)

SR_PRIV int command_get_fw_version(libusb_device_handle *devhdl,
				   struct version_info *vi);
SR_PRIV int command_get_revid_version(libusb_device_handle *devhdl,
				      uint8_t *revid);
SR_PRIV int command_start_acquisition(libusb_device_handle *devhdl,
                      uint64_t samplerate, gboolean samplewide, gboolean la_mode);
SR_PRIV int command_stop_acquistition(libusb_device_handle *devhdl);

SR_PRIV int command_fpga_config(libusb_device_handle *devhdl);
SR_PRIV int command_fpga_setting(libusb_device_handle *devhdl, uint32_t setting_count);

SR_PRIV int command_dso_ctrl(libusb_device_handle *devhdl, uint64_t command);

SR_PRIV int command_get_status(libusb_device_handle *devhdl,
                   unsigned char *status,
                               int begin, int end);

SR_PRIV int command_vth(libusb_device_handle *devhdl, double vth);

SR_PRIV int command_wr_nvm(libusb_device_handle *devhdl, unsigned char *ctx, uint8_t len);
SR_PRIV int command_rd_nvm(libusb_device_handle *devhdl, unsigned char *ctx, uint8_t addr, uint8_t len);

#endif
