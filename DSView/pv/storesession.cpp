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

#include "storesession.h"

#include <pv/sigsession.h>
#include <pv/data/logic.h>
#include <pv/data/logicsnapshot.h>
#include <pv/view/signal.h>

#include <boost/foreach.hpp>

using boost::dynamic_pointer_cast;
using boost::mutex;
using boost::shared_ptr;
using boost::thread;
using boost::lock_guard;
using std::deque;
using std::make_pair;
using std::min;
using std::pair;
using std::set;
using std::string;
using std::vector;

namespace pv {

const size_t StoreSession::BlockSize = 1024 * 1024;

StoreSession::StoreSession(const std::string &file_name,
    SigSession &session) :
	_file_name(file_name),
	_session(session),
	_units_stored(0),
	_unit_count(0)
{
}

StoreSession::~StoreSession()
{
	wait();
}

pair<uint64_t, uint64_t> StoreSession::progress() const
{
    //lock_guard<mutex> lock(_mutex);
	return make_pair(_units_stored, _unit_count);
}

const QString& StoreSession::error() const
{
    //lock_guard<mutex> lock(_mutex);
	return _error;
}

bool StoreSession::start()
{
    const vector< shared_ptr<view::Signal> > sigs(_session.get_signals());
    set< boost::shared_ptr<data::SignalData> > data_set;
    BOOST_FOREACH(const boost::shared_ptr<view::Signal> sig, sigs) {
        assert(sig);
        data_set.insert(sig->data());
    }

	// Check we have logic data
	if (data_set.empty() || sigs.empty()) {
		_error = tr("No data to save.");
		return false;
	}

	if (data_set.size() > 1) {
        _error = tr("DSView currently only has support for "
			"storing a single data stream.");
		return false;
	}

	// Get the logic data
	//shared_ptr<data::SignalData
	shared_ptr<data::Logic> data;
	if (!(data = dynamic_pointer_cast<data::Logic>(*data_set.begin()))) {
        _error = tr("DSView currently only has support for "
			"storing a logic data.");
		return false;
	}

	// Get the snapshot
	const deque< shared_ptr<data::LogicSnapshot> > &snapshots =
		data->get_snapshots();

	if (snapshots.empty()) {
		_error = tr("No snapshots to save.");
		return false;
	}

	const shared_ptr<data::LogicSnapshot> snapshot(snapshots.front());
	assert(snapshot);

	// Make a list of probes
	char **const probes = new char*[sigs.size() + 1];
	for (size_t i = 0; i < sigs.size(); i++) {
		shared_ptr<view::Signal> sig(sigs[i]);
		assert(sig);
		probes[i] = strdup(sig->get_name().toUtf8().constData());
	}
	probes[sigs.size()] = NULL;

	// Begin storing
	if (sr_session_save_init(_file_name.c_str(),
		data->samplerate(), probes) != SR_OK) {
		_error = tr("Error while saving.");
		return false;
	}

	// Delete the probes array
	for (size_t i = 0; i <= sigs.size(); i++)
		free(probes[i]);
	delete[] probes;

	_thread = boost::thread(&StoreSession::store_proc, this, snapshot);
	return true;
}

void StoreSession::wait()
{
	if (_thread.joinable())
		_thread.join();
}

void StoreSession::cancel()
{
	_thread.interrupt();
}

void StoreSession::store_proc(shared_ptr<data::LogicSnapshot> snapshot)
{
	assert(snapshot);

	uint64_t start_sample = 0;

	/// TODO: Wrap this in a std::unique_ptr when we transition to C++11
    uint8_t *data =  NULL;
    //uint8_t *const data = new uint8_t[BlockSize];
    //assert(data);

	const int unit_size = snapshot->unit_size();
	assert(unit_size != 0);

	{
        //lock_guard<mutex> lock(_mutex);
		_unit_count = snapshot->get_sample_count();
	}

	const unsigned int samples_per_block = BlockSize / unit_size;

	while (!boost::this_thread::interruption_requested() &&
		start_sample < _unit_count)
	{
		progress_updated();

		const uint64_t end_sample = min(
			start_sample + samples_per_block, _unit_count);
        data = snapshot->get_samples(start_sample, end_sample);

		if(sr_session_append(_file_name.c_str(), data, unit_size,
			end_sample - start_sample) != SR_OK)
		{
			_error = tr("Error while saving.");
			break;
		}

		start_sample = end_sample;

		{
            //lock_guard<mutex> lock(_mutex);
			_units_stored = start_sample;
		}
	}

	progress_updated();

    //delete[] data;
}

} // pv
