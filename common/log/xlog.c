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
#include <pthread.h>

#define RECEIVER_MAX_COUNT  10
#define LOG_MAX_LENGTH      1000

enum xlog_receiver_type{ 
    RECEIVER_TYPE_CONSOLE = 0,
    RECEIVER_TYPE_FILE = 1,
    RECEIVER_TYPE_CALLBACK = 2,
};

struct xlog_receiver_info;

typedef void (*xlog_print_func)(struct xlog_receiver_info *info, const char *domain, const char *format, va_list args);

struct xlog_receiver_info
{
    int     _type; //see enum xlog_receiver_type
    int     _enable;
    FILE    *_file;
    xlog_print_func _fn; // print function
    xlog_receive_callback _rev; //user callback
};

struct xlog_context
{
    struct xlog_receiver_info _receivers[RECEIVER_MAX_COUNT];
    int     _log_level;    
    char    _error[50];
    int     _count;
    pthread_mutex_t _mutext;
};

struct xlog_writer{
    char    _domain[20];
    xlog_context    *_ctx;
};

/**
 * the default mode process
 */
static void print_to_console(struct xlog_receiver_info *info, const char *domain, const char *format, va_list args)
{
    (void)info;

    if (domain && *domain){
        fprintf(stderr, "%s", domain);
        fprintf(stderr, ": ");
    }
    
	vfprintf(stderr, format, args);
	fprintf(stderr, "\n");
    fflush(stderr);
}
 
/**
 * the file mode process
 */
static void print_to_file(struct xlog_receiver_info *info, const char *domain, const char *format, va_list args)
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
static void print_to_user_callback(struct xlog_receiver_info *info, const char *domain, const char *format, va_list args)
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

    fmtl = vsnprintf(buf + wr, LOG_MAX_LENGTH - wr - 1, format, args);
    wr += fmtl;
    *(buf + wr) = '\n';
    wr += 1;
 
	info->_rev(buf, wr);
}

static xlog_context* xlog_new_context(int bConsole)
{
    int i=0;
    xlog_context *ctx = (xlog_context*)malloc(sizeof(xlog_context));

    if (ctx != NULL){       
        for(i=0; i< RECEIVER_MAX_COUNT; i++){
            ctx->_receivers[i]._fn = NULL;
            ctx->_receivers[i]._file = NULL;
            ctx->_receivers[i]._enable = 0;
            ctx->_receivers[i]._rev = NULL;
            ctx->_receivers[i]._type = -1;
        }
        ctx->_count = 0;
        ctx->_log_level = XLOG_LEVEL_INFO;

        if (bConsole){
            ctx->_receivers[0]._fn = print_to_console;
            ctx->_receivers[0]._type = RECEIVER_TYPE_CONSOLE;
            ctx->_receivers[0]._enable = 1;  
            ctx->_count = 1;
        }        

        pthread_mutex_init(&ctx->_mutext, NULL);
    }
    
    return ctx;
}

/**
 * create a log context, the console is default log receiver
*/
XLOG_API xlog_context* xlog_new()
{
    return xlog_new_context(1);
}

/*
	create a log context
*/
XLOG_API xlog_context* xlog_new2(int bConsole)
{
    return xlog_new_context(bConsole);    
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

        pthread_mutex_destroy(&ctx->_mutext);

        free(ctx); 
    } 
}

/**
 * 	append a log data receiver, return 0 if success.
 */
XLOG_API int xlog_add_receiver(xlog_context* ctx, xlog_receive_callback rev, int *out_index)
{
    int i;

    if (ctx == NULL){
        return -1;
    }
    if (rev == NULL){
        strcpy(ctx->_error, "param @rev is null");
        return -1;
    }

    pthread_mutex_lock(&ctx->_mutext);

    for (i = 0; i < RECEIVER_MAX_COUNT; i++){
        if (ctx->_receivers[i]._fn == NULL)
            break;
    }

    if (i == RECEIVER_MAX_COUNT){
        strcpy(ctx->_error, "receiver count is full");
        pthread_mutex_unlock(&ctx->_mutext);
        return -1;
    }

    ctx->_receivers[i]._type = RECEIVER_TYPE_CALLBACK;
    ctx->_receivers[i]._rev = rev;
    ctx->_receivers[i]._fn = print_to_user_callback;
    ctx->_receivers[i]._enable = 1;
    ctx->_count++;

    if (out_index)
        *out_index = i;
    
    pthread_mutex_unlock(&ctx->_mutext);
    
    return 0;
}

/**
 * 	append a log data receiver, return 0 if success.
 * 	the log data will be writed to file.
 */
