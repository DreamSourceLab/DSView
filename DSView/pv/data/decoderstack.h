/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2014 DreamSourceLab <support@dreamsourcelab.com>
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
#include <libsigrokdecode/libsigrokdecode.h>

#include <list>

#include <boost/optional.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

#include <QObject>
#include <QString>

#include "../data/decode/row.h"
#include "../data/decode/rowdata.h"
#include "../data/signaldata.h"

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
    void remove(boost::shared_ptr<decode::Decoder>& decoder);
    void build_row();

	int64_t samples_decoded() const;

	/**
	 * Extracts sorted annotations between two period into a vector.
	 */
	void get_annotation_subset(
		std::vector<pv::data::decode::Annotation> &dest,
		const decode::Row &row, uint64_t start_sample,
		uint64_t end_sample) const;

    uint64_t get_max_annotation(const decode::Row &row);
    uint64_t get_min_annotation(const decode::Row &row); // except instant(end=start) annotation

    std::map<const decode::Row, bool> get_rows_gshow();
    std::map<const decode::Row, bool> get_rows_lshow();
    void set_rows_gshow(const decode::Row row, bool show);
    void set_rows_lshow(const decode::Row row, bool show);

    bool has_annotations(const decode::Row &row) const;

    uint64_t list_annotation_size() const;
    uint64_t list_annotation_size(uint16_t row_index) const;


    bool list_annotation(decode::Annotation &ann,
                        uint16_t row_index, uint64_t col_index) const;


    bool list_row_title(int row, QString &title) const;

	QString error_message();

	void clear();
    void init();

	uint64_t get_max_sample_count() const;

	void begin_decode();

    void stop_decode();

    int list_rows_size();

    bool options_changed() const;
    void set_options_changed(bool changed);

    uint64_t sample_count() const;
    uint64_t sample_rate() const;

    bool out_of_memory() const;

private:
    boost::optional<uint64_t> wait_for_data() const;

//    void decode_data(const uint64_t sample_count,
//        const unsigned int unit_size, srd_session *const session);

    void decode_data(const uint64_t decode_start, const uint64_t decode_end,
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
	 * This mutex prevents more than one decode operation occuring
	 * concurrently.
	 * @todo A proper solution should be implemented to allow multiple
	 * decode operations.
	 */
    static boost::mutex _global_decode_mutex;

	std::list< boost::shared_ptr<decode::Decoder> > _stack;

	boost::shared_ptr<pv::data::LogicSnapshot> _snapshot;

    //mutable boost::mutex _input_mutex;
    //mutable boost::condition_variable _input_cond;
    uint64_t _sample_count;
	bool _frame_complete;

    mutable boost::recursive_mutex _output_mutex;
    //mutable boost::mutex _output_mutex;
	int64_t	_samples_decoded;

    std::map<const decode::Row, decode::RowData> _rows;
    std::map<const decode::Row, bool> _rows_gshow;
    std::map<const decode::Row, bool> _rows_lshow;
    std::map<std::pair<const srd_decoder*, int>, decode::Row> _class_rows;

	QString _error_message;

    std::unique_ptr<boost::thread> _decode_thread;
    decode_state _decode_state;

    bool _options_changed;
    bool _no_memory;

	friend class DecoderStackTest::TwoDecoderStack;
};

} // namespace data
} // namespace pv

#endif // DSVIEW_PV_DATA_DECODERSTACK_H
