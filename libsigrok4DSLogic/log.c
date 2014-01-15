/*
 * This file is part of the libsigrok project.
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

#include <stdarg.h>
#include <stdio.h>
#include "libsigrok.h"
#include "libsigrok-internal.h"

/**
 * @file
 *
 * Controlling the libsigrok message logging functionality.
 */

/**
 * @defgroup grp_logging Logging
 *
 * Controlling the libsigrok message logging functionality.
 *
 * @{
 */

/* Currently selected libsigrok loglevel. Default: SR_LOG_WARN. */
static int sr_loglevel = SR_LOG_WARN; /* Show errors+warnings per default. */

/* Function prototype. */
static int sr_logv(void *cb_data, int loglevel, const char *format,
		   va_list args);

/* Pointer to the currently selected log callback. Default: sr_logv(). */
static sr_log_callback_t sr_log_callback = sr_logv;

/*
 * Pointer to private data that can be passed to the log callback.
 * This can be used (for example) by C++ GUIs to pass a "this" pointer.
 */
static void *sr_log_callback_data = NULL;

/* Log domain (a short string that is used as prefix for all messages). */
/** @cond PRIVATE */
#define LOGDOMAIN_MAXLEN 30
#define LOGDOMAIN_DEFAULT "sr: "
/** @endcond */
static char sr_log_domain[LOGDOMAIN_MAXLEN + 1] = LOGDOMAIN_DEFAULT;

/**
 * Set the libsigrok loglevel.
 *
 * This influences the amount of log messages (debug messages, error messages,
 * and so on) libsigrok will output. Using SR_LOG_NONE disables all messages.
 *
 * Note that this function itself will also output log messages. After the
 * loglevel has changed, it will output a debug message with SR_LOG_DBG for
 * example. Whether this message is shown depends on the (new) loglevel.
 *
 * @param loglevel The loglevel to set (SR_LOG_NONE, SR_LOG_ERR, SR_LOG_WARN,
 *                 SR_LOG_INFO, SR_LOG_DBG, or SR_LOG_SPEW).
 *
 * @return SR_OK upon success, SR_ERR_ARG upon invalid loglevel.
 *
 * @since 0.1.0
 */
SR_API int sr_log_loglevel_set(int loglevel)
{
	if (loglevel < SR_LOG_NONE || loglevel > SR_LOG_SPEW) {
		sr_err("Invalid loglevel %d.", loglevel);
		return SR_ERR_ARG;
	}

	sr_loglevel = loglevel;

	sr_dbg("libsigrok loglevel set to %d.", loglevel);

	return SR_OK;
}

/**
 * Get the libsigrok loglevel.
 *
 * @return The currently configured libsigrok loglevel.
 *
 * @since 0.1.0
 */
SR_API int sr_log_loglevel_get(void)
{
	return sr_loglevel;
}

/**
 * Set the libsigrok logdomain string.
 *
 * @param logdomain The string to use as logdomain for libsigrok log
 *                  messages from now on. Must not be NULL. The maximum
 *                  length of the string is 30 characters (this does not
 *                  include the trailing NUL-byte). Longer strings are
 *                  silently truncated.
 *                  In order to not use a logdomain, pass an empty string.
 *                  The function makes its own copy of the input string, i.e.
 *                  the caller does not need to keep it around.
 *
 * @return SR_OK upon success, SR_ERR_ARG upon invalid logdomain.
 *
 * @since 0.1.0
 */
SR_API int sr_log_logdomain_set(const char *logdomain)
{
	if (!logdomain) {
		sr_err("log: %s: logdomain was NULL", __func__);
		return SR_ERR_ARG;
	}

	/* TODO: Error handling. */
	snprintf((char *)&sr_log_domain, LOGDOMAIN_MAXLEN, "%s", logdomain);

	sr_dbg("Log domain set to '%s'.", (const char *)&sr_log_domain);

	return SR_OK;
}

