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
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

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

struct sr_lib_context
{	
	libsigrok_event_callback_t event_callback;
	struct sr_context	*sr_ctx;
	GList*				device_list; // All device instance, sr_dev_inst* type
	pthread_mutex_t		mutext;
	GThread				*hotplug_thread; 
	int					lib_exit_flag;
	int					attach_event_flag;
	int					detach_event_flag;
	int					is_waitting_reconnect;
	int					check_reconnect_times;
	struct libusb_device *attach_device_handle;
	struct libusb_device *detach_device_handle;
	struct sr_device_info current_device;
};

static void hotplug_event_listen_callback(struct libusb_context *ctx, struct libusb_device *dev, int event);
static void usb_hotplug_process_proc();
static void destroy_device_instance(struct sr_dev_inst *dev);

static struct sr_lib_context lib_ctx = {
	.event_callback = NULL,
	.sr_ctx = NULL,
	.device_list = NULL,
	.hotplug_thread = NULL, 
	.lib_exit_flag = 0,
	.attach_event_flag = 0,
	.detach_event_flag = 0,
	.is_waitting_reconnect = 0,
	.check_reconnect_times = 0,
	.attach_device_handle = NULL,
	.detach_device_handle = NULL,
	.current_device = {
		.handle = NULL,
		.name[0] = '\0',
		.is_current = 0,
		.dev_type = DEV_TYPE_UNKOWN,
	},
};

/**
 * Must call first
 */
SR_API int sr_lib_init()
{
	int ret = 0;
	struct sr_dev_driver **drivers = NULL;
	struct sr_dev_driver **dr = NULL;

	if (lib_ctx.sr_ctx != NULL){
		return SR_ERR_HAVE_DONE;
	}

	sr_log_init(); //try init log

	sr_info("Init %s.", SR_LIB_NAME);

	ret = sr_init(&lib_ctx.sr_ctx);
    if (ret != SR_OK){
		return ret;
	}
	lib_ctx.lib_exit_flag = 0;	

	// Initialise all libsigrok drivers
	drivers = sr_driver_list();
	for (dr = drivers; *dr; dr++) {
		if (sr_driver_init(lib_ctx.sr_ctx, *dr) != SR_OK) {
			sr_err("Failed to initialize driver '%s'", (*dr)->name);
			return SR_ERR;
		}
	}
	pthread_mutex_init(&lib_ctx.mutext, NULL); //init locker

	lib_ctx.sr_ctx->hotplug_tv.tv_sec = 0;
    lib_ctx.sr_ctx->hotplug_tv.tv_usec = 0;

	sr_listen_hotplug(lib_ctx.sr_ctx, hotplug_event_listen_callback);

	/** Start usb hotplug thread */
	lib_ctx.hotplug_thread = g_thread_new("hotplug_proc", usb_hotplug_process_proc, NULL);

	return SR_OK;
}

/**
 * Free all resource before program exits
 */
SR_API int sr_lib_exit()
{
	GSList *l; 
 
	if (lib_ctx.sr_ctx == NULL){
		return SR_ERR_HAVE_DONE;
	}

	sr_info("Uninit %s.", SR_LIB_NAME);

	sr_close_hotplug(lib_ctx.sr_ctx);

	lib_ctx.lib_exit_flag = 1; //all thread to exit

	if (lib_ctx.hotplug_thread != NULL){
		g_thread_join(lib_ctx.hotplug_thread);
		lib_ctx.hotplug_thread = NULL;
	}

	// Release all device
	for (l = lib_ctx.device_list; l; l = l->next){ 
		destroy_device_instance((struct sr_dev_inst*)l->data);
	}
	g_safe_free_list(lib_ctx.device_list); 

	pthread_mutex_destroy(&lib_ctx.mutext);  //uninit locker

	if (sr_exit(lib_ctx.sr_ctx) != SR_OK){
		sr_err("%s", "call sr_exit error");
	}
    lib_ctx.sr_ctx = NULL;

	sr_log_uninit(); //try uninit log

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

		sr_info("Firmware resource path:%s", DS_RES_PATH);
	}
}

