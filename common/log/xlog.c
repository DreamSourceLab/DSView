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

#include "xlog.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>

#define RECEIVER_MAX_COUNT  10
#define LOG_MAX_LENGTH      1000

enum xlog_receiver_type{ 
    RECEIVER_TYPE_CONSOLE = 0,
    RECEIVER_TYPE_FILE = 1,
    RECEIVER_TYPE_CALLBACK = 2,
};

struct xlog_receiver_info;

typedef void (*xlog_print_func)(struct xlog_receiver_info *info, const char *domain, const char *prefix, const char *format, va_list args);

struct xlog_receiver_info
{
    int _type; //see enum xlog_receiver_type  
    FILE    *_file;
    xlog_print_func _fn; // print function
    xlog_receiver _rev; //user callback
};

struct xlog_context
{
    struct xlog_receiver_info _receivers[RECEIVER_MAX_COUNT];
    int     _log_level;    
    char    _error[50];
    int     _count;
};

struct xlog_writer{
    char    _domain[20];
    xlog_context    *_ctx;
};

/**
 * the default mode process
 */
static void print_to_console(struct xlog_receiver_info *info, const char *domain, const char *prefix, const char *format, va_list args)
{
    (void)info;

    if (domain && *domain){
        fprintf(stderr, "%s", domain);
        fprintf(stderr, ": ");
    }
    if (prefix){
        fprintf(stderr, "%s", prefix);
        fprintf(stderr, ": ");
    }
    
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");
}
 
/**
 * the file mode process
 */
static void print_to_file(struct xlog_receiver_info *info, const char *domain, const char *prefix, const char *format, va_list args)
{
    char buf[LOG_MAX_LENGTH + 1];
    int fmtl;
    int wr=0;
    int strl;

    if (info->_file == NULL){
        return;
    }

    if (domain && *domain){
        strl = strlen(domain);
        strcpy(buf + wr, domain);
        wr += strl;
        strcpy(buf + wr, ": ");
        wr += 2;
    }
    if (prefix && *prefix){
        strl = strlen(prefix);
        strcpy(buf + wr, prefix);
        wr += strl;
        strcpy(buf + wr, ": ");
        wr += 2;
    } 

    fmtl = vsnprintf(buf + wr, LOG_MAX_LENGTH - wr - 1, format, args);
    wr += fmtl;
    *(buf + wr) = '\n';
    wr += 1;

	fwrite(buf, wr, 1, info->_file);
    fflush(info->_file);
}

/**
 * the callback mode process
 */
static void print_to_user_callback(struct xlog_receiver_info *info, const char *domain, const char *prefix, const char *format, va_list args)
{
    char buf[LOG_MAX_LENGTH + 1];
    int fmtl;
    int wr=0;
    int strl;

    if (info->_rev == NULL){
        return;
    }

    if (domain && *domain){
        strl = strlen(domain);
        strcpy(buf + wr, domain);
        wr += strl;
        strcpy(buf + wr, ": ");
        wr += 2;
    }
    if (prefix && *prefix){
        strl = strlen(prefix);
        strcpy(buf + wr, prefix);
        wr += strl;
        strcpy(buf + wr, ": ");
        wr += 2;
    }

    fmtl = vsnprintf(buf + wr, LOG_MAX_LENGTH - wr - 1, format, args);
    wr += fmtl;
    *(buf + wr) = '\n';
    wr += 1;

	info->_rev(buf, wr);
}

/**
 * create a log context, the console is default log receiver
*/
XLOG_API xlog_context* xlog_new()
{
    int i=0;
    xlog_context *ctx = (xlog_context*)malloc(sizeof(xlog_context));
    if (ctx != NULL){       
        for(i=0; i< RECEIVER_MAX_COUNT; i++){
            ctx->_receivers[i]._fn = NULL;
            ctx->_receivers[i]._file = NULL;
        }
        ctx->_receivers[0]._fn = print_to_console;
        ctx->_receivers[0]._type = RECEIVER_TYPE_CONSOLE;
        ctx->_log_level = XLOG_INFO;
        ctx->_count = 1;
    }
    return ctx;
}

/**
 * 	free a log context, return 0 if success.
 */
XLOG_API void xlog_free(xlog_context* ctx)
{   
    int i=0;

    if (ctx != NULL){
        for (i = 0; i < ctx->_count; i++)
        {
            if (ctx->_receivers[i]._file != NULL 
                    && ctx->_receivers[i]._type == RECEIVER_TYPE_FILE)
            {
                fclose(ctx->_receivers[i]._file); // close the log file
                ctx->_receivers[i]._file = NULL;
            }
        }

        free(ctx); 
    } 
}

/**
 * 	append a log data receiver, return 0 if success.
 */
XLOG_API int xlog_add_receiver(xlog_context* ctx, xlog_receiver rev, int *out_index)
{
    int i;

    if (ctx == NULL){
        return -1;
    }
    if (rev == NULL){
        strcpy(ctx->_error, "param @rev is null");
        return -1;
    }

    for (i = 0; i < RECEIVER_MAX_COUNT; i++){
        if (ctx->_receivers[i]._fn == NULL)
            break;
    }

    if (i == RECEIVER_MAX_COUNT){
        strcpy(ctx->_error, "receiver count is full");
        return -1;
    }

    ctx->_receivers[i]._type = RECEIVER_TYPE_CALLBACK;
    ctx->_receivers[i]._rev = rev;
    ctx->_receivers[i]._fn = print_to_user_callback;
    ctx->_count++;

    if (out_index)
        *out_index = i;
    
    return 0;
}

