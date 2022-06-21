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
#include <string.h>

#include <QFileInfo>
#include <zip.h>
#include <assert.h>

#ifdef _WIN32
#include <QTextCodec>
#endif

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
    char f_name[256];
    File::ConvertFileName(name, f_name, sizeof(f_name));

    if (sr_session_load(f_name) == SR_OK) {
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
    struct zip *archive;
    struct zip_file *zf;
    struct zip_stat zs;
    int ret;
    char *dec_file;
    QJsonArray dec_array;
    QJsonParseError error;

    char f_name[256];
    File::ConvertFileName(_path, f_name, sizeof(f_name));

    archive = zip_open(f_name, 0, &ret);
    if (archive) {
        /* read "decoders" */
        if (zip_stat(archive, "decoders", 0, &zs) != -1) {
            dec_file = (char *)g_try_malloc(zs.size);
            if (dec_file) {
                zf = zip_fopen_index(archive, zs.index, 0);
                zip_fread(zf, dec_file, zs.size);
                zip_fclose(zf);

                //QString sessionData = QString::fromUtf8(dec_file);
                QJsonDocument sessionDoc = QJsonDocument::fromJson(QByteArray::fromRawData(dec_file, zs.size), &error);
                dec_array = sessionDoc.array();
            }
        }

        zip_close(archive);        
    }

    return dec_array;
}

QJsonDocument File::get_session()
{
    struct zip *archive;
    struct zip_file *zf;
    struct zip_stat zs;
    int ret;
    char *dec_file;
    QJsonDocument sessionDoc;
    QJsonParseError error;

    char f_name[256];
    File::ConvertFileName(_path, f_name, sizeof(f_name));

    archive = zip_open(f_name, 0, &ret);
    if (archive) {
        /* read "decoders" */
        if (zip_stat(archive, "session", 0, &zs) != -1) {
            dec_file = (char *)g_try_malloc(zs.size);
            if (dec_file) {
                zf = zip_fopen_index(archive, zs.index, 0);
                zip_fread(zf, dec_file, zs.size);
                zip_fclose(zf);

                //QString sessionData = QString::fromUtf8(dec_file);
                sessionDoc = QJsonDocument::fromJson(QByteArray::fromRawData(dec_file, zs.size), &error);
            }
        }

        zip_close(archive);
    }

    return sessionDoc;
}

void File::ConvertFileName(QString fileName, char *out_name, int size)
{
    assert(out_name);
    assert(size > 0);
    memset(out_name, 0, size);

    char *src = NULL;
#ifdef _WIN32
    QTextCodec *code = QTextCodec::codecForName("GB2312");
    if (code != NULL){
       src = code->fromUnicode(fileName).data();
    }
#endif 
    if (src == NULL){
        src = fileName.toUtf8().data();
    }

    int len = strlen(src);
    if (len >= size){
        assert(false);
    }
    strcpy(out_name, src);
}

} // device
} // pv
