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

#ifndef _SRD_LOG_H_
#define _SRD_LOG_H_

#include <log/xlog.h>
#include "libsigrokdecode.h"

extern xlog_writer *srd_log;

/**
 * Init a private log context
 */
SRD_PRIV void srd_log_init();

/**
 * Destroy the private log context
 */
SRD_PRIV void srd_log_uninit();

/**
 * Use a shared context, and drop the private log context
 */
SRD_API void srd_log_set_context(xlog_context *ctx);

/**
 * Set the private log context level
 */
SRD_API void srd_log_level(int level);

#define LOG_PREFIX "" 
#define srd_err(fmt, args...) xlog_err(srd_log, LOG_PREFIX fmt, ## args)
#define srd_warn(fmt, args...) xlog_warn(srd_log, LOG_PREFIX fmt, ## args)
#define srd_info(fmt, args...) xlog_info(srd_log, LOG_PREFIX fmt, ## args)
#define srd_dbg(fmt, args...) xlog_dbg(srd_log, LOG_PREFIX fmt, ## args)
#define srd_detail(fmt, args...) xlog_detail(srd_log, LOG_PREFIX fmt, ## args)

#endif