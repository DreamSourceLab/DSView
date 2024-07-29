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

#include <libsigrokdecode.h>

#include "sigsession.h"
#include "mainwindow.h"

#include "data/analogsnapshot.h"
#include "data/dsosnapshot.h"
#include "data/logicsnapshot.h"
#include "data/decoderstack.h"
#include "data/decode/decoder.h"
#include "data/decodermodel.h"
#include "data/spectrumstack.h"
#include "data/mathstack.h"

#include "view/analogsignal.h"
#include "view/dsosignal.h"
#include "view/logicsignal.h"
#include "view/groupsignal.h"
#include "view/decodetrace.h"
#include "view/spectrumtrace.h"
#include "view/lissajoustrace.h"
#include "view/mathtrace.h"

#include <assert.h>
#include <stdexcept>
#include <sys/stat.h>
#include <map>
#include <QString>

#include "data/decode/decoderstatus.h"
#include "dsvdef.h"
#include "log.h"
#include "config/appconfig.h"
#include "utility/path.h"
#include "ui/msgbox.h"
#include "ui/langresource.h"

namespace pv
{
    SessionData::SessionData()
    {
        _cur_snap_samplerate = 0;
        _cur_samplelimits = 0;
        _trig_pos = 0;
    }

    void SessionData::clear()
    {
        logic.clear();
        analog.clear();
        dso.clear();
        _trig_pos = 0;
    }

    // TODO: This should not be necessary
    SigSession *SigSession::_session = NULL;

    SigSession::SigSession()
    {
        // TODO: This should not be necessary
        _session = this; 

        _map_zoom = 0;
        _repeat_hold_prg = 0;
        _repeat_intvl = 1;
        _error = No_err;
        _is_instant = false;
        _is_working = false;
        _is_saving = false;
        _device_status = ST_INIT;
        _noData_cnt = 0;
        _data_lock = false;
        _data_updated = false;
        _clt_mode = COLLECT_SINGLE;
        _rt_refresh_time_id = 0;
        _rt_ck_refresh_time_id = 0;
        _view_data = NULL;
        _capture_data = NULL;
        _is_stream_mode = false;
        _is_action = false;
        _decoder_pannel = NULL;
        _is_triged = false;
        _dso_status_valid = false;
        _is_task_end = false;
        _capture_work_time = 0;

        _data_list.push_back(new SessionData());
        _data_list.push_back(new SessionData());
        _view_data = _data_list[0];
        _capture_data = _data_list[0];

        this->add_msg_listener(this);

        _decoder_model = new pv::data::DecoderModel(NULL);

        _lissajous_trace = NULL;
        _math_trace = NULL;
        _is_decoding = false;
        _bClose = false;
        _callback = NULL;
        _work_time_id = 0;
        _capture_times = 0;
        _confirm_store_time_id = 0;
        _repeat_wait_prog_step = 10;

        _device_agent.set_callback(this);

        _feed_timer.SetCallback(std::bind(&SigSession::feed_timeout, this));
        _repeat_timer.SetCallback(std::bind(&SigSession::repeat_capture_wait_timeout, this));
        _repeat_wait_prog_timer.SetCallback(std::bind(&SigSession::repeat_wait_prog_timeout, this));
        _refresh_rt_timer.SetCallback(std::bind(&SigSession::realtime_refresh_timeout, this));       
        _trig_check_timer.SetCallback(std::bind(&SigSession::trig_check_timeout, this));
    }

    SigSession::SigSession(SigSession &o)
    {
        (void)o;
    }

    SigSession::~SigSession()
    {
        for(auto p : _data_list){
            p->clear();
            delete p;
        }
        _data_list.clear();
    }

    bool SigSession::init()
    {
        ds_log_set_context(dsv_log_context());

        ds_set_event_callback(device_lib_event_callback);

        ds_set_datafeed_callback(data_feed_callback);

        // firmware resource directory
        QString resdir = GetFirmwareDir();
        std::string res_path = pv::path::ToUnicodePath(resdir);
        ds_set_firmware_resource_dir(res_path.c_str());

        if (ds_lib_init() != SR_OK)
        {
            dsv_err("DSView run ERROR: collect lib init failed.");
            return false;
        }

        return true;
    }

    void SigSession::uninit()
    {
        this->Close();

        ds_lib_exit();
    }

    bool SigSession::set_default_device()
    {
        assert(!_is_saving);
        
        if (_is_working){
            dsv_info("SigSession::set_default_device()，The current device is working, now to stop it.");
            dsv_info("SigSession::set_default_device(), stop capture");
            stop_capture();
        }

        struct ds_device_base_info *array = NULL;
        int count = 0;

        dsv_info("Set default device.");

        if (ds_get_device_list(&array, &count) != SR_OK)
        {
            dsv_err("Get device list error!");
            return false;
        }
        if (count < 1 || array == NULL)
        {
            dsv_err("Error! Device list is empty, can't set default device.");
            return false;
        }

        struct ds_device_base_info *dev = (array + count - 1);
        ds_device_handle dev_handle = dev->handle;

        g_free(array);

        if (set_device(dev_handle))
        {
            return true;
        }
        return false;
    }