/**
 * Set event callback, event type see enum libsigrok_event_type
 */
SR_API void sr_set_event_callback(libsigrok_event_callback_t cb)
{
	lib_ctx.event_callback = cb;
}

/**
 * Get the device list, if the field _handle is 0, the list visited to end.
 * User need call free() to release the buffer. If the list is empty, the out_list is null.
 */
SR_API int sr_device_get_list(struct sr_device_info** out_list, int *out_count)
{
	int num;
	struct sr_device_info *array = NULL;
	struct sr_device_info *p = NULL;
	GList *l;
	struct sr_dev_inst *dev;

	if (out_list == NULL){
		return SR_ERR_ARG;
	}
	*out_list = NULL;

	pthread_mutex_lock(&lib_ctx.mutext);

	num  = g_slist_length(lib_ctx.device_list);
	if (num == 0){
		pthread_mutex_unlock(&lib_ctx.mutext);
		return SR_OK;
	}

	array = (struct sr_device_info*)malloc(sizeof(struct sr_device_info) * (num+1));
	if (array == NULL){
		pthread_mutex_unlock(&lib_ctx.mutext);
		return SR_ERR_MALLOC;
	}

	p = array;

	for (l=lib_ctx.device_list; l; l = l->next){
		dev = l->data;
		p->handle = dev->handle;
		strncpy(p->name, dev->name, sizeof(dev->name) - 1);
		p->dev_type = dev->dev_type;
		p->is_current = (dev->handle == lib_ctx.current_device.handle);
		p++;	 
	} 

	p->handle = 0; //is the end
	p->name[0] = '\0';

	if (out_count){
		*out_count = num;
	}

	pthread_mutex_unlock(&lib_ctx.mutext);

	*out_list  = array;
	return SR_OK;
}

/**-------------------internal function ---------------*/
/**
 * Check whether the USB device is in the device list.
 */
SR_PRIV int sr_usb_device_is_exists(libusb_device *usb_dev)
{
	GList *l;
	struct sr_dev_inst *dev;
	struct sr_usb_dev_inst *usb_dev_info;
	int bFind = 0;

	if (usb_dev == NULL){
		sr_err("%s", "sr_usb_device_is_exists(), @usb_dev is null.");
		return 0;
	}

	pthread_mutex_lock(&lib_ctx.mutext);

	for (l = lib_ctx.device_list; l; l = l->next){
		dev = l->data;
		usb_dev_info = dev->conn;
		if (dev->dev_type == DEV_TYPE_USB 
				&& usb_dev_info != NULL 
			 	&& usb_dev_info->usb_dev == usb_dev){
			bFind = 1;
		}
	} 

	pthread_mutex_unlock(&lib_ctx.mutext);

	return bFind;
}

/**
 * Remove one device from the list, and destory it.
 * User need to call sr_device_get_list() to get the new list.
 */
SR_API int sr_remove_device(sr_device_handle handle)
{
	GList *l;
	struct sr_dev_inst *dev;
	int bFind = 0;

	if (handle == NULL){
		return SR_ERR_ARG;
	}

	pthread_mutex_lock(&lib_ctx.mutext);

	for (l = lib_ctx.device_list; l; l = l->next){
		dev = l->data;
		if (dev->handle == handle){
			lib_ctx.device_list = g_slist_remove(lib_ctx.device_list, l->data);
			destroy_device_instance(dev);
			bFind = 1;
			break;
		}
	}
	pthread_mutex_unlock(&lib_ctx.mutext);

	if (bFind == 0){
		return SR_ERR_CALL_STATUS;
	}
	return SR_OK;
}

/**
 * Get the current device info.
 * If the current device is not exists, the handle filed will be set null.
 */
