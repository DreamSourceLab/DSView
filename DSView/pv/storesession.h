/*
 * This file is part of the PulseView project.
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

#ifndef DSVIEW_PV_STORESESSION_H
#define DSVIEW_PV_STORESESSION_H

#include <stdint.h>

#include <string>

#include <boost/thread.hpp>

#include <QObject>

namespace pv {

class SigSession;

namespace data {
class LogicSnapshot;
}

class StoreSession : public QObject
{
	Q_OBJECT

private:
	static const size_t BlockSize;

public:
    StoreSession(const std::string &file_name,
        SigSession &session);

	~StoreSession();

	std::pair<uint64_t, uint64_t> progress() const;

	const QString& error() const;

	bool start();

	void wait();

	void cancel();

private:
	void store_proc(boost::shared_ptr<pv::data::LogicSnapshot> snapshot);

signals:
	void progress_updated();

private:
	const std::string _file_name;
    SigSession &_session;

	boost::thread _thread;

    //mutable boost::mutex _mutex;
	uint64_t _units_stored;
	uint64_t _unit_count;
	QString _error;
};

} // pv

#endif // DSVIEW_PV_STORESESSION_H
