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

/*
*	example:
*  	xlog_context *ctx = xlog_new();
*	xlog_writer *wr = xlog_create_writer(ctx, "module name");
*	xlog_err(wr, "count:%d", 100);
*	xlog_free(ctx); //free the context, all writer will can't to use
*/

#ifndef	_X_LOG_H_
#define _X_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _WIN32
#define XLOG_API __attribute__((visibility("default")))
#else
#define XLOG_API
#endif

/** loglevels. */
enum xlog_level_code
{
	XLOG_LEVEL_NONE = 0, /**< Output no messages at all. */
	XLOG_LEVEL_ERR  = 1, /**< Output error messages. */
	XLOG_LEVEL_WARN = 2, /**< Output warnings. */
	XLOG_LEVEL_INFO = 3, /**< Output informational messages. */
	XLOG_LEVEL_DBG  = 4, /**< Output debug messages. */
	XLOG_LEVEL_DETAIL = 5, /**< Output detailed messages. */
};

/*
	a log module instance.
*/
struct xlog_context;
typedef struct xlog_context xlog_context;

struct xlog_writer;
typedef struct xlog_writer xlog_writer;

/**
 * 	define log data receiver type
*/
typedef void (*xlog_receive_callback)(const char *data, int length);

/*
	create a log context, the console is default log receiver
*/
XLOG_API xlog_context* xlog_new();

/*
	create a log context
*/
XLOG_API xlog_context* xlog_new2(int bConsole);

/**
 * 	free a log context, return 0 if success.
 * 	and all xlog_writer will be can't to use.
 */
XLOG_API void xlog_free(xlog_context* ctx);

/**
 * 	append a log data receiver, return 0 if success.
 */
XLOG_API int xlog_add_receiver(xlog_context* ctx, xlog_receive_callback rev, int *out_index);

/**
 * 	append a log data receiver, return 0 if success.
 * 	the log data will be writed to file.
 */
XLOG_API int xlog_add_receiver_from_file(xlog_context* ctx, const char *file_path, int *out_index, int bAppend);

/**
 * Clear the log file, and reopen from the path.
*/
XLOG_API int xlog_reset_log_file(xlog_context* ctx, int receiver_index, const char *file_path);

/**
 * 	remove a log data receiver,return 0 if success.
 */
XLOG_API int xlog_remove_receiver_by_index(xlog_context* ctx, int index);

/**
 * Set the log receiver enable to disable.
*/
XLOG_API int xlog_set_receiver_enable(xlog_context* ctx, int index, int bEnable);

/**
 * 	clear all receiver,return 0 if success.
 */
XLOG_API int xlog_clear_all_receiver(xlog_context* ctx);

/**
 * 	get last error.
 */
XLOG_API const char* xlog_get_error(xlog_context* ctx);

/**
 * 	set the log level,return 0 if success. see enum xlog_level_code
 */
XLOG_API int xlog_set_level(xlog_context* ctx, int level);

/**
 * create a new writer
 * use free to delete the returns object.
 */
XLOG_API xlog_writer* xlog_create_writer(xlog_context* ctx, const char *domain);

/**
 * free a log writer
 */
XLOG_API void xlog_free_writer(xlog_writer *wr);

/**
 * 	set the main prefix string, return 0 if success.
 */
XLOG_API int xlog_set_domain(xlog_writer* wr, const char *domain);


//-------------------------------------------------print api

/**
 * print a error message, return 0 if success.
 */
XLOG_API int xlog_err(xlog_writer *wr, const char *format, ...);

/**
 * print a warning message, return 0 if success.
 */
XLOG_API int xlog_warn(xlog_writer *wr, const char *format, ...);

/**
 * print a informational message, return 0 if success.
 */
XLOG_API int xlog_info(xlog_writer *wr, const char *format, ...);

/**
 * print a debug message, return 0 if success.
 */
XLOG_API int xlog_dbg(xlog_writer *wr, const char *format, ...);

/**
 * print a detailed message, return 0 if success.
 */
XLOG_API int xlog_detail(xlog_writer *wr, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