    bool SigSession::set_device(ds_device_handle dev_handle)
    {
        assert(!_is_saving);
        assert(!_is_working);
        assert(_callback);

        ds_device_handle old_dev = _device_agent.handle();
 
        _callback->trigger_message(DSV_MSG_CURRENT_DEVICE_CHANGE_PREV);
        // Release the old device.
        _device_agent.release();
        _device_status = ST_INIT;

        if (ds_active_device(dev_handle) != SR_OK)
        {
            dsv_err("Switch device error!");
            return false;
        }

        _device_agent.update();
        set_collect_mode(COLLECT_SINGLE);

        if (_device_agent.is_file()){
            std::string dev_name = pv::path::ToUnicodePath(_device_agent.name());
            dsv_info("Switch to file \"%s\" done.", dev_name.c_str());
        }
        else
            dsv_info("Switch to device \"%s\" done.", _device_agent.name().toUtf8().data());

        clear_all_decoder();

        _view_data->clear();
        _capture_data->clear();
        _capture_data = _view_data;
 
        init_signals();

        set_cur_snap_samplerate(_device_agent.get_sample_rate());
        set_cur_samplelimits(_device_agent.get_sample_limit());

        // The current device changed.
        _callback->trigger_message(DSV_MSG_CURRENT_DEVICE_CHANGED);

        int lastError = ds_get_last_error();
        bool ret = true;

        switch (lastError)
        {
            case SR_ERR_DEVICE_FIRMWARE_VERSION_LOW:{
                QString strMsg = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_TO_RECONNECT_FOR_FIRMWARE), 
                        "Please reconnect the device!");
                _callback->delay_prop_msg(strMsg);
                ret = false;
                break;
            }
            case SR_ERR_FIRMWARE_NOT_EXIST:{
                QString strMsg = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_FIRMWARE_NOT_EXIST), 
                    "Firmware not exist!");
                _callback->delay_prop_msg(strMsg);
                ret = false;
                break;
            }
            case SR_ERR_DEVICE_USB_IO_ERROR:{
                QString strMsg = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_DEVICE_USB_IO_ERROR), 
                    "USB io error!");
                _callback->delay_prop_msg(strMsg);
                ret = false;
                break;
            }
            case SR_ERR_DEVICE_IS_EXCLUSIVE:{
                QString strMsg = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_DEVICE_BUSY_SWITCH_FAILED), 
                        "Device is busy!");
                if (old_dev != NULL_HANDLE)
                    MsgBox::Show(strMsg);
                else
                    _callback->delay_prop_msg(strMsg);
                ret = false;
                break;
            }
            case SR_ERR_DEVICE_NO_DRIVER:
            {
                QString strMsg = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_DEVICE_NO_DRIVER), 
                    "No driver!");
                _callback->delay_prop_msg(strMsg);
                ret = false;
                break;
            }
        }

        return ret;
    }

    bool SigSession::set_file(QString name)
    {
        assert(!_is_saving);
        assert(!_is_working);

        std::string file_name = pv::path::ToUnicodePath(name);
        dsv_info("Load file: \"%s\"", file_name.c_str());

        std::string file_str = name.toUtf8().toStdString();

        if (ds_device_from_file(file_str.c_str()) != SR_OK)
        {
            dsv_err("Load file error!");
            return false;
        }

        return set_default_device();
    }

    void SigSession::close_file(ds_device_handle dev_handle)
    {
        assert(dev_handle);

        if (dev_handle == _device_agent.handle() && _is_working)
        {
            dsv_err("The virtual device is running, can't remove it.");
            return;
        }
        bool isCurrent = dev_handle == _device_agent.handle();

        if (ds_remove_device(dev_handle) != SR_OK)
        {
            dsv_err("Remove virtual deivice error!");
        }

        if (isCurrent)
            set_default_device();
    }

    bool SigSession::have_hardware_data()
    {
        if (_device_agent.have_instance() && _device_agent.is_hardware())
        {
            Snapshot *data = get_signal_snapshot();
            return data->have_data();
        }
        return false;
    }

    struct ds_device_base_info *SigSession::get_device_list(int &out_count, int &actived_index)
    {
        out_count = 0;
        actived_index = -1;
        struct ds_device_base_info *array = NULL;

        if (ds_get_device_list(&array, &out_count) == SR_OK)
        {
            actived_index = ds_get_actived_device_index();
            return array;
        }
        return NULL;
    }

    uint64_t SigSession::cur_samplerate()
    {
        // samplerate for current viewport
        if (_device_agent.get_work_mode() == DSO)
            return _device_agent.get_sample_rate();
        else
            return cur_snap_samplerate();
    }

    uint64_t SigSession::cur_snap_samplerate()
    {
        // samplerate for current snapshot
        return _capture_data->_cur_snap_samplerate;
    }

    uint64_t SigSession::cur_samplelimits(){
        return _capture_data->_cur_samplelimits;
    }

    double SigSession::cur_sampletime()
    {
        return cur_samplelimits() * 1.0 / cur_samplerate();
    }

    double SigSession::cur_snap_sampletime()
    {
        return cur_samplelimits() * 1.0 / cur_snap_samplerate();
    }

    double SigSession::get_logic_data_view_time()
    {
        return _view_data->get_logic()->get_ring_sample_count() * 1.0 / cur_snap_samplerate();
    }

    double SigSession::cur_view_time()
    {
        return _device_agent.get_time_base() * DS_CONF_DSO_HDIVS * 1.0 / SR_SEC(1);
    }

    void SigSession::set_cur_snap_samplerate(uint64_t samplerate)
    {
        assert(samplerate != 0);
        
        _capture_data->_cur_snap_samplerate = samplerate;
        _capture_data->get_logic()->set_samplerate(samplerate);
        _capture_data->get_analog()->set_samplerate(samplerate);
        _capture_data->get_dso()->set_samplerate(samplerate);

        int mode = _device_agent.get_work_mode();

        if (mode == DSO)
        { 
            for(auto s : _signals){
                if (s->get_type() == SR_CHANNEL_DSO){
                    view::DsoSignal *ch = (view::DsoSignal*)s;
                    uint64_t k = ch->get_vDial()->get_value();
                    _capture_data->get_dso()->set_measure_voltage_factor(k, ch->get_index()); 
                    _capture_data->get_dso()->set_data_scale(ch->get_scale(), ch->get_index());                                                 
                }
            }
        }
  
        // DecoderStack
        for (auto d : _decode_traces)
        {
            d->decoder()->set_samplerate(samplerate);
        }

        // Math
        if (_math_trace && _math_trace->enabled())
            _math_trace->get_math_stack()->set_samplerate(_device_agent.get_sample_rate());
        // SpectrumStack
        for (auto m : _spectrum_traces){
            m->get_spectrum_stack()->set_samplerate(samplerate);
        }

        _callback->cur_snap_samplerate_changed();
    }

    void SigSession::set_cur_samplelimits(uint64_t samplelimits)
    {
        assert(samplelimits != 0);
        _capture_data->_cur_samplelimits = samplelimits;
    }

    void SigSession::capture_init()
    {
        // update instant setting
        _device_agent.set_config_bool(SR_CONF_INSTANT, _is_instant);
        _callback->update_capture();

        set_cur_snap_samplerate(_device_agent.get_sample_rate());
        set_cur_samplelimits(_device_agent.get_sample_limit());

        _data_updated = false;
        _trigger_flag = false;
        _trigger_ch = 0;
        _hw_replied = false;
        _rt_refresh_time_id = 0;
        _rt_ck_refresh_time_id = 0; 
        _noData_cnt = 0;
        
        data_unlock();

        // Init data container
        _capture_data->clear();

        int mode = _device_agent.get_work_mode();
        if (mode == DSO)
        { 
            for (auto m : _spectrum_traces){ 
                m->get_spectrum_stack()->init();
            }

            if (_math_trace){
                _math_trace->get_math_stack()->init();
            }
        }   

        // update current hw offset
        for (auto s : _signals)
        {  
            if (s->signal_type() == SR_CHANNEL_DSO){   
                auto sig = (view::DsoSignal*)s;
                sig->set_zero_ratio(sig->get_zero_ratio());
                sig->set_stop_scale(1);
            }            
            else if (s->signal_type() == SR_CHANNEL_ANALOG){  
                auto sig = (view::AnalogSignal*)s;
                sig->set_zero_ratio(sig->get_zero_ratio());
            }
        }

        // Start timer 
        if (mode == DSO || mode == ANALOG)
            _feed_timer.Start(FeedInterval);
        else
            _feed_timer.Stop();
    }

    bool SigSession::start_capture(bool instant)
    {
        _is_action = true;
        int ret = action_start_capture(instant);
        _is_action = false;
        return ret;
    }

    bool SigSession::action_start_capture(bool instant)
    {
        assert(_callback);

        dsv_info("Start collect.");

        if (_is_working)
        {
            dsv_err("Error! Is working now.");
            return false;
        }

        if (_signals.empty()){
            dsv_info("ERROR: channel list is empty, unable to capture data.");
            return false;
        }

        // Check that a device instance has been selected.
        if (_device_agent.have_instance() == false)
        {
            dsv_err("Error!No device selected");
            assert(false);
        }
        if (_device_status == ST_RUNNING || _device_agent.is_collecting())
        {
            dsv_err("Error!Device is running.");
            return false;
        }

        clear_all_decode_task2();
        clear_decode_result(); 
        
        _capture_data->clear();
        _view_data->clear();       
        _is_stream_mode = false;
        _capture_times = 0;
        _dso_packet_count = 0;
        _dso_status_valid = false;

        _capture_data = _view_data;
        set_cur_snap_samplerate(_device_agent.get_sample_rate());
        set_cur_samplelimits(_device_agent.get_sample_limit());

        set_session_time(QDateTime::currentDateTime());

        int mode = _device_agent.get_work_mode();
        if (mode == LOGIC)
        {
            if (is_repeat_mode()
                    && _device_agent.is_hardware() 
                    && _device_agent.is_stream_mode()){
                set_repeat_intvl(0.1);
            }

            if (_device_agent.is_hardware()){
                _is_stream_mode = _device_agent.is_stream_mode();
            }
            else if (_device_agent.is_demo() || _device_agent.is_file()){
                _is_stream_mode = true;
            }

            if (is_loop_mode() && !_is_stream_mode){
                set_collect_mode(COLLECT_SINGLE); // Reset the capture mode.
            }

            if (is_loop_mode() && _device_agent.is_demo())
            {
                QString opt_mode = _device_agent.get_demo_operation_mode();
                if (opt_mode != "random"){
                    set_collect_mode(COLLECT_SINGLE);
                }
            }

            if (_device_agent.is_hardware() || _device_agent.is_demo()){
                bool bv = is_loop_mode() && _is_stream_mode;
                _device_agent.set_config_bool(SR_CONF_LOOP_MODE, bv);
            }
        }

        if (mode == DSO && _device_agent.is_hardware()){
            uint32_t ref_max = 0;
            uint32_t ref_min = 0;
            _device_agent.get_config_uint32(SR_CONF_REF_MIN, ref_min);        
            _device_agent.get_config_uint32(SR_CONF_REF_MAX, ref_max);
            _view_data->get_dso()->set_ref_range(ref_max, ref_min);
        }
       
        update_view();

        // update setting
        if (_device_agent.is_file())
            _is_instant = true;
        else
            _is_instant = instant;

        _callback->trigger_message(DSV_MSG_START_COLLECT_WORK_PREV);

        if (exec_capture())
        {   
            _work_time_id++;
            _is_working = true;
            _callback->trigger_message(DSV_MSG_START_COLLECT_WORK);           

            // Start a timer, for able to refresh the view per (1000 / 30)ms.
            if (is_realtime_refresh()){
                _refresh_rt_timer.Start(1000 / 30);
            }

            return true;
        }

        return false;
    }

    bool SigSession::exec_capture()
    {
        if (_device_agent.is_collecting())
        {
            dsv_err("Error!Device is running.");
            return false;
        }

        if (_device_agent.have_enabled_channel() == false)
        {
            QString err_str(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_NO_ENABLED_CHANNEL), "No channels enabled!"));
            MsgBox::Show(err_str);
            return false;
        }
        
        _capture_times++;
        _is_triged = false;

        int mode = _device_agent.get_work_mode();
        bool bAddDecoder = false;
        bool bSwapBuffer = false;
   
        if (mode == DSO || mode == ANALOG)
        {
            // reset measure of dso signal
            for (auto s : _signals){ 
                if (s->signal_type() == SR_CHANNEL_DSO){
                    view::DsoSignal *dsoSig = (view::DsoSignal*)s;
                    dsoSig->set_mValid(false);
                }
            }   
        }
        else
        {    
            if (is_single_mode())
            {
                if (_is_stream_mode)
                    bAddDecoder = true;
            }
            else if (is_repeat_mode())
            { 
                if (_is_stream_mode)
                {
                    if (_capture_times == 1)
                        bAddDecoder = true;
                    else
                        bSwapBuffer = true;
                }
                else{
                    bSwapBuffer = true;
                }             
            }
            else if (is_loop_mode())
            {
                 
            }
        }

        if (mode == LOGIC && _device_agent.is_hardware() 
            && _device_agent.get_hardware_operation_mode() == LO_OP_BUFFER)
        {
            _trig_check_timer.Start(200);
        }

        if (bAddDecoder){
            clear_all_decode_task2();
            clear_decode_result(); 
        }

        // Set the buffer to store the captured data
        if (bSwapBuffer){
            int buf_index = 0;
            for(int i=0; i<(int)_data_list.size(); i++){
                if (_data_list[i] == _view_data){
                    buf_index = i;
                    break;
                }
            }

            buf_index = (buf_index + 1) % 2;
            _capture_data = _data_list[buf_index];
            _capture_data->clear();

            set_cur_snap_samplerate(_device_agent.get_sample_rate());
            set_cur_samplelimits(_device_agent.get_sample_limit());
        }

        capture_init();

        _is_task_end = false;

        if (_device_agent.start() == false){
            dsv_err("Start collect error!");
            return false;
        }

        if (mode == LOGIC)
        {
            for (auto de : _decode_traces){
                if (bAddDecoder){
                    de->decoder()->set_capture_end_flag(false);                 
                    de->frame_ended();
                    add_decode_task(de);
                }
            } 
        }

        return true;
    }

    bool SigSession::stop_capture()
    {
        _is_action = true;
        int ret = action_stop_capture();
        _is_action = false;
        return ret;
    }

    bool SigSession::action_stop_capture()
    { 
        if (!_is_working)
            return false;

        dsv_info("Stop collect.");

        if (_bClose)
        {
            _is_working = false;
            _repeat_timer.Stop();
            _repeat_wait_prog_timer.Stop();
            _refresh_rt_timer.Stop();
            exit_capture();
            return true;
        }

        bool wait_upload = false;
        if (is_single_mode() && _device_agent.get_work_mode() == LOGIC)
        { 
           _device_agent.get_config_bool(SR_CONF_WAIT_UPLOAD, wait_upload);
        }

        if (!wait_upload)
        {
            _is_working = false;
            _repeat_timer.Stop();
            _repeat_wait_prog_timer.Stop();
            _refresh_rt_timer.Stop();

            if (_repeat_hold_prg != 0 && is_repeat_mode()){
                _repeat_hold_prg = 0;
                _callback->repeat_hold(_repeat_hold_prg);
            }                

            _callback->trigger_message(DSV_MSG_END_COLLECT_WORK_PREV);

            exit_capture();

            data_unlock();

            if (is_repeat_mode() && _device_status != ST_RUNNING){
                _callback->trigger_message(DSV_MSG_END_COLLECT_WORK);
            }

            return true;
        }
        else
        {
            dsv_info("Data is uploading from device data buffer, waiting for stop.");
        }
        return false;
    }

    void SigSession::exit_capture()
    {
        _is_instant = false;

        _feed_timer.Stop();

        if (_device_agent.is_collecting())
            _device_agent.stop();
    }

    bool SigSession::get_capture_status(bool &triggered, int &progress)
    {
        uint64_t sample_limits = cur_samplelimits();
        sr_status status;

        if (_device_agent.get_device_status(status, true))
        { 
            triggered = status.trig_hit & 0x01;
            uint64_t captured_cnt = status.trig_hit >> 2;

            captured_cnt = ((uint64_t)status.captured_cnt0 +
                            ((uint64_t)status.captured_cnt1 << 8) +
                            ((uint64_t)status.captured_cnt2 << 16) +
                            ((uint64_t)status.captured_cnt3 << 24) +
                            (captured_cnt << 32));

            int mode = _device_agent.get_work_mode();

            if (mode == DSO)
                captured_cnt = captured_cnt * _signals.size() / get_ch_num(SR_CHANNEL_DSO);
                
            if (triggered)
                progress = (sample_limits - captured_cnt) * 100.0 / sample_limits;
            else
                progress = captured_cnt * 100.0 / sample_limits;

            if (progress == 100 && mode == LOGIC && _capture_data->get_logic()->have_data() == false){
                progress = 0;
            }

            return true;
        }
        return false;
    }

    std::vector<view::Signal *> &SigSession::get_signals()
    {
        return _signals;
    } 

    void SigSession::check_update()
    {
        ds_lock_guard lock(_data_mutex);

        if (_device_agent.is_collecting() == false)
            return;

        if (_data_updated)
        {
            if (_device_agent.get_work_mode() != LOGIC)
                data_updated();
                
            _data_updated = false;
            _noData_cnt = 0;
            data_auto_unlock();
        }
        else
        {
            if (++_noData_cnt >= (WaitShowTime / FeedInterval))
                nodata_timeout();
        }
    }

    void SigSession::init_signals()
    { 
        if (_device_agent.have_instance() == false)
        {
            assert(false);
        }

        std::vector<view::Signal *> sigs;
        unsigned int logic_probe_count = 0;
        unsigned int dso_probe_count = 0;
        unsigned int analog_probe_count = 0;

        _capture_data->clear();
        _view_data->clear();
        set_cur_snap_samplerate(_device_agent.get_sample_rate());
        set_cur_samplelimits(_device_agent.get_sample_limit());    

        // Detect what data types we will receive
        if (_device_agent.have_instance())
        {
            for (const GSList *l = _device_agent.get_channels(); l; l = l->next)
            {
                const sr_channel *const probe = (const sr_channel *)l->data;

                switch (probe->type)
                {
                case SR_CHANNEL_LOGIC:
                    if (probe->enabled)
                        logic_probe_count++;
                    break;

                case SR_CHANNEL_DSO:
                    dso_probe_count++;
                    break;

                case SR_CHANNEL_ANALOG:
                    if (probe->enabled)
                        analog_probe_count++;
                    break;
                }
            }
        }

        int mode = _device_agent.get_work_mode();

        for (GSList *l = _device_agent.get_channels(); l; l = l->next)
        {
            sr_channel *probe = (sr_channel *)l->data;
            assert(probe); 

            if (mode == LOGIC && probe->type != SR_CHANNEL_LOGIC){
                continue;
            }
            
            if (mode == ANALOG && probe->type != SR_CHANNEL_ANALOG){
                continue;
            }

            if (mode == DSO && probe->type != SR_CHANNEL_DSO){
                continue;
            }

            switch (probe->type)
            {
            case SR_CHANNEL_LOGIC:
                if (probe->enabled){
                    view::Signal *signal = new view::LogicSignal(_view_data->get_logic(), probe);
                    sigs.push_back(signal);
                }
                break;

            case SR_CHANNEL_DSO:{
                    view::Signal *signal = new view::DsoSignal(_view_data->get_dso(), probe);
                    sigs.push_back(signal);
                }
                break;

            case SR_CHANNEL_ANALOG:
                if (probe->enabled){
                    view::Signal *signal = new view::AnalogSignal(_view_data->get_analog(), probe);
                    sigs.push_back(signal);
                }
                break;
            } 
        }
 
        clear_signals();
        std::vector<view::Signal *>().swap(_signals);
        _signals = sigs;
        make_channels_view_index();

        spectrum_rebuild();
        lissajous_disable();
        math_disable();

        if (_signals.empty()){
            dsv_info("ERROR: Unable to create any channel.");
        }        
    }

    void SigSession::reload()
    {
        if (_device_agent.have_instance() == false)
        {
            assert(false);
        }

        if (_is_working)
            return;

        std::vector<view::Signal *> sigs;
        view::Signal *signal = NULL;
        int logic_chan_num = 0;
        int dso_chan_num = 0; 
        int start_view_dex = -1;

        set_cur_snap_samplerate(_device_agent.get_sample_rate());
        set_cur_samplelimits(_device_agent.get_sample_limit());

        int mode = _device_agent.get_work_mode();

        // Make the logic probe list
        for (GSList *l = _device_agent.get_channels(); l; l = l->next)
        {
            sr_channel *probe = (sr_channel *)l->data;
  
            signal = NULL; 

            if (mode == LOGIC && probe->type != SR_CHANNEL_LOGIC){
                continue;
            }

            if (mode == ANALOG && probe->type != SR_CHANNEL_ANALOG){
                continue;
            }
 
            switch (probe->type)
            {
            case SR_CHANNEL_LOGIC:
                if (probe->enabled)
                {    
                    logic_chan_num++;

                    auto i = _signals.begin();

                    while (i != _signals.end())
                    {
                        if ((*i)->get_index() == probe->index)
                        {                            
                            if ((*i)->signal_type() == SR_CHANNEL_LOGIC){
                                view::LogicSignal *logicSig = (view::LogicSignal*)(*i);
                                signal = new view::LogicSignal(logicSig, _view_data->get_logic(), probe);
                                if (logicSig->get_view_index() < start_view_dex || start_view_dex == -1)
                                    start_view_dex = logicSig->get_view_index();
                            }
                               
                            break;
                        }
                        i++;
                    }

                    if (signal == NULL)
                    {
                        signal = new view::LogicSignal(_view_data->get_logic(), probe);
                    }
                }
                break;

            case SR_CHANNEL_ANALOG:
                if (probe->enabled)
                { 
                    dso_chan_num++;

                    auto i = _signals.begin();
                    while (i != _signals.end())
                    {
                        if ((*i)->get_index() == probe->index)
                        {
                            if ((*i)->signal_type() == SR_CHANNEL_ANALOG){
                                view::AnalogSignal *analogSig = (view::AnalogSignal*)(*i);
                                signal = new view::AnalogSignal(analogSig, _view_data->get_analog(), probe);
                            }                               
                            break;
                        }
                        i++;
                    }
                    if (signal == NULL)
                    {
                        signal = new view::AnalogSignal(_view_data->get_analog(), probe);
                    }
                }
                break;
            }
            if (signal != NULL)
                sigs.push_back(signal);
        }

        if (!sigs.empty())
        { 
            std::vector<int> view_indexs;
            for(auto s : _signals){
                view_indexs.push_back(s->get_view_index());
            }

            dsv_info("SigSession::reload(), clear signals");
            clear_signals();
            std::vector<view::Signal *>().swap(_signals);
            _signals = sigs;
            make_channels_view_index(start_view_dex);

            if (mode == LOGIC){
                for (unsigned int i=0; i<view_indexs.size() && i<_signals.size(); i++){
                    _signals[i]->set_view_index(view_indexs[i]);
                }
            }
        }
        else if (mode == LOGIC || mode == ANALOG){
            dsv_info("ERROR: Unable to create any channel.");
            _signals.clear();
        }

        spectrum_rebuild();
    }

    void SigSession::refresh(int holdtime)
    {
        ds_lock_guard lock(_data_mutex);

        data_lock();
        _view_data->get_logic()->init();

        clear_all_decode_task2();
        clear_decode_result();

        _view_data->get_dso()->init();
       
        for (auto m : _spectrum_traces)
        { 
            m->get_spectrum_stack()->init();
        }

        if (_math_trace)
            _math_trace->get_math_stack()->init();

        _view_data->get_analog()->init();

        _out_timer.TimeOut(holdtime, std::bind(&SigSession::feed_timeout, this));
        _data_updated = true;
    }

    void SigSession::data_auto_lock(int lock)
    {
        _data_auto_lock = lock;
    }

    void SigSession::data_auto_unlock()
    {
        if (_data_auto_lock > 0)
            _data_auto_lock--;
        else if (_data_auto_lock < 0)
            _data_auto_lock = 0;
    }

    bool SigSession::get_data_auto_lock()
    {
        return _data_auto_lock != 0;
    }

    void SigSession::feed_in_header(const sr_dev_inst *sdi)
    {
        (void)sdi;
        _callback->receive_header();
    }

    void SigSession::feed_in_meta(const sr_dev_inst *sdi,
                                  const sr_datafeed_meta &meta)
    {
        (void)sdi;

        for (const GSList *l = meta.config; l; l = l->next)
        {
            const sr_config *const src = (const sr_config *)l->data;
            switch (src->key)
            {
            case SR_CONF_SAMPLERATE:
                /// @todo handle samplerate changes
                /// samplerate = (uint64_t *)src->value;
                break;
            }
        }
    }

    void SigSession::feed_in_trigger(const ds_trigger_pos &trigger_pos)
    {
        _hw_replied = true;

        if (_device_agent.get_work_mode() != DSO)
        {
            _trigger_flag = (trigger_pos.status & 0x01);
            if (_trigger_flag)
            {
                _capture_data->_trig_pos = trigger_pos.real_pos;

                // Update trig position for current view.
                if (_capture_data == _view_data){
                    _callback->receive_trigger(_capture_data->_trig_pos);
                }                
            }
        }
        else
        {
            int probe_count = 0;
            int probe_en_count = 0;

            for (const GSList *l = _device_agent.get_channels(); l; l = l->next)
            {
                const sr_channel *const probe = (const sr_channel *)l->data;
                if (probe->type == SR_CHANNEL_DSO)
                {
                    probe_count++;
                    if (probe->enabled)
                        probe_en_count++;
                }
            }

            _capture_data->_trig_pos = trigger_pos.real_pos * probe_count / probe_en_count;
            _callback->receive_trigger(_capture_data->_trig_pos);
        }
    }

    void SigSession::feed_in_logic(const sr_datafeed_logic &o)
    {  
        if (_capture_data->get_logic()->memory_failed())
        {
            dsv_err("Unexpected logic packet");
            return;
        }

        if (!_is_triged && o.length > 0)
        {
            _is_triged = true;
            _trig_time = QDateTime::currentDateTime();
        }  

        if (_capture_data->get_logic()->last_ended())
        {
            _capture_data->get_logic()->set_loop(is_loop_mode());

            bool bNotFree = _is_decoding && _view_data == _capture_data;

            _capture_data->get_logic()->first_payload(o, 
                            _device_agent.get_sample_limit(),
                            _device_agent.get_channels(),
                            !bNotFree);

            // @todo Putting this here means that only listeners querying
            // for logic will be notified. Currently the only user of
            // frame_began is DecoderStack, but in future we need to signal
            // this after both analog and logic sweeps have begun.
            _callback->frame_began();
        }
        else
        {
            // Append to the existing data snapshot
            _capture_data->get_logic()->append_payload(o);
        }

        if (_capture_data->get_logic()->memory_failed())
        {
            _error = Malloc_err;
            _callback->session_error();
            return;
        }

        set_receive_data_len(o.length * 8 / get_ch_num(SR_CHANNEL_LOGIC));

        _data_updated = true;
    }

    void SigSession::feed_in_dso(const sr_datafeed_dso &o)
    {
        if (_capture_data->get_dso()->memory_failed())
        {
            dsv_err("Unexpected dso packet");
            return; // This dso packet was not expected.
        }

        if (_is_instant == false){
            sr_status status;

            if (_device_agent.get_device_status(status, false)){
                _dso_status_valid = true;
                _dso_status = status;
            }
        }

        _dso_packet_count++;

        if (!_is_triged && o.num_samples > 0)
        {
            _is_triged = true;
            _trig_time = QDateTime::currentDateTime();
            set_session_time(_trig_time);
        }

        if (_capture_data->get_dso()->last_ended())
        { 
            // reset scale of dso signal
            for (auto s : _signals)
            { 
                if (s->signal_type() == SR_CHANNEL_DSO){   
                    view::DsoSignal *dsoSig = (view::DsoSignal*)s;
                    dsoSig->set_scale(dsoSig->get_view_rect().height()); 
                }
            }

            // first payload
            _capture_data->get_dso()->first_payload(o, 
                    _device_agent.get_sample_limit(), 
                    _device_agent.get_channels(),
                    _is_instant,
                    _device_agent.is_file());
        }
        else
        {
            // Append to the existing data snapshot
            _capture_data->get_dso()->append_payload(o);
        }

        for (auto s : _signals)
        { 
            if (s->signal_type() == SR_CHANNEL_DSO && (s->enabled())){
                view::DsoSignal *dsoSig = (view::DsoSignal*)s;
                dsoSig->paint_prepare();
            }                
        }

        if (o.num_samples != 0 && (!_is_instant || _dso_packet_count == 1))
        {
            // update current sample rate
            set_cur_snap_samplerate(_device_agent.get_sample_rate());
        }

        if (_capture_data->get_dso()->memory_failed())
        {
            _error = Malloc_err;
            _callback->session_error();
            return;
        }

        // calculate related spectrum results
        for (auto m : _spectrum_traces)
        {
            if (m->enabled())
                m->get_spectrum_stack()->calc_fft();
        }

        // calculate related math results
        if (_math_trace && _math_trace->enabled())
        {
            _math_trace->get_math_stack()->realloc(_device_agent.get_sample_limit());
            _math_trace->get_math_stack()->calc_math(_math_trace->get_vDialfactor());
        }

        _trigger_flag = o.trig_flag;
        _trigger_ch = o.trig_ch;

        //Trigger update()
        set_receive_data_len(o.num_samples);

        if (!_is_instant)
            data_lock();

        _data_updated = true;
    }

    void SigSession::feed_in_analog(const sr_datafeed_analog &o)
    {
        if (_capture_data->get_analog()->memory_failed())
        {
            dsv_err("Unexpected analog packet");
            return; // This analog packet was not expected.
        }

        if (_capture_data->get_analog()->last_ended())
        {
            // reset scale of analog signal
            for (auto s : _signals)
            { 
                if (s->signal_type() == SR_CHANNEL_ANALOG){   
                    view::AnalogSignal *analogSig = (view::AnalogSignal*)s;
                    analogSig->set_scale(analogSig->get_totalHeight());
                }
            }

            // first payload
            _capture_data->get_analog()->first_payload(o, _device_agent.get_sample_limit(), _device_agent.get_channels());
        }
        else
        {
            // Append to the existing data snapshot
            _capture_data->get_analog()->append_payload(o);
        }

        if (_capture_data->get_analog()->memory_failed())
        {
            _error = Malloc_err;
            _callback->session_error();
            return;
        }

        set_receive_data_len(o.num_samples);
        _data_updated = true;
    }

    void SigSession::data_feed_in(const struct sr_dev_inst *sdi,
                                  const struct sr_datafeed_packet *packet)
    {
        assert(sdi);
        assert(packet); 

        ds_lock_guard lock(_data_mutex);

        if (_data_lock && packet->type != SR_DF_END)
            return;

        if (packet->type != SR_DF_END &&
            packet->status != SR_PKT_OK)
        {
            _error = Pkt_data_err;
            _callback->session_error();
            return;
        }

        switch (packet->type)
        {
        case SR_DF_HEADER:
            feed_in_header(sdi);
            break;

        case SR_DF_META:
            assert(packet->payload);
            feed_in_meta(sdi,
                         *(const sr_datafeed_meta *)packet->payload);
            break;

        case SR_DF_TRIGGER:
            assert(packet->payload);
            feed_in_trigger(*(const ds_trigger_pos *)packet->payload);
            break;

        case SR_DF_LOGIC:
            assert(packet->payload);
            assert(!_is_task_end);
            feed_in_logic(*(const sr_datafeed_logic *)packet->payload);
            break;

        case SR_DF_DSO:
            assert(packet->payload);
            assert(!_is_task_end);
            feed_in_dso(*(const sr_datafeed_dso *)packet->payload);
            break;

        case SR_DF_ANALOG:
            assert(packet->payload);
            assert(!_is_task_end);
            feed_in_analog(*(const sr_datafeed_analog *)packet->payload);
            break;

        case SR_DF_OVERFLOW:
        {
            if (_error == No_err)
            {
                _error = Data_overflow;
                _callback->session_error();
            }
            break;
        }
        case SR_DF_END:
        {
            dsv_info("------------SR_DF_END packet.");

            _capture_data->get_logic()->capture_ended();
            _capture_data->get_dso()->capture_ended();
            _capture_data->get_analog()->capture_ended();
            _is_task_end = true;

            if (packet->status != SR_PKT_OK)
            {
                _error = Pkt_data_err;
                _callback->session_error();
            }
            else
            {
                int mode = _device_agent.get_work_mode();

                // Post a message to start all decode tasks.
                if (mode == LOGIC){
                    auto logic_data = _capture_data->get_logic();
                    if (is_loop_mode() && logic_data->get_loop_offset() > 0){
                        uint64_t milliseconds = _capture_work_time / 1000000;
                        QDateTime sessionTime = QDateTime::currentDateTime();
                        sessionTime = sessionTime.addMSecs(-milliseconds);
                        set_session_time(sessionTime);
                    }

                    _callback->trigger_message(DSV_MSG_REV_END_PACKET);
                }
                else{
                    if (mode == DSO && _is_instant){             
                        sr_status status;

                        if (_device_agent.get_device_status(status, false)){
                            _dso_status_valid = true;
                            _dso_status = status;
                        }
                    }

                    _callback->frame_ended();
                }                     
            }

            break;
        }
     }
    }

    void SigSession::data_feed_callback(const struct sr_dev_inst *sdi,
                                        const struct sr_datafeed_packet *packet)
    {
        assert(_session);
        _session->data_feed_in(sdi, packet);
    }

    uint16_t SigSession::get_ch_num(int type)
    {
        uint16_t num_channels = 0;
        uint16_t logic_ch_num = 0;
        uint16_t dso_ch_num = 0;
        uint16_t analog_ch_num = 0;

        if (_device_agent.have_instance())
        {
            for (auto s : _signals)
            { 
                if (!s->enabled())
                    continue;
                
                if (s->signal_type() == SR_CHANNEL_LOGIC)
                    logic_ch_num++;
                else if (s->signal_type() == SR_CHANNEL_DSO)
                    dso_ch_num++;
                else if (s->signal_type() == SR_CHANNEL_ANALOG)
                    analog_ch_num++;
            }
        }

        switch (type)
        {
        case SR_CHANNEL_LOGIC:
            num_channels = logic_ch_num;
            break;
        case SR_CHANNEL_DSO:
            num_channels = dso_ch_num;
            break;
        case SR_CHANNEL_ANALOG:
            num_channels = analog_ch_num;
            break;
        default:
            num_channels = logic_ch_num + dso_ch_num + analog_ch_num;
            break;
        }

        return num_channels;
    }

    bool SigSession::add_decoder(srd_decoder *const dec, bool silent, DecoderStatus *dstatus,
                                 std::list<pv::data::decode::Decoder *> &sub_decoders, view::Trace* &out_trace)
    {
        if (dec == NULL)
        {
            dsv_err("Decoder instance is null!");
            assert(false);
        }

        out_trace = NULL;

        //dsv_info("Create new decoder,name:\"%s\",id:\"%s\"", dec->name, dec->id);

        try
        {
            bool ret = false;

            // Create the decoder
            std::map<const srd_channel *, int> probes;
            data::DecoderStack *decoder_stack = new data::DecoderStack(this, dec, dstatus);
            assert(decoder_stack);

            // Make a list of all the probes
            std::vector<const srd_channel *> all_probes;

            for (const GSList *i = dec->channels; i; i = i->next){
                all_probes.push_back((const srd_channel *)i->data);
            }

            for (const GSList *i = dec->opt_channels; i; i = i->next){
                all_probes.push_back((const srd_channel *)i->data);
            }

            decoder_stack->stack().front()->set_probes(probes);

            // Create the decode signal
            view::DecodeTrace *trace = new view::DecodeTrace(this, decoder_stack, _decode_traces.size());
            assert(trace);

            // add sub decoder
            for (auto sub : sub_decoders)
            {
                trace->decoder()->add_sub_decoder(sub);
            }

            if (sub_decoders.size() > 0){
                auto lst_sub = sub_decoders.end();
                lst_sub--;
                QString sub_dec_name((*lst_sub)->decoder()->name);
                if (sub_dec_name != "")
                    trace->set_name(sub_dec_name);
            }

            sub_decoders.clear();

            // set view early for decode start/end region setting
            for (auto s : _signals)
            {
                if (s->get_view())
                {
                    trace->set_view(s->get_view());
                    break;
                }
            }

            if (silent)
            {
                ret = true;
            }
            else if (trace->create_popup(true))
            {
                ret = true;
            }

            if (ret)
            {
                _decode_traces.push_back(trace);

                // add decode task from ui
                if (!silent && have_view_data())
                {
                    add_decode_task(trace);
                }

                signals_changed();
                data_updated();
                
                out_trace = trace;
            }
            else
            {
                delete trace;
            }

            return ret;
        }
        catch (...)
        {
            dsv_err("Error!add_decoder() throws an exception.");
        }

        return false;
    }

    int SigSession::get_trace_index_by_key_handel(void *handel)
    {
        int dex = 0;

        for (auto tr : _decode_traces)
        {
            if (tr->decoder()->get_key_handel() == handel)
            {
                return dex;
            }
            ++dex;
        }

        return -1;
    }

    void SigSession::remove_decoder(int index)
    {
        int size = (int)_decode_traces.size();
        assert(index < size);

        auto it = _decode_traces.begin() + index;
        auto trace = (*it);
        _decode_traces.erase(it);

        bool isRunning = trace->decoder()->IsRunning();

        remove_decode_task(trace);

        if (isRunning)
        {
            // destroy it in thread
            trace->_delete_flag = true;
        }
        else
        {
            delete trace;
            signals_changed();
        }
    }

    void SigSession::remove_decoder_by_key_handel(void *handel)
    {
        int dex = get_trace_index_by_key_handel(handel);
        remove_decoder(dex);
    }

    void SigSession::rst_decoder(int index)
    {
        auto trace = get_decoder_trace(index);

        if (trace == NULL){
            return;
        }

        if (!trace->create_popup(false)){
            return;
        }

        if (have_view_data() && !is_working())
        {
            remove_decode_task(trace); // remove old task
            trace->decoder()->clear();
            add_decode_task(trace);
            data_updated();
        }
    }

    void SigSession::rst_decoder_by_key_handel(void *handel)
    {
        int dex = get_trace_index_by_key_handel(handel);
        rst_decoder(dex);
    }

    void SigSession::spectrum_rebuild()
    {
        bool has_dso_signal = false;

        for (auto s : _signals)
        { 
            if (s->signal_type() == SR_CHANNEL_DSO){
                has_dso_signal = true;
                // check already have
                auto iter = _spectrum_traces.begin();

                for (unsigned int i = 0; i < _spectrum_traces.size(); i++, iter++){
                    if ((*iter)->get_index() == s->get_index())
                        break;
                }

                // if not, rebuild
                if (iter == _spectrum_traces.end())
                {
                    auto spectrum_stack = new data::SpectrumStack(this, s->get_index());
                    auto spectrum_trace = new view::SpectrumTrace(this, spectrum_stack, s->get_index());
                    _spectrum_traces.push_back(spectrum_trace);
                }
            }
        }

        if (!has_dso_signal)
        {
            RELEASE_ARRAY(_spectrum_traces);
        }

        signals_changed();
    }

    void SigSession::lissajous_rebuild(bool enable, int xindex, int yindex, double percent)
    {
        DESTROY_OBJECT(_lissajous_trace);
        _lissajous_trace = new view::LissajousTrace(enable, _view_data->get_dso(), xindex, yindex, percent);
        signals_changed();
    }

    void SigSession::lissajous_disable()
    {
        if (_lissajous_trace)
            _lissajous_trace->set_enable(false);
    }

    void SigSession::math_rebuild(bool enable, view::DsoSignal *dsoSig1,
                                  view::DsoSignal *dsoSig2,
                                  data::MathStack::MathType type)
    {
        ds_lock_guard lock(_data_mutex);

        assert(dsoSig1);
        assert(dsoSig2);

        DESTROY_OBJECT(_math_trace);

        auto math_stack = new data::MathStack(this, dsoSig1, dsoSig2, type);        
        _math_trace = new view::MathTrace(enable, math_stack, dsoSig1, dsoSig2);

        if (_math_trace && _math_trace->enabled())
        {
            int rt = _view_data->get_dso()->samplerate();
            if (rt > 0){
                _math_trace->get_math_stack()->set_samplerate(rt);
                _math_trace->get_math_stack()->realloc(_device_agent.get_sample_limit());
                _math_trace->get_math_stack()->calc_math(_math_trace->get_vDialfactor());
            }           
        }
        signals_changed();
    }

    void SigSession::math_disable()
    {
        if (_math_trace)
            _math_trace->set_enable(false);
    }

    void SigSession::nodata_timeout()
    {
        int flag;
        _device_agent.get_config_byte(SR_CONF_TRIGGER_SOURCE, flag);
        if (flag != DSO_TRIGGER_AUTO)
        {
            _callback->show_wait_trigger();
        }
    }

    void SigSession::feed_timeout()
    {
        data_unlock();

        if (!_data_updated)
        {
            if (++_noData_cnt >= (WaitShowTime / FeedInterval))
                nodata_timeout();
        }
    }

    data::Snapshot *SigSession::get_snapshot(int type)
    {
        if (type == SR_CHANNEL_LOGIC)
            return _view_data->get_logic();
        else if (type == SR_CHANNEL_ANALOG)
            return _view_data->get_analog();
        else if (type == SR_CHANNEL_DSO)
            return _view_data->get_dso();
        else
            return NULL;
    }

    void SigSession::clear_error()
    {
        _error_pattern = 0;
        _error = No_err;
    }

    int SigSession::get_repeat_hold()
    {
        if (_is_working && is_repeat_mode())
            return _repeat_hold_prg;
        else
            return 0;
    }

    void SigSession::auto_end()
    {
        for (auto s : _signals)
        { 
            if (s->signal_type() == SR_CHANNEL_DSO)
            {
                view::DsoSignal *dsoSig = (view::DsoSignal*)s;
                dsoSig->auto_end();
            }
        }
    }

    void SigSession::Open()
    {
    }

    void SigSession::Close()
    {
        if (_bClose)
            return;

        _bClose = true;

        // Stop decode thread.
        clear_all_decoder(false);

        dsv_info("SigSession::Close(), stop capture");
        stop_capture();

        // TODO: This should not be necessary
        _session = NULL;
    }

    // append a decode task, and try create a thread
    void SigSession::add_decode_task(view::DecodeTrace *trace)
    {
        std::lock_guard<std::mutex> lock(_decode_task_mutex);
        _decode_tasks.push_back(trace);

        if (!_is_decoding){
            if (_decode_thread.joinable())
                _decode_thread.join();

            _decode_thread = std::thread(&SigSession::decode_task_proc, this);
            _is_decoding = true;
        }
    }

    void SigSession::remove_decode_task(view::DecodeTrace *trace)
    {
        std::lock_guard<std::mutex> lock(_decode_task_mutex);

        for (auto it = _decode_tasks.begin(); it != _decode_tasks.end(); it++)
        {
            if ((*it) == trace)
            {
                (*it)->decoder()->stop_decode_work();
                _decode_tasks.erase(it);
                dsv_info("remove a waiting decode task");
                return;
            }
        }

        // the task maybe is running
        trace->decoder()->stop_decode_work();
    }

    void SigSession::clear_all_decoder(bool bUpdateView)
    {
        if (_decode_traces.empty())
            return;

        // create the wait task deque
        int dex = -1;
        clear_all_decode_task(dex);

        view::DecodeTrace *runningTrace = NULL;
        if (dex != -1)
        {
            runningTrace = _decode_traces[dex];
            runningTrace->_delete_flag = true; // destroy it in thread
        }

        for (auto trace : _decode_traces)
        {
            if (trace != runningTrace)
                delete trace;
        }
        _decode_traces.clear();

        if (!_bClose && bUpdateView)
            signals_changed();
    }

    void SigSession::clear_all_decode_task(int &runningDex)
    {
        if (true)
        {
            std::lock_guard<std::mutex> lock(_decode_task_mutex);

            // remove wait task
            for (auto trace : _decode_tasks)
            {
                trace->decoder()->stop_decode_work(); // set decode proc stop flag
            }
            _decode_tasks.clear();
        }

        // make sure the running task can stop
        runningDex = -1;
        int dex = 0;
        for (auto trace : _decode_traces)
        {
            if (trace->decoder()->IsRunning())
            {
                trace->decoder()->stop_decode_work();
                runningDex = dex;
            }
            dex++;
        }

        // Wait the thread end.
        if (_decode_thread.joinable())
            _decode_thread.join();
    }

    view::DecodeTrace *SigSession::get_decoder_trace(int index)
    {
        if (index >= 0 && index < (int)_decode_traces.size())
        {
            return _decode_traces[index];
        }
        assert(false);
    }

    view::DecodeTrace *SigSession::get_top_decode_task()
    {
        std::lock_guard<std::mutex> lock(_decode_task_mutex);

        auto it = _decode_tasks.begin();
        if (it != _decode_tasks.end())
        {
            auto p = (*it);
            _decode_tasks.erase(it);
            return p;
        }

        return NULL;
    }

    // the decode task thread proc
    void SigSession::decode_task_proc()
    {
        dsv_info("------->decode thread start");
        auto task = get_top_decode_task();

        while (task != NULL)
        {
            if (!task->_delete_flag)
            {
                task->decoder()->begin_decode_work();
            }

            if (task->_delete_flag)
            {
                dsv_info("destroy a decoder in task thread");

                DESTROY_QT_LATER(task);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (!_bClose)
                {
                    signals_changed();
                }
            }

            task = get_top_decode_task();
        }

        _view_data->get_logic()->decode_end();

        dsv_info("------->decode thread end");
        _is_decoding = false;        
    }

    Snapshot *SigSession::get_signal_snapshot()
    {
        int mode = _device_agent.get_work_mode();
        if (mode == ANALOG)
            return _view_data->get_analog();
        else if (mode == DSO)
            return _view_data->get_dso();
        else
            return _view_data->get_logic();
    }

    void SigSession::device_lib_event_callback(int event)
    {
        if (_session == NULL)
        {
            dsv_err("Error!Global variable \"_session\" is null.");
            return;
        }
        _session->on_device_lib_event(event);
    }

    void SigSession::on_device_lib_event(int event)
    {
        if (_callback == NULL)
        {
            dsv_detail("The callback is null, so the device event was ignored.");
            return;
        }

        switch (event)
        {
        case DS_EV_DEVICE_RUNNING:
            _device_status = ST_RUNNING;
            set_receive_data_len(0);
            break;

        case DS_EV_DEVICE_STOPPED:
            _device_status = ST_STOPPED;
            // Confirm that SR_DF_END was received
            if (   !_capture_data->get_logic()->last_ended() 
                || !_capture_data->get_dso()->last_ended()
                || !_capture_data->get_analog()->last_ended())
            {
                dsv_err("Error!The data is not completed.");
                assert(false);
            }
            break;

        case DS_EV_COLLECT_TASK_START:
            _callback->trigger_message(DSV_MSG_COLLECT_START);
            break;

        case DS_EV_COLLECT_TASK_END:
        case DS_EV_COLLECT_TASK_END_BY_ERROR:
        case DS_EV_COLLECT_TASK_END_BY_DETACHED:
        {
            _callback->trigger_message(DSV_MSG_COLLECT_END);

            if (_capture_data->get_logic()->last_ended() == false)
                dsv_err("The collected data is error!");

            if (_capture_data->get_dso()->last_ended() == false)
                dsv_err("The collected data is error!");

            if (_capture_data->get_analog()->last_ended() == false)
                dsv_err("The collected data is error!");

            // trig next collect
            if (is_repeat_mode() && _is_working && event == DS_EV_COLLECT_TASK_END)
            {
                _callback->trigger_message(DSV_MSG_TRIG_NEXT_COLLECT);
            }
            else
            {
                _is_working = false;
                _is_instant = false;
                _callback->trigger_message(DSV_MSG_END_COLLECT_WORK);
            }
        }
        break;

        case DS_EV_NEW_DEVICE_ATTACH:
            _callback->trigger_message(DSV_MSG_NEW_USB_DEVICE);
        break;

        case DS_EV_CURRENT_DEVICE_DETACH:
        {
            if (_is_working){
                dsv_info("SigSession::on_device_lib_event,DS_EV_CURRENT_DEVICE_DETACH, stop capture");
                stop_capture();
            }

            _callback->trigger_message(DSV_MSG_CURRENT_DEVICE_DETACHED);
        }            
        break;

        case DS_EV_INACTIVE_DEVICE_DETACH:
            _callback->trigger_message(DSV_MSG_DEVICE_LIST_UPDATED); // Update list only.
            break;
        
        case DS_EV_DEVICE_SPEED_NOT_MATCH:
            _callback->trigger_message(DS_EV_DEVICE_SPEED_NOT_MATCH);
            break;

        default:
            dsv_err("Error!Unknown device event.");
            break;
        }
    }

    void SigSession::add_msg_listener(IMessageListener *ln)
    {
        _msg_listeners.push_back(ln);
    }

    void SigSession::broadcast_msg(int msg)
    {
        for (IMessageListener *cb : _msg_listeners)
        {
            cb->OnMessage(msg);
        }
    }

    void SigSession::set_collect_mode(DEVICE_COLLECT_MODE m)
    {
        assert(!_is_working);

        if (_clt_mode != m)
        {
            _clt_mode = m;
            _repeat_hold_prg = 0;
        }

        _callback->trigger_message(DSV_MSG_COLLECT_MODE_CHANGED);
    }

    void SigSession::repeat_capture_wait_timeout()
    {
        _repeat_timer.Stop();
        _repeat_wait_prog_timer.Stop();

        _repeat_hold_prg = 0;

        if (_is_working)
        {
            _callback->repeat_hold(_repeat_hold_prg);
            exec_capture();
        }
    }

    void SigSession::repeat_wait_prog_timeout()
    {
        _repeat_hold_prg -= _repeat_wait_prog_step;

        if (_repeat_hold_prg < 0)
            _repeat_hold_prg = 0;

        if (_is_working)
            _callback->repeat_hold(_repeat_hold_prg);
    }

    void SigSession::OnMessage(int msg)
    {
        switch (msg)
        {
        case DSV_MSG_DEVICE_OPTIONS_UPDATED:
            reload();
            break;

        case DSV_MSG_TRIG_NEXT_COLLECT:
            {
                if (_is_working && is_repeat_mode())
                {
                    if (_repeat_intvl > 0)
                    {
                        _repeat_hold_prg = 100;
                        _repeat_timer.Start(_repeat_intvl * 1000);
                        int intvl = _repeat_intvl * 1000 / 20;

                        if (intvl >= 100){
                            _repeat_wait_prog_step = 5;
                        }
                        else if (_repeat_intvl >= 1){
                            intvl = _repeat_intvl * 1000 / 10;
                            _repeat_wait_prog_step = 10;
                        }
                        else{
                            intvl = _repeat_intvl * 1000 / 5;
                            _repeat_wait_prog_step = 20;
                        }

                        _repeat_wait_prog_timer.Start(intvl);
                    }
                    else
                    {
                        _repeat_hold_prg = 0;
                        exec_capture();
                    }
                }
            }
            break;

        case DSV_MSG_REV_END_PACKET:
            {
                if (_device_agent.get_work_mode() == LOGIC)
                {  
                    bool bAddDecoder = false;
                    bool bSwapBuffer = false;

                    if (is_single_mode())
                    {
                        if (!_is_stream_mode)
                            bAddDecoder = true;
                    }
                    else if(is_repeat_mode())
                    {
                        if (!_is_stream_mode){
                            bAddDecoder = true;
                            bSwapBuffer = true;
                        }
                        else if (_capture_times > 1){
                            bAddDecoder = true;
                            bSwapBuffer = true;
                        }
                    }
                    else if (is_loop_mode())
                    {
                        bAddDecoder = true;
                    }

                    if (is_repeat_mode())
                    {
                        AppConfig &app = AppConfig::Instance();
                        bool swapBackBufferAlways = app.appOptions.swapBackBufferAlways;
                        if (!swapBackBufferAlways && !_is_working && _capture_times > 1){
                            bAddDecoder = false;
                            bSwapBuffer = false;
                            _capture_data->clear();
                        }
                    }

                    if (bAddDecoder){
                        clear_all_decode_task2();
                        clear_decode_result();
                    }

                    _trig_check_timer.Stop();

                    //Switch the caputrued data buffer to view.
                    if (bSwapBuffer)
                    {
                        if (_view_data != _capture_data)
                            _view_data->clear();
                        
                        _view_data = _capture_data; 
                        attach_data_to_signal(_view_data); 
                        set_session_time(_trig_time);

                        _callback->receive_trigger(_view_data->_trig_pos); //Update trig position.

                        _callback->trigger_message(DSV_MSG_DATA_POOL_CHANGED);
                    }

                    for (auto de : _decode_traces)
                    {
                        de->decoder()->set_capture_end_flag(true);

                        if (bAddDecoder){ 
                            de->frame_ended();
                            add_decode_task(de);
                        }
                    }

                    _callback->frame_ended();
                }
            }
            break;

        case DSV_MSG_COLLECT_END:   
            break;

        case DS_EV_DEVICE_SPEED_NOT_MATCH:
            {
                QString strMsg(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_DEVICE_SPEED_TOO_LOW), "Speed too low!"));
                _callback->delay_prop_msg(strMsg);
            }
            break;
        }     
    }

    void SigSession::DeviceConfigChanged()
    {
        // Nonthing.
    }

    bool SigSession::switch_work_mode(int mode)
    {
        assert(!_is_working);
        int cur_mode = _device_agent.get_work_mode();

        if (cur_mode != mode)
        {
            set_collect_mode(COLLECT_SINGLE);

            _device_agent.set_config_int16(SR_CONF_DEVICE_MODE, mode);

            if (cur_mode == LOGIC){
                clear_all_decode_task2();
                clear_decode_result();
            }

            _is_stream_mode = false;
            if (mode == LOGIC){
                if (_device_agent.is_hardware()){
                    _is_stream_mode = _device_agent.is_stream_mode();
                }
                else if (_device_agent.is_demo()){
                    _is_stream_mode = true;
                }
            }

            _capture_data->clear();
            _view_data->clear();
            _capture_data = _view_data;              
            
            init_signals();

            set_cur_snap_samplerate(_device_agent.get_sample_rate());
            set_cur_samplelimits(_device_agent.get_sample_limit()); 

            dsv_info("Switch work mode to:%d", mode);

            broadcast_msg(DSV_MSG_DEVICE_MODE_CHANGED);

            return true;
        }
        return false;
    }

    bool SigSession::is_first_store_confirm()
    {
        if (_work_time_id != _confirm_store_time_id){
            _confirm_store_time_id = _work_time_id;
            return true;
        }
        return false;
    }

    void SigSession::realtime_refresh_timeout()
    {
        _rt_refresh_time_id++;
    }

    bool SigSession::have_new_realtime_refresh(bool keep)
    {
        if (_rt_ck_refresh_time_id != _rt_refresh_time_id){
            if (!keep){
                _rt_ck_refresh_time_id = _rt_refresh_time_id;
            }
            return true;
        }
        return false;    
    }

    void SigSession::clear_decode_result()
    {
        for (auto de : _decode_traces){
            de->decoder()->init();
            de->decoder()->set_capture_end_flag(false);
        }
        _callback->trigger_message(DSV_MSG_CLEAR_DECODE_DATA);
    }

    void SigSession::attach_data_to_signal(SessionData *data)
    {
        assert(data);

        view::LogicSignal *s1;
        view::AnalogSignal *s2;
        view::DsoSignal *s3;

        for (auto sig : _signals){
            int type = sig->signal_type();
            switch(type){
                case SR_CHANNEL_LOGIC:
                    s1 = (view::LogicSignal*)sig;
                    s1->set_data(data->get_logic());
                    break;
                case SR_CHANNEL_ANALOG:
                    s2 = (view::AnalogSignal*)sig;
                    s2->set_data(data->get_analog());
                    break;
                case SR_CHANNEL_DSO:
                    s3 = (view::DsoSignal*)sig;
                    s3->set_data(data->get_dso());
                    break;
            }
        }
    }

    void SigSession::clear_signals()
    {   
        DESTROY_OBJECT(_math_trace);
        
        for (int i=0; i< (int)_signals.size(); i++)
        {
            auto *p = _signals[i];
            p->sig_released(p);
            
            DESTROY_QT_LATER(p);
        }
        _signals.clear();
    }

    view::Signal* SigSession::get_signal_by_index(int index)
    {
        for (int i=0; i< (int)_signals.size(); i++)
        {
            auto *p = _signals[i];
            if (p->get_index() == index)
                return p;
        }
        return NULL;
    }

    bool SigSession::is_realtime_refresh()
    {
        if (is_loop_mode())
            return true;
        if (_is_stream_mode && is_single_mode())
            return true;
        if (_is_stream_mode && is_repeat_mode() && is_single_buffer())
            return true;
        return false;     
    }

    void SigSession::on_load_config_end()
    {
        set_cur_snap_samplerate(_device_agent.get_sample_rate());
        set_cur_samplelimits(_device_agent.get_sample_limit());
    }

    void SigSession::clear_view_data()
    {
        _view_data->clear();
        data_updated();
    }

    void SigSession::set_trace_name(view::Trace *trace, QString name)
    {
        assert(trace);

        trace->set_name(name);

        int traceType = trace->get_type();

        if (traceType == SR_CHANNEL_LOGIC || traceType == SR_CHANNEL_ANALOG)
        { 
            _device_agent.set_channel_name(trace->get_index(), name.toUtf8());
        }
        else if (traceType == SR_CHANNEL_DECODER && _decoder_pannel != NULL){
            _decoder_pannel->update_deocder_item_name(trace, name.toUtf8().data());
        }
    }

    void SigSession::set_decoder_row_label(int index, QString label)
    {
        auto trace = get_decoder_trace(index);
        if (trace != NULL){
            set_trace_name(trace, label);
        }
    }

    view::Trace* SigSession::get_channel_by_index(int orgIndex)
    {
        for(auto t : _signals){
            if (t->get_index() == orgIndex){
                return t;
            }
        }
        return NULL;
    }

    void SigSession::make_channels_view_index(int start_dex)
    {
        int index = 0;

        if (start_dex != -1)
            index  = start_dex;

        for(auto t : _signals){
            t->set_view_index(index++);
        }
    }

    void SigSession::trig_check_timeout()
    {   
        bool triged = false;
        int pro;

        if (_is_triged){
            _trig_check_timer.Stop();
            return;
        }
        
        if (get_capture_status(triged, pro) && triged)
        {
            _trig_time = QDateTime::currentDateTime();
            _is_triged = true;
            _trig_check_timer.Stop();
        }
    }

    void SigSession::update_dso_data_scale()
    {
        int mode = _device_agent.get_work_mode();
        
        if (mode == DSO)
        { 
            for(auto s : _signals){
                if (s->get_type() == SR_CHANNEL_DSO){
                    view::DsoSignal *ch = (view::DsoSignal*)s;
                    _capture_data->get_dso()->set_data_scale(ch->get_scale(), ch->get_index());                                                 
                }
            }
        }
    }

    int64_t SigSession::get_ring_sample_count()
    {
        int mode = _device_agent.get_work_mode();
        if (mode == LOGIC){
            return _view_data->get_logic()->get_ring_sample_count();
        }
        else if (mode == DSO){
            return _view_data->get_dso()->get_ring_sample_count();
        }
        else{
            return _view_data->get_analog()->get_ring_sample_count();
        }
    }

    void SigSession::update_lang_text()
    {
        for (auto trace : _spectrum_traces){
            trace->update_lang_text();
        }
    }

    bool SigSession::have_decoded_result()
    {
        for (auto trace : _decode_traces){
            if (trace->decoder()->get_result_count() > 0){
                return true;
            }
        }

        return false;
    }

    void SigSession::apply_samplerate()
    {
        on_load_config_end();
    }

    void SigSession::ProcessPowerEvent(bool bEnterSleep)
    {
        if (bEnterSleep){
            if (_is_working && _device_agent.is_hardware()){
                stop_capture();
            }
            ds_close_all_device();
        }
        else{
            ds_reload_device_list();
        }
    }

} // namespace pv
