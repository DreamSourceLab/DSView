/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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
#include <libsigrokdecode/libsigrokdecode.h>

#include <boost/foreach.hpp>
#include <boost/thread/thread.hpp>

#include <stdexcept>

#include <QDebug>

#include "decoderstack.h"

#include <pv/data/logic.h>
#include <pv/data/logicsnapshot.h>
#include <pv/data/decode/decoder.h>
#include <pv/data/decode/annotation.h>
#include <pv/sigsession.h>
#include <pv/view/logicsignal.h>

using boost::lock_guard;
using boost::mutex;
using boost::optional;
using boost::shared_ptr;
using boost::unique_lock;
using std::deque;
using std::make_pair;
using std::max;
using std::min;
using std::list;
using std::map;
using std::pair;
using std::vector;

using namespace pv::data::decode;

namespace pv {
namespace data {

const double DecoderStack::DecodeMargin = 1.0;
const double DecoderStack::DecodeThreshold = 0.2;
const int64_t DecoderStack::DecodeChunkLength = 1024 * 1024;
const unsigned int DecoderStack::DecodeNotifyPeriod = 1;

mutex DecoderStack::_global_decode_mutex;

DecoderStack::DecoderStack(pv::SigSession &session,
	const srd_decoder *const dec) :
	_session(session),
	_sample_count(0),
	_frame_complete(false),
    _samples_decoded(0),
    _decode_state(Stopped),
    _options_changed(false),
    _no_memory(false)
{
	connect(&_session, SIGNAL(frame_began()),
		this, SLOT(on_new_frame()));
	connect(&_session, SIGNAL(data_received()),
		this, SLOT(on_data_received()));
	connect(&_session, SIGNAL(frame_ended()),
		this, SLOT(on_frame_ended()));

	_stack.push_back(shared_ptr<decode::Decoder>(
		new decode::Decoder(dec)));

    build_row();
}

DecoderStack::~DecoderStack()
{
//	if (_decode_thread.joinable()) {
//		_decode_thread.interrupt();
//		_decode_thread.join();
//	}
    stop_decode();
    _stack.clear();
    _rows.clear();
    _rows_gshow.clear();
    _rows_lshow.clear();
    _class_rows.clear();
}

const std::list< boost::shared_ptr<decode::Decoder> >&
DecoderStack::stack() const
{
	return _stack;
}

void DecoderStack::push(boost::shared_ptr<decode::Decoder> decoder)
{
	assert(decoder);
	_stack.push_back(decoder);
    build_row();
    _options_changed = true;
}

void DecoderStack::remove(boost::shared_ptr<Decoder> &decoder)
{
	// Find the decoder in the stack
	list< shared_ptr<Decoder> >::iterator iter = _stack.begin();
    for(int i = 0; i < _stack.size(); i++, iter++)
        if ((*iter) == decoder)
            break;

	// Delete the element
    if (iter != _stack.end())
        _stack.erase(iter);

    build_row();
    _options_changed = true;
}

void DecoderStack::build_row()
{
    _rows.clear();
    // Add classes
    BOOST_FOREACH (const shared_ptr<decode::Decoder> &dec, _stack)
    {
        assert(dec);
        const srd_decoder *const decc = dec->decoder();
        assert(dec->decoder());

        // Add a row for the decoder if it doesn't have a row list
        if (!decc->annotation_rows) {
            const Row row(decc);
            _rows[row] = decode::RowData();
            std::map<const decode::Row, bool>::const_iterator iter = _rows_gshow.find(row);
            if (iter == _rows_gshow.end()) {
                if (row.title().contains("bit", Qt::CaseInsensitive) ||
                    row.title().contains("warning", Qt::CaseInsensitive)) {
                    _rows_gshow[row] = false;
                    _rows_lshow[row] = false;
                } else {
                    _rows_gshow[row] = true;
                    _rows_lshow[row] = true;
                }
            }
        }

        // Add the decoder rows
        for (const GSList *l = decc->annotation_rows; l; l = l->next)
        {
            const srd_decoder_annotation_row *const ann_row =
                (srd_decoder_annotation_row *)l->data;
            assert(ann_row);

            const Row row(decc, ann_row);

            // Add a new empty row data object
            _rows[row] = decode::RowData();
            std::map<const decode::Row, bool>::const_iterator iter = _rows_gshow.find(row);
            if (iter == _rows_gshow.end()) {
                if (row.title().contains("bit", Qt::CaseInsensitive) ||
                    row.title().contains("warning", Qt::CaseInsensitive)) {
                    _rows_gshow[row] = false;
                    _rows_lshow[row] = false;
                } else {
                    _rows_gshow[row] = true;
                    _rows_lshow[row] = true;
                }
            }

            // Map out all the classes
            for (const GSList *ll = ann_row->ann_classes;
                ll; ll = ll->next)
                _class_rows[make_pair(decc,
                    GPOINTER_TO_INT(ll->data))] = row;
        }
    }
}

int64_t DecoderStack::samples_decoded() const
{
	lock_guard<mutex> decode_lock(_output_mutex);
	return _samples_decoded;
}

void DecoderStack::get_annotation_subset(
	std::vector<pv::data::decode::Annotation> &dest,
	const Row &row, uint64_t start_sample,
	uint64_t end_sample) const
{
	lock_guard<mutex> lock(_output_mutex);

    std::map<const Row, decode::RowData>::const_iterator iter =
        _rows.find(row);
    if (iter != _rows.end())
		(*iter).second.get_annotation_subset(dest,
			start_sample, end_sample);
}

uint64_t DecoderStack::get_max_annotation(const Row &row)
{
    lock_guard<mutex> lock(_output_mutex);

    std::map<const Row, decode::RowData>::const_iterator iter =
        _rows.find(row);
    if (iter != _rows.end())
        return (*iter).second.get_max_annotation();

    return 0;
}

uint64_t DecoderStack::get_min_annotation(const Row &row)
{
    lock_guard<mutex> lock(_output_mutex);

    std::map<const Row, decode::RowData>::const_iterator iter =
        _rows.find(row);
    if (iter != _rows.end())
        return (*iter).second.get_min_annotation();

    return 0;
}

std::map<const decode::Row, bool>& DecoderStack::get_rows_gshow()
{
    return _rows_gshow;
}

std::map<const decode::Row, bool>& DecoderStack::get_rows_lshow()
{
    return _rows_lshow;
}

void DecoderStack::set_rows_gshow(const decode::Row row, bool show)
{
    std::map<const decode::Row, bool>::const_iterator iter = _rows_gshow.find(row);
    if (iter != _rows_gshow.end()) {
        _rows_gshow[row] = show;
    }
}

void DecoderStack::set_rows_lshow(const decode::Row row, bool show)
{
    std::map<const decode::Row, bool>::const_iterator iter = _rows_lshow.find(row);
    if (iter != _rows_lshow.end()) {
        _rows_lshow[row] = show;
    }
}

bool DecoderStack::has_annotations(const Row &row) const
{
    lock_guard<mutex> lock(_output_mutex);

    std::map<const Row, decode::RowData>::const_iterator iter =
        _rows.find(row);
    if (iter != _rows.end())
        if(0 == (*iter).second.get_max_sample())
            return false;
        else
            return true;
    else
        return false;
}

uint64_t DecoderStack::list_annotation_size() const
{
    uint64_t max_annotation_size = 0;
    int row = 0;
    for (map<const Row, RowData>::const_iterator i = _rows.begin();
        i != _rows.end(); i++) {
        map<const Row, bool>::const_iterator iter = _rows_lshow.find((*i).first);
        if (iter != _rows_lshow.end() && (*iter).second)
            max_annotation_size = max(max_annotation_size,
                (*i).second.get_annotation_size());
    }

    return max_annotation_size;
}

bool DecoderStack::list_annotation(pv::data::decode::Annotation &ann,
                                  uint16_t row_index, uint64_t col_index) const
{
    int row = 0;
    for (map<const Row, RowData>::const_iterator i = _rows.begin();
        i != _rows.end(); i++) {
        map<const Row, bool>::const_iterator iter = _rows_lshow.find((*i).first);
        if (iter != _rows_lshow.end() && (*iter).second) {
            if (row_index-- == 0) {
                return (*i).second.get_annotation(ann, col_index);
            }
        }
    }

    return false;
}


bool DecoderStack::list_row_title(int row, QString &title) const
{
    int index = 0;
    for (map<const Row, RowData>::const_iterator i = _rows.begin();
        i != _rows.end(); i++) {
        map<const Row, bool>::const_iterator iter = _rows_lshow.find((*i).first);
        if (iter != _rows_lshow.end() && (*iter).second) {
            if (row-- == 0) {
                title = (*i).first.title();
                return 1;
            }
        }
    }
    return 0;
}

QString DecoderStack::error_message()
{
	lock_guard<mutex> lock(_output_mutex);
	return _error_message;
}

void DecoderStack::clear()
{
	_sample_count = 0;
	_frame_complete = false;
	_samples_decoded = 0;
    //new_decode_data();
	_error_message = QString();
    for (map<const Row, RowData>::const_iterator i = _rows.begin();
        i != _rows.end(); i++)
        _rows[(*i).first] = decode::RowData();
    _no_memory = false;
}

void DecoderStack::stop_decode()
{
    _snapshot.reset();

    if(_decode_state == Stopped) {
        clear();
        return;
    }

    if (_decode_thread.get()) {
        _decode_thread->interrupt();
        _decode_thread->join();
        _decode_state = Stopped;
    }
    _decode_thread.reset();
    clear();
}

void DecoderStack::begin_decode()
{
	shared_ptr<pv::view::LogicSignal> logic_signal;
	shared_ptr<pv::data::Logic> data;

    if (!_options_changed)
        return;
//	if (_decode_thread.joinable()) {
//		_decode_thread.interrupt();
//		_decode_thread.join();
//	}
    stop_decode();

	// Check that all decoders have the required channels
	BOOST_FOREACH(const shared_ptr<decode::Decoder> &dec, _stack)
		if (!dec->have_required_probes()) {
			_error_message = tr("One or more required channels "
				"have not been specified");
			return;
		}

	// We get the logic data of the first channel in the list.
	// This works because we are currently assuming all
	// LogicSignals have the same data/snapshot
	BOOST_FOREACH (const shared_ptr<decode::Decoder> &dec, _stack)
		if (dec && !dec->channels().empty() &&
			((logic_signal = (*dec->channels().begin()).second)) &&
			((data = logic_signal->logic_data())))
			break;

	if (!data)
		return;

	// Check we have a snapshot of data
	const deque< shared_ptr<pv::data::LogicSnapshot> > &snapshots =
		data->get_snapshots();
	if (snapshots.empty())
		return;
	_snapshot = snapshots.front();

	// Get the samplerate and start time
	_start_time = data->get_start_time();
	_samplerate = data->samplerate();
	if (_samplerate == 0.0)
		_samplerate = 1.0;

    //_decode_thread = boost::thread(&DecoderStack::decode_proc, this);
    _decode_thread.reset(new boost::thread(&DecoderStack::decode_proc, this));
}

uint64_t DecoderStack::get_max_sample_count() const
{
	uint64_t max_sample_count = 0;

    for (map<const Row, RowData>::const_iterator i = _rows.begin();
        i != _rows.end(); i++)
		max_sample_count = max(max_sample_count,
			(*i).second.get_max_sample());

	return max_sample_count;
}

boost::optional<uint64_t> DecoderStack::wait_for_data() const
{
	unique_lock<mutex> input_lock(_input_mutex);
	while(!boost::this_thread::interruption_requested() &&
		!_frame_complete && (uint64_t)_samples_decoded >= _sample_count)
		_input_cond.wait(input_lock);
	return boost::make_optional(
		!boost::this_thread::interruption_requested() &&
		((uint64_t)_samples_decoded < _sample_count || !_frame_complete),
		_sample_count);
}

void DecoderStack::decode_data(
    const uint64_t decode_start, const uint64_t decode_end,
    const unsigned int unit_size, srd_session *const session)
{
    uint8_t *chunk = NULL;

    const uint64_t chunk_sample_count =
        DecodeChunkLength / _snapshot->unit_size();

    for (uint64_t i = decode_start;
        !boost::this_thread::interruption_requested() &&
            i < decode_end && !_no_memory;
        i += chunk_sample_count)
    {
        //lock_guard<mutex> decode_lock(_global_decode_mutex);

        const uint64_t chunk_end = min(
            i + chunk_sample_count, decode_end);
        chunk = _snapshot->get_samples(i, chunk_end);

        if (srd_session_send(session, i, chunk_end, chunk,
                (chunk_end - i) * unit_size, unit_size) != SRD_OK) {
            _error_message = tr("Decoder reported an error");
            break;
        }

        {
            lock_guard<mutex> lock(_output_mutex);
            _samples_decoded = chunk_end - decode_start + 1;
        }

        if (i % DecodeNotifyPeriod == 0)
            new_decode_data();

    }
    _options_changed = false;
    decode_done();
}

void DecoderStack::decode_proc()
{
    lock_guard<mutex> decode_lock(_global_decode_mutex);

    optional<uint64_t> sample_count;
	srd_session *session;
	srd_decoder_inst *prev_di = NULL;
    uint64_t decode_start;
    uint64_t decode_end;

	assert(_snapshot);

	// Create the session
	srd_session_new(&session);
	assert(session);

    _decode_state = Running;

	// Create the decoders
	const unsigned int unit_size = _snapshot->unit_size();

	BOOST_FOREACH(const shared_ptr<decode::Decoder> &dec, _stack)
	{
		srd_decoder_inst *const di = dec->create_decoder_inst(session, unit_size);

		if (!di)
		{
			_error_message = tr("Failed to create decoder instance");
			srd_session_destroy(session);
			return;
		}

		if (prev_di)
			srd_inst_stack (session, prev_di, di);

		prev_di = di;
        decode_start = dec->decode_start();
        decode_end = min(dec->decode_end(), _snapshot->get_sample_count());
	}

	// Get the intial sample count
	{
		unique_lock<mutex> input_lock(_input_mutex);
        sample_count = _sample_count = _snapshot->get_sample_count();
	}

	// Start the session
	srd_session_metadata_set(session, SRD_CONF_SAMPLERATE,
		g_variant_new_uint64((uint64_t)_samplerate));

	srd_pd_output_callback_add(session, SRD_OUTPUT_ANN,
		DecoderStack::annotation_callback, this);

	srd_session_start(session);

//	do {
//		decode_data(*sample_count, unit_size, session);
//	} while(_error_message.isEmpty() && (sample_count = wait_for_data()));
    //decode_data(*sample_count, unit_size, session);
    decode_data(decode_start, decode_end, unit_size, session);

	// Destroy the session
	srd_session_destroy(session);

    _decode_state = Stopped;
}

uint64_t DecoderStack::sample_count() const
{
    if (_snapshot)
        return _snapshot->get_sample_count();
    else
        return 0;
}

uint64_t DecoderStack::sample_rate() const
{
    return _samplerate;
}

void DecoderStack::annotation_callback(srd_proto_data *pdata, void *decoder)
{
	assert(pdata);
	assert(decoder);

	DecoderStack *const d = (DecoderStack*)decoder;
	assert(d);

	lock_guard<mutex> lock(d->_output_mutex);

    if (d->_no_memory)
        return;

	const Annotation a(pdata);

	// Find the row
	assert(pdata->pdo);
	assert(pdata->pdo->di);
	const srd_decoder *const decc = pdata->pdo->di->decoder;
	assert(decc);

    map<const Row, decode::RowData>::iterator row_iter = d->_rows.end();
	
	// Try looking up the sub-row of this class
	const map<pair<const srd_decoder*, int>, Row>::const_iterator r =
		d->_class_rows.find(make_pair(decc, a.format()));
	if (r != d->_class_rows.end())
        row_iter = d->_rows.find((*r).second);
	else
	{
		// Failing that, use the decoder as a key
        row_iter = d->_rows.find(Row(decc));
	}

    assert(row_iter != d->_rows.end());
    if (row_iter == d->_rows.end()) {
		qDebug() << "Unexpected annotation: decoder = " << decc <<
			", format = " << a.format();
		assert(0);
		return;
	}

	// Add the annotation
    if (!(*row_iter).second.push_annotation(a))
        d->_no_memory = true;
}

void DecoderStack::on_new_frame()
{
    //begin_decode();
}

void DecoderStack::on_data_received()
{
//	{
//		unique_lock<mutex> lock(_input_mutex);
//		if (_snapshot)
//			_sample_count = _snapshot->get_sample_count();
//	}
//	_input_cond.notify_one();
}

void DecoderStack::on_frame_ended()
{
//	{
//		unique_lock<mutex> lock(_input_mutex);
//		if (_snapshot)
//			_frame_complete = true;
//	}
//	_input_cond.notify_one();
    _options_changed = true;
    begin_decode();
}

int DecoderStack::list_rows_size()
{
    int rows_size = 0;
    int row = 0;
    for (map<const Row, RowData>::const_iterator i = _rows.begin();
        i != _rows.end(); i++) {
        map<const Row, bool>::const_iterator iter = _rows_lshow.find((*i).first);
        if (iter != _rows_lshow.end() && (*iter).second)
            rows_size++;
    }
    return rows_size;
}

bool DecoderStack::options_changed() const
{
    return _options_changed;
}

void DecoderStack::set_options_changed(bool changed)
{
    _options_changed = changed;
}

} // namespace data
} // namespace pv
