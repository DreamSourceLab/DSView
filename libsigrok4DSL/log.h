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

#ifndef _SR_LOG_H_
#define _SR_LOG_H_

#include <log/xlog.h>
#include "libsigrok.h"

extern xlog_writer *sr_log;

/**
 * Init a private log context
 */
SR_PRIV void sr_log_init();

/**
 * Destroy the private log context
 */
SR_PRIV void sr_log_uninit();

/**
 * Use a shared context, and drop the private log context
 */
SR_API void ds_log_set_context(xlog_context *ctx);

/**
 * Set the private log context level
 */
SR_API void ds_log_level(int level);

#define LOG_PREFIX "" 
#define sr_err(fmt, args...) xlog_err(sr_log, LOG_PREFIX fmt, ## args)
#define sr_warn(fmt, args...) xlog_warn(sr_log, LOG_PREFIX fmt, ## args)
#define sr_info(fmt, args...) xlog_info(sr_log, LOG_PREFIX fmt, ## args)
#define sr_dbg(fmt, args...) xlog_dbg(sr_log, LOG_PREFIX fmt, ## args)
#define sr_detail(fmt, args...) xlog_detail(sr_log, LOG_PREFIX fmt, ## args)

#endif