XLOG_API int xlog_add_receiver_from_file(xlog_context* ctx, const char *file_path, int *out_index, int bAppend)
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

    pthread_mutex_lock(&ctx->_mutext);

    for (i = 0; i < RECEIVER_MAX_COUNT; i++){
        if (ctx->_receivers[i]._fn == NULL)
            break;
    }

    if (i == RECEIVER_MAX_COUNT){
        strcpy(ctx->_error, "receiver list is full");
        pthread_mutex_unlock(&ctx->_mutext);
        return -1;
    }

    if (bAppend)
        fh = fopen(file_path, "a+");
    else
        fh = fopen(file_path, "w+");

    if (fh == NULL){
        strcpy(ctx->_error, "open file error");
        pthread_mutex_unlock(&ctx->_mutext);
        return -1;
    }

    ctx->_receivers[i]._type = RECEIVER_TYPE_FILE;
    ctx->_receivers[i]._fn = print_to_file;
    ctx->_receivers[i]._file = fh;
    ctx->_receivers[i]._enable = 1;
    ctx->_count++;

    if (out_index)
        *out_index = i;

    pthread_mutex_unlock(&ctx->_mutext);
    
    return 0;
}

/**
 * Clear the log file, and reopen from the path.
*/
XLOG_API int xlog_reset_log_file(xlog_context* ctx, int receiver_index, const char *file_path)
{
    FILE *fh = NULL;
    int ret = 0;

    if (ctx == NULL || file_path == NULL || *file_path == 0){
        return -1;
    }

    pthread_mutex_lock(&ctx->_mutext);

    if (receiver_index < ctx->_count && ctx->_receivers[receiver_index]._file != NULL)
    {
        fclose(ctx->_receivers[receiver_index]._file);
        ctx->_receivers[receiver_index]._file = NULL;

        fh = fopen(file_path, "w+");

        if (fh == NULL){
            strcpy(ctx->_error, "open file error");
            ret = -1;
        }
        else{
            ctx->_receivers[receiver_index]._file = fh;
        }
    }
    else{
        ret = -1;
    }

    pthread_mutex_unlock(&ctx->_mutext);
    return ret;
}

/**
 * 	remove a log data receiver,return 0 if success.
 */
XLOG_API int xlog_remove_receiver_by_index(xlog_context* ctx, int index)
{
    if (ctx == NULL){
        return -1;
    }

    pthread_mutex_lock(&ctx->_mutext);

    if (index < 0 || index >= ctx->_count){
        strcpy(ctx->_error, "index out of range");
        pthread_mutex_unlock(&ctx->_mutext);
        return -1;
    } 

    ctx->_receivers[index]._fn = NULL;
    ctx->_receivers[index]._enable = 0;
    index++;

    while (index < RECEIVER_MAX_COUNT)
    {
        ctx->_receivers[index-1]._fn = ctx->_receivers[index]._fn;
        ctx->_receivers[index-1]._type = ctx->_receivers[index]._type;
        ctx->_receivers[index-1]._rev = ctx->_receivers[index]._rev;
        ctx->_receivers[index-1]._file = ctx->_receivers[index]._file;
        ctx->_receivers[index-1]._enable = ctx->_receivers[index]._enable;
        index++;
    }
    ctx->_count--;

    pthread_mutex_unlock(&ctx->_mutext);
    
    return 0;    
}

/**
 * Set the log receiver enable to disable.
*/
XLOG_API int xlog_set_receiver_enable(xlog_context* ctx, int index, int bEnable)
{
     if (ctx == NULL){
        return -1;
    }

    pthread_mutex_lock(&ctx->_mutext);

    if (index < 0 || index >= ctx->_count){
        strcpy(ctx->_error, "index out of range");
        pthread_mutex_unlock(&ctx->_mutext);
        return -1;
    } 

    ctx->_receivers[index]._enable = bEnable > 0 ? 1 : 0;

    pthread_mutex_unlock(&ctx->_mutext);
    
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

    pthread_mutex_lock(&ctx->_mutext);

    for (i = 0; i < RECEIVER_MAX_COUNT; i++){
        ctx->_receivers[i]._fn = NULL;
        ctx->_receivers[i]._enable = 0;

        if (ctx->_receivers[i]._file != NULL){
            fclose(ctx->_receivers[i]._file);
            ctx->_receivers[i]._file = NULL;
        }
    }
    ctx->_count = 0;

    pthread_mutex_unlock(&ctx->_mutext);

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
    if (level < XLOG_LEVEL_NONE)
        level = XLOG_LEVEL_NONE;
    if (level > XLOG_LEVEL_DETAIL)
        level = XLOG_LEVEL_DETAIL;

    ctx->_log_level = level;

    return 0;    
} 

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
        strncpy(wr->_domain, domain, sizeof(wr->_domain) - 1);
    }
    return 0;
}

