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

//#include <libusb.h>
#include "command.h"
//#include "libsigrok.h"
#include "dsl.h"
#include <assert.h>

SR_PRIV int command_get_fw_version(libusb_device_handle *devhdl,
                   struct version_info *vi)
{
	int ret;

	ret = libusb_control_transfer(devhdl, LIBUSB_REQUEST_TYPE_VENDOR |
		LIBUSB_ENDPOINT_IN, CMD_GET_FW_VERSION, 0x0000, 0x0000,
        (unsigned char *)vi, sizeof(struct version_info), 3000);

	if (ret < 0) {
        sr_err("Unable to get version info: %s.",
		       libusb_error_name(ret));
        return SR_ERR;
	}

    return SR_OK;
}

SR_PRIV int command_get_revid_version(libusb_device_handle *devhdl,
				      uint8_t *revid)
{
	int ret;

	ret = libusb_control_transfer(devhdl, LIBUSB_REQUEST_TYPE_VENDOR |
		LIBUSB_ENDPOINT_IN, CMD_GET_REVID_VERSION, 0x0000, 0x0000,
        revid, 1, 3000);

	if (ret < 0) {
        sr_err("Unable to get REVID: %s.", libusb_error_name(ret));
        return SR_ERR;
	}

    return SR_OK;
}

SR_PRIV int command_start_acquisition(libusb_device_handle *devhdl,
                      uint64_t samplerate, gboolean samplewide, gboolean la_mode)
{
	struct cmd_start_acquisition cmd;
	int delay = 0, ret;

    (void) samplerate;

    cmd.flags = la_mode ? CMD_START_FLAGS_MODE_LA : 0;

    cmd.flags |= CMD_START_FLAGS_CLK_30MHZ;
    delay = 0;

    sr_info("GPIF delay = %d, clocksource = %sMHz.", delay,
		(cmd.flags & CMD_START_FLAGS_CLK_48MHZ) ? "48" : "30");

//	if (delay <= 0 || delay > MAX_SAMPLE_DELAY) {
//        sr_err("Unable to sample at %" PRIu64 "Hz.", samplerate);
//        return SR_ERR;
//	}

	cmd.sample_delay_h = (delay >> 8) & 0xff;
	cmd.sample_delay_l = delay & 0xff;

	/* Select the sampling width. */
	cmd.flags |= samplewide ? CMD_START_FLAGS_SAMPLE_16BIT :
		CMD_START_FLAGS_SAMPLE_8BIT;

	/* Send the control message. */
	ret = libusb_control_transfer(devhdl, LIBUSB_REQUEST_TYPE_VENDOR |
			LIBUSB_ENDPOINT_OUT, CMD_START, 0x0000, 0x0000,
			(unsigned char *)&cmd, sizeof(cmd), 3000);
	if (ret < 0) {
        sr_err("Unable to send start command: %s.",
		       libusb_error_name(ret));
        return SR_ERR;
	}

    return SR_OK;
}

SR_PRIV int command_stop_acquisition(libusb_device_handle *devhdl)
{
    struct cmd_start_acquisition cmd;
    int ret;

    /* stop acquisition command */
    cmd.flags = CMD_START_FLAGS_STOP;
    cmd.sample_delay_h = 0;
    cmd.sample_delay_l = 0;

    /* Send the control message. */
    ret = libusb_control_transfer(devhdl, LIBUSB_REQUEST_TYPE_VENDOR |
            LIBUSB_ENDPOINT_OUT, CMD_START, 0x0000, 0x0000,
            (unsigned char *)&cmd, sizeof(cmd), 3000);
    if (ret < 0) {
        sr_err("Unable to send stop command: %s.",
               libusb_error_name(ret));
        return SR_ERR;
    }

    return SR_OK;
}

SR_PRIV int command_fpga_config(libusb_device_handle *devhdl)
{
    struct cmd_cfg_count cmd;
    int ret;

    /* ... */
    cmd.byte0 = (uint8_t)0;
    cmd.byte1 = (uint8_t)0;
    cmd.byte2 = (uint8_t)0;

    /* Send the control message. */
    ret = libusb_control_transfer(devhdl, LIBUSB_REQUEST_TYPE_VENDOR |
            LIBUSB_ENDPOINT_OUT, CMD_CONFIG, 0x0000, 0x0000,
            (unsigned char *)&cmd, sizeof(cmd), 3000);
    if (ret < 0) {
        sr_err("Unable to send FPGA configure command: %s.",
               libusb_error_name(ret));
        return SR_ERR;
    }

    return SR_OK;
}

SR_PRIV int command_fpga_setting(libusb_device_handle *devhdl, uint32_t setting_count)
{
    struct cmd_setting_count cmd;
    int ret;

    /* ... */
    cmd.byte0 = (uint8_t)setting_count;
    cmd.byte1 = (uint8_t)(setting_count >> 8);
    cmd.byte2 = (uint8_t)(setting_count >> 16);

    /* Send the control message. */
    ret = libusb_control_transfer(devhdl, LIBUSB_REQUEST_TYPE_VENDOR |
            LIBUSB_ENDPOINT_OUT, CMD_SETTING, 0x0000, 0x0000,
            (unsigned char *)&cmd, sizeof(cmd), 3000);
    if (ret < 0) {
        sr_err("Unable to send FPGA setting command: %s.",
               libusb_error_name(ret));
        return SR_ERR;
    }

    return SR_OK;
}

