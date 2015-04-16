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

#include <stdlib.h>
#include <glib.h>
#include <libusb.h>
#include "libsigrok.h"
#include "libsigrok-internal.h"

/* SR_CONF_CONN takes one of these: */
#define CONN_USB_VIDPID  "^([0-9a-z]{4})\\.([0-9a-z]{4})$"
#define CONN_USB_BUSADDR "^(\\d+)\\.(\\d+)$"

/* Some USBTMC-specific enums, as defined in the USBTMC standard. */
#define SUBCLASS_USBTMC 0x03
#define USBTMC_USB488   0x01

/* Message logging helpers with subsystem-specific prefix string. */
#define LOG_PREFIX "usb: "
#define sr_log(l, s, args...) sr_log(l, LOG_PREFIX s, ## args)
#define sr_spew(s, args...) sr_spew(LOG_PREFIX s, ## args)
#define sr_dbg(s, args...) sr_dbg(LOG_PREFIX s, ## args)
#define sr_info(s, args...) sr_info(LOG_PREFIX s, ## args)
#define sr_warn(s, args...) sr_warn(LOG_PREFIX s, ## args)
#define sr_err(s, args...) sr_err(LOG_PREFIX s, ## args)

/**
 * Find USB devices according to a connection string.
 *
 * @param usb_ctx libusb context to use while scanning.
 * @param conn Connection string specifying the device(s) to match. This
 * can be of the form "<bus>.<address>", or "<vendorid>.<productid>".
 *
 * @return A GSList of struct sr_usb_dev_inst, with bus and address fields
 * matching the device that matched the connection string. The GSList and
 * its contents must be freed by the caller.
 */
SR_PRIV GSList *sr_usb_find(libusb_context *usb_ctx, const char *conn)
{
    struct sr_usb_dev_inst *usb;
    struct libusb_device **devlist;
    struct libusb_device_descriptor des;
    GSList *devices;
    GRegex *reg;
    GMatchInfo *match;
    int vid, pid, bus, addr, b, a, ret, i;
    char *mstr;

    vid = pid = bus = addr = 0;
    reg = g_regex_new(CONN_USB_VIDPID, 0, 0, NULL);
    if (g_regex_match(reg, conn, 0, &match)) {
        if ((mstr = g_match_info_fetch(match, 1)))
            vid = strtoul(mstr, NULL, 16);
        g_free(mstr);

        if ((mstr = g_match_info_fetch(match, 2)))
            pid = strtoul(mstr, NULL, 16);
        g_free(mstr);
        sr_dbg("Trying to find USB device with VID:PID = %04x:%04x.",
               vid, pid);
    } else {
        //g_match_info_unref(match);
        g_match_info_free(match);
        g_regex_unref(reg);
        reg = g_regex_new(CONN_USB_BUSADDR, 0, 0, NULL);
        if (g_regex_match(reg, conn, 0, &match)) {
            if ((mstr = g_match_info_fetch(match, 1)))
                bus = strtoul(mstr, NULL, 10);
            g_free(mstr);

            if ((mstr = g_match_info_fetch(match, 2)))
                addr = strtoul(mstr, NULL, 10);
            g_free(mstr);
            sr_dbg("Trying to find USB device with bus.address = "
                   "%d.%d.", bus, addr);
        }
    }
    //g_match_info_unref(match);
    g_match_info_free(match);
    g_regex_unref(reg);

    if (vid + pid + bus + addr == 0) {
        sr_err("Neither VID:PID nor bus.address was specified.");
        return NULL;
    }

    if (bus > 64) {
        sr_err("Invalid bus specified: %d.", bus);
        return NULL;
    }

    if (addr > 127) {
        sr_err("Invalid address specified: %d.", addr);
        return NULL;
    }

    /* Looks like a valid USB device specification, but is it connected? */
    devices = NULL;
    libusb_get_device_list(usb_ctx, &devlist);
    for (i = 0; devlist[i]; i++) {
        if ((ret = libusb_get_device_descriptor(devlist[i], &des))) {
            sr_err("Failed to get device descriptor: %s.",
                   libusb_error_name(ret));
            continue;
        }

        if (vid + pid && (des.idVendor != vid || des.idProduct != pid))
            continue;

        b = libusb_get_bus_number(devlist[i]);
        a = libusb_get_device_address(devlist[i]);
        if (bus + addr && (b != bus || a != addr))
            continue;

        sr_dbg("Found USB device (VID:PID = %04x:%04x, bus.address = "
               "%d.%d).", des.idVendor, des.idProduct, b, a);

        usb = sr_usb_dev_inst_new(libusb_get_bus_number(devlist[i]),
                libusb_get_device_address(devlist[i]), NULL);
        devices = g_slist_append(devices, usb);
    }
    libusb_free_device_list(devlist, 1);

    sr_dbg("Found %d device(s).", g_slist_length(devices));

    return devices;
}