SR_API int sr_get_current_device_info(struct sr_device_info *info)
{
	if (info == NULL){
		return SR_ERR_ARG;
	}

	info->handle = NULL;
	info->name[0] = '\0';
	info->is_current = 0;
	info->dev_type = DEV_TYPE_UNKOWN;

	if (lib_ctx.current_device.handle != NULL){
		info->handle = lib_ctx.current_device.handle;
		strncpy(info->name, lib_ctx.current_device.name, sizeof(info->name));
		info->is_current = 1;
		info->dev_type = lib_ctx.current_device.dev_type;
	}

	return SR_OK;
}

/**-------------------private function ---------------*/

static int update_device_handle(struct libusb_device *old_dev, struct libusb_device *new_dev)
{
	GList *l;
	struct sr_dev_inst *dev;
	struct sr_usb_dev_inst *usb_dev_info;
	int bFind = 0;
	uint8_t bus;
	uint8_t address;

	pthread_mutex_lock(&lib_ctx.mutext);

	for (l = lib_ctx.device_list; l; l = l->next){
		dev = l->data;
		usb_dev_info = dev->conn;
		if (dev->dev_type == DEV_TYPE_USB 
				&& usb_dev_info != NULL 
			 	&& usb_dev_info->usb_dev == old_dev){

			bus = libusb_get_bus_number(new_dev);
			address = libusb_get_device_address(new_dev);

			if (bus == usb_dev_info->bus && address == usb_dev_info->address){
				bFind = 1;
				usb_dev_info->usb_dev = new_dev;
			}
			else{
				sr_err("Try to update the device handle, but the bus and addres is not the same!");
			}			
			
			break;
		}
	} 

	pthread_mutex_unlock(&lib_ctx.mutext);
	return bFind;
}

static void hotplug_event_listen_callback(struct libusb_context *ctx, struct libusb_device *dev, int event)
{
	int bDone = 0;

	if (dev == NULL){
		sr_err("%s", "hotplug_event_listen_callback(), @dev is null.");
		return;
	}

	if (event == USB_EV_HOTPLUG_ATTACH){
		sr_info("One device attached, handle:%p", dev);

		if (lib_ctx.is_waitting_reconnect){
			if (lib_ctx.attach_device_handle != NULL){
				sr_err("One attached device haven't processed complete,handle:%p", 
							lib_ctx.attach_device_handle);
			}

			if (lib_ctx.detach_device_handle == NULL){
				sr_err("%s", "The detached device handle is null, but the status is waitting for reconnect.");
			}
			else{
				if (update_device_handle(lib_ctx.detach_device_handle, dev)){
					bDone = 1;
					sr_info("%s", "One device loose contact, but it reconnect success.");
				}
				else{
					sr_err("Update device handle error! can't find the old.");
				}
				lib_ctx.detach_device_handle = NULL;
			}			
		}
		if (bDone == 0){
			lib_ctx.attach_event_flag = 1;  // Is a new device attched.
			lib_ctx.attach_device_handle = dev;
		}
		lib_ctx.is_waitting_reconnect = 0;
	}
	else if (event == USB_EV_HOTPLUG_DETTACH){
		sr_info("One device detached,handle:%p", dev);

		if (lib_ctx.detach_device_handle != NULL){
			sr_err("One detached device haven't processed complete,handle:%p", 
						lib_ctx.detach_device_handle);
		}
		/**
		 * Begin to wait the device reconnect, if timeout, will process the detach event.
		 */
		lib_ctx.is_waitting_reconnect = 1;
		lib_ctx.check_reconnect_times = 0;
		lib_ctx.detach_device_handle = dev;
	}
	else{
		sr_err("%s", "Unknown usb device event");
	}
}

