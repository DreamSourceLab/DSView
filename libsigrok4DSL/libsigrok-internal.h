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

//#include <stdarg.h>
#include <glib.h>
#include "config.h" /* Needed for HAVE_LIBUSB_1_0 and others. */
#include <libusb-1.0/libusb.h>
#include "libsigrok.h"


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

#undef min
#define min(a,b) ((a)<(b)?(a):(b))
#undef max
#define max(a,b) ((a)>(b)?(a):(b))

#define g_safe_free(p) 			if((p)) g_free((p)); ((p)) = NULL;
#define g_safe_free_list(p) 	if((p)) g_slist_free((p)); ((p)) = NULL;

struct sr_context {
	libusb_context*			libusb_ctx;
	libusb_hotplug_callback_handle hotplug_handle;
	hotplug_event_callback  hotplug_callback;
	void 					*hotplug_user_data;
	struct 					timeval hotplug_tv;
	libsigrok_event_callback_t event_callback;
	GList 					*deiveList; // All device instance, sr_dev_inst* type
};

struct sr_session {
	/** List of struct sr_dev pointers. */
	GSList *devs;
	/** List of struct datafeed_callback pointers. */
	GSList *datafeed_callbacks;
    gboolean running;

	unsigned int num_sources;

	/*
	 * Both "sources" and "pollfds" are of the same size and contain pairs
	 * of descriptor and callback function. We can not embed the GPollFD
	 * into the source struct since we want to be able to pass the array
	 * of all poll descriptors to g_poll().
	 */
	struct source *sources;
	GPollFD *pollfds;
	int source_timeout;

	/*
	 * These are our synchronization primitives for stopping the session in
	 * an async fashion. We need to make sure the session is stopped from
	 * within the session thread itself.
	 */
    GMutex stop_mutex;
	gboolean abort_session;
};

struct sr_usb_dev_inst {
	uint8_t bus;
	uint8_t address;
	struct libusb_device_handle *devhdl;
};

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


/*
 * Oscilloscope
 */
#define MAX_TIMEBASE SR_SEC(10)
#define MIN_TIMEBASE SR_NS(10)

struct ds_trigger {
    uint16_t trigger_en;
    uint16_t trigger_mode;
    uint16_t trigger_pos;
    uint16_t trigger_stages;
    unsigned char trigger_logic[TriggerStages+1];
    unsigned char trigger0_inv[TriggerStages+1];
    unsigned char trigger1_inv[TriggerStages+1];
    char trigger0[TriggerStages+1][MaxTriggerProbes];
    char trigger1[TriggerStages+1][MaxTriggerProbes];
    uint32_t trigger0_count[TriggerStages+1];
    uint32_t trigger1_count[TriggerStages+1];
};


/*--- device.c --------------------------------------------------------------*/

SR_PRIV struct sr_channel *sr_channel_new(uint16_t index, int type,
                                          gboolean enabled, const char *name);
SR_PRIV void sr_dev_probes_free(struct sr_dev_inst *sdi);

/* Generic device instances */
SR_PRIV struct sr_dev_inst *sr_dev_inst_new(int mode, int index, int status,
                                            const char *vendor, const char *model, const char *version);
SR_PRIV void sr_dev_inst_free(struct sr_dev_inst *sdi);

/* USB-specific instances */
SR_PRIV struct sr_usb_dev_inst *sr_usb_dev_inst_new(uint8_t bus,
		uint8_t address, struct libusb_device_handle *hdl);
SR_PRIV GSList *sr_usb_find_usbtmc(libusb_context *usb_ctx);
SR_PRIV void sr_usb_dev_inst_free(struct sr_usb_dev_inst *usb);

/* Serial-specific instances */
SR_PRIV struct sr_serial_dev_inst *sr_serial_dev_inst_new(const char *port,
		const char *serialcomm);
SR_PRIV void sr_serial_dev_inst_free(struct sr_serial_dev_inst *serial);


/*--- hwdriver.c ------------------------------------------------------------*/

typedef int (*sr_receive_data_callback_t)(int fd, int revents, const struct sr_dev_inst *sdi);

SR_PRIV void sr_hw_cleanup_all(void);
SR_PRIV int sr_source_remove(int fd);
SR_PRIV int sr_source_add(int fd, int events, int timeout,
        sr_receive_data_callback_t cb, void *cb_data);

/*--- session.c -------------------------------------------------------------*/

SR_PRIV int sr_session_send(const struct sr_dev_inst *sdi,
		const struct sr_datafeed_packet *packet);
SR_PRIV int sr_session_stop_sync(void);

SR_PRIV int usb_hotplug_callback(struct libusb_context *ctx, struct libusb_device *dev,
                                  libusb_hotplug_event event, void *user_data);

SR_PRIV int sr_session_source_add(int fd, int events, int timeout,
		sr_receive_data_callback_t cb, const struct sr_dev_inst *sdi);
SR_PRIV int sr_session_source_add_pollfd(GPollFD *pollfd, int timeout,
		sr_receive_data_callback_t cb, const struct sr_dev_inst *sdi);
SR_PRIV int sr_session_source_add_channel(GIOChannel *channel, int events,
		int timeout, sr_receive_data_callback_t cb, const struct sr_dev_inst *sdi);
SR_PRIV int sr_session_source_remove(int fd);
SR_PRIV int sr_session_source_remove_pollfd(GPollFD *pollfd);
SR_PRIV int sr_session_source_remove_channel(GIOChannel *channel);

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

SR_PRIV uint16_t ds_trigger_get_mask0(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode);
SR_PRIV uint16_t ds_trigger_get_value0(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode);
SR_PRIV uint16_t ds_trigger_get_edge0(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode);
SR_PRIV uint16_t ds_trigger_get_mask1(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode);
SR_PRIV uint16_t ds_trigger_get_value1(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode);
SR_PRIV uint16_t ds_trigger_get_edge1(uint16_t stage, uint16_t msc, uint16_t lsc, gboolean qutr_mode, gboolean half_mode);

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


SR_PRIV int ezusb_reset(struct libusb_device_handle *hdl, int set_clear);
SR_PRIV int ezusb_install_firmware(libusb_device_handle *hdl,
				   const char *filename);
SR_PRIV int ezusb_upload_firmware(libusb_device *dev, int configuration,
				  const char *filename);

/*--- hardware/common/usb.c -------------------------------------------------*/

SR_PRIV GSList *sr_usb_find(libusb_context *usb_ctx, const char *conn);
SR_PRIV int sr_usb_open(libusb_context *usb_ctx, struct sr_usb_dev_inst *usb);

/*--- backend.c -------------------------------------------------------------*/
SR_PRIV int sr_init(struct sr_context **ctx);
SR_PRIV int sr_exit(struct sr_context *ctx);

/*--- lib_main.c -------------------------------------------------*/
SR_PRIV char* sr_get_firmware_res_path();

#endif
