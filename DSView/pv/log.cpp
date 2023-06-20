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
#include <QString>
#include <QDir>
#include  "config/appconfig.h"
#include "utility/path.h"
#include <string>

xlog_writer *dsv_log = nullptr;
static xlog_context *log_ctx = nullptr;
static bool b_logfile = false;
static int log_file_index = -1; 

void dsv_log_init()
{
    if (log_ctx == nullptr){
        log_ctx = xlog_new();  
        dsv_log = xlog_create_writer(log_ctx, "DSView"); 
    }
} 

void dsv_log_uninit()
{ 
    xlog_free(log_ctx);
    xlog_free_writer(dsv_log);
    log_ctx = nullptr;
    dsv_log = nullptr;
}

xlog_context* dsv_log_context()
{
    return log_ctx;
}

void dsv_log_level(int l)
{
    xlog_set_level(log_ctx, l);
    dsv_info("%s%d", "Set log level: ", l);
}

void dsv_log_enalbe_logfile(bool append)
{
    if (!b_logfile && log_ctx){
        b_logfile = true; 
        
        QString lf = get_dsv_log_path();
        
        dsv_info("%s\"%s\"", "Store log to file: ", lf.toUtf8().data());

        std::string log_file = pv::path::ToUnicodePath(lf);

        int ret = xlog_add_receiver_from_file(log_ctx, log_file.c_str(), &log_file_index, append);
        if (ret != 0){
            dsv_err("Create log file error!");
        }
    }
}

void dsv_clear_log_file()
{   
    if (b_logfile && log_ctx)
    {
        QString lf = get_dsv_log_path();
        std::string log_file = pv::path::ToUnicodePath(lf);
        int ret = xlog_reset_log_file(log_ctx, log_file_index, log_file.c_str());

        if (ret != 0){
            dsv_err("Clear log file error!");
        }
    }
    else{
        QDir dir;
        QString filePath = get_dsv_log_path();
        if (dir.exists(filePath))
        {
            dir.remove(filePath);
        }
    }
}

void dsv_set_log_file_enable(bool flag)
{
    if (b_logfile && log_ctx)
    {
        xlog_set_receiver_enable(log_ctx, log_file_index, flag);
    }
}

void dsv_remove_log_file()
{
    if (b_logfile && log_ctx)
    {
        b_logfile = false;
        xlog_remove_receiver_by_index(log_ctx, log_file_index);
        log_file_index = -1;
    }
}

QString get_dsv_log_path()
{
    QString lf;

    #ifdef Q_OS_LINUX
        lf = QDir::homePath() + "/DSView.log";
    #endif

    #ifdef _WIN32
        lf = GetUserDataDir() + "/DSView.log";
    #endif

    #ifdef Q_OS_DARWIN
        lf = GetUserDataDir() + "/DSView.log";
    #endif

    return lf;
}