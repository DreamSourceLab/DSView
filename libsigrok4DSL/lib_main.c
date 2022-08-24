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
#define _sleep(m) usleep((m)*1000)
#endif

#undef LOG_PREFIX
#define LOG_PREFIX "lib_main: "

char DS_RES_PATH[500] = {0};

struct sr_lib_context
{
	dslib_event_callback_t event_callback;
	struct sr_context *sr_ctx;
	GList *device_list; // All device instance, sr_dev_inst* type
	pthread_mutex_t mutext;
	GThread *hotplug_thread;
	int lib_exit_flag;
	int attach_event_flag;
	int detach_event_flag;
	int is_waitting_reconnect;
	int check_reconnect_times;
	struct libusb_device *attach_device_handle;
	struct libusb_device *detach_device_handle;
	struct ds_device_info actived_device_info;
	struct sr_dev_inst *actived_device_instance;
	int collect_stop_flag;
	GThread *collect_thread;
	ds_datafeed_callback_t data_forward_callback;
	int callback_thread_count;
};

static void hotplug_event_listen_callback(struct libusb_context *ctx, struct libusb_device *dev, int event);
static void usb_hotplug_process_proc();
static void destroy_device_instance(struct sr_dev_inst *dev);
static void close_device_instance(struct sr_dev_inst *dev);
static int open_device_instance(struct sr_dev_inst *dev);
static void collect_run_proc();
static void post_event_async(int event);

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
	.actived_device_info = {
		.handle = 0,
		.name[0] = '\0',
		.dev_type = DEV_TYPE_UNKOWN,
	},
	.actived_device_instance = NULL,
	.collect_stop_flag = 0,
	.data_forward_callback = NULL,
	.collect_thread = NULL,
	.callback_thread_count = 0,
};

/**
 * Must call first
 */