/**
 * 	append a log data receiver, return 0 if success.
 * 	the log data will be writed to file.
 */
XLOG_API int xlog_add_receiver_from_file(xlog_context* ctx, const char *file_path, int *out_index)
{
    int i;
    FILE *fh = NULL;

    if (ctx == NULL){
        return -1;
    }
    if (!file_path || *file_path == 0){
        strcpy(ctx->_error, "param @file_path is null");
        return -1;
    }

    for (i = 0; i < RECEIVER_MAX_COUNT; i++){
        if (ctx->_receivers[i]._fn == NULL)
            break;
    }

    if (i == RECEIVER_MAX_COUNT){
        strcpy(ctx->_error, "receiver list is full");
        return -1;
    }

    fh = fopen(file_path, "a+");
    if (fh == NULL){
        strcpy(ctx->_error, "open file error");
        return -1;
    }

    ctx->_receivers[i]._type = RECEIVER_TYPE_FILE;
    ctx->_receivers[i]._fn = print_to_file;
    ctx->_receivers[i]._file = fh;
    ctx->_count++;

    if (out_index)
        *out_index = i;
    
    return 0;
}

/**
 * 	remove a log data receiver,return 0 if success.
 */
XLOG_API int xlog_remove_receiver_by_index(xlog_context* ctx, int index)
{
    if (ctx == NULL){
        return -1;
    }
    if (index < 0 || index >= ctx->_count){
        strcpy(ctx->_error, "index out of range");
        return -1;
    } 

    ctx->_receivers[index]._fn = NULL;
    index++;

    while (index < RECEIVER_MAX_COUNT)
    {
        ctx->_receivers[index-1]._fn = ctx->_receivers[index]._fn;
        ctx->_receivers[index-1]._type = ctx->_receivers[index]._type;
        ctx->_receivers[index-1]._rev = ctx->_receivers[index]._rev;
        ctx->_receivers[index-1]._file = ctx->_receivers[index]._file;
    }
    ctx->_count--;
    
    return 0;    
}

/**
 * 	clear all receiver,return 0 if success.
 */
XLOG_API int xlog_clear_all_receiver(xlog_context* ctx)
{
    int i;
    if (ctx == NULL){
        return -1;
    }

    for (i = 0; i < RECEIVER_MAX_COUNT; i++){
        ctx->_receivers[i]._fn = NULL;
    }
    ctx->_count = 0;

    return 0;    
}

/**
 * 	get last error.
 */
XLOG_API const char* xlog_get_error(xlog_context* ctx)
{
    if (ctx != NULL && ctx->_error[0] != 0){
        return ctx->_error;
    }
    return NULL;
}


/**
 * 	set the log level,return 0 if success. see enum xlog_level_code
 */
XLOG_API int xlog_set_level(xlog_context* ctx, int level)
{
    if (ctx == NULL){
        return -1;
    }
    if (level < XLOG_NONE || level > XLOG_SPEW){
        strcpy(ctx->_error, "@level value must between 0 and 5");
        return -1;
    }
    ctx->_log_level = level;

    return 0;    
}

//-------------------------------------------------print api

/**
 * create a new writer
 */
XLOG_API xlog_writer* xlog_create_writer(xlog_context* ctx, const char *domain)
{
    xlog_writer *wr = NULL;

    if (ctx != NULL){
        wr = (xlog_writer*)malloc(sizeof(xlog_writer));
        wr->_ctx = ctx;
        xlog_set_domain(wr, domain);
        return wr;
    }    
    return NULL;
}

/**
 * free a log writer
 */
XLOG_API void xlog_free_writer(xlog_writer *wr)
{
    if (wr != NULL){
        free(wr);
    }
}

/**
 * 	set the main prefix string, return 0 if success.
 */
XLOG_API int xlog_set_domain(xlog_writer* wr, const char *domain)
{
    if (wr == NULL){
        return -1;
    }
    wr->_domain[0] = '\0';

    if (domain && *domain){
        strncpy(wr->_domain, domain, sizeof(wr->_domain));
    }
    return 0;
}

/**
 * print a log data, return 0 if success.
 */
XLOG_API int xlog_print(xlog_writer *wr, int level, const char *prefix, const char *format, ...)
{   
    int i;
    struct xlog_receiver_info *inf;
    va_list args;

    if (wr == NULL){
        return -1;
    }
    xlog_context *ctx = wr->_ctx;

    if (ctx == NULL || ctx->_log_level < level){
        return -1;
    }
    if (ctx->_count == 0){
        return -1;
    }

    for (i = 0; i < RECEIVER_MAX_COUNT; i++){
        inf = &ctx->_receivers[i];
        if (inf->_fn != NULL){
            va_start(args, format);
            inf->_fn(inf, wr->_domain, prefix, format, args);
            va_end(args);
        }
        else{
            break;
        }
    } 

    return 0;
}