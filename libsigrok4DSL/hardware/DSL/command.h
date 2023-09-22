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
#include "../../libsigrok-internal.h"

/* Protocol commands */
#define CMD_CTL_WR              0xb0
#define CMD_CTL_RD_PRE          0xb1
#define CMD_CTL_RD              0xb2

// read only
#define bmGPIF_DONE     (1 << 7)
#define bmFPGA_DONE     (1 << 6)
#define bmFPGA_INIT_B   (1 << 5)
// write only
#define bmCH_CH0        (1 << 7)
#define bmCH_COM        (1 << 6)
#define bmCH_CH1        (1 << 5)
// read/write
#define bmSYS_OVERFLOW  (1 << 4)
#define bmSYS_CLR       (1 << 3)
#define bmSYS_EN        (1 << 2)
#define bmLED_RED       (1 << 1)
#define bmLED_GREEN     (1 << 0)

#define bmWR_PROG_B        (1 << 2)
#define bmWR_INTRDY        (1 << 7)
#define bmWR_WORDWIDE      (1 << 0)

#define VTH_ADDR 0x78
#define SEC_DATA_ADDR 0x75
#define SEC_CTRL_ADDR 0x73
#define CTR1_ADDR 0x71
#define CTR0_ADDR 0x70
#define COMB_ADDR 0x68
#define EI2C_ADDR 0x60
#define ADCC_ADDR 0x48
#define HW_STATUS_ADDR 0x05
#define HDL_VERSION_ADDR 0x04

#define bmSECU_READY	(1 << 3)
#define bmSECU_PASS		(1 << 4)
#define SECU_STEPS		8
#define SECU_START		0x0513
#define SECU_CHECK		0x0219
#define SECU_EEP_ADDR	0x3C00
#define SECU_TRY_CNT	8

#define EI2C_CTR_OFF   0x2
#define EI2C_RXR_OFF   0x3
#define EI2C_SR_OFF    0x4
#define EI2C_TXR_OFF   0x3
#define EI2C_CR_OFF    0x4
#define EI2C_SEL_OFF   0x7

#define bmEI2C_EN (1 << 7)

#define bmEI2C_STA (1 << 7)
#define bmEI2C_STO (1 << 6)
#define bmEI2C_RD (1 << 5)
#define bmEI2C_WR (1 << 4)
#define bmEI2C_NACK (1 << 3)

#define bmEI2C_RXNACK (1 << 7)
#define bmEI2C_TIP (1 << 1)

#define EI2C_AWR 0x82
#define EI2C_ARD 0x83

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
    DSL_CTL_I2C_STATUS		= 15,

    DSL_CTL_DSO_EN0			= 16,
    DSL_CTL_DSO_DC0			= 17,
    DSL_CTL_DSO_ATT0		= 18,
    DSL_CTL_DSO_EN1			= 19,
    DSL_CTL_DSO_DC1			= 20,
    DSL_CTL_DSO_ATT1		= 21,

    DSL_CTL_AWG_WR			= 22,
    DSL_CTL_I2C_PROBE		= 23,
    DSL_CTL_I2C_EXT         = 24,
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
    uint8_t comb_comp;
    uint8_t fgain0_code;
    uint8_t fgain1_code;
    uint8_t fgain2_code;
    uint8_t fgain3_code;
    uint8_t comb_fgain0_code;
    uint8_t comb_fgain1_code;
    uint8_t comb_fgain2_code;
    uint8_t comb_fgain3_code;
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
