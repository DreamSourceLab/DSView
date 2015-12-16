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

#ifndef DSVIEW_PV_DATA_DECODERSTACK_H
#define DSVIEW_PV_DATA_DECODERSTACK_H

#include "signaldata.h"

#include <list>

#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <QObject>
#include <QString>

#include <pv/data/decode/row.h>
#include <pv/data/decode/rowdata.h>

struct srd_decoder;
struct srd_decoder_annotation_row;
struct srd_channel;
struct srd_proto_data;
struct srd_session;

namespace DecoderStackTest {
class TwoDecoderStack;
}

namespace pv {

class SigSession;

namespace view {
class LogicSignal;
}

namespace data {

class LogicSnapshot;

namespace decode {
class Annotation;
class Decoder;
}

class Logic;

class DecoderStack : public QObject, public SignalData
{
	Q_OBJECT

private:
	static const double DecodeMargin;
	static const double DecodeThreshold;
	static const int64_t DecodeChunkLength;
	static const unsigned int DecodeNotifyPeriod;

public:
    enum decode_state {
        Stopped,
        Running
    };

public:
	DecoderStack(pv::SigSession &_session,
		const srd_decoder *const decoder);

	virtual ~DecoderStack();

	const std::list< boost::shared_ptr<decode::Decoder> >& stack() const;
	void push(boost::shared_ptr<decode::Decoder> decoder);
	void remove(int index);

	int64_t samples_decoded() const;

    std::vector< std::pair<decode::Row, bool> > get_visible_rows() const;

	/**
	 * Extracts sorted annotations between two period into a vector.
	 */
	void get_annotation_subset(
		std::vector<pv::data::decode::Annotation> &dest,
		const decode::Row &row, uint64_t start_sample,
		uint64_t end_sample) const;

    uint64_t get_max_annotation(const decode::Row &row);

    bool has_annotations(const decode::Row &row) const;

	QString error_message();

	void clear();

	uint64_t get_max_sample_count() const;

	void begin_decode();

    void stop_decode();

    int cur_rows_size();

    void options_changed(bool changed);

    uint64_t sample_count() const;

private:
    boost::optional<uint64_t> wait_for_data() const;

    void decode_data(const uint64_t sample_count,
        const unsigned int unit_size, srd_session *const session);

	void decode_proc();

	static void annotation_callback(srd_proto_data *pdata,
		void *decoder);

private slots:
	void on_new_frame();

	void on_data_received();

	void on_frame_ended();

signals:
	void new_decode_data();
    void decode_done();

private:
	pv::SigSession &_session;

	/**
	 * This mutex prevents more than one decode operation occurring
	 * concurrently.
	 * @todo A proper solution should be implemented to allow multiple
	 * decode operations.
	 */
	static boost::mutex _global_decode_mutex;

	std::list< boost::shared_ptr<decode::Decoder> > _stack;

	boost::shared_ptr<pv::data::LogicSnapshot> _snapshot;

	mutable boost::mutex _input_mutex;
	mutable boost::condition_variable _input_cond;
    uint64_t _sample_count;
	bool _frame_complete;

	mutable boost::mutex _output_mutex;
	int64_t	_samples_decoded;

	std::map<const decode::Row, decode::RowData> _rows;

	std::map<std::pair<const srd_decoder*, int>, decode::Row> _class_rows;

	QString _error_message;

    std::unique_ptr<boost::thread> _decode_thread;
    decode_state _decode_state;

    bool _options_changed;

	friend class DecoderStackTest::TwoDecoderStack;
};

} // namespace data
} // namespace pv

#endif // DSVIEW_PV_DATA_DECODERSTACK_H
