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

#include "path.h"
#ifdef _WIN32
#include <QTextCodec>
#include "../log.h"
#include <string.h>
#endif

namespace pv{
namespace path{

    std::string ConvertPath(QString fileName)
    {
        return fileName.toUtf8().toStdString();
    }

    QString GetDirectoryName(QString path)
    {
        int lstdex = path.lastIndexOf('/');
        if (lstdex != -1)
        {
            return path.left(lstdex);
        }
        return path;
    }

    std::string ToUnicodePath(QString path)
    {
        std::string str;
         
#ifdef _WIN32
        QTextCodec *codec = QTextCodec::codecForName("System");
        if (codec != NULL){
            QByteArray str_tmp = codec->fromUnicode(path);
            str = str_tmp.data();
        } 
        else{
            dsv_err("Error: can't get \"System\" page code");
            str = path.toUtf8().data();
        }       
#else
        str = path.toUtf8().data();        
#endif

        return str;
    }
}
}