/**
 * Find USB devices supporting the USBTMC class
 *
 * @param usb_ctx libusb context to use while scanning.
 *
 * @return A GSList of struct sr_usb_dev_inst, with bus and address fields
 * indicating devices with USBTMC support.
 */
SR_PRIV GSList *sr_usb_find_usbtmc(libusb_context *usb_ctx)
{
    struct sr_usb_dev_inst *usb;
    struct libusb_device **devlist;
    struct libusb_device_descriptor des;
    struct libusb_config_descriptor *confdes;
    const struct libusb_interface_descriptor *intfdes;
    GSList *devices;
    int confidx, intfidx, ret, i;

    devices = NULL;
    libusb_get_device_list(usb_ctx, &devlist);
    for (i = 0; devlist[i]; i++) {
        if ((ret = libusb_get_device_descriptor(devlist[i], &des))) {
            sr_err("Failed to get device descriptor: %s.",
                   libusb_error_name(ret));
            continue;
        }

        for (confidx = 0; confidx < des.bNumConfigurations; confidx++) {
            if (libusb_get_config_descriptor(devlist[i], confidx, &confdes) != 0) {
                sr_err("Failed to get configuration descriptor.");
                break;
            }
            for (intfidx = 0; intfidx < confdes->bNumInterfaces; intfidx++) {
                intfdes = confdes->interface[intfidx].altsetting;
                if (intfdes->bInterfaceClass != LIBUSB_CLASS_APPLICATION
                        || intfdes->bInterfaceSubClass != SUBCLASS_USBTMC
                        || intfdes->bInterfaceProtocol != USBTMC_USB488)
                    continue;
                sr_dbg("Found USBTMC device (VID:PID = %04x:%04x, bus.address = "
                       "%d.%d).", des.idVendor, des.idProduct,
                       libusb_get_bus_number(devlist[i]),
                       libusb_get_device_address(devlist[i]));

                usb = sr_usb_dev_inst_new(libusb_get_bus_number(devlist[i]),
                        libusb_get_device_address(devlist[i]), NULL);
                devices = g_slist_append(devices, usb);
            }
            libusb_free_config_descriptor(confdes);
        }
    }
    libusb_free_device_list(devlist, 1);

    sr_dbg("Found %d device(s).", g_slist_length(devices));

    return devices;
}

SR_PRIV int sr_usb_open(libusb_context *usb_ctx, struct sr_usb_dev_inst *usb)
{
    struct libusb_device **devlist;
    struct libusb_device_descriptor des;
    int ret, r, cnt, i, a, b;

    sr_dbg("Trying to open USB device %d.%d.", usb->bus, usb->address);

    if ((cnt = libusb_get_device_list(usb_ctx, &devlist)) < 0) {
        sr_err("Failed to retrieve device list: %s.",
               libusb_error_name(cnt));
        return SR_ERR;
    }

    ret = SR_ERR;
    for (i = 0; i < cnt; i++) {
        if ((r = libusb_get_device_descriptor(devlist[i], &des)) < 0) {
            sr_err("Failed to get device descriptor: %s.",
                   libusb_error_name(r));
            continue;
        }

        b = libusb_get_bus_number(devlist[i]);
        a = libusb_get_device_address(devlist[i]);
        if (b != usb->bus || a != usb->address)
            continue;

        if ((r = libusb_open(devlist[i], &usb->devhdl)) < 0) {
            sr_err("Failed to open device: %s.",
                   libusb_error_name(r));
            break;
        }

        sr_dbg("Opened USB device (VID:PID = %04x:%04x, bus.address = "
               "%d.%d).", des.idVendor, des.idProduct, b, a);

        ret = SR_OK;
        break;
    }

    libusb_free_device_list(devlist, 1);

    return ret;
}
