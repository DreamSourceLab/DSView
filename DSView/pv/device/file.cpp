/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include "file.h"
#include "inputfile.h"
#include "sessionfile.h"

#include <QFileInfo>
#include <assert.h>
#include "../utility/path.h"
#include <stdlib.h>
#include "../ZipMaker.h"
#include "../log.h"

namespace pv {
namespace device {

File::File(QString path)
:_path(path)
{
}

File::~File(){

}

QString File::format_device_title()
{
    QFileInfo fi(_path);
    return fi.fileName();
}

File* File::create(QString name)
{ 
    auto f_name = path::ConvertPath(name);

    if (sr_session_load(f_name.c_str()) == SR_OK) {
		GSList *devlist = NULL;
		sr_session_dev_list(&devlist);
		sr_session_destroy();

		if (devlist) {
			sr_dev_inst *const sdi = (sr_dev_inst*)devlist->data;
			g_slist_free(devlist);
			if (sdi) {
				sr_dev_close(sdi);
				sr_dev_clear(sdi->driver);
				return new SessionFile(name);
			}
		}
	}

    return new InputFile(name);
}

QJsonArray File::get_decoders()
{  
    QJsonArray dec_array;
    QJsonParseError error; 

    /* read "decoders" */ 
    auto f_name = path::ConvertPath(_path);
    ZipReader rd(f_name.c_str());
    auto *data = rd.GetInnterFileData("decoders");

    if (data != NULL){
        QByteArray raw_bytes = QByteArray::fromRawData(data->data(), data->size());
        QString jsonStr(raw_bytes.data());
        QByteArray qbs = jsonStr.toUtf8(); 
        QJsonDocument sessionDoc = QJsonDocument::fromJson(qbs, &error);

        if (error.error != QJsonParseError::NoError){
            QString estr = error.errorString();
            dsv_err("File::get_decoders(), parse json error:\"%s\"!", estr.toUtf8().data());
        } 

        dec_array = sessionDoc.array();
        rd.ReleaseInnerFileData(data);
    }
 
    return dec_array;
}

QJsonDocument File::get_session()
{ 
    QJsonDocument sessionDoc;
    QJsonParseError error;

    auto f_name = path::ConvertPath(_path);
    ZipReader rd(f_name.c_str());
    auto *data = rd.GetInnterFileData("session");

     if (data != NULL){
        QByteArray raw_bytes = QByteArray::fromRawData(data->data(), data->size());
        QString jsonStr(raw_bytes.data());
        QByteArray qbs = jsonStr.toUtf8();
        sessionDoc = QJsonDocument::fromJson(qbs, &error);

        if (error.error != QJsonParseError::NoError){
            QString estr = error.errorString();
            dsv_err("File::get_session(), parse json error:\"%s\"!", estr.toUtf8().data());
        } 

        rd.ReleaseInnerFileData(data);
    }

    return sessionDoc;
}

} // device
} // pv