//-------------------------------------------------print api

/**
 * print a error message, return 0 if success.
 */
XLOG_API int xlog_err(xlog_writer *wr, const char *format, ...)
{   
    int i;
    struct xlog_receiver_info *inf;
    va_list args;
    xlog_context *ctx;

    if (wr == NULL){
        return -1;
    }

    ctx = wr->_ctx;
    if (ctx == NULL || ctx->_log_level < XLOG_LEVEL_ERR || ctx->_count < 1){
        return -1;
    }

    pthread_mutex_lock(&ctx->_mutext);

    for (i = 0; i < ctx->_count; i++){
        inf = &ctx->_receivers[i];

        if (inf->_fn != NULL && inf->_enable){
            va_start(args, format);
            inf->_fn(inf, wr->_domain, format, args);
            va_end(args);
        }        
    }

    pthread_mutex_unlock(&ctx->_mutext);

    return 0;
}

/**
 * print a warning message, return 0 if success.
 */
XLOG_API int xlog_warn(xlog_writer *wr, const char *format, ...)
{
    int i;
    struct xlog_receiver_info *inf;
    va_list args;
    xlog_context *ctx;

    if (wr == NULL){
        return -1;
    }

    ctx = wr->_ctx;
    if (ctx == NULL || ctx->_log_level < XLOG_LEVEL_WARN || ctx->_count < 1){
        return -1;
    }

    pthread_mutex_lock(&ctx->_mutext);

    for (i = 0; i < ctx->_count; i++){
        inf = &ctx->_receivers[i];

        if (inf->_fn != NULL && inf->_enable){
            va_start(args, format);
            inf->_fn(inf, wr->_domain, format, args);
            va_end(args);
        }        
    }

    pthread_mutex_unlock(&ctx->_mutext);

    return 0;    
}

/**
 * print a informational message, return 0 if success.
 */
XLOG_API int xlog_info(xlog_writer *wr, const char *format, ...)
{
    int i;
    struct xlog_receiver_info *inf;
    va_list args;
    xlog_context *ctx;

    if (wr == NULL){
        return -1;
    }

    ctx = wr->_ctx;
    if (ctx == NULL || ctx->_log_level < XLOG_LEVEL_INFO || ctx->_count < 1){
        return -1;
    }

    pthread_mutex_lock(&ctx->_mutext);

    for (i = 0; i < ctx->_count; i++){
        inf = &ctx->_receivers[i];

        if (inf->_fn != NULL && inf->_enable){
            va_start(args, format);
            inf->_fn(inf, wr->_domain, format, args);
            va_end(args);
        }        
    }

    pthread_mutex_unlock(&ctx->_mutext);

    return 0;    
}

/**
 * print a debug message, return 0 if success.
 */
XLOG_API int xlog_dbg(xlog_writer *wr, const char *format, ...)
{
    int i;
    struct xlog_receiver_info *inf;
    va_list args;
    xlog_context *ctx;

    if (wr == NULL){
        return -1;
    }

    ctx = wr->_ctx;
    if (ctx == NULL || ctx->_log_level < XLOG_LEVEL_DBG || ctx->_count < 1){
        return -1;
    }

    pthread_mutex_lock(&ctx->_mutext);

    for (i = 0; i < ctx->_count; i++){
        inf = &ctx->_receivers[i];

        if (inf->_fn != NULL && inf->_enable){
            va_start(args, format);
            inf->_fn(inf, wr->_domain, format, args);
            va_end(args);
        }        
    }

    pthread_mutex_unlock(&ctx->_mutext);

    return 0;      
}

/**
 * print a detailed message, return 0 if success.
 */
XLOG_API int xlog_detail(xlog_writer *wr, const char *format, ...)
{
    int i;
    struct xlog_receiver_info *inf;
    va_list args;
    xlog_context *ctx;

    if (wr == NULL){
        return -1;
    }

    ctx = wr->_ctx;
    if (ctx == NULL || ctx->_log_level < XLOG_LEVEL_DETAIL || ctx->_count < 1){
        return -1;
    }

    pthread_mutex_lock(&ctx->_mutext);

    for (i = 0; i < ctx->_count; i++){
        inf = &ctx->_receivers[i];

        if (inf->_fn != NULL && inf->_enable){
            va_start(args, format);
            inf->_fn(inf, wr->_domain, format, args);
            va_end(args);
        }        
    }

    pthread_mutex_unlock(&ctx->_mutext);

    return 0;   
}