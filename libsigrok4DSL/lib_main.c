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

struct device_all_info
{
	struct sr_device_info _base_info;
};

#define SR_DEVICE_MAX_COUNT 100

static char DS_RES_PATH[500] = {0}; 
static struct sr_context *var_sr_context = NULL;
static libsigrok_event_callback_t *var_event_callback = NULL;
static struct  device_all_info* var_device_array[SR_DEVICE_MAX_COUNT] = {0};
static int var_cur_device_count = 0;
 
 //----------------------------private function----------------

 /**
  * Free device resource
 */
 static sr_free_device(struct device_all_info *dev)
 {
	if (dev){
		free(dev);
	}
 }

/**
 * Must call first
 */
SR_API int sr_lib_init()
{
	int ret = 0;
	struct sr_dev_driver **drivers = NULL;
	struct sr_dev_driver **dr = NULL;

	ret = sr_init(&var_sr_context);
    if (ret != SR_OK){
		return ret;
	}

	// Initialise all libsigrok drivers
	drivers = sr_driver_list();
	for (dr = drivers; *dr; dr++) {
		if (sr_driver_init(var_sr_context, *dr) != SR_OK) {
			sr_err("Failed to initialize driver '%s'", (*dr)->name);
			return SR_ERR;
		}
	}

	return SR_OK;
}

/**
 * Free all resource before program exits
 */
SR_API int sr_lib_exit()
{    
	struct sr_dev_driver **drivers = NULL;
	struct sr_dev_driver **dr = NULL;
	int i = 0;

	// free all device
	for (i=0; i<var_cur_device_count; i++){
		sr_free_device(var_device_array[i]);
		var_device_array[i] = NULL;
	}
   
	// Clear all the drivers
    drivers = sr_driver_list();
    for (dr = drivers; *dr; dr++){
		sr_dev_clear(*dr);
	}

	  if (var_sr_context != NULL){
        
		if (sr_exit(var_sr_context) != SR_OK)
			sr_err("%s", "call sr_exit error");
        var_sr_context = NULL;
    }

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

SR_PRIV char* sr_get_firmware_res_path()
{
	return DS_RES_PATH;
}

/**
 * Set event callback, event type see enum libsigrok_event_type
 */
SR_API void sr_set_event_callback(libsigrok_event_callback_t *cb)
{
    var_event_callback = cb;
}

/**
 * When device attached or detached, scan all devices to get the new list.
 * The current list will be changed
 */
SR_API int sr_device_scan_list()
{
	if (var_sr_context == NULL){
		sr_err("%s", "Must call 'sr_lib_init' first.");
		return SR_ERR;
	}
}
