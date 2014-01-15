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

#ifndef LIBSIGROK_SIGROK_INTERNAL_H
#define LIBSIGROK_SIGROK_INTERNAL_H

#include <stdarg.h>
#include <glib.h>
#include "config.h" /* Needed for HAVE_LIBUSB_1_0 and others. */
#ifdef HAVE_LIBUSB_1_0
#include <libusb.h>
#endif

/**
 * @file
 *
 * libsigrok private header file, only to be used internally.
 */

/*--- Macros ----------------------------------------------------------------*/

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#ifndef ARRAY_AND_SIZE
#define ARRAY_AND_SIZE(a) (a), ARRAY_SIZE(a)
#endif

struct sr_context {
#ifdef HAVE_LIBUSB_1_0
	libusb_context *libusb_ctx;
#endif
};

#ifdef HAVE_LIBUSB_1_0
struct sr_usb_dev_inst {
	uint8_t bus;
	uint8_t address;
	struct libusb_device_handle *devhdl;
};
#endif

#define SERIAL_PARITY_NONE 0
#define SERIAL_PARITY_EVEN 1
#define SERIAL_PARITY_ODD  2
struct sr_serial_dev_inst {
	char *port;
	char *serialcomm;
	int fd;
};

/* Private driver context. */
struct drv_context {
	struct sr_context *sr_ctx;
	GSList *instances;
};

/*--- log.c -----------------------------------------------------------------*/

SR_PRIV int sr_log(int loglevel, const char *format, ...);
SR_PRIV int sr_spew(const char *format, ...);
SR_PRIV int sr_dbg(const char *format, ...);
SR_PRIV int sr_info(const char *format, ...);
SR_PRIV int sr_warn(const char *format, ...);
SR_PRIV int sr_err(const char *format, ...);

/*--- device.c --------------------------------------------------------------*/

SR_PRIV struct sr_probe *sr_probe_new(int index, int type,
                                      gboolean enabled, const char *name);
SR_PRIV void sr_dev_probes_free(struct sr_dev_inst *sdi);

/* Generic device instances */
SR_PRIV struct sr_dev_inst *sr_dev_inst_new(int mode, int index, int status,
                                            const char *vendor, const char *model, const char *version);
SR_PRIV void sr_dev_inst_free(struct sr_dev_inst *sdi);

#ifdef HAVE_LIBUSB_1_0
/* USB-specific instances */
SR_PRIV struct sr_usb_dev_inst *sr_usb_dev_inst_new(uint8_t bus,
		uint8_t address, struct libusb_device_handle *hdl);
SR_PRIV GSList *sr_usb_find_usbtmc(libusb_context *usb_ctx);
SR_PRIV void sr_usb_dev_inst_free(struct sr_usb_dev_inst *usb);
#endif

/* Serial-specific instances */
SR_PRIV struct sr_serial_dev_inst *sr_serial_dev_inst_new(const char *port,
		const char *serialcomm);
SR_PRIV void sr_serial_dev_inst_free(struct sr_serial_dev_inst *serial);


/*--- hwdriver.c ------------------------------------------------------------*/

SR_PRIV void sr_hw_cleanup_all(void);
SR_PRIV struct sr_config *sr_config_new(int key, GVariant *data);
SR_PRIV void sr_config_free(struct sr_config *src);
SR_PRIV int sr_source_remove(int fd);
SR_PRIV int sr_source_add(int fd, int events, int timeout,
        sr_receive_data_callback_t cb, void *cb_data);

/*--- session.c -------------------------------------------------------------*/

SR_PRIV int sr_session_send(const struct sr_dev_inst *sdi,
		const struct sr_datafeed_packet *packet);
SR_PRIV int sr_session_stop_sync(void);

/*--- std.c -----------------------------------------------------------------*/

typedef int (*dev_close_t)(struct sr_dev_inst *sdi);
typedef void (*std_dev_clear_t)(void *priv);

SR_PRIV int std_hw_init(struct sr_context *sr_ctx, struct sr_dev_driver *di,
		const char *prefix);
SR_PRIV int std_hw_dev_acquisition_stop_serial(struct sr_dev_inst *sdi,
		void *cb_data, dev_close_t hw_dev_close_fn,
		struct sr_serial_dev_inst *serial, const char *prefix);
SR_PRIV int std_session_send_df_header(const struct sr_dev_inst *sdi,
		const char *prefix);
SR_PRIV int std_dev_clear(const struct sr_dev_driver *driver,
		std_dev_clear_t clear_private);

/*--- trigger.c -------------------------------------------------*/
SR_PRIV uint64_t sr_trigger_get_mask0(uint16_t stage);
SR_PRIV uint64_t sr_trigger_get_mask1(uint16_t stage);
SR_PRIV uint64_t sr_trigger_get_value0(uint16_t stage);
SR_PRIV uint64_t sr_trigger_get_value1(uint16_t stage);
SR_PRIV uint64_t sr_trigger_get_edge0(uint16_t stage);
SR_PRIV uint64_t sr_trigger_get_edge1(uint16_t stage);

/*--- hardware/common/serial.c ----------------------------------------------*/

enum {
	SERIAL_RDWR = 1,
	SERIAL_RDONLY = 2,
	SERIAL_NONBLOCK = 4,
};

typedef gboolean (*packet_valid_t)(const uint8_t *buf);

SR_PRIV int serial_open(struct sr_serial_dev_inst *serial, int flags);
SR_PRIV int serial_close(struct sr_serial_dev_inst *serial);
SR_PRIV int serial_flush(struct sr_serial_dev_inst *serial);
SR_PRIV int serial_write(struct sr_serial_dev_inst *serial,
		const void *buf, size_t count);
SR_PRIV int serial_read(struct sr_serial_dev_inst *serial, void *buf,
		size_t count);
SR_PRIV int serial_set_params(struct sr_serial_dev_inst *serial, int baudrate,
		int bits, int parity, int stopbits, int flowcontrol, int rts, int dtr);
SR_PRIV int serial_set_paramstr(struct sr_serial_dev_inst *serial,
		const char *paramstr);
SR_PRIV int serial_readline(struct sr_serial_dev_inst *serial, char **buf,
		int *buflen, gint64 timeout_ms);
SR_PRIV int serial_stream_detect(struct sr_serial_dev_inst *serial,
				 uint8_t *buf, size_t *buflen,
				 size_t packet_size, packet_valid_t is_valid,
				 uint64_t timeout_ms, int baudrate);

/*--- hardware/common/ezusb.c -----------------------------------------------*/

#ifdef HAVE_LIBUSB_1_0
SR_PRIV int ezusb_reset(struct libusb_device_handle *hdl, int set_clear);
SR_PRIV int ezusb_install_firmware(libusb_device_handle *hdl,
				   const char *filename);
SR_PRIV int ezusb_upload_firmware(libusb_device *dev, int configuration,
				  const char *filename);
#endif

/*--- hardware/common/usb.c -------------------------------------------------*/

#ifdef HAVE_LIBUSB_1_0
SR_PRIV GSList *sr_usb_find(libusb_context *usb_ctx, const char *conn);
SR_PRIV int sr_usb_open(libusb_context *usb_ctx, struct sr_usb_dev_inst *usb);
#endif



#endif
