/*
 * This file is part of the libsigrokdecode project.
 *
 * Copyright (C) 2011-2012 Uwe Hermann <uwe@hermann-uwe.de>
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

#include "config.h"
#include "libsigrokdecode-internal.h" /* First, so we avoid a _POSIX_C_SOURCE warning. */
#include "libsigrokdecode.h"
#include <stdarg.h>
#include <stdio.h>
#include <glib/gprintf.h>

/**
 * @file
 *
 * Controlling the libsigrokdecode message logging functionality.
 */

/**
 * @defgroup grp_logging Logging
 *
 * Controlling the libsigrokdecode message logging functionality.
 *
 * @{
 */

/* Currently selected libsigrokdecode loglevel. Default: SRD_LOG_WARN. */
static int cur_loglevel = SRD_LOG_WARN; /* Show errors+warnings per default. */

/* Function prototype. */
static int srd_logv(void *cb_data, int loglevel, const char *format,
		    va_list args);

/* Pointer to the currently selected log callback. Default: srd_logv(). */
static srd_log_callback srd_log_cb = srd_logv;

/*
 * Pointer to private data that can be passed to the log callback.
 * This can be used (for example) by C++ GUIs to pass a "this" pointer.
 */
static void *srd_log_cb_data = NULL;

/**
 * Set the libsigrokdecode loglevel.
 *
 * This influences the amount of log messages (debug messages, error messages,
 * and so on) libsigrokdecode will output. Using SRD_LOG_NONE disables all
 * messages.
 *
 * Note that this function itself will also output log messages. After the
 * loglevel has changed, it will output a debug message with SRD_LOG_DBG for
 * example. Whether this message is shown depends on the (new) loglevel.
 *
 * @param loglevel The loglevel to set (SRD_LOG_NONE, SRD_LOG_ERR,
 *                 SRD_LOG_WARN, SRD_LOG_INFO, SRD_LOG_DBG, or SRD_LOG_SPEW).
 *
 * @return SRD_OK upon success, SRD_ERR_ARG upon invalid loglevel.
 *
 * @since 0.1.0
 */
SRD_API int srd_log_loglevel_set(int loglevel)
{
	if (loglevel < SRD_LOG_NONE || loglevel > SRD_LOG_SPEW) {
		srd_err("Invalid loglevel %d.", loglevel);
		return SRD_ERR_ARG;
	}

	cur_loglevel = loglevel;

	srd_dbg("libsigrokdecode loglevel set to %d.", loglevel);

	return SRD_OK;
}

/**
 * Get the libsigrokdecode loglevel.
 *
 * @return The currently configured libsigrokdecode loglevel.
 *
 * @since 0.1.0
 */
SRD_API int srd_log_loglevel_get(void)
{
	return cur_loglevel;
}

/**
 * Set the libsigrokdecode log callback to the specified function.
 *
 * @param cb Function pointer to the log callback function to use.
 *           Must not be NULL.
 * @param cb_data Pointer to private data to be passed on. This can be used
 *                by the caller to pass arbitrary data to the log functions.
 *                This pointer is only stored or passed on by libsigrokdecode,
 *                and is never used or interpreted in any way. The pointer
 *                is allowed to be NULL if the caller doesn't need/want to
 *                pass any data.
 *
 * @return SRD_OK upon success, SRD_ERR_ARG upon invalid arguments.
 *
 * @since 0.3.0
 */
SRD_API int srd_log_callback_set(srd_log_callback cb, void *cb_data)
{
	if (!cb) {
		srd_err("log: %s: cb was NULL", __func__);
		return SRD_ERR_ARG;
	}

	/* Note: 'cb_data' is allowed to be NULL. */

	srd_log_cb = cb;
	srd_log_cb_data = cb_data;

	return SRD_OK;
}

/**
 * Set the libsigrokdecode log callback to the default built-in one.
 *
 * Additionally, the internal 'srd_log_cb_data' pointer is set to NULL.
 *
 * @return SRD_OK upon success, a (negative) error code otherwise.
 *
 * @since 0.1.0
 */
SRD_API int srd_log_callback_set_default(void)
{
	/*
	 * Note: No log output in this function, as it should safely work
	 * even if the currently set log callback is buggy/broken.
	 */
	srd_log_cb = srd_logv;
	srd_log_cb_data = NULL;

	return SRD_OK;
}

static int srd_logv(void *cb_data, int loglevel, const char *format,
		    va_list args)
{
	/* This specific log callback doesn't need the void pointer data. */
	(void)cb_data;

	/* Only output messages of at least the selected loglevel(s). */
	if (loglevel > cur_loglevel)
		return SRD_OK;

	if (fputs("srd: ", stderr) < 0
			|| g_vfprintf(stderr, format, args) < 0
			|| putc('\n', stderr) < 0)
		return SRD_ERR;

	return SRD_OK;
}

/** @private */
SRD_PRIV int srd_log(int loglevel, const char *format, ...)
{
	int ret;
	va_list args;

	va_start(args, format);
	ret = srd_log_cb(srd_log_cb_data, loglevel, format, args);
	va_end(args);

	return ret;
}

/** @} */
