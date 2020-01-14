/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2013 DreamSourceLab <support@dreamsourcelab.com>
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

#ifndef DSVIEW_PV_SIGSESSION_H
#define DSVIEW_PV_SIGSESSION_H

#include <libsigrok4DSL/libsigrok.h>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/thread.hpp>

#include <string>
#include <utility>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <stdint.h>

#include <QObject>
#include <QString>
#include <QLine>
#include <QVector>
#include <QMap>
#include <QVariant>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>
#include <QJsonObject>

#include <libusb.h>

#include "view/mathtrace.h"
#include "data/mathstack.h"

struct srd_decoder;
struct srd_channel;

namespace pv {

class DeviceManager;

namespace data {
class SignalData;
class Snapshot;
class Analog;
class AnalogSnapshot;
class Dso;
class DsoSnapshot;
class Logic;
class LogicSnapshot;
class Group;
class GroupSnapshot;
class DecoderModel;
class MathStack;
}

namespace device {
class DevInst;
}

namespace view {
class Signal;
class GroupSignal;
class DecodeTrace;
class SpectrumTrace;
class LissajousTrace;
class MathTrace;
}

namespace decoder {
class Decoder;
class DecoderFactory;
}

class SigSession : public QObject
{
        Q_OBJECT

private:
    static constexpr float Oversampling = 2.0f;
    static const int RefreshTime = 500;
    static const int RepeatHoldDiv = 20;

public:
    static const int FeedInterval = 50;
    static const int WaitShowTime = 500;

public:
	enum capture_state {
        Init,
		Stopped,
		Running
	};

    enum run_mode {
        Single,
        Repetitive
    };

    enum error_state {
        No_err,
        Hw_err,
        Malloc_err,
        Test_data_err,
        Test_timeout_err,
        Pkt_data_err,
        Data_overflow
    };

public:
	SigSession(DeviceManager &device_manager);

	~SigSession();

    boost::shared_ptr<device::DevInst> get_device() const;

	/**
	 * Sets device instance that will be used in the next capture session.
	 */
    void set_device(boost::shared_ptr<device::DevInst> dev_inst);

    void set_file(QString name);

    void close_file(boost::shared_ptr<pv::device::DevInst> dev_inst);

    void set_default_device(boost::function<void (const QString)> error_handler);

    void release_device(device::DevInst *dev_inst);

	capture_state get_capture_state() const;

    uint64_t cur_samplerate() const;
    uint64_t cur_snap_samplerate() const;
    uint64_t cur_samplelimits() const;
    double cur_sampletime() const;
    double cur_snap_sampletime() const;
    double cur_view_time() const;

    void set_cur_snap_samplerate(uint64_t samplerate);
    void set_cur_samplelimits(uint64_t samplelimits);
    void set_session_time(QDateTime time);
    QDateTime get_session_time() const;
    uint64_t get_trigger_pos() const;

    void start_capture(bool instant,
		boost::function<void (const QString)> error_handler);
    void capture_init();
    bool get_capture_status(bool &triggered, int &progress);
    void container_init();

    std::set< boost::shared_ptr<data::SignalData> > get_data() const;

	std::vector< boost::shared_ptr<view::Signal> >
		get_signals();

    std::vector< boost::shared_ptr<view::GroupSignal> >
        get_group_signals();

#ifdef ENABLE_DECODE
    bool add_decoder(srd_decoder *const dec, bool silent);

    std::vector< boost::shared_ptr<view::DecodeTrace> >
        get_decode_signals() const;

    void remove_decode_signal(view::DecodeTrace *signal);

    void remove_decode_signal(int index);

    void rst_decoder(int index);

    void rst_decoder(view::DecodeTrace *signal);

    pv::data::DecoderModel* get_decoder_model() const;
#endif

    std::vector< boost::shared_ptr<view::SpectrumTrace> >
        get_spectrum_traces();

    boost::shared_ptr<view::LissajousTrace>
        get_lissajous_trace();

    boost::shared_ptr<view::MathTrace>
        get_math_trace();

    void init_signals();

    void add_group();

    void del_group();

    void start_hotplug_proc(boost::function<void (const QString)> error_handler);
    void stop_hotplug_proc();
	void register_hotplug_callback();
    void deregister_hotplug_callback();

    uint16_t get_ch_num(int type);
    
    bool get_instant();

    bool get_data_lock();
    void data_auto_lock(int lock);
    void data_auto_unlock();
    bool get_data_auto_lock();
    void spectrum_rebuild();
    void lissajous_rebuild(bool enable, int xindex, int yindex, double percent);
    void lissajous_disable();
    void math_rebuild(bool enable,
                      boost::shared_ptr<pv::view::DsoSignal> dsoSig1,
                      boost::shared_ptr<pv::view::DsoSignal> dsoSig2,
                      data::MathStack::MathType type);
    void math_disable();

    bool trigd() const;
    uint8_t trigd_ch() const;

    boost::shared_ptr<data::Snapshot> get_snapshot(int type);

    error_state get_error() const;
    void set_error(error_state state);
    void clear_error();
    uint64_t get_error_pattern() const;

    run_mode get_run_mode() const;
    void set_run_mode(run_mode mode);
    int get_repeat_intvl() const;
    void set_repeat_intvl(int interval);
    bool isRepeating() const;
    bool repeat_check();
    int get_repeat_hold() const;

    int get_map_zoom() const;

    void set_save_start(uint64_t start);
    void set_save_end(uint64_t end);
    uint64_t get_save_start() const;
    uint64_t get_save_end() const;
    bool get_saving() const;
    void set_saving(bool saving);
    void set_stop_scale(float scale);
    float stop_scale() const;

