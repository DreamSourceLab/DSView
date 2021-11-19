/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2013 DreamSourceLab <support@dreamsourcelab.com>
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

#include "dsvdef.h"
#include <string.h>

#ifdef DS_DEBUG_TRACE
#include <stdio.h>
    void ds_print(const char *s){
        printf(s);
    }     
#endif

#include <QTextStream>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#else
#include <QTextCodec>
#endif 

namespace DecoderDataFormat
{
      int Parse(const char *name){
        if (strcmp(name, "ascii") == 0){
            return (int)ascii;
        }
        if (strcmp(name, "dec") == 0){
            return (int)dec;
        }
         if (strcmp(name, "hex") == 0){
            return (int)hex;
        }
         if (strcmp(name, "oct") == 0){
            return (int)oct;
        }
         if (strcmp(name, "bin") == 0){
            return (int)bin;
        }
        return (int)ascii;
    }
}


namespace app
{
    QWidget* get_app_window_instance(QWidget *ins, bool bSet){

        static QWidget *g_ins = NULL;
        if (bSet){
            g_ins = ins;
        }
        return g_ins;
    }

    bool is_app_top_window(QWidget* w){
         QWidget *top =get_app_window_instance(NULL, NULL);
         if (top && top == w){
             return true;
         }
         return false;
      }

    void set_utf8(QTextStream &stream){
        #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        stream.setEncoding(QStringConverter::Utf8);      
        #else
        QTextCodec *code=QTextCodec::codecForName("UTF-8");
        stream.setCodec(code);
        #endif
    }
}
