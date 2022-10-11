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


#include "../../libsigrok-internal.h"
#include "command.h"
#include "dsl.h"
#include <assert.h>
#include "../../log.h"

SR_PRIV int command_ctl_wr(libusb_device_handle *devhdl, struct ctl_wr_cmd cmd)
{
    int ret;

    assert(devhdl);

    /* Send the control command. */
    ret = libusb_control_transfer(devhdl, LIBUSB_REQUEST_TYPE_VENDOR |
            LIBUSB_ENDPOINT_OUT, CMD_CTL_WR, 0x0000, 0x0000,
            (unsigned char *)&cmd, cmd.header.size+sizeof(struct ctl_header), 3000);
    if (ret < 0) {
        sr_err("Unable to send CMD_CTL_WR command(dest:%d/offset:%d/size:%d): %s.",
               cmd.header.dest, cmd.header.offset, cmd.header.size,
               libusb_error_name(ret));
        return SR_ERR;
    }

    return SR_OK;
}

SR_PRIV int command_ctl_rd(libusb_device_handle *devhdl, struct ctl_rd_cmd cmd)
{
    int ret;

    assert(devhdl);

    /* Send the control message. */
    ret = libusb_control_transfer(devhdl, LIBUSB_REQUEST_TYPE_VENDOR |
            LIBUSB_ENDPOINT_OUT, CMD_CTL_RD_PRE, 0x0000, 0x0000,
            (unsigned char *)&cmd, sizeof(struct ctl_header), 3000);
    if (ret < 0) {
        sr_err("Unable to send CMD_CTL_RD_PRE command(dest:%d/offset:%d/size:%d): %s.",
               cmd.header.dest, cmd.header.offset, cmd.header.size,
               libusb_error_name(ret));
        return SR_ERR;
    }

    g_usleep(10*1000);

    /* Send the control message. */
    ret = libusb_control_transfer(devhdl, LIBUSB_REQUEST_TYPE_VENDOR |
        LIBUSB_ENDPOINT_IN, CMD_CTL_RD, 0x0000, 0x0000,
        (unsigned char *)cmd.data, cmd.header.size, 3000);

    if (ret < 0) {
        sr_err("Unable to send CMD_CTL_RD command: %s.",
               libusb_error_name(ret));
        return SR_ERR;
    }

    return SR_OK;
}