SR_PRIV int command_dso_ctrl(libusb_device_handle *devhdl, uint64_t command)
{
    struct cmd_control cmd;
    int ret;

    /* ... */
    cmd.byte0 = (uint8_t)command;
    cmd.byte1 = (uint8_t)(command >> 8);
    cmd.byte2 = (uint8_t)(command >> 16);
    cmd.byte3 = (uint8_t)(command >> 24);
    cmd.byte4 = (uint8_t)(command >> 32);
    cmd.byte5 = (uint8_t)(command >> 40);
    cmd.byte6 = (uint8_t)(command >> 48);
    cmd.byte7 = (uint8_t)(command >> 56);


    /* Send the control command. */
    ret = libusb_control_transfer(devhdl, LIBUSB_REQUEST_TYPE_VENDOR |
            LIBUSB_ENDPOINT_OUT, CMD_CONTROL, 0x0000, 0x0000,
            (unsigned char *)&cmd, sizeof(cmd), 3000);
    if (ret < 0) {
        sr_err("Unable to send oscilloscope control command: %s.",
               libusb_error_name(ret));
        return SR_ERR;
    }

    return SR_OK;
}

SR_PRIV int command_get_status(libusb_device_handle *devhdl,
                   unsigned char *status, int begin, int end)
{
    struct cmd_status_info cmd;
    int ret;

    /* status acquisition command */
    assert(begin >= 0);
    assert(end >= 0);
    cmd.begin = begin;
    cmd.end = end;

    /* Send the control message. */
    ret = libusb_control_transfer(devhdl, LIBUSB_REQUEST_TYPE_VENDOR |
            LIBUSB_ENDPOINT_OUT, CMD_STATUS_INFO, 0x0000, 0x0000,
            (unsigned char *)&cmd, sizeof(cmd), 3000);
    if (ret < 0) {
        sr_err("Unable to send status info: %s.",
               libusb_error_name(ret));
        return SR_ERR;
    }

    g_usleep(10*1000);

    ret = libusb_control_transfer(devhdl, LIBUSB_REQUEST_TYPE_VENDOR |
        LIBUSB_ENDPOINT_IN, CMD_STATUS, 0x0000, 0x0000,
        (unsigned char *)status, CMD_STATUS_CNT, 3000);

    if (ret < 0) {
        sr_err("Unable to get status info: %s.",
               libusb_error_name(ret));
        return SR_ERR;
    }

    return SR_OK;
}

SR_PRIV int command_wr_reg(libusb_device_handle *devhdl, uint8_t value, uint8_t addr)
{
    int ret;
    uint16_t cmd;

    cmd = value + (addr << 8);
    /* Send the control command. */
    ret = libusb_control_transfer(devhdl, LIBUSB_REQUEST_TYPE_VENDOR |
            LIBUSB_ENDPOINT_OUT, CMD_WR_REG, 0x0000, 0x0000,
            (unsigned char *)&cmd, sizeof(cmd), 3000);
    if (ret < 0) {
        sr_err("Unable to write REG @ address %d : %s.",
               addr, libusb_error_name(ret));
        return SR_ERR;
    }

    return SR_OK;
}

SR_PRIV int command_wr_nvm(libusb_device_handle *devhdl, unsigned char *ctx, uint8_t len)
{
    int ret;

    /* Send the control message. */
    ret = libusb_control_transfer(devhdl, LIBUSB_REQUEST_TYPE_VENDOR |
        LIBUSB_ENDPOINT_OUT, CMD_WR_NVM, 0x0000, 0x0000,
        (unsigned char *)ctx, len, 3000);

    if (ret < 0) {
        sr_err("Unable to get status info: %s.",
               libusb_error_name(ret));
        return SR_ERR;
    }

    return SR_OK;
}

SR_PRIV int command_rd_nvm(libusb_device_handle *devhdl, unsigned char *ctx, uint8_t addr, uint8_t len)
{
    int ret;

    struct cmd_nvm_info nvm_info;
    assert(len <= 32);
    nvm_info.addr = addr;
    nvm_info.len = len;

    /* Send the control message. */
    ret = libusb_control_transfer(devhdl, LIBUSB_REQUEST_TYPE_VENDOR |
            LIBUSB_ENDPOINT_OUT, CMD_RD_NVM_PRE, 0x0000, 0x0000,
            (unsigned char *)&nvm_info, sizeof(struct cmd_nvm_info), 3000);
    if (ret < 0) {
        sr_err("Unable to send CMD_ZERO_RD_PRE command: %s.",
               libusb_error_name(ret));
        return SR_ERR;
    }

    g_usleep(10*1000);

    /* Send the control message. */
    ret = libusb_control_transfer(devhdl, LIBUSB_REQUEST_TYPE_VENDOR |
        LIBUSB_ENDPOINT_IN, CMD_RD_NVM, 0x0000, 0x0000,
        (unsigned char *)ctx, len, 3000);

    if (ret < 0) {
        sr_err("Unable to get zero info: %s.",
               libusb_error_name(ret));
        return SR_ERR;
    }

    return SR_OK;
}

SR_PRIV int command_get_fpga_done(libusb_device_handle *devhdl,
                   uint8_t *fpga_done)
{
    int ret;

    ret = libusb_control_transfer(devhdl, LIBUSB_REQUEST_TYPE_VENDOR |
        LIBUSB_ENDPOINT_IN, CMD_FPGA_DONE, 0x0000, 0x0000,
        fpga_done, 1, 3000);

    if (ret < 0) {
        sr_err("Unable to get fpga done info: %s.",
               libusb_error_name(ret));
        return SR_ERR;
    }

    return SR_OK;
}