SR_API int ds_lib_init()
{
	int ret = 0;
	struct sr_dev_driver **drivers = NULL;
	struct sr_dev_driver **dr = NULL;

	if (lib_ctx.sr_ctx != NULL)
	{
		return SR_ERR_HAVE_DONE;
	}

	sr_log_init(); // try init log

	sr_info("Init %s.", SR_LIB_NAME);

	ret = sr_init(&lib_ctx.sr_ctx);
	if (ret != SR_OK)
	{
		return ret;
	}
	lib_ctx.lib_exit_flag = 0;

	// Init trigger.
	ds_trigger_init();

	// Initialise all libsigrok drivers
	drivers = sr_driver_list();
	for (dr = drivers; *dr; dr++)
	{
		if (sr_driver_init(lib_ctx.sr_ctx, *dr) != SR_OK)
		{
			sr_err("Failed to initialize driver '%s'", (*dr)->name);
			return SR_ERR;
		}
	}
	pthread_mutex_init(&lib_ctx.mutext, NULL); // init locker

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
SR_API int ds_lib_exit()
{
	GSList *l;

	if (lib_ctx.sr_ctx == NULL)
	{
		return SR_ERR_HAVE_DONE;
	}

	sr_info("Uninit %s.", SR_LIB_NAME);

	if (lib_ctx.collect_thread != NULL)
	{
		ds_stop_collect(); // stop collect.
	}

	sr_close_hotplug(lib_ctx.sr_ctx);

	lib_ctx.lib_exit_flag = 1; // all thread to exit

	if (lib_ctx.hotplug_thread != NULL)
	{
		g_thread_join(lib_ctx.hotplug_thread);
		lib_ctx.hotplug_thread = NULL;
	}

	// Release all device
	for (l = lib_ctx.device_list; l; l = l->next)
	{
		destroy_device_instance((struct sr_dev_inst *)l->data);
	}
	g_safe_free_list(lib_ctx.device_list);

	pthread_mutex_destroy(&lib_ctx.mutext); // uninit locker

	// Uninit trigger.
	ds_trigger_destroy();

	if (sr_exit(lib_ctx.sr_ctx) != SR_OK)
	{
		sr_err("%s", "call sr_exit error");
	}
	lib_ctx.sr_ctx = NULL;

	sr_log_uninit(); // try uninit log

	return SR_OK;
}

/**
 * Set the firmware binary file directory
 */
SR_API void ds_set_firmware_resource_dir(const char *dir)
{
	if (dir)
	{
		strcpy(DS_RES_PATH, dir);

		int len = strlen(DS_RES_PATH);
		if (DS_RES_PATH[len - 1] != '/')
		{
			DS_RES_PATH[len] = '/';
			DS_RES_PATH[len + 1] = 0;
		}

		sr_info("Firmware resource path:\"%s\"", DS_RES_PATH);
	}
}

/**
 * Set event callback, event type see enum libsigrok_event_type
 */
SR_API void ds_set_event_callback(dslib_event_callback_t cb)
{
	lib_ctx.event_callback = cb;
}

/**
 * Set the data receive callback.
 */
SR_API void ds_set_datafeed_callback(ds_datafeed_callback_t cb)
{
	lib_ctx.data_forward_callback = cb;
}

/**
 * Get the device list, if the field _handle is 0, the list visited to end.
 * User need call free() to release the buffer. If the list is empty, the out_list is null.
 */
SR_API int ds_get_device_list(struct ds_device_info **out_list, int *out_count)
{
	int num;
	struct ds_device_info *array = NULL;
	struct ds_device_info *p = NULL;
	GList *l;
	struct sr_dev_inst *dev;

	if (out_list == NULL)
	{
		return SR_ERR_ARG;
	}
	*out_list = NULL;

	pthread_mutex_lock(&lib_ctx.mutext);

	num = g_slist_length(lib_ctx.device_list);
	if (num == 0)
	{
		pthread_mutex_unlock(&lib_ctx.mutext);
		return SR_OK;
	}

	array = (struct ds_device_info *)malloc(sizeof(struct ds_device_info) * (num + 1));
	if (array == NULL)
	{
		pthread_mutex_unlock(&lib_ctx.mutext);
		return SR_ERR_MALLOC;
	}

	p = array;

	for (l = lib_ctx.device_list; l; l = l->next)
	{
		dev = l->data;
		p->handle = dev->handle;
		strncpy(p->name, dev->name, sizeof(dev->name) - 1);
		p->dev_type = dev->dev_type;
		p++;
	}

	p->handle = 0; // is the end
	p->name[0] = '\0';

	if (out_count)
	{
		*out_count = num;
	}

	pthread_mutex_unlock(&lib_ctx.mutext);

	*out_list = array;
	return SR_OK;
}

/**
 * Active a device.
 */
SR_API int ds_active_device(ds_device_handle handle)
{
	GList *l;
	struct sr_dev_inst *dev;
	int bFind = 0;
	int ret;

	if (handle == NULL)
	{
		return SR_ERR_ARG;
	}
	ret = SR_OK;

	sr_info("%s", "Begin set current device.");

	if (lib_ctx.collect_thread != NULL)
	{
		sr_err("%s", "One device is collecting, switch device error.");
		return SR_ERR_CALL_STATUS;
	}

	pthread_mutex_lock(&lib_ctx.mutext);

	if (lib_ctx.actived_device_instance != NULL)
	{

		sr_info("Close the previous device \"%s\"", lib_ctx.actived_device_instance->name);

		close_device_instance(lib_ctx.actived_device_instance);
		lib_ctx.actived_device_instance = NULL;
		lib_ctx.actived_device_info.handle = NULL;
		lib_ctx.actived_device_info.name[0] = '\0';
	}

	// To open the new.
	for (l = lib_ctx.device_list; l; l = l->next)
	{
		dev = l->data;
		if (dev->handle == handle)
		{
			bFind = 1;

			if (dev->dev_type == DEV_TYPE_USB && DS_RES_PATH[0] == '\0')
			{
				sr_err("%s", "Please call ds_set_firmware_resource_dir() to set the firmware resource path.");
			}

			sr_info("Switch \"%s\" to current device.", dev->name);

			if (open_device_instance(dev) == SR_OK)
			{
				lib_ctx.actived_device_info.handle = dev->handle;
				lib_ctx.actived_device_info.dev_type = dev->dev_type;
				strncpy(lib_ctx.actived_device_info.name, dev->name, sizeof(lib_ctx.actived_device_info.name) - 1);
				lib_ctx.actived_device_instance = dev;
			}
			else
			{
				sr_err("%s", "Open device error!");
				ret = SR_ERR_CALL_STATUS;
			}
			break;
		}
	}

	pthread_mutex_unlock(&lib_ctx.mutext);

	sr_info("%s", "End of setting current device.");

	if (!bFind)
	{
		sr_err("ds_active_device() error, can't find the device.");
		return SR_ERR_CALL_STATUS;
	}

	return ret;
}

/**
 * Active a device
 * if @index is -1, will select the last one.
 */
SR_API int ds_active_device_by_index(int index)
{
	GList *l;
	struct sr_dev_inst *dev;
	ds_device_handle handle = NULL;
	ds_device_handle lst_handle = NULL;
	int i = 0;

	pthread_mutex_lock(&lib_ctx.mutext);

	// Get index
	for (l = lib_ctx.device_list; l; l = l->next)
	{
		dev = l->data;
		lst_handle = dev->handle;

		if (index == i)
		{
			handle = dev->handle;
			break;
		}
		++i;
	}
	pthread_mutex_unlock(&lib_ctx.mutext);

	if (index == -1)
	{
		handle = lst_handle; // Get the last one.
	}

	if (handle == NULL)
	{
		sr_err("%s", "ds_active_device_by_index(), index is error!");
		return SR_ERR_CALL_STATUS;
	}

	return ds_active_device(handle);
}

/**
 * Get the selected device index.
 */
SR_API int ds_get_actived_device_index()
{
	int dex = -1;
	GList *l;
	int i = 0;

	if (lib_ctx.actived_device_instance == NULL)
	{
		return -1;
	}

	pthread_mutex_lock(&lib_ctx.mutext);

	for (l = lib_ctx.device_list; l; l = l->next)
	{
		if ((struct sr_dev_inst *)l->data == lib_ctx.actived_device_instance)
		{
			dex = i;
			break;
		}
		i++;
	}

	pthread_mutex_unlock(&lib_ctx.mutext);

	return dex;
}

/**
 * Get the decive supports work mode, mode list: LOGIC、ANALOG、DSO
 * return type see struct sr_dev_mode.
 */
SR_API const GSList *ds_get_actived_device_mode_list()
{
	GList *l;
	struct sr_dev_inst *dev; 

	dev = lib_ctx.actived_device_instance ;

	if (dev == NULL){
		sr_err("%s", "Have no actived device.");
	}
	if (dev->driver == NULL || dev->driver->dev_mode_list){
		sr_err("%s", "Module not implemented.");
		return NULL;
	}

	 return dev->driver->dev_mode_list(dev);
}

/**
 * Remove one device from the list, and destory it.
 * User need to call ds_get_device_list() to get the new list.
 */
SR_API int ds_remove_device(ds_device_handle handle)
{
	GList *l;
	struct sr_dev_inst *dev;
	struct sr_dev_inst *di;
	int bFind = 0;

	di = lib_ctx.actived_device_instance;

	if (handle == NULL)
	{
		return SR_ERR_ARG;
	}

	if (di != NULL && di->handle == handle && ds_is_collecting())
	{
		sr_err("%s", "Device is collecting, can't remove it.");
		return SR_ERR_CALL_STATUS;
	}

	pthread_mutex_lock(&lib_ctx.mutext);

	for (l = lib_ctx.device_list; l; l = l->next)
	{
		dev = l->data;
		if (dev->handle == handle)
		{
			lib_ctx.device_list = g_slist_remove(lib_ctx.device_list, l->data);

			if (dev == lib_ctx.actived_device_instance)
			{
				lib_ctx.actived_device_instance = NULL;
			}
			if (lib_ctx.actived_device_info.handle == dev->handle)
			{
				lib_ctx.actived_device_info.handle = 0;
				lib_ctx.actived_device_info.name[0] = '\0';
			}

			destroy_device_instance(dev);
			bFind = 1;
			break;
		}
	}
	pthread_mutex_unlock(&lib_ctx.mutext);

	if (bFind == 0)
	{
		return SR_ERR_CALL_STATUS;
	}
	return SR_OK;
}

/**
 * Get the actived device info.
 * If the actived device is not exists, the handle filed will be set null.
 */
SR_API int ds_get_actived_device_info(struct ds_device_info *fill_info)
{
	if (fill_info == NULL)
	{
		return SR_ERR_ARG;
	}

	fill_info->handle = NULL;
	fill_info->name[0] = '\0';
	fill_info->dev_type = DEV_TYPE_UNKOWN;

	if (lib_ctx.actived_device_info.handle != NULL)
	{
		fill_info->handle = lib_ctx.actived_device_info.handle;
		strncpy(fill_info->name, lib_ctx.actived_device_info.name, sizeof(fill_info->name));
		fill_info->dev_type = lib_ctx.actived_device_info.dev_type;
		return SR_OK;
	}

	return SR_ERR_CALL_STATUS;
}

/**
 * Get actived device work model. mode list:LOGIC、ANALOG、DSO
 */
SR_API int ds_get_actived_device_mode()
{
	int mode = UNKNOWN_DSL_MODE;
	pthread_mutex_lock(&lib_ctx.mutext);

	if (lib_ctx.actived_device_instance != NULL)
	{
		mode = lib_ctx.actived_device_instance->mode;
	}
	pthread_mutex_unlock(&lib_ctx.mutext);

	return mode;
}

/**
 * Start collect data
 */
SR_API int ds_start_collect()
{
	int ret;
	struct sr_dev_inst *di;
	di = lib_ctx.actived_device_instance;

	sr_info("%s", "Start collect.");

	if (lib_ctx.collect_thread != NULL)
	{
		sr_err("%s", "Error,it's collecting!");
		return SR_ERR_CALL_STATUS;
	}
	if (di == NULL)
	{
		sr_err("%s", "Please set a current device first.");
		return SR_ERR_CALL_STATUS;
	}
	if (di->status == SR_ST_INITIALIZING)
	{
		sr_err("Error!The device is initializing.");
		return SR_ERR_CALL_STATUS;
	}
	if (ds_channel_is_enabled() == 0)
	{
		sr_err("%s", "There have no useable channel, unable to collect.");
		return SR_ERR_CALL_STATUS;
	}
	if (lib_ctx.data_forward_callback == NULL)
	{
		sr_err("%s", "Error! Data forwarding callback is not set, see \"ds_set_datafeed_callback()\".");
		return SR_ERR_CALL_STATUS;
	}

	lib_ctx.collect_stop_flag = 0;

	// Create new session.
	sr_session_new();

	ret = open_device_instance(di); // open device
	if (ret != SR_OK)
	{
		sr_err("%s", "Open device error!");
		return ret;
	}

	lib_ctx.collect_thread = g_thread_new("collect_run_proc", collect_run_proc, NULL);

	return SR_OK;
}

static void collect_run_proc()
{
	int ret;
	struct sr_dev_inst *di;
	di = lib_ctx.actived_device_instance;

	sr_info("%s", "Collect thread start.");

	if (di == NULL || di->driver == NULL || di->driver->dev_acquisition_start == NULL)
	{
		sr_err("%s", "The device cannot be used.");
		goto END;
	}

	ret = di->driver->dev_acquisition_start(di, (void *)di);
	if (ret != SR_OK)
	{
		sr_err("Failed to start acquisition of device in "
			   "running session: %d",
			   ret);
		goto END;
	}

	ret = sr_session_run();
	if (ret != SR_OK)
	{
		sr_err("%s", "Run session error!");
		lib_ctx.collect_thread = NULL;
		goto END;
	}

END:
	sr_info("%s", "Collect thread end.");

	lib_ctx.collect_thread = NULL;
}

/**
 * Stop collect data
 */
SR_API int ds_stop_collect()
{
	struct sr_dev_inst *di;
	di = lib_ctx.actived_device_instance;

	sr_info("%s", "Stop collect.");

	if (lib_ctx.collect_thread == NULL)
	{
		sr_err("%s", "It's not collecting now.");
		return SR_ERR_CALL_STATUS;
	}

	lib_ctx.collect_stop_flag = 1;

	// Stop current session.
	sr_session_stop();

	g_thread_join(lib_ctx.collect_thread); // Wait the collect thread ends.
	lib_ctx.collect_thread = NULL;

	close_device_instance(di);

	// Destroy current session.
	sr_session_destroy();

	return SR_OK;
}

int ds_trigger_is_enabled()
{
	GSList *l;
	struct sr_channel *p;

	if (lib_ctx.actived_device_instance != NULL)
	{
		for (l = lib_ctx.actived_device_instance->channels; l; l = l->next)
		{
			p = (struct sr_channel *)l->data;
			if (p->trigger && p->trigger[0] != '\0')
				return 1;
		}
	}
	return 0;
}

/**
 *  heck that at least one probe is enabled
 */
int ds_channel_is_enabled()
{
	GSList *l;
	struct sr_channel *p;

	if (lib_ctx.actived_device_instance != NULL)
	{
		for (l = lib_ctx.actived_device_instance->channels; l; l = l->next)
		{
			p = (struct sr_channel *)l->data;
			if (p->enabled)
				return 1;
		}
	}
	return 0;
}

/**-------------------public end ---------------*/

/**-------------------config -------------------*/
SR_API int ds_get_actived_device_config(const struct sr_channel *ch,
                         const struct sr_channel_group *cg,
                         int key, GVariant **data)
{
	if (lib_ctx.actived_device_instance == NULL){
		sr_err("%s", "Have no actived device.");
		return SR_ERR_CALL_STATUS;
	}

	return sr_config_get(lib_ctx.actived_device_instance->driver, 
				lib_ctx.actived_device_instance, 
				ch, 
				cg, 
				key, 
				data);
}

SR_API int ds_set_actived_device_config(const struct sr_channel *ch,
                         const struct sr_channel_group *cg,
                         int key, GVariant *data)
{
	if (lib_ctx.actived_device_instance == NULL){
		sr_err("%s", "Have no actived device.");
		return SR_ERR_CALL_STATUS;
	}
	
	return sr_config_set(
				lib_ctx.actived_device_instance, 
				ch, 
				cg, 
				key, 
				data);
}

SR_API int ds_get_actived_device_config_list(const struct sr_channel_group *cg,
                          int key, GVariant **data)
{
	if (lib_ctx.actived_device_instance == NULL){
		sr_err("%s", "Have no actived device.");
		return SR_ERR_CALL_STATUS;
	}

	return sr_config_list(lib_ctx.actived_device_instance->driver,
				lib_ctx.actived_device_instance,
				cg, 
				key, 
				data);
}

SR_API const struct sr_config_info* ds_get_actived_device_config_info(int key)
{
	if (lib_ctx.actived_device_instance == NULL){
		sr_err("%s", "Have no actived device.");
		return SR_ERR_CALL_STATUS;
	}

	return sr_config_info_get(key);
}

SR_API const struct sr_config_info* ds_get_actived_device_config_info_by_name(const char *optname)
{
	if (lib_ctx.actived_device_instance == NULL){
		sr_err("%s", "Have no actived device.");
		return SR_ERR_CALL_STATUS;
	}
	return sr_config_info_name_get(optname);
}

SR_API int ds_get_actived_device_status(struct sr_status *status, gboolean prg)
{
	if (lib_ctx.actived_device_instance == NULL){
		sr_err("%s", "Have no actived device.");
		return SR_ERR_CALL_STATUS;
	}

	return sr_status_get(lib_ctx.actived_device_instance, status, prg);
}

SR_API struct sr_config *ds_new_config(int key, GVariant *data)
{
	return sr_config_new(key, data);
}

SR_API void ds_free_config(struct sr_config *src)
{
	sr_config_free(src);
}

/**-----------channel -------------*/
SR_API int ds_enable_device_channel(const struct sr_channel *ch, gboolean enable)
{
	if (lib_ctx.actived_device_instance == NULL){
		return SR_ERR_CALL_STATUS;
	}
	return sr_enable_device_channel(lib_ctx.actived_device_instance, ch, enable);
}

/**-------------------config end-------------------*/

/**-------------------internal function ---------------*/
/**
 * Check whether the USB device is in the device list.
 */
SR_PRIV int sr_usb_device_is_exists(libusb_device *usb_dev)
{
	GList *l;
	struct sr_dev_inst *dev;
	int bFind = 0;

	if (usb_dev == NULL)
	{
		sr_err("%s", "sr_usb_device_is_exists(), @usb_dev is null.");
		return 0;
	}

	pthread_mutex_lock(&lib_ctx.mutext);

	for (l = lib_ctx.device_list; l; l = l->next)
	{
		dev = l->data;
		if (dev->handle == (ds_device_handle)usb_dev)
		{
			bFind = 1;
		}
	}

	pthread_mutex_unlock(&lib_ctx.mutext);

	return bFind;
}

/**
 * Forward the data.
 */
SR_PRIV int ds_data_forward(const struct sr_dev_inst *sdi,
							const struct sr_datafeed_packet *packet)
{
	if (!sdi)
	{
		sr_err("%s: sdi was NULL", __func__);
		return SR_ERR_ARG;
	}

	if (!packet)
	{
		sr_err("%s: packet was NULL", __func__);
		return SR_ERR_ARG;
	}

	if (lib_ctx.data_forward_callback != NULL && lib_ctx.collect_stop_flag == 0)
	{
		lib_ctx.data_forward_callback(sdi, packet);
		return SR_OK;
	}
	return SR_ERR;
}

SR_PRIV int current_device_acquisition_stop()
{
	struct sr_dev_inst *di;
	di = lib_ctx.actived_device_instance;
	if (di != NULL && di->driver && di->driver->dev_acquisition_stop)
	{
		return di->driver->dev_acquisition_stop(di, (void *)di);
	}
	return SR_ERR;
}

/**
 * Check if the device is collecting.
 */
SR_API int ds_is_collecting()
{
	if (lib_ctx.collect_thread != NULL)
	{
		return 1;
	}
	return 0;
}

/**--------------------internal function end-----------*/

/**-------------------private function ---------------*/

static int update_device_handle(struct libusb_device *old_dev, struct libusb_device *new_dev)
{
	GList *l;
	struct sr_dev_inst *dev;
	struct sr_usb_dev_inst *usb_dev_info;
	uint8_t bus;
	uint8_t address;
	int bFind = 0;

	pthread_mutex_lock(&lib_ctx.mutext);

	for (l = lib_ctx.device_list; l; l = l->next)
	{
		dev = l->data;
		usb_dev_info = dev->conn;
		if (usb_dev_info != NULL && dev->handle == (ds_device_handle)old_dev)
		{
			// Release the old device and the resource.
			if (dev == lib_ctx.actived_device_instance)
			{
				sr_info("%s", "Release the old device's resource.");
				close_device_instance(dev);
			}

			bus = libusb_get_bus_number(new_dev);
			address = libusb_get_device_address(new_dev);
			usb_dev_info->usb_dev = new_dev;
			usb_dev_info->bus = bus;
			usb_dev_info->address = address;
			dev->handle = new_dev;
			bFind = 1;

			// Reopen the device.
			if (dev == lib_ctx.actived_device_instance)
			{
				sr_info("%s", "Reopen the current device.");
				open_device_instance(dev);
			}
			break;
		}
	}

	pthread_mutex_unlock(&lib_ctx.mutext);

	if (bFind)
	{
		return SR_OK;
	}

	return SR_ERR;
}

static void hotplug_event_listen_callback(struct libusb_context *ctx, struct libusb_device *dev, int event)
{
	int bDone = 0;

	if (dev == NULL)
	{
		sr_err("%s", "hotplug_event_listen_callback(), @dev is null.");
		return;
	}

	if (event == USB_EV_HOTPLUG_ATTACH)
	{
		sr_info("One device attached,handle:%p", dev);

		if (lib_ctx.is_waitting_reconnect)
		{
			if (lib_ctx.attach_device_handle != NULL)
			{
				sr_err("One attached device haven't processed complete,handle:%p",
					   lib_ctx.attach_device_handle);
			}

			if (lib_ctx.detach_device_handle == NULL)
			{
				sr_err("%s", "The detached device handle is null, but the status is waitting for reconnect.");
			}
			else
			{
				if (update_device_handle(lib_ctx.detach_device_handle, dev) != SR_OK)
				{
					bDone = 1;
					sr_info("%s", "One device loose contact, but it reconnect success.");
				}
				else
				{
					sr_err("Update device handle error! can't find the old.");
				}
				lib_ctx.detach_device_handle = NULL;
			}
		}
		if (bDone == 0)
		{
			lib_ctx.attach_event_flag = 1; // Is a new device attched.
			lib_ctx.attach_device_handle = dev;
		}
		lib_ctx.is_waitting_reconnect = 0;
	}
	else if (event == USB_EV_HOTPLUG_DETTACH)
	{
		sr_info("One device detached,handle:%p", dev);

		if (lib_ctx.detach_device_handle != NULL)
		{
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
	else
	{
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

	sr_info("%s", "Process device attach event.");

	if (lib_ctx.attach_device_handle == NULL)
	{
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
			if (dev_list != NULL)
			{
				sr_info("Get new device list by driver \"%s\"", dr->name);

				pthread_mutex_lock(&lib_ctx.mutext);
				cur_list = lib_ctx.device_list;

				for (l = dev_list; l; l = l->next)
				{
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

	// Tell user one new device attched, and the list is updated.

	if (num > 0)
	{
		post_event_async(DS_EV_NEW_DEVICE_ATTACH);
	}

	lib_ctx.attach_device_handle = NULL;
}

static void process_detach_event()
{
	libusb_device *ev_dev;
	int ev = DS_EV_INACTIVE_DEVICE_DETACH;
	GList *l;
	struct sr_dev_inst *dev;
	struct sr_dev_driver *driver_ins;

	sr_info("%s", "Process device detach event.");

	ev_dev = lib_ctx.detach_device_handle;
	if (ev_dev == NULL)
	{
		sr_err("%s", "The detached device handle is null.");
		return;
	}
	lib_ctx.detach_device_handle = NULL;

	pthread_mutex_lock(&lib_ctx.mutext);

	for (l = lib_ctx.device_list; l; l = l->next)
	{
		dev = l->data;

		if (dev->handle == (ds_device_handle)ev_dev)
		{
			if (lib_ctx.actived_device_instance != dev && ds_is_collecting()){
				sr_info("%s", "The actived device is detached, will stop collect thread.");
				ds_stop_collect();
			}

			// Found the device, and remove it.
			lib_ctx.device_list = g_slist_remove(lib_ctx.device_list, l->data);
			destroy_device_instance(dev);
			if (dev == lib_ctx.actived_device_instance)
			{
				lib_ctx.actived_device_instance = NULL;
			}
			break;
		}
	}
	pthread_mutex_unlock(&lib_ctx.mutext);

	if (ev_dev == lib_ctx.actived_device_info.handle)
		ev = DS_EV_CURRENT_DEVICE_DETACH;

	// Tell user a new device detached, and the list is updated.
	post_event_async(ev);
}

static void usb_hotplug_process_proc()
{
	sr_info("%s", "Hotplug thread start!");

	while (!lib_ctx.lib_exit_flag)
	{
		sr_hotplug_wait_timout(lib_ctx.sr_ctx);

		if (lib_ctx.attach_event_flag)
		{
			process_attach_event();
			lib_ctx.attach_event_flag = 0;
		}
		if (lib_ctx.detach_event_flag)
		{
			process_detach_event();
			lib_ctx.detach_event_flag = 0;
		}

		_sleep(100);

		if (lib_ctx.is_waitting_reconnect)
		{
			lib_ctx.check_reconnect_times++;
			// 500ms
			if (lib_ctx.check_reconnect_times == 5)
			{
				// Device loose contact,wait for it reconnection timeout.
				lib_ctx.detach_event_flag = 1; // use detach event
				lib_ctx.is_waitting_reconnect = 0;
			}
		}
	}

	if (lib_ctx.callback_thread_count > 0)
	{
		sr_info("%d callback thread is actived, waiting all ends...", lib_ctx.callback_thread_count);
	}

	// Wait all callback thread end.
	while (lib_ctx.callback_thread_count > 0)
	{
		_sleep(100);
	}

	sr_info("%s", "Hotplug thread end!");
}

static void destroy_device_instance(struct sr_dev_inst *dev)
{
	if (dev == NULL || dev->driver == NULL)
	{
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

static void close_device_instance(struct sr_dev_inst *dev)
{
	if (dev == NULL || dev->driver == NULL)
	{
		sr_err("%s", "close_device_instance() argument error.");
		return;
	}
	struct sr_dev_driver *driver_ins;
	driver_ins = dev->driver;

	if (driver_ins->dev_close)
		driver_ins->dev_close(dev);
}

static int open_device_instance(struct sr_dev_inst *dev)
{
	if (dev == NULL || dev->driver == NULL)
	{
		sr_err("%s", "open_device_instance() argument error.");
		return SR_ERR_ARG;
	}
	struct sr_dev_driver *driver_ins;
	driver_ins = dev->driver;

	if (driver_ins->dev_open)
	{
		driver_ins->dev_open(dev);
		return SR_OK;
	}

	return SR_ERR_CALL_STATUS;
}

static void post_event_proc(int event)
{
	if (lib_ctx.event_callback != NULL)
	{
		lib_ctx.event_callback(event);
	}

	pthread_mutex_lock(&lib_ctx.mutext);
	lib_ctx.callback_thread_count--;
	;
	pthread_mutex_unlock(&lib_ctx.mutext);
}

static void post_event_async(int event)
{
	pthread_mutex_lock(&lib_ctx.mutext);
	lib_ctx.callback_thread_count++;
	pthread_mutex_unlock(&lib_ctx.mutext);

	g_thread_new("callback_thread", post_event_proc, event);
}

/**-------------------private function end---------------*/