    void exit_capture();

private:
	void set_capture_state(capture_state state);

private:
    /**
     * Attempts to autodetect the format. Failing that
     * @param filename The filename of the input file.
     * @return A pointer to the 'struct sr_input_format' that should be
     * 	used, or NULL if no input format was selected or
     * 	auto-detected.
     */
    static sr_input_format* determine_input_file_format(
        const std::string &filename);

    static sr_input* load_input_file_format(
        const std::string &filename,
        boost::function<void (const QString)> error_handler,
        sr_input_format *format = NULL);

    void sample_thread_proc(boost::shared_ptr<device::DevInst> dev_inst,
                            boost::function<void (const QString)> error_handler);

    // data feed
	void feed_in_header(const sr_dev_inst *sdi);
	void feed_in_meta(const sr_dev_inst *sdi,
		const sr_datafeed_meta &meta);
    void feed_in_trigger(const ds_trigger_pos &trigger_pos);
	void feed_in_logic(const sr_datafeed_logic &logic);
    void feed_in_dso(const sr_datafeed_dso &dso);
	void feed_in_analog(const sr_datafeed_analog &analog);
	void data_feed_in(const struct sr_dev_inst *sdi,
		const struct sr_datafeed_packet *packet);
	static void data_feed_in_proc(const struct sr_dev_inst *sdi,
		const struct sr_datafeed_packet *packet, void *cb_data);

    // thread for hotplug
    void hotplug_proc(boost::function<void (const QString)> error_handler);
    static int hotplug_callback(struct libusb_context *ctx, struct libusb_device *dev,
                                libusb_hotplug_event event, void *user_data);

private:
	DeviceManager &_device_manager;

	/**
	 * The device instance that will be used in the next capture session.
	 */
    boost::shared_ptr<device::DevInst> _dev_inst;

    mutable boost::mutex _sampling_mutex;
	capture_state _capture_state;
    bool _instant;
    uint64_t _cur_snap_samplerate;
    uint64_t _cur_samplelimits;

    //mutable boost::mutex _signals_mutex;
	std::vector< boost::shared_ptr<view::Signal> > _signals;
    std::vector< boost::shared_ptr<view::GroupSignal> > _group_traces;
#ifdef ENABLE_DECODE
    std::vector< boost::shared_ptr<view::DecodeTrace> > _decode_traces;
    pv::data::DecoderModel *_decoder_model;
#endif
    std::vector< boost::shared_ptr<view::SpectrumTrace> > _spectrum_traces;
    boost::shared_ptr<view::LissajousTrace> _lissajous_trace;
    boost::shared_ptr<view::MathTrace> _math_trace;

    mutable boost::mutex _data_mutex;
	boost::shared_ptr<data::Logic> _logic_data;
	boost::shared_ptr<data::LogicSnapshot> _cur_logic_snapshot;
    boost::shared_ptr<data::Dso> _dso_data;
    boost::shared_ptr<data::DsoSnapshot> _cur_dso_snapshot;
	boost::shared_ptr<data::Analog> _analog_data;
	boost::shared_ptr<data::AnalogSnapshot> _cur_analog_snapshot;
    boost::shared_ptr<data::Group> _group_data;
    boost::shared_ptr<data::GroupSnapshot> _cur_group_snapshot;
    int _group_cnt;

	std::unique_ptr<boost::thread> _sampling_thread;

	libusb_hotplug_callback_handle _hotplug_handle;
    std::unique_ptr<boost::thread> _hotplug;
    bool _hot_attach;
    bool _hot_detach;

    QTimer _feed_timer;
    int    _noData_cnt;
    bool _data_lock;
    bool _data_updated;
    int _data_auto_lock;

    QDateTime _session_time;
    uint64_t _trigger_pos;
    bool _trigger_flag;
    uint8_t _trigger_ch;
    bool _hw_replied;

    error_state _error;
    uint64_t _error_pattern;

    run_mode _run_mode;
    int _repeat_intvl;
    bool _repeating;
    int _repeat_hold_prg;

    int _map_zoom;

    uint64_t _save_start;
    uint64_t _save_end;
    bool _saving;

    bool _dso_feed;
    float _stop_scale;

signals:
	void capture_state_changed(int state);

	void signals_changed();

	void data_updated();

    void receive_data(quint64 length);

    void device_attach();
    void device_detach();

    void receive_trigger(quint64 trigger_pos);

    void receive_header();

    void dso_ch_changed(uint16_t num);

    void frame_began();

    void data_received();

    void frame_ended();

    void device_setted();

    void zero_adj();
    void progressSaveFileValueChanged(int percent);

    void decode_done();

    void show_region(uint64_t start, uint64_t end, bool keep);

    void show_wait_trigger();

    void session_save();

    void session_error();

    void repeat_hold(int percent);
    void repeat_resume();

    void cur_snap_samplerate_changed();

    void update_capture();

public slots:
    void reload();
    void refresh(int holdtime);
    void stop_capture();
    void check_update();
    // repeat
    void set_repeating(bool repeat);
    void set_map_zoom(int index);
    // OSC auto
    void auto_end();

private slots:
    void data_lock();
    void data_unlock();
    void nodata_timeout();
    void feed_timeout();
    void repeat_update();

private:
	// TODO: This should not be necessary. Multiple concurrent
	// sessions should should be supported and it should be
	// possible to associate a pointer with a ds_session.
	static SigSession *_session;
};

} // namespace pv

#endif // DSVIEW_PV_SIGSESSION_H
