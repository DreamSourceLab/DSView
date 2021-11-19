/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2016 DreamSourceLab <support@dreamsourcelab.com>
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

#ifndef DSVIEW_PV_STORESESSION_H
#define DSVIEW_PV_STORESESSION_H

#include <stdint.h>
#include <string>
#include <thread>
  
#include <QObject>

#include <libsigrok4DSL/libsigrok.h>

#include "ZipMaker.h"

namespace pv {

class SigSession;

namespace data {
class Snapshot;
}

namespace dock {
class ProtocolDock;
}

class StoreSession : public QObject
{
	Q_OBJECT

private:
    const static int File_Version = 2;

public:
    StoreSession(SigSession *session);

	~StoreSession();

    SigSession* session();

	std::pair<uint64_t, uint64_t> progress();

	const QString& error();

    bool save_start();

    bool export_start();

	void wait();

	void cancel();

private:
    void save_proc(pv::data::Snapshot *snapshot);
    QString meta_gen(data::Snapshot *snapshot);
    void export_proc(pv::data::Snapshot *snapshot);
   
    QString decoders_gen();
 

public:    
    QJsonArray json_decoders();
    bool load_decoders(dock::ProtocolDock *widget, QJsonArray dec_array);
    QString MakeSaveFile(bool bDlg);
    QString MakeExportFile(bool bDlg);

    inline QString GetFileName()
        { return _file_name;}

    bool IsLogicDataType();
 

private:
    QList<QString> getSuportedExportFormats();
    double get_integer(GVariant * var);
    void MakeChunkName(char *chunk_name, int chunk_num, int index, int type, int version);

signals:
	void progress_updated();

public:
    QString          _sessionFile;

private:
    QString         _file_name;
    QString         _suffix;
    SigSession      *_session;

	std::thread   _thread;

    const struct sr_output_module* _outModule;
 
	uint64_t        _units_stored;
	uint64_t        _unit_count;
    bool            _has_error;
	QString         _error;
    volatile bool   _canceled;
    ZipMaker        m_zipDoc;  
};

} // pv

#endif // DSVIEW_PV_STORESESSION_H
