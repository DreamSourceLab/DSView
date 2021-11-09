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

class DecoderStatus;

typedef std::lock_guard<std::mutex> ds_lock_guard;

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

using namespace pv::device;

//created by MainWindow
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

private:
   // SigSession(SigSession &o);
  
public:
    explicit SigSession(DeviceManager *device_manager);

	~SigSession(); 

    DevInst* get_device();

	/**
	 * Sets device instance that will be used in the next capture session.
	 */
    void set_device(DevInst *dev_inst);

    void deselect_device();

    void set_file(QString name);

    void close_file(DevInst *dev_inst);

    void set_default_device(boost::function<void (const QString)> error_handler);

    void release_device(DevInst *dev_inst);

	capture_state get_capture_state();

    uint64_t cur_samplerate();

    uint64_t cur_snap_samplerate();

    uint64_t cur_samplelimits();

    double cur_sampletime();

    double cur_snap_sampletime();

    double cur_view_time();

    void set_cur_snap_samplerate(uint64_t samplerate);

    void set_cur_samplelimits(uint64_t samplelimits);

    void set_session_time(QDateTime time);

    QDateTime get_session_time();

    uint64_t get_trigger_pos();

    void start_capture(bool instant, boost::function<void (const QString)> error_handler);

    bool get_capture_status(bool &triggered, int &progress);

    void container_init();

    std::set<data::SignalData*> get_data();

	std::vector<view::Signal*>& get_signals();

    std::vector<view::GroupSignal*>& get_group_signals();

    bool add_decoder(srd_decoder *const dec, bool silent, DecoderStatus *dstatus);

    void remove_decoder(int index);
  
    std::vector<view::DecodeTrace*>& get_decode_signals();
 
    void rst_decoder(int index);
 
    pv::data::DecoderModel* get_decoder_model();

    std::vector<view::SpectrumTrace*>& get_spectrum_traces();

    view::LissajousTrace* get_lissajous_trace();

    view::MathTrace* get_math_trace();

    void init_signals();

    void add_group();

    void del_group();

    void start_hotplug_work(boost::function<void (const QString)> error_handler);

    void stop_hotplug_work();	

    uint16_t get_ch_num(int type);
    
    bool get_instant();

    bool get_data_lock();

    void data_auto_lock(int lock);

    void data_auto_unlock();

    bool get_data_auto_lock();

    void spectrum_rebuild();

    void lissajous_rebuild(bool enable, int xindex, int yindex, double percent);

    void lissajous_disable();

    void math_rebuild(bool enable,pv::view::DsoSignal *dsoSig1,
                      pv::view::DsoSignal *dsoSig2,
                      data::MathStack::MathType type);

    void math_disable();

    bool trigd();

    uint8_t trigd_ch();

    data::Snapshot* get_snapshot(int type);

    error_state get_error();

    void set_error(error_state state);

    void clear_error();

    uint64_t get_error_pattern();

    run_mode get_run_mode();

    void set_run_mode(run_mode mode);

    int get_repeat_intvl();

    void set_repeat_intvl(int interval);

    bool isRepeating();

    bool repeat_check();

    int get_repeat_hold();

    int get_map_zoom();

    void set_save_start(uint64_t start);

    void set_save_end(uint64_t end);

    uint64_t get_save_start();

    uint64_t get_save_end();

    bool get_saving();

    void set_saving(bool saving);

    void set_stop_scale(float scale);

    float stop_scale();

    void exit_capture();

    sr_dev_inst* get_dev_inst_c();

    void Open();

    void Close();

    void clear_all_decoder(); 

    inline bool is_closed(){
        return _bClose;
    }
 
private:
	void set_capture_state(capture_state state);

    void register_hotplug_callback();

    void deregister_hotplug_callback();

    bool do_add_decoder(srd_decoder *const dec, bool silent, DecoderStatus *dstatus);

    void add_decode_task(view::DecodeTrace *trace);

    void remove_decode_task(view::DecodeTrace *trace);

    void clear_all_decode_task(int &runningDex); 
  
    view::DecodeTrace* get_decoder_trace(int index);

    void decode_task_proc();

    view::DecodeTrace* get_top_decode_task();
    
    void capture_init();

    void do_stop_capture();

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

    void sample_thread_proc(DevInst *dev_inst,
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
	DeviceManager   *_device_manager;

	/**
	 * The device instance that will be used in the next capture session.
	 */
    DevInst                 *_dev_inst;
    mutable std::mutex      _sampling_mutex;
    mutable std::mutex      _data_mutex;
    mutable std::mutex      _decode_task_mutex;
 
    std::thread             _hotplug_thread;
    std::thread             _sampling_thread;   
    std::thread             _decode_thread;

    volatile bool           _bHotplugStop;
    volatile bool           _bDecodeRunning;

	capture_state           _capture_state;
    bool                    _instant;
    uint64_t                _cur_snap_samplerate;
    uint64_t                _cur_samplelimits;
 
	std::vector<view::Signal*>      _signals;
    std::vector<view::GroupSignal*> _group_traces;

    std::vector<view::DecodeTrace*> _decode_traces;
    std::vector<view::DecodeTrace*> _decode_tasks;
    pv::data::DecoderModel          *_decoder_model;

    std::vector<view::SpectrumTrace*> _spectrum_traces;
    view::LissajousTrace            *_lissajous_trace;
    view::MathTrace                 *_math_trace;
  
	data::Logic              *_logic_data; 
    data::Dso                *_dso_data; 
	data::Analog             *_analog_data;
    data::Group              *_group_data; 
    int                      _group_cnt;
 
	libusb_hotplug_callback_handle _hotplug_handle;
    
    bool        _hot_attach;
    bool        _hot_detach;

    QTimer      _feed_timer;
    int         _noData_cnt;
    bool        _data_lock;
    bool        _data_updated;
    int         _data_auto_lock;

    QDateTime   _session_time;
    uint64_t    _trigger_pos;
    bool        _trigger_flag;
    uint8_t     _trigger_ch;
    bool        _hw_replied;

    error_state _error;
    uint64_t    _error_pattern;

    run_mode    _run_mode;
    int         _repeat_intvl;
    bool        _repeating;
    int         _repeat_hold_prg;

    int         _map_zoom;

    uint64_t    _save_start;
    uint64_t    _save_end;
    bool        _saving;

    bool        _dso_feed;
    float       _stop_scale; 
    bool        _bClose;

private:
	// TODO: This should not be necessary. Multiple concurrent
	// sessions should should be supported and it should be
	// possible to associate a pointer with a ds_session.
	static SigSession *_session;
};

} // namespace pv

#endif // DSVIEW_PV_SIGSESSION_H