static void process_attach_event()
{
	struct sr_dev_driver **drivers;
	GList *dev_list;
	GSList *l;
	GList *cur_list;
	struct sr_dev_driver *dr;
	int num = 0;

	sr_info("%s", "Process device attch event.");

	if (lib_ctx.attach_device_handle  == NULL){
		sr_err("%s", "The attached device handle is null.");
		return;
	}

	drivers = sr_driver_list();

	while (*drivers)
	{
		dr = *drivers;

		if (dr->driver_type == DRIVER_TYPE_HARDWARE)
		{
			dev_list = dr->scan(NULL);
			if (dev_list != NULL){
				sr_info("Get new device list by driver \"%s\"", dr->name);

				pthread_mutex_lock(&lib_ctx.mutext);
				cur_list = lib_ctx.device_list;

				for (l= dev_list; l; l = l->next){
					cur_list = g_slist_append(cur_list, l->data);
					num++;
				}

				lib_ctx.device_list = cur_list;
				pthread_mutex_unlock(&lib_ctx.mutext);
				g_slist_free(dev_list);
			}
		}  

		drivers++;
	}

	if (lib_ctx.event_callback != NULL && num > 0){
		// Tell user one new device attched, and the list is updated.
        lib_ctx.event_callback(SR_EV_NEW_DEVICE_ATTACH);
	}

	lib_ctx.attach_device_handle  = NULL;
}

static void process_detach_event()
{
	libusb_device *ev_dev;
    int ev = SR_EV_INACTIVE_DEVICE_DETACH;
	GList *l;
	struct sr_dev_inst *dev;
	struct sr_usb_dev_inst *usb_dev_info;
	struct sr_dev_driver *driver_ins;
	
	sr_info("%s", "Process device detach event.");
 
	ev_dev = lib_ctx.detach_device_handle;
	if (ev_dev == NULL){
		sr_err("%s", "The detached device handle is null.");
		return;
	}
	lib_ctx.detach_device_handle = NULL;

	pthread_mutex_lock(&lib_ctx.mutext);

	for (l = lib_ctx.device_list; l; l = l->next){
		dev = l->data;
		usb_dev_info = dev->conn;

		if (dev->dev_type == DEV_TYPE_USB 
				&& usb_dev_info != NULL 
			 	&& usb_dev_info->usb_dev == ev_dev){ 		
			//Found the device, and remove it.
			lib_ctx.device_list = g_slist_remove(lib_ctx.device_list, l->data);
			destroy_device_instance(dev);
			break;
		}
	}
	pthread_mutex_unlock(&lib_ctx.mutext);

	if (ev_dev == lib_ctx.current_device.handle)
        ev = SR_EV_CURRENT_DEVICE_DETACH;

	if (lib_ctx.event_callback != NULL){
		//Tell user one new device detached, and the list is updated.
		lib_ctx.event_callback(ev);		
	}
}

static void usb_hotplug_process_proc()
{
	sr_info("%s", "Hotplug thread start!");

    while (!lib_ctx.lib_exit_flag)
	{
		sr_hotplug_wait_timout(lib_ctx.sr_ctx);

		if (lib_ctx.attach_event_flag){
			process_attach_event();
			lib_ctx.attach_event_flag = 0;
		}
		if (lib_ctx.detach_event_flag){
			process_detach_event();
			lib_ctx.detach_event_flag = 0;
		}

		_sleep(100);

		if (lib_ctx.is_waitting_reconnect){
			lib_ctx.check_reconnect_times++;

			// 500ms
			if (lib_ctx.check_reconnect_times == 5){
				//Device loose contact,wait for it reconnection timeout.
				lib_ctx.detach_event_flag = 1; // use detach event
				lib_ctx.is_waitting_reconnect = 0;
			}              
		}
	}
	
	sr_info("%s", "Hotplug thread end!");
}


static void destroy_device_instance(struct sr_dev_inst *dev)
{
	if (dev == NULL || dev->driver == NULL){
		sr_err("%s", "destroy_device_instance() argument error.");
		return;
	} 
	struct sr_dev_driver *driver_ins;
	driver_ins = dev->driver;

	if (driver_ins->dev_destroy)
		driver_ins->dev_destroy(dev);
	else if (driver_ins->dev_close)
		driver_ins->dev_close(dev);
}
