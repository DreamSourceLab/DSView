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

#include <libusb.h>
#include "command.h"
//#include "libsigrok.h"
#include "libsigrok-internal.h"
#include "dslogic.h"

SR_PRIV int command_get_fw_version(libusb_device_handle *devhdl,
                   struct version_info *vi)
{
	int ret;

	ret = libusb_control_transfer(devhdl, LIBUSB_REQUEST_TYPE_VENDOR |
		LIBUSB_ENDPOINT_IN, CMD_GET_FW_VERSION, 0x0000, 0x0000,
		(unsigned char *)vi, sizeof(struct version_info), 100);

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
		revid, 1, 100);

	if (ret < 0) {
        sr_err("Unable to get REVID: %s.", libusb_error_name(ret));
        return SR_ERR;
	}

    return SR_OK;
}

SR_PRIV int command_start_acquisition(libusb_device_handle *devhdl,
				      uint64_t samplerate, gboolean samplewide)
{
	struct cmd_start_acquisition cmd;
	int delay = 0, ret;

	/* Compute the sample rate. */
//	if (samplewide && samplerate > MAX_16BIT_SAMPLE_RATE) {
//        sr_err("Unable to sample at %" PRIu64 "Hz "
//		       "when collecting 16-bit samples.", samplerate);
//        return SR_ERR;
//	}

//    if ((SR_MHZ(48) % samplerate) == 0) {
//		cmd.flags = CMD_START_FLAGS_CLK_48MHZ;
//        delay = SR_MHZ(48) / samplerate - 1;
//		if (delay > MAX_SAMPLE_DELAY)
//			delay = 0;
//	}

//    if (delay == 0 && (SR_MHZ(30) % samplerate) == 0) {
//		cmd.flags = CMD_START_FLAGS_CLK_30MHZ;
//        delay = SR_MHZ(30) / samplerate - 1;
//	}
    cmd.flags = CMD_START_FLAGS_CLK_30MHZ;
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
    cmd.byte0 = (uint8_t)XC6SLX9_BYTE_CNT;
    cmd.byte1 = (uint8_t)(XC6SLX9_BYTE_CNT >> 8);
    cmd.byte2 = (uint8_t)(XC6SLX9_BYTE_CNT >> 16);

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

SR_PRIV int command_dso_ctrl(libusb_device_handle *devhdl, uint32_t command)
{
    struct cmd_control cmd;
    int ret;

    /* ... */
    cmd.byte0 = (uint8_t)command;
    cmd.byte1 = (uint8_t)(command >> 8);
    cmd.byte2 = (uint8_t)(command >> 16);
    cmd.byte3 = (uint8_t)(command >> 24);


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
