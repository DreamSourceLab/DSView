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

#ifndef LIBDSLOGIC_HARDWARE_COMMAND_H
#define LIBDSLOGIC_HARDWARE_COMMAND_H

#include <glib.h>
#include "libsigrok.h"

/* Protocol commands */
#define CMD_GET_FW_VERSION		0xb0
#define CMD_GET_REVID_VERSION	0xb1
#define CMD_START               0xb2
#define CMD_CONFIG              0xb3
#define CMD_SETTING             0xb4
#define CMD_CONTROL             0xb5

#define CMD_START_FLAGS_WIDE_POS	5
#define CMD_START_FLAGS_CLK_SRC_POS	6
#define CMD_START_FLAGS_STOP_POS	7

#define CMD_START_FLAGS_SAMPLE_8BIT	(0 << CMD_START_FLAGS_WIDE_POS)
#define CMD_START_FLAGS_SAMPLE_16BIT	(1 << CMD_START_FLAGS_WIDE_POS)

#define CMD_START_FLAGS_CLK_30MHZ	(0 << CMD_START_FLAGS_CLK_SRC_POS)
#define CMD_START_FLAGS_CLK_48MHZ	(1 << CMD_START_FLAGS_CLK_SRC_POS)

#define CMD_START_FLAGS_STOP        (1 << CMD_START_FLAGS_STOP_POS)

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
};

#pragma pack(pop)

SR_PRIV int command_get_fw_version(libusb_device_handle *devhdl,
				   struct version_info *vi);
SR_PRIV int command_get_revid_version(libusb_device_handle *devhdl,
				      uint8_t *revid);
SR_PRIV int command_start_acquisition(libusb_device_handle *devhdl,
				      uint64_t samplerate, gboolean samplewide);
SR_PRIV int command_stop_acquistition(libusb_device_handle *devhdl);

SR_PRIV int command_fpga_config(libusb_device_handle *devhdl);
SR_PRIV int command_fpga_setting(libusb_device_handle *devhdl, uint32_t setting_count);

SR_PRIV int command_dso_ctrl(libusb_device_handle *devhdl, uint32_t command);

#endif
