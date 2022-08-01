/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 * 
 * Copyright (C) 2022 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "libsigrok-internal.h"
#include "log.h"

#ifdef _WIN32
  #include <windows.h>
  #define _sleep(m) Sleep((m))
#else
  #include <unistd.h>
  #define _sleep(m) usleep((m) * 1000)
#endif

#undef LOG_PREFIX 
#define LOG_PREFIX "lib_main: "

char DS_RES_PATH[500] = {0}; 
static struct sr_context *sr_ctx = NULL;
static libsigrok_event_callback_t event_callback = NULL;
static GList*	all_device_list = NULL; // All device instance, sr_dev_inst* type
static GThread	*hotplug_thread = NULL;

static int sr_exit_flag = 0;
static int attach_event_flag = 0;
static int detach_event_flag = 0;
static int wait_redo_attch_flag = 0;
static int wait_redo_attch_times = 0;
 
static void hotplug_event_listen_callback(struct libusb_context *ctx, struct libusb_device *dev, int event);
static void usb_hotplug_process_proc();

/**
 * Must call first
 */
SR_API int sr_lib_init()
{ 
	int ret = 0;
	struct sr_dev_driver **drivers = NULL;
	struct sr_dev_driver **dr = NULL;

	if (sr_ctx != NULL){
		return SR_ERR_HAVE_DONE;
	}

	ret = sr_init(&sr_ctx);
    if (ret != SR_OK){
		return ret;
	}
	sr_exit_flag = 0;

	// Initialise all libsigrok drivers
	drivers = sr_driver_list();
	for (dr = drivers; *dr; dr++) {
		if (sr_driver_init(sr_ctx, *dr) != SR_OK) {
			sr_err("Failed to initialize driver '%s'", (*dr)->name);
			return SR_ERR;
		}
	}

	sr_ctx->hotplug_tv.tv_sec = 0;
    sr_ctx->hotplug_tv.tv_usec = 0;

	sr_listen_hotplug(sr_ctx, hotplug_event_listen_callback);

	/** Start usb hotplug thread */
	hotplug_thread = g_thread_new("hotplug_proc", usb_hotplug_process_proc, NULL);

	return SR_OK;
}

/**
 * Free all resource before program exits
 */
SR_API int sr_lib_exit()
{    
	struct sr_dev_driver **drivers = NULL;
	struct sr_dev_driver **dr = NULL;
	struct sr_dev_driver *driver_ins;
	GSList *l;
	struct sr_dev_inst *dev;
 
	if (sr_ctx == NULL){
		return SR_ERR_HAVE_DONE;
	}

	sr_close_hotplug(sr_ctx);

	sr_exit_flag = 1; //all thread to exit

	if (hotplug_thread != NULL){
		g_thread_join(hotplug_thread);
		hotplug_thread = NULL;
	}

	// Release all device
	for (l = all_device_list; l; l = l->next)
	{
		dev = l->data;
		if (dev && dev->driver)
		{
			driver_ins = dev->driver;

			if (driver_ins->dev_destroy)
				driver_ins->dev_destroy(dev);
			else if (driver_ins->dev_close)
				driver_ins->dev_close(dev);
		}
	}
	g_safe_free_list(all_device_list);
    

	if (sr_exit(sr_ctx) != SR_OK){
		sr_err("%s", "call sr_exit error");
	}
    sr_ctx = NULL;

    return SR_OK;
}

/**
 * Set the firmware binary file directory
 */
SR_API void sr_set_firmware_resource_dir(const char *dir)
{
	if (dir){
		strcpy(DS_RES_PATH, dir);

		int len = strlen(DS_RES_PATH);
		if (DS_RES_PATH[len-1] != '/'){
			DS_RES_PATH[len] = '/'; 
			DS_RES_PATH[len + 1] = 0;
		}
	}
}

/**
 * Set event callback, event type see enum libsigrok_event_type
 */
SR_API void sr_set_event_callback(libsigrok_event_callback_t *cb)
{
	event_callback = cb;
}


/**-------------------private function ---------------*/

static int scan_hardware_device()
{

}

static void hotplug_event_listen_callback(struct libusb_context *ctx, struct libusb_device *dev, int event)
{
	if (event == USB_EV_HOTPLUG_ATTACH){
		sr_info("One device attached.");

		if (wait_redo_attch_flag){
			sr_info("%s", "Device loose contact, but it reconnect success.");
		}
		wait_redo_attch_flag = 0;
		attach_event_flag = 1;
	}
	else if (event == USB_EV_HOTPLUG_DETTACH){
		sr_info("One device detached.");
		wait_redo_attch_flag = 1;  //Begin wait the device reconnect, if timeout, will process the detach event.
		wait_redo_attch_times = 0;
	}
	else{
		sr_err("%s", "Unknown usb event");
		wait_redo_attch_flag = 1;
	}
}

static void process_attach_event()
{
	sr_info("%s", "Process device attch event.");
}

static void process_detach_event()
{
	sr_info("%s", "Process device detach event.");
}

static void usb_hotplug_process_proc()
{
	sr_info("%s", "Hotplug thread start!");

	while (!sr_exit_flag)
	{
		sr_hotplug_wait_timout(sr_ctx);

		if (attach_event_flag){
			process_attach_event();
			attach_event_flag = 0;
		}
		if (detach_event_flag){
			process_detach_event();
			detach_event_flag = 0;
		}

		_sleep(100);

		if (wait_redo_attch_flag){
			wait_redo_attch_times++;

			// 500ms
			if (wait_redo_attch_times == 5){
				//Device loose contact,wait for it reconnection timeout.
				detach_event_flag = 1; // use detach event
				wait_redo_attch_flag = 0;
			}              
		}
	}
	
	sr_info("%s", "Hotplug thread end!");
}