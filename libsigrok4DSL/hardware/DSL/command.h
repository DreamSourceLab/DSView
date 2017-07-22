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
#define CMD_CTL_WR              0xb0
#define CMD_CTL_RD_PRE          0xb1
#define CMD_CTL_RD              0xb2

#define bmGPIF_DONE     (1 << 7)
#define bmFPGA_DONE     (1 << 6)
#define bmFPGA_INIT_B   (1 << 5)
#define bmSYS_OVERFLOW  (1 << 4)
#define bmSYS_CLR       (1 << 3)
#define bmSYS_EN        (1 << 2)
#define bmLED_RED       (1 << 1)
#define bmLED_GREEN     (1 << 0)

#define bmWR_PROG_B        (1 << 2)
#define bmWR_INTRDY        (1 << 7)
#define bmWR_WORDWIDE      (1 << 0)

#define VTH_ADDR 0x78
#define EEWP_ADDR 0x70
#define COMB_ADDR 0x68

enum {
    DSL_CTL_FW_VERSION		= 0,
    DSL_CTL_REVID_VERSION	= 1,
    DSL_CTL_HW_STATUS		= 2,
    DSL_CTL_PROG_B			= 3,
    DSL_CTL_SYS				= 4,
    DSL_CTL_LED				= 5,
    DSL_CTL_INTRDY			= 6,
    DSL_CTL_WORDWIDE		= 7,

    DSL_CTL_START			= 8,
    DSL_CTL_STOP			= 9,
    DSL_CTL_BULK_WR			= 10,
    DSL_CTL_REG				= 11,
    DSL_CTL_NVM				= 12,

    DSL_CTL_I2C_DSO			= 13,
    DSL_CTL_I2C_REG			= 14,
    DSL_CTL_DSO_MEASURE		= 15,
};

#pragma pack(push, 1) // byte align
struct version_info {
	uint8_t major;
	uint8_t minor;
};

struct cmd_zero_info {
    uint8_t zero_addr;
    uint8_t voff0;
    uint8_t voff1;
    uint8_t voff2;
    uint8_t voff3;
    uint8_t voff4;
    uint8_t voff5;
    uint8_t voff6;
    uint8_t voff7;
    uint8_t voff8;
    uint8_t voff9;
    uint8_t voff10;
    uint8_t voff11;
    uint8_t voff12;
    uint8_t voff13;
    uint8_t voff14;
    uint8_t voff15;
    uint8_t diff0;
    uint8_t diff1;
    uint8_t trans0;
    uint8_t trans1;
};

struct cmd_vga_info {
    uint8_t vga_addr;
    uint16_t vga0;
    uint16_t vga1;
    uint16_t vga2;
    uint16_t vga3;
    uint16_t vga4;
    uint16_t vga5;
    uint16_t vga6;
    uint16_t vga7;
};

struct ctl_header {
    uint8_t dest;
    uint16_t offset;
    uint8_t size;
};
struct ctl_wr_cmd {
    struct ctl_header header;
    uint8_t data[60];
};
struct ctl_rd_cmd {
    struct ctl_header header;
    uint8_t *data;
};
#pragma pack(pop)


SR_PRIV int command_ctl_wr(libusb_device_handle *devhdl, struct ctl_wr_cmd cmd);
SR_PRIV int command_ctl_rd(libusb_device_handle *devhdl, struct ctl_rd_cmd cmd);

#endif
