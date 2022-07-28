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
static struct sr_context *g_sr_ctx = NULL;
 
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

	if (g_sr_ctx != NULL){
		return SR_ERR_HAVE_DONE;
	}

	ret = sr_init(&g_sr_ctx);
    if (ret != SR_OK){
		return ret;
	}

	// Initialise all libsigrok drivers
	drivers = sr_driver_list();
	for (dr = drivers; *dr; dr++) {
		if (sr_driver_init(g_sr_ctx, *dr) != SR_OK) {
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
	struct sr_dev_driver *driver_ins;
	GSList *l;
	struct sr_dev_inst *dev;
  
	if (g_sr_ctx == NULL){
		return SR_ERR_HAVE_DONE;
	}

	// Release all device
	for (l=g_sr_ctx->deiveList; l; l = l->next)
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
	g_safe_free_list(g_sr_ctx->deiveList);

	if (sr_exit(g_sr_ctx) != SR_OK){
		sr_err("%s", "call sr_exit error");
	}
    g_sr_ctx = NULL;

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
    if (g_sr_ctx == NULL)
		sr_lib_init();
	
	if (g_sr_ctx != NULL){
		g_sr_ctx->event_callback = cb;
	}
}