/**
 * Get the currently configured libsigrok logdomain.
 *
 * @return A copy of the currently configured libsigrok logdomain
 *         string. The caller is responsible for g_free()ing the string when
 *         it is no longer needed.
 *
 * @since 0.1.0
 */
SR_API char *sr_log_logdomain_get(void)
{
	return g_strdup((const char *)&sr_log_domain);
}

/**
 * Set the libsigrok log callback to the specified function.
 *
 * @param cb Function pointer to the log callback function to use.
 *           Must not be NULL.
 * @param cb_data Pointer to private data to be passed on. This can be used by
 *                the caller to pass arbitrary data to the log functions. This
 *                pointer is only stored or passed on by libsigrok, and is
 *                never used or interpreted in any way. The pointer is allowed
 *                to be NULL if the caller doesn't need/want to pass any data.
 *
 * @return SR_OK upon success, SR_ERR_ARG upon invalid arguments.
 *
 * @since 0.1.0
 */
SR_API int sr_log_callback_set(sr_log_callback_t cb, void *cb_data)
{
	if (!cb) {
		sr_err("log: %s: cb was NULL", __func__);
		return SR_ERR_ARG;
	}

	/* Note: 'cb_data' is allowed to be NULL. */

	sr_log_callback = cb;
	sr_log_callback_data = cb_data;

	return SR_OK;
}

/**
 * Set the libsigrok log callback to the default built-in one.
 *
 * Additionally, the internal 'sr_log_callback_data' pointer is set to NULL.
 *
 * @return SR_OK upon success, a negative error code otherwise.
 *
 * @since 0.1.0
 */
SR_API int sr_log_callback_set_default(void)
{
	/*
	 * Note: No log output in this function, as it should safely work
	 * even if the currently set log callback is buggy/broken.
	 */
	sr_log_callback = sr_logv;
	sr_log_callback_data = NULL;

	return SR_OK;
}

static int sr_logv(void *cb_data, int loglevel, const char *format, va_list args)
{
	int ret;

	/* This specific log callback doesn't need the void pointer data. */
	(void)cb_data;

	/* Only output messages of at least the selected loglevel(s). */
	if (loglevel > sr_loglevel)
		return SR_OK; /* TODO? */

	if (sr_log_domain[0] != '\0')
		fprintf(stderr, "%s", sr_log_domain);
	ret = vfprintf(stderr, format, args);
	fprintf(stderr, "\n");

	return ret;
}

/** @private */
SR_PRIV int sr_log(int loglevel, const char *format, ...)
{
	int ret;
	va_list args;

	va_start(args, format);
	ret = sr_log_callback(sr_log_callback_data, loglevel, format, args);
	va_end(args);

	return ret;
}

/** @private */
SR_PRIV int sr_spew(const char *format, ...)
{
	int ret;
	va_list args;

	va_start(args, format);
	ret = sr_log_callback(sr_log_callback_data, SR_LOG_SPEW, format, args);
	va_end(args);

	return ret;
}

/** @private */
SR_PRIV int sr_dbg(const char *format, ...)
{
	int ret;
	va_list args;

	va_start(args, format);
	ret = sr_log_callback(sr_log_callback_data, SR_LOG_DBG, format, args);
	va_end(args);

	return ret;
}

/** @private */
SR_PRIV int sr_info(const char *format, ...)
{
	int ret;
	va_list args;

	va_start(args, format);
	ret = sr_log_callback(sr_log_callback_data, SR_LOG_INFO, format, args);
	va_end(args);

	return ret;
}

/** @private */
SR_PRIV int sr_warn(const char *format, ...)
{
	int ret;
	va_list args;

	va_start(args, format);
	ret = sr_log_callback(sr_log_callback_data, SR_LOG_WARN, format, args);
	va_end(args);

	return ret;
}

/** @private */
SR_PRIV int sr_err(const char *format, ...)
{
	int ret;
	va_list args;

	va_start(args, format);
	ret = sr_log_callback(sr_log_callback_data, SR_LOG_ERR, format, args);
	va_end(args);

	return ret;
}

/** @} */
