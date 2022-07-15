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

struct sr_context *lib_sr_context = NULL;
char DS_RES_PATH[500] = {0}; 
libsigrok_event_callback_t *lib_event_callback = NULL;

SR_API int sr_lib_init()
{
    return sr_init(&lib_sr_context);
}

SR_API int sr_lib_exit()
{   
    if (lib_sr_context != NULL){
        return sr_exit(lib_sr_context);
        lib_sr_context = NULL;
    }
    return SR_OK;
}

void sr_set_firmware_resource_dir(const char *dir)
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
 * event type see enum libsigrok_event_type
 */
SR_API void sr_set_event_callback(libsigrok_event_callback_t *cb)
{
    lib_event_callback = cb;
}