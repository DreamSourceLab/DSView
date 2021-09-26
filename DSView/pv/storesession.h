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

#include <boost/thread.hpp>

#include <QObject>

#include <libsigrok4DSL/libsigrok.h>
#include <libsigrokdecode4DSL/libsigrokdecode.h>

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
    StoreSession(SigSession &session);

	~StoreSession();

    SigSession& session();

	std::pair<uint64_t, uint64_t> progress() const;

	const QString& error() const;

    bool save_start(QString session_file);

    bool export_start();

	void wait();

	void cancel();

private:
    void save_proc(boost::shared_ptr<pv::data::Snapshot> snapshot);
    QString meta_gen(boost::shared_ptr<data::Snapshot> snapshot);
    void export_proc(boost::shared_ptr<pv::data::Snapshot> snapshot);
    #ifdef ENABLE_DECODE
    QString decoders_gen();
    #endif

public:
    #ifdef ENABLE_DECODE
    QJsonArray json_decoders();
    void load_decoders(dock::ProtocolDock *widget, QJsonArray dec_array);
    #endif

private:
    QList<QString> getSuportedExportFormats();
    double get_integer(GVariant * var);

signals:
	void progress_updated();

private:
    QString _file_name;
    QString _suffix;
    SigSession &_session;

	boost::thread _thread;

    const struct sr_output_module* _outModule;

    //mutable boost::mutex _mutex;
	uint64_t _units_stored;
	uint64_t _unit_count;
    bool _has_error;
	QString _error;
    bool _canceled;
};

} // pv

#endif // DSVIEW_PV_STORESESSION_H
