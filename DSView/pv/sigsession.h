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
 
#include <set>
#include <string>
#include <vector>
#include <stdint.h> 
#include <QString>
#include <thread>
#include <QDateTime>
#include <list>

#include "view/mathtrace.h"
#include "data/mathstack.h"
#include "interface/icallbacks.h"
#include "dstimer.h"
#include <libsigrok.h>
#include "deviceagent.h"
#include "eventobject.h"
#include "data/logicsnapshot.h"
#include "data/analogsnapshot.h"
#include "data/dsosnapshot.h"
 
struct srd_decoder;
struct srd_channel;
class DecoderStatus;

typedef std::lock_guard<std::mutex> ds_lock_guard;

namespace pv {

namespace data {
class SignalData;
class Snapshot;
class AnalogSnapshot;
class DsoSnapshot;
class LogicSnapshot;
class DecoderModel;
class MathStack;

namespace decode {
    class Decoder;
  }
}

namespace view {
class Signal;
class GroupSignal;
class DecodeTrace;
class SpectrumTrace;
class LissajousTrace;
class MathTrace;
}

enum DEVICE_STATUS_TYPE
{
    ST_INIT = 0,
    ST_RUNNING = 1,
    ST_STOPPED = 2,
};

enum DEVICE_COLLECT_MODE
{
    COLLECT_SINGLE = 0,
    COLLECT_REPEAT = 1, 
    COLLECT_LOOP = 2,
}; 

class SessionData
{
public:
    SessionData(); 

    inline data::LogicSnapshot* get_logic(){
        return &logic;
    }

    inline data::AnalogSnapshot* get_analog(){
        return &analog;
    }

    inline data::DsoSnapshot* get_dso(){
        return &dso;
    }

