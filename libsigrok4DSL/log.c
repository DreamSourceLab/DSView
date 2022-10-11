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
 
 #include "log.h"

 #define LOG_DOMAIN  "sr"

xlog_writer *sr_log = 0;
static xlog_context *log_ctx = 0; //private log context
static int  is_private_log = 0;
static int  log_level_value = -1;

/**
 * Init a private log context
 */
SR_PRIV void sr_log_init()
{
    if (!log_ctx && !sr_log){
        log_ctx = xlog_new();
        sr_log = xlog_create_writer(log_ctx, LOG_DOMAIN);
        is_private_log = 1;

        if (log_level_value != -1){
            xlog_set_level(log_ctx, log_level_value);
        }
    }
}

/**
 * Destroy the private log context
 */
SR_PRIV void sr_log_uninit()
{
    if (is_private_log && log_ctx){
        xlog_free(log_ctx);
        log_ctx = 0;
        xlog_free_writer(sr_log);
        sr_log = 0;
        is_private_log = 0;
    }
}

/**
 * Use a shared context, and drop the private log context
 */
SR_API void ds_log_set_context(xlog_context *ctx)
{   
    if (ctx){
         sr_log_uninit();
         sr_log = xlog_create_writer(ctx, LOG_DOMAIN);

         if (log_level_value != -1){
             xlog_set_level(ctx, log_level_value);            
         }
    }
}

/**
 * Set the private log context level
 */
SR_API void ds_log_level(int level)
{   
    log_level_value = level;
    if (log_ctx)
    {
        xlog_set_level(log_ctx, level);
    }
}