    void clear();

public:
    uint64_t       _cur_snap_samplerate;
    uint64_t       _cur_samplelimits;
    uint64_t       _trig_pos;

private:
    data::LogicSnapshot   logic;
    data::AnalogSnapshot  analog;
    data::DsoSnapshot     dso;
};

using namespace pv::data;

//created by MainWindow
class SigSession:
    public IMessageListener,
    public IDeviceAgentCallback
{
private:
    static constexpr float Oversampling = 2.0f;

public:
    static const int RefreshTime = 500;
    static const int RepeatHoldDiv = 20;
    static const int FeedInterval = 50;
    static const int WaitShowTime = 500;

   enum SESSION_ERROR_STATUS {
        No_err,
        Hw_err,
        Malloc_err, 
        Test_timeout_err,
        Pkt_data_err,
        Data_overflow
    };

private:
    SigSession(SigSession &o);
  
public:
    explicit SigSession();

	~SigSession(); 

    inline DeviceAgent* get_device(){
        return &_device_agent;
    }

    inline void set_callback(ISessionCallback *callback){
        _callback = callback;
    }

    bool init();
    void uninit();
    void Open();
    void Close();
    
    bool set_default_device();
    bool set_device(ds_device_handle dev_handle);
    bool set_file(QString name);
    void close_file(ds_device_handle dev_handle);
    bool start_capture(bool instant);
    bool stop_capture();
    bool switch_work_mode(int mode);

    uint64_t cur_samplerate();
    uint64_t cur_snap_samplerate();
    uint64_t cur_samplelimits();
    double cur_sampletime();
    double cur_snap_sampletime();
    double cur_view_time();

    inline bool re_start(){
        if (_is_working)
            stop_capture();
        return start_capture(_is_instant);
    } 

    inline QDateTime get_session_time(){
        return _session_time;
    }

    inline QDateTime get_trig_time(){
        return _trig_time;
    }

    inline bool is_triged(){
        return _is_triged;
    }

    inline uint64_t get_trigger_pos(){
        return _view_data->_trig_pos;
    }

    bool is_first_store_confirm();
    bool get_capture_status(bool &triggered, int &progress);

    inline void clear_store_confirm_flag(){
        _confirm_store_time_id = _work_time_id;
    }

	std::vector<view::Signal*>& get_signals(); 

    bool add_decoder(srd_decoder *const dec, bool silent, DecoderStatus *dstatus, 
                        std::list<pv::data::decode::Decoder*> &sub_decoders, view::Trace* &out_trace);
    int get_trace_index_by_key_handel(void *handel);
    void remove_decoder(int index);
    void remove_decoder_by_key_handel(void *handel); 

    inline std::vector<view::DecodeTrace*>& get_decode_signals(){
        return _decode_traces;
    }

    void rst_decoder(int index); 
    void rst_decoder_by_key_handel(void *handel);

    inline pv::data::DecoderModel* get_decoder_model(){
         return _decoder_model;
    }

    inline std::vector<view::SpectrumTrace*>& get_spectrum_traces(){
        return _spectrum_traces;
    }

    inline view::LissajousTrace* get_lissajous_trace(){
        return _lissajous_trace;
    }

    inline view::MathTrace* get_math_trace(){
        return _math_trace;
    }
 
    uint16_t get_ch_num(int type); 
 
    inline bool is_data_lock(){
        return _data_lock;
    }

    void data_auto_lock(int lock);
    void data_auto_unlock();
    bool get_data_auto_lock();
    void spectrum_rebuild();
    void lissajous_rebuild(bool enable, int xindex, int yindex, double percent);
    void lissajous_disable();

    void math_rebuild(bool enable,pv::view::DsoSignal *dsoSig1,
                      pv::view::DsoSignal *dsoSig2,
                      data::MathStack::MathType type);

    inline bool trigd(){
        return _trigger_flag;
    }

    inline uint8_t trigd_ch(){
        return _trigger_ch;
    }

    data::Snapshot* get_snapshot(int type);

    inline SESSION_ERROR_STATUS get_error(){
        return _error;
    }

    inline void set_error(SESSION_ERROR_STATUS state){
        _error = state;
    }

    void clear_error();

    inline uint64_t get_error_pattern(){
        return _error_pattern;
    }

    inline double get_repeat_intvl(){
        return _repeat_intvl;    
    }

    inline void set_repeat_intvl(double interval){
        _repeat_intvl = interval;
    }
   
    int get_repeat_hold();

    inline void set_save_start(uint64_t start){
        _save_start = start;
    }

    inline uint64_t get_save_start(){
        return _save_start;
    }

    inline void set_save_end(uint64_t end){
        _save_end = end;
    }

    inline uint64_t get_save_end(){
        return _save_end;
    }

    void clear_all_decoder(bool bUpdateView = true); 

    inline bool is_closed(){
        return _bClose;
    }

    inline bool is_instant(){
        return _is_instant;
    }

    inline bool is_working(){
        return _is_working || _device_status == ST_RUNNING;
    }

    inline bool is_init_status(){
        return _device_status == ST_INIT;
    }

    // The collect thread is running.
    inline bool is_running_status(){
        return _device_status == ST_RUNNING;
    }

    inline bool is_stopped_status(){
        return _device_status == ST_STOPPED;
    }

    void set_collect_mode(DEVICE_COLLECT_MODE m);

    inline int get_collect_mode(){
        return (int)_clt_mode;
    }

    inline bool is_repeat_mode(){
        return _clt_mode == COLLECT_REPEAT;
    }

    inline bool is_single_mode(){
        return _clt_mode == COLLECT_SINGLE;
    }

    inline bool is_loop_mode(){
        return _clt_mode == COLLECT_LOOP;
    }

    bool is_realtime_refresh();

    inline bool is_repeating(){
        return _clt_mode == COLLECT_REPEAT && !_is_instant;
    }

    inline void session_save(){
        _callback->session_save();
    }

    inline void show_region(uint64_t start, uint64_t end, bool keep){
        _callback->show_region(start, end, keep);
    }

    inline void decode_done(){
        _callback->decode_done();
    }

    inline bool is_saving(){
        return _is_saving;
    }

    inline void set_saving(bool flag){
        _is_saving = flag;
    }

    inline DeviceEventObject* device_event_object(){
        return &_device_event;
    }
   
    void reload();
    void refresh(int holdtime);  
    void check_update(); 

    inline void set_map_zoom(int index){
        _map_zoom = index;
    }

    inline int get_map_zoom(){
        return _map_zoom;
    }

    inline bool is_single_buffer(){
        return _view_data == _capture_data;
    }

    inline void update_view(){
        _callback->data_updated();
    }

    void auto_end();
    bool have_hardware_data();
    struct ds_device_base_info* get_device_list(int &out_count, int &actived_index);
    void add_msg_listener(IMessageListener *ln);
    void broadcast_msg(int msg);    
    bool have_new_realtime_refresh(bool keep);
    view::DecodeTrace* get_decoder_trace(int index);
    view::Signal* get_signal_by_index(int index);

    inline bool have_view_data(){
        return get_signal_snapshot()->have_data();
    }

    void on_load_config_end();
    void init_signals(); 

    inline bool is_doing_action(){
        return _is_action;
    }

    void clear_view_data();
    void set_trace_name(view::Trace *trace, QString name);
    void set_decoder_row_label(int index, QString label);

    inline void set_decoder_pannel(IDecoderPannel *pannel){
        _decoder_pannel = pannel;
    }

    void update_dso_data_scale();

    inline sr_status get_dso_status(){
        return _dso_status;
    }

    inline bool dso_status_is_valid(){
        return _dso_status_valid;
    }

    double get_logic_data_view_time();

    int64_t get_ring_sample_count();

    inline bool dso_data_is_out_off_range(){
        return _view_data->get_dso()->data_is_out_off_range();
    }

    void update_lang_text();

    bool have_decoded_result();

    void apply_samplerate();

    void ProcessPowerEvent(bool bEnterSleep);

    inline void set_capture_work_time(uint64_t tm){
        _capture_work_time = tm;
    }

private:
    void set_cur_samplelimits(uint64_t samplelimits);
    void set_cur_snap_samplerate(uint64_t samplerate);
    void math_disable();

    bool exec_capture();
    void exit_capture();

    inline void data_updated(){
        _callback->data_updated();
    }

    inline void signals_changed(){
        _callback->signals_changed();
    }

    inline void set_receive_data_len(quint64 len){
        _callback->receive_data_len(len);
    }
  
    void add_decode_task(view::DecodeTrace *trace);
    void remove_decode_task(view::DecodeTrace *trace);
    void clear_all_decode_task(int &runningDex);

    inline void clear_all_decode_task2(){
        int run_dex = 0;
        clear_all_decode_task(run_dex);
    }
   
    void decode_task_proc();
    view::DecodeTrace* get_top_decode_task();    

    void capture_init(); 
    void nodata_timeout();
    void feed_timeout();    
    void clear_decode_result();
    void attach_data_to_signal(SessionData *data);

    bool action_start_capture(bool instant);
    bool action_stop_capture();

    inline void set_session_time(QDateTime time){
        _session_time = time;
    }
  
    //IMessageListener
    void OnMessage(int msg) override;

    //IDeviceAgentCallback
    void DeviceConfigChanged() override;

private:
    /**
     * Attempts to autodetect the format. Failing that
     * @param filename The filename of the input file.
     * @return A pointer to the 'struct sr_input_format' that should be
     * 	used, or NULL if no input format was selected or
     * 	auto-detected.
     */
    static sr_input_format* determine_input_file_format(const std::string &filename); 

    // data feed
	void feed_in_header(const sr_dev_inst *sdi);
	void feed_in_meta(const sr_dev_inst *sdi, const sr_datafeed_meta &meta);
    void feed_in_trigger(const ds_trigger_pos &trigger_pos);
	void feed_in_logic(const sr_datafeed_logic &o);

    void feed_in_dso(const sr_datafeed_dso &o);
	void feed_in_analog(const sr_datafeed_analog &o);    
	void data_feed_in(const struct sr_dev_inst *sdi,
		        const struct sr_datafeed_packet *packet); 

	static void data_feed_callback(const struct sr_dev_inst *sdi,
		        const struct sr_datafeed_packet *packet);

    static void device_lib_event_callback(int event);
    
    void on_device_lib_event(int event);
    Snapshot* get_signal_snapshot();
    void repeat_capture_wait_timeout();
    void repeat_wait_prog_timeout();
    void realtime_refresh_timeout();
    void trig_check_timeout();

    void clear_signals(); 

    inline void data_lock(){
        _data_lock = true;
    }

    inline void data_unlock(){
        _data_lock = false;
    }

    view::Trace* get_channel_by_index(int orgIndex);
    void make_channels_view_index(int start_dex = -1);
 
private:
    mutable std::mutex      _sampling_mutex;
    mutable std::mutex      _data_mutex;
    mutable std::mutex      _decode_task_mutex;  
    std::thread             _decode_thread;
    volatile bool           _is_decoding;
 
	std::vector<view::Signal*>      _signals; 
    std::vector<view::DecodeTrace*> _decode_traces;
    std::vector<view::DecodeTrace*> _decode_tasks;
    pv::data::DecoderModel          *_decoder_model;
    std::vector<view::SpectrumTrace*> _spectrum_traces;
    view::LissajousTrace            *_lissajous_trace;
    view::MathTrace                 *_math_trace;
  
    DsTimer     _feed_timer;
    DsTimer     _out_timer;
    DsTimer     _repeat_timer;
    DsTimer     _repeat_wait_prog_timer;
    DsTimer     _refresh_rt_timer;
    DsTimer     _trig_check_timer;
   
    int         _noData_cnt;
    bool        _data_lock;
    bool        _data_updated;
    int         _data_auto_lock;

    QDateTime   _session_time;
    QDateTime   _trig_time;
    bool        _is_triged;
    bool        _trigger_flag;
    uint8_t     _trigger_ch;
    bool        _hw_replied;

    SESSION_ERROR_STATUS _error;
    uint64_t    _error_pattern;
    int         _map_zoom;  
    bool        _bClose;  
 
    uint64_t    _save_start;
    uint64_t    _save_end; 
    volatile bool  _is_working;
    double      _repeat_intvl; // The progress wait timer interval.
    int         _repeat_hold_prg; // The time sleep progress
    int         _repeat_wait_prog_step;
    bool        _is_saving;
    bool        _is_instant;
    volatile int  _device_status;
    int         _work_time_id;
    int         _capture_times; 
    int         _confirm_store_time_id;
    uint64_t    _rt_refresh_time_id;
    uint64_t    _rt_ck_refresh_time_id;
    DEVICE_COLLECT_MODE    _clt_mode;
    bool        _is_stream_mode;
    
    bool        _is_action;
    uint64_t    _dso_packet_count;
    bool        _is_task_end;
 

    ISessionCallback *_callback;
    DeviceAgent   _device_agent;
    std::vector<IMessageListener*> _msg_listeners;
    DeviceEventObject   _device_event;
    SessionData       *_view_data;
    SessionData       *_capture_data;
    std::vector<SessionData*> _data_list;
    IDecoderPannel  *_decoder_pannel;
    sr_status       _dso_status;
    bool            _dso_status_valid;
    uint64_t        _capture_work_time;
   
private:
	// TODO: This should not be necessary. Multiple concurrent
	// sessions should should be supported and it should be
	// possible to associate a pointer with a ds_session.
	static SigSession *_session;
};

} // namespace pv

#endif // DSVIEW_PV_SIGSESSION_H
