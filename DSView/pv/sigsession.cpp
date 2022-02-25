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


#include <libsigrokdecode4DSL/libsigrokdecode.h>


#include "sigsession.h"
#include "mainwindow.h"
#include "devicemanager.h"
#include "device/device.h"
#include "device/file.h"

#include "data/analog.h"
#include "data/analogsnapshot.h"
#include "data/dso.h"
#include "data/dsosnapshot.h"
#include "data/logic.h"
#include "data/logicsnapshot.h"
#include "data/group.h"
#include "data/groupsnapshot.h"
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
#include <QDebug>
#include <map>
 
#include "data/decode/decoderstatus.h"
#include "dsvdef.h"
 
namespace pv {

// TODO: This should not be necessary
SigSession* SigSession::_session = NULL;

SigSession::SigSession(DeviceManager *device_manager) 
{
    _dev_inst = NULL;
    _device_manager = device_manager;
    // TODO: This should not be necessary
    _session = this;
    _hot_attach = false;
    _hot_detach = false;
    _group_cnt = 0;
    _bHotplugStop = false;  

    _map_zoom = 0;
    _repeat_hold_prg = 0;
    _repeating = false;
    _repeat_intvl = 1;
    _run_mode = Single;    
    _error = No_err;
    _instant = false;
    _capture_state = Init;
  
    _noData_cnt = 0;
    _data_lock = false;
    _data_updated = false;

    _decoder_model = new pv::data::DecoderModel(NULL);

    _lissajous_trace = NULL;
    _math_trace = NULL;
    _saving = false;
    _dso_feed = false;
    _stop_scale = 1; 
    _bDecodeRunning = false;
    _bClose = false;
    _callback = NULL;

    // Create snapshots & data containers 
    _logic_data = new data::Logic(new data::LogicSnapshot()); 
    _dso_data = new data::Dso(new data::DsoSnapshot()); 
    _analog_data = new data::Analog(new data::AnalogSnapshot()); 
    _group_data = new data::Group();
    _group_cnt = 0;

    _sr_ctx = NULL;

    _feed_timer.Stop(); 
    _feed_timer.SetCallback(std::bind(&SigSession::feed_timeout, this)); 
}

SigSession::SigSession(SigSession &o){(void)o;}

SigSession::~SigSession()
{ 
}

DevInst* SigSession::get_device()
{
    return _dev_inst;
}

void SigSession::deselect_device()
{ 
    RELEASE_ARRAY(_decode_traces);
    RELEASE_ARRAY(_group_traces);
    _dev_inst = NULL;
}

/*
  when be called, it will call 4DSL lib sr_session_new, and create a session struct in the lib
*/
void SigSession::set_device(DevInst *dev_inst)
{
    // Ensure we are not capturing before setting the device
    
    assert(dev_inst);

     if (_dev_inst){
        _dev_inst->dev_inst();
     }

    if (_dev_inst) {
        sr_session_datafeed_callback_remove_all();
        _dev_inst->release();
    }

    _dev_inst = dev_inst;

    RELEASE_ARRAY(_decode_traces);
    RELEASE_ARRAY(_group_traces);

    if (_dev_inst) {
        try {
            _dev_inst->use(this);
            _cur_snap_samplerate = _dev_inst->get_sample_rate();
            _cur_samplelimits = _dev_inst->get_sample_limit();

            if (_dev_inst->dev_inst()->mode == DSO)
                set_run_mode(Repetitive);
            else
                set_run_mode(Single);
        } catch(const QString e) {
            throw(e);
            return;
        }
        sr_session_datafeed_callback_add(data_feed_in_proc, NULL);
        _callback->device_setted();
    }
}


void SigSession::set_file(QString name)
{
    // Deslect the old device, because file type detection in File::create
    // destorys the old session inside libsigrok.
    deselect_device();

    try {
        set_device(device::File::create(name));
    } catch(const QString e) {
        throw(e);
        return;
    }
}

void SigSession::close_file(DevInst *dev_inst)
{
    assert(dev_inst);
    assert(_device_manager);

    try {
        dev_inst->device_updated();
        set_repeating(false);
        stop_capture();
        capture_state_changed(SigSession::Stopped);
        _device_manager->del_device(dev_inst);
    } catch(const QString e) {
        throw(e);
        return;
    }
}

void SigSession::set_default_device()
{
    assert(_device_manager);

    DevInst *default_device = NULL;

    const std::list<DevInst*> &devices = _device_manager->devices();
    if (!devices.empty()) {
        // Fall back to the first device in the list.
        default_device = devices.front();

        // Try and find the DreamSourceLab device and select that by default
        for (DevInst *dev : devices)
            if (dev->dev_inst() && !dev->name().contains("virtual")) {
                default_device = dev;
                break;
            }
    }

    if (default_device != NULL){
           try {
            set_device(default_device);
        } catch(const QString e) {
            _callback->show_error(e);
            return;
        }
    }
}

void SigSession::release_device(DevInst *dev_inst)
{
    if (_dev_inst == NULL)
        return;

    assert(dev_inst);
   // assert(_dev_inst == dev_inst);
    assert(get_capture_state() != Running);

    _dev_inst =  NULL;
}

SigSession::capture_state SigSession::get_capture_state()
{ 
    std::lock_guard<std::mutex> lock(_sampling_mutex);
    return _capture_state;
}

uint64_t SigSession::cur_samplelimits()
{
    return _cur_samplelimits;
}

uint64_t SigSession::cur_samplerate()
{
    // samplerate for current viewport
    if (_dev_inst->dev_inst()->mode == DSO)
        return _dev_inst->get_sample_rate();
    else
        return cur_snap_samplerate();
}

uint64_t SigSession::cur_snap_samplerate()
{
    // samplerate for current snapshot
    return _cur_snap_samplerate;
}

double SigSession::cur_sampletime()
{
    return  cur_samplelimits() * 1.0 / cur_samplerate();
}

double SigSession::cur_snap_sampletime()
{
    return  cur_samplelimits() * 1.0 / cur_snap_samplerate();
}

double SigSession::cur_view_time()
{
    return _dev_inst->get_time_base() * DS_CONF_DSO_HDIVS * 1.0 / SR_SEC(1);
}

void SigSession::set_cur_snap_samplerate(uint64_t samplerate)
{
    assert(samplerate != 0);
    _cur_snap_samplerate = samplerate;
    // sample rate for all SignalData
    // Logic/Analog/Dso
    if (_logic_data)
        _logic_data->set_samplerate(_cur_snap_samplerate);
    if (_analog_data)
        _analog_data->set_samplerate(_cur_snap_samplerate);
    if (_dso_data)
        _dso_data->set_samplerate(_cur_snap_samplerate);
    // Group
    if (_group_data)
        _group_data->set_samplerate(_cur_snap_samplerate);


    // DecoderStack
    for(auto &d : _decode_traces)
        d->decoder()->set_samplerate(_cur_snap_samplerate);

    // Math
    if (_math_trace && _math_trace->enabled())
        _math_trace->get_math_stack()->set_samplerate(_dev_inst->get_sample_rate());
    // SpectrumStack
    for(auto & m : _spectrum_traces)
        m->get_spectrum_stack()->set_samplerate(_cur_snap_samplerate);

    _callback->cur_snap_samplerate_changed();
}

void SigSession::set_cur_samplelimits(uint64_t samplelimits)
{
    assert(samplelimits != 0);
    _cur_samplelimits = samplelimits;
}


void SigSession::capture_init()
{
    if (!_instant)
        set_repeating(get_run_mode() == Repetitive);
    // update instant setting
    _dev_inst->set_config(NULL, NULL, SR_CONF_INSTANT, g_variant_new_boolean(_instant));
    _callback->update_capture();

    set_cur_snap_samplerate(_dev_inst->get_sample_rate());
    set_cur_samplelimits(_dev_inst->get_sample_limit());
    set_stop_scale(1);
    _data_updated = false;
    _trigger_flag = false;
    _trigger_ch = 0;
    _hw_replied = false;
    
    if (_dev_inst->dev_inst()->mode != LOGIC)
        _feed_timer.Start(FeedInterval);
    else
        _feed_timer.Stop();

    _noData_cnt = 0;
    data_unlock();

    // container init
    container_init();

    // update current hw offset
    for(auto &s : _signals)
    {
        assert(s);
        view::DsoSignal *dsoSig = NULL;
        if ((dsoSig = dynamic_cast<view::DsoSignal*>(s))) {
            dsoSig->set_zero_ratio(dsoSig->get_zero_ratio());
        }
        view::AnalogSignal *analogSig = NULL;
        if ((analogSig = dynamic_cast<view::AnalogSignal*>(s))) {
            analogSig->set_zero_ratio(analogSig->get_zero_ratio());
        }
    }
}

void SigSession::container_init()
{
    // Logic
    if (_logic_data)
        _logic_data->init();

    // Group
    if (_group_data)
        _group_data->init();

    // Dso
    if (_analog_data)
        _analog_data->init();

    // Analog
    if (_dso_data)
        _dso_data->init();

    // SpectrumStack
    for(auto &m : _spectrum_traces)
    {
        assert(m);
        m->get_spectrum_stack()->init();
    }

    if (_math_trace)
        _math_trace->get_math_stack()->init();


    // DecoderModel
    //pv::data::DecoderModel *decoder_model = get_decoder_model();
    //decoder_model->setDecoderStack(NULL);
    // DecoderStack
    for(auto &d : _decode_traces)
    { 
        d->decoder()->init();
    }

}

void SigSession::start_capture(bool instant)
{  
    // Check that a device instance has been selected.
    if (!_dev_inst) {
        qDebug() << "No device selected";
        capture_state_changed(SigSession::Stopped);
        return;
    }
    assert(_dev_inst->dev_inst());

    qDebug()<<"start capture, device title:"<<_dev_inst->format_device_title();

    if (!_dev_inst->is_usable()) {
        _error = Hw_err;
        _callback->session_error();
        capture_state_changed(SigSession::Stopped);
        return;
    }

    // stop previous capture
    stop_capture();

    // reset measure of dso signal
    for(auto &s : _signals)
    {
        view::DsoSignal *dsoSig = NULL;
        if ((dsoSig = dynamic_cast<view::DsoSignal*>(s)))
            dsoSig->set_mValid(false);
    }

    // update setting 

    if (_dev_inst->name() != "virtual-session")
        _instant = instant;
    else
        _instant = true;

    capture_init();

    // Check that at least one probe is enabled
    const GSList *l;
    for (l = _dev_inst->dev_inst()->channels; l; l = l->next) {
        sr_channel *const probe = (sr_channel*)l->data;
        assert(probe);
        if (probe->enabled)
            break;
    }
    if (!l) {
        _callback->show_error("No probes enabled.");
        data_updated();
        set_repeating(false);
        capture_state_changed(SigSession::Stopped);
        return;
    }

    if (_sampling_thread.joinable()){
        _sampling_thread.join();
    } 

    if (sr_check_session_start_before() != 0){
        assert(false);
    }
    _sampling_thread  = std::thread(&SigSession::sample_thread_proc, this, _dev_inst);
}


void SigSession::sample_thread_proc(DevInst *dev_inst)
{
    assert(dev_inst);
    assert(dev_inst->dev_inst());

    try {
        dev_inst->start();
    } catch(const QString e) {
        _callback->show_error(e);
        return;
    }

    receive_data(0);
    set_capture_state(Running);
     //session loop
    dev_inst->run();
    set_capture_state(Stopped);

    // Confirm that SR_DF_END was received
    assert(_logic_data->snapshot()->last_ended());
    assert(_dso_data->snapshot()->last_ended());
    assert(_analog_data->snapshot()->last_ended());
}

void SigSession::stop_capture()
{
    do_stop_capture();
    int dex = 0;
    clear_all_decode_task(dex);
}

void SigSession::do_stop_capture()
{
    data_unlock(); 

    if (_dev_inst){
        _dev_inst->stop();
    }

    // Check that sampling stopped
    if (_sampling_thread.joinable()){
        _sampling_thread.join();
    }
}

bool SigSession::get_capture_status(bool &triggered, int &progress)
{
    uint64_t sample_limits = cur_samplelimits();
    sr_status status;
    if (sr_status_get(_dev_inst->dev_inst(), &status, true) == SR_OK){
        triggered = status.trig_hit & 0x01;
        uint64_t captured_cnt = status.trig_hit >> 2;
        captured_cnt = ((uint64_t)status.captured_cnt0 +
                       ((uint64_t)status.captured_cnt1 << 8) +
                       ((uint64_t)status.captured_cnt2 << 16) +
                       ((uint64_t)status.captured_cnt3 << 24) +
                       (captured_cnt << 32));
        if (_dev_inst->dev_inst()->mode == DSO)
            captured_cnt = captured_cnt * _signals.size() / get_ch_num(SR_CHANNEL_DSO);
        if (triggered)
            progress = (sample_limits - captured_cnt) * 100.0 / sample_limits;
        else
            progress = captured_cnt * 100.0 / sample_limits;
        return true;
    }
    return false;
}

std::vector<view::Signal*>& SigSession::get_signals()
{ 
    return _signals;
}

std::vector<view::GroupSignal*>& SigSession::get_group_signals()
{ 
    return _group_traces;
}

std::set<data::SignalData*> SigSession::get_data()
{ 
    std::set<data::SignalData*> data;

    for(auto &sig : _signals) {
        assert(sig);
        data.insert(sig->data());
    }

    return data;
}

bool SigSession::get_instant()
{
    return _instant;
}

void SigSession::set_capture_state(capture_state state)
{ 
    std::lock_guard<std::mutex> lock(_sampling_mutex);
    _capture_state = state;
    data_updated();
    capture_state_changed(state);
}
 
void SigSession::check_update()
{
    ds_lock_guard lock(_data_mutex);

    if (_capture_state != Running)
        return;

    if (_data_updated) {
        data_updated();
        _data_updated = false;
        _noData_cnt = 0;
        data_auto_unlock();
    } else {
        if (++_noData_cnt >= (WaitShowTime/FeedInterval))
            nodata_timeout();
    }
}

void SigSession::add_group()
{
    std::list<int> probe_index_list;

    auto i = _signals.begin();
    while (i != _signals.end()) {
        if ((*i)->get_type() == SR_CHANNEL_LOGIC && (*i)->selected())
            probe_index_list.push_back((*i)->get_index());
        i++;
    }

    if (probe_index_list.size() > 1) { 
        _group_data->init();
        _group_data->set_samplerate(_cur_snap_samplerate);
        auto signal =  new view::GroupSignal("New Group", _group_data, probe_index_list, _group_cnt);
        _group_traces.push_back(signal);
        _group_cnt++;

        const auto &snapshots = _logic_data->get_snapshots();
        if (!snapshots.empty())
        { 
             auto p = new data::GroupSnapshot(snapshots.front(), signal->get_index_list());
            _group_data->push_snapshot(p); 
        }

        signals_changed();
        data_updated();
    }
}

void SigSession::del_group()
{
    auto i = _group_traces.begin();

    while (i != _group_traces.end()) { 
        
        pv::view::GroupSignal *psig = *(i);

        if (psig->selected()) {
            auto j = _group_traces.begin();
            while(j != _group_traces.end()) {
                if ((*j)->get_sec_index() > psig->get_sec_index())
                    (*j)->set_sec_index((*j)->get_sec_index() - 1);
                j++;
            }

            auto &snapshots = _group_data->get_snapshots();
            if (!snapshots.empty()) {
                int dex = psig->get_sec_index();
                pv::data::GroupSnapshot *pshot =  _group_data->get_snapshots().at(dex);
                delete pshot;      
                 
                auto k = snapshots.begin();
                k += (*i)->get_sec_index();
                _group_data->get_snapshots().erase(k);
            }
             
            delete psig;

            i = _group_traces.erase(i);

            _group_cnt--;
            continue;
        }
        i++;
    }

    signals_changed();
    data_updated();
}

void SigSession::init_signals()
{
    assert(_dev_inst);
    stop_capture();

    std::vector<view::Signal*> sigs;
    view::Signal *signal = NULL;
    unsigned int logic_probe_count = 0;
    unsigned int dso_probe_count = 0;
    unsigned int analog_probe_count = 0;

    if (_logic_data)
        _logic_data->clear();
    if (_dso_data)
        _dso_data->clear();
    if (_analog_data)
        _analog_data->clear();
    if (_group_data)
        _group_data->clear();


    // Clear the decode traces 
    RELEASE_ARRAY(_decode_traces);

    // Detect what data types we will receive
    if(_dev_inst) {
        assert(_dev_inst->dev_inst());
        for (const GSList *l = _dev_inst->dev_inst()->channels;
            l; l = l->next) {
            const sr_channel *const probe = (const sr_channel *)l->data;

            switch(probe->type) {
            case SR_CHANNEL_LOGIC:
                if(probe->enabled)
                    logic_probe_count++;
                break;

            case SR_CHANNEL_DSO:
                dso_probe_count++;
                break;

            case SR_CHANNEL_ANALOG:
                if(probe->enabled)
                    analog_probe_count++;
                break;
            }
        }
    }

    // Make the logic probe list 
    RELEASE_ARRAY(_group_traces);

    std::vector<view::GroupSignal *>().swap(_group_traces);

    for (GSList *l = _dev_inst->dev_inst()->channels; l; l = l->next)
    {
        sr_channel *probe =
            (sr_channel *)l->data;
        assert(probe);
        signal = NULL;

        switch (probe->type)
        {
        case SR_CHANNEL_LOGIC:
            if (probe->enabled)
                signal = new view::LogicSignal(_dev_inst, _logic_data, probe);
            break;

        case SR_CHANNEL_DSO:
            signal = new view::DsoSignal(_dev_inst, _dso_data, probe);
            break;

        case SR_CHANNEL_ANALOG:
            if (probe->enabled)
                signal = new view::AnalogSignal(_dev_inst, _analog_data, probe);
            break;
        }
        if (signal != NULL)
            sigs.push_back(signal);
    }

    RELEASE_ARRAY(_signals);
    std::vector<view::Signal *>().swap(_signals);
    _signals = sigs;

    spectrum_rebuild();
    lissajous_disable();
    math_disable();
}

void SigSession::reload()
{
    assert(_dev_inst);

    if (_capture_state == Running)
        stop_capture();

    std::vector<view::Signal*> sigs;
    view::Signal *signal = NULL;

    // Make the logic probe list
    for (GSList *l = _dev_inst->dev_inst()->channels; l; l = l->next)
    {
        sr_channel *probe =
            (sr_channel *)l->data;
        assert(probe);
        signal = NULL;

        switch (probe->type)
        {
        case SR_CHANNEL_LOGIC:
            if (probe->enabled)
            {
                auto i = _signals.begin();
                while (i != _signals.end())
                {
                    if ((*i)->get_index() == probe->index)
                    {
                        view::LogicSignal *logicSig = NULL;
                        if ((logicSig = dynamic_cast<view::LogicSignal *>(*i)))
                            signal = new view::LogicSignal(logicSig, _logic_data, probe);
                        break;
                    }
                    i++;
                }
                if (signal == NULL)
                {
                    signal = new view::LogicSignal(_dev_inst, _logic_data, probe);
                }
            }
            break;

        case SR_CHANNEL_ANALOG:
            if (probe->enabled)
            {
                auto i = _signals.begin();
                while (i != _signals.end())
                {
                    if ((*i)->get_index() == probe->index)
                    {
                        view::AnalogSignal *analogSig = NULL;
                        if ((analogSig = dynamic_cast<view::AnalogSignal *>(*i)))
                            signal = new view::AnalogSignal(analogSig, _analog_data, probe);
                        break;
                    }
                    i++;
                }
                if (signal == NULL)
                {
                    signal = new view::AnalogSignal(_dev_inst, _analog_data, probe);
                }
            }
            break;
        }
        if (signal != NULL)
            sigs.push_back(signal);
    }

    if (!sigs.empty())
    {
        RELEASE_ARRAY(_signals);
        std::vector<view::Signal *>().swap(_signals);
        _signals = sigs;
    }

    spectrum_rebuild();
}

void SigSession::refresh(int holdtime)
{
    ds_lock_guard lock(_data_mutex);

    data_lock();

    if (_logic_data) {
        _logic_data->init(); 

        for(auto &d : _decode_traces)
        { 
            d->decoder()->init();
        }
    }

    if (_dso_data) {
        _dso_data->init();
        // SpectrumStack
        for(auto &m : _spectrum_traces)
        {
            assert(m);
            m->get_spectrum_stack()->init();
        }

        if (_math_trace)
            _math_trace->get_math_stack()->init();
    }

    if (_analog_data) {
        _analog_data->init(); 
    }
 
    _out_timer.TimeOut(holdtime, std::bind(&SigSession::feed_timeout, this));
    _data_updated = true;
}

void SigSession::data_lock()
{
    _data_lock = true;
}

void SigSession::data_unlock()
{
    _data_lock = false;
}

bool SigSession::get_data_lock()
{
    return _data_lock;
}

void SigSession::data_auto_lock(int lock) {
    _data_auto_lock = lock;
}

void SigSession::data_auto_unlock() {
    if (_data_auto_lock > 0)
        _data_auto_lock--;
    else if (_data_auto_lock < 0)
        _data_auto_lock = 0;
}

bool SigSession::get_data_auto_lock() {
    return _data_auto_lock != 0;
}

void SigSession::feed_in_header(const sr_dev_inst *sdi)
{
    (void)sdi;
    _trigger_pos = 0;
   _callback->receive_header();
}

void SigSession::feed_in_meta(const sr_dev_inst *sdi,
    const sr_datafeed_meta &meta)
{
    (void)sdi;

    for (const GSList *l = meta.config; l; l = l->next) {
        const sr_config *const src = (const sr_config*)l->data;
        switch (src->key) {
        case SR_CONF_SAMPLERATE:
            /// @todo handle samplerate changes
            /// samplerate = (uint64_t *)src->value;
            break;
        default:
            // Unknown metadata is not an error.
            break;
        }
    }
}

void SigSession::feed_in_trigger(const ds_trigger_pos &trigger_pos)
{
    _hw_replied = true;
    if (_dev_inst->dev_inst()->mode != DSO) {
        _trigger_flag = (trigger_pos.status & 0x01);
        if (_trigger_flag) {
            _trigger_pos = trigger_pos.real_pos;
            _callback->receive_trigger(_trigger_pos);
        }
    } else {
        int probe_count = 0;
        int probe_en_count = 0;
        for (const GSList *l = _dev_inst->dev_inst()->channels;
            l; l = l->next) {
            const sr_channel *const probe = (const sr_channel *)l->data;
            if (probe->type == SR_CHANNEL_DSO) {
                probe_count++;
                if (probe->enabled)
                    probe_en_count++;
            }
        }
        _trigger_pos = trigger_pos.real_pos * probe_count / probe_en_count;
        _callback->receive_trigger(_trigger_pos);
    }
}

void SigSession::feed_in_logic(const sr_datafeed_logic &logic)
{ 
    if (!_logic_data || _logic_data->snapshot()->memory_failed()) {
        qDebug() << "Unexpected logic packet";
        return;
    }

    if (logic.data_error == 1) {
        _error = Test_data_err;
        _error_pattern = logic.error_pattern;
        _callback->session_error();
    }

    if (_logic_data->snapshot()->last_ended()) {
        _logic_data->snapshot()->first_payload(logic, _dev_inst->get_sample_limit(), _dev_inst->dev_inst()->channels);
        // @todo Putting this here means that only listeners querying
        // for logic will be notified. Currently the only user of
        // frame_began is DecoderStack, but in future we need to signal
        // this after both analog and logic sweeps have begun.
        _callback->frame_began();
    } else {
        // Append to the existing data snapshot
        _logic_data->snapshot()->append_payload(logic);
    }

    if (_logic_data->snapshot()->memory_failed()) {
        _error = Malloc_err;
        _callback->session_error();
        return;
    }

    receive_data(logic.length * 8 / get_ch_num(SR_CHANNEL_LOGIC));
    
    _callback->data_received(); 
    
    _data_updated = true;
}

void SigSession::feed_in_dso(const sr_datafeed_dso &dso)
{  
    if(!_dso_data || _dso_data->snapshot()->memory_failed())
    {
        qDebug() << "Unexpected dso packet";
        return;	// This dso packet was not expected.
    }

    if (_dso_data->snapshot()->last_ended())
    {
        std::map<int, bool> sig_enable;
        // reset scale of dso signal
        for(auto &s : _signals)
        {
            assert(s);
            view::DsoSignal *dsoSig = NULL;
            if ((dsoSig = dynamic_cast<view::DsoSignal*>(s))) {
                dsoSig->set_scale(dsoSig->get_view_rect().height());
                sig_enable[dsoSig->get_index()] = dsoSig->enabled();
            }
        }

        // first payload
        _dso_data->snapshot()->first_payload(dso, _dev_inst->get_sample_limit(), sig_enable, _instant);
    } else {
        // Append to the existing data snapshot
        _dso_data->snapshot()->append_payload(dso);
    }

    for(auto &s : _signals) {
        view::DsoSignal *dsoSig = NULL;
        if ((dsoSig = dynamic_cast<view::DsoSignal*>(s)) && (dsoSig->enabled()))
            dsoSig->paint_prepare();
    }

    if (dso.num_samples != 0) {
        // update current sample rate
        set_cur_snap_samplerate(_dev_inst->get_sample_rate());
 
    }

    if (_dso_data->snapshot()->memory_failed()) {
        _error = Malloc_err;
        _callback->session_error();
        return;
    }

    // calculate related spectrum results
    for(auto &m : _spectrum_traces)
    {
        assert(m);
        if (m->enabled())
            m->get_spectrum_stack()->calc_fft();
    }

    // calculate related math results
    if (_math_trace && _math_trace->enabled()) {
        _math_trace->get_math_stack()->realloc(_dev_inst->get_sample_limit());
        _math_trace->get_math_stack()->calc_math();
    }

    _trigger_flag = dso.trig_flag;
    _trigger_ch = dso.trig_ch;
    receive_data(dso.num_samples);

    if (!_instant)
        data_lock();

    _data_updated = true;
}

void SigSession::feed_in_analog(const sr_datafeed_analog &analog)
{ 

    if(!_analog_data || _analog_data->snapshot()->memory_failed())
    {
        qDebug() << "Unexpected analog packet";
        return;	// This analog packet was not expected.
    }

    if (_analog_data->snapshot()->last_ended())
    {
        // reset scale of analog signal
        for(auto &s : _signals)
        {
            assert(s);
            view::AnalogSignal *analogSig = NULL;
            if ((analogSig = dynamic_cast<view::AnalogSignal*>(s))) {
                analogSig->set_scale(analogSig->get_totalHeight());
            }
        }

        // first payload
        _analog_data->snapshot()->first_payload(analog, _dev_inst->get_sample_limit(), _dev_inst->dev_inst()->channels);
    } else {
        // Append to the existing data snapshot
        _analog_data->snapshot()->append_payload(analog);
    }

    if (_analog_data->snapshot()->memory_failed()) {
        _error = Malloc_err;
        _callback->session_error();
        return;
    }

    receive_data(analog.num_samples);
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
        packet->status != SR_PKT_OK) {
        _error = Pkt_data_err;
        _callback->session_error();
        return;
    }

    switch (packet->type) {
    case SR_DF_HEADER:
        feed_in_header(sdi);
        break;

    case SR_DF_META:
        assert(packet->payload);
        feed_in_meta(sdi,
            *(const sr_datafeed_meta*)packet->payload);
        break;

    case SR_DF_TRIGGER:
        assert(packet->payload);
        feed_in_trigger(*(const ds_trigger_pos*)packet->payload);
        break;

    case SR_DF_LOGIC:
        assert(packet->payload);
        feed_in_logic(*(const sr_datafeed_logic*)packet->payload);
        break;

    case SR_DF_DSO:
        assert(packet->payload);
        feed_in_dso(*(const sr_datafeed_dso*)packet->payload);
        break;

    case SR_DF_ANALOG:
        assert(packet->payload);
        feed_in_analog(*(const sr_datafeed_analog*)packet->payload);
        break;

    case SR_DF_OVERFLOW:
    {
        if (_error == No_err) {
            _error = Data_overflow;
            _callback->session_error();
        }
        break;
    }
    case SR_DF_END:
    { 
        if (!_logic_data->snapshot()->empty())
        {
            for (auto &g : _group_traces)
            {
                assert(g);

                auto p = new data::GroupSnapshot(_logic_data->get_snapshots().front(), g->get_index_list());
                _group_data->push_snapshot(p);
            }
        }
        _logic_data->snapshot()->capture_ended();
        _dso_data->snapshot()->capture_ended();
        _analog_data->snapshot()->capture_ended();

        for (auto trace : _decode_traces){
            trace->decoder()->frame_ended();
            trace->frame_ended();
            add_decode_task(trace);
        } 

        if (packet->status != SR_PKT_OK) {
            _error = Pkt_data_err;
            _callback->session_error();
        }

        _callback->frame_ended();

        if (get_device()->dev_inst()->mode != LOGIC){
             set_session_time(QDateTime::currentDateTime());
        }
           
        break;
    }
    }
}

void SigSession::data_feed_in_proc(const struct sr_dev_inst *sdi,
    const struct sr_datafeed_packet *packet, void *cb_data)
{
    (void) cb_data;
    assert(_session);
    _session->data_feed_in(sdi, packet);
}

/*
 * hotplug function
 */
void SigSession::hotplug_callback(void *ctx, void *dev, int event, void *user_data) {

    (void)ctx;
    (void)dev;
    (void)user_data;

    if (1 == event) {
        _session->_hot_attach = true;
        qDebug("receive event:DreamSourceLab Hardware attached!");
    }else if (2 == event) {
        _session->_hot_detach = true;
        qDebug("receive event:DreamSourceLab Hardware detached!");
    }else{
        qDebug("Unhandled event %d\n", event);
    }
}

void SigSession::hotplug_proc()
{  
    if (!_dev_inst)
        return; 
        
    try {
        while(_session && !_bHotplugStop) {

            sr_hotplug_wait_timout(_sr_ctx);

            if (_hot_attach) {
                qDebug("process event:DreamSourceLab hardware attached!");
                _callback->device_attach();
                _hot_attach = false;
            }
            if (_hot_detach) {
                qDebug("process event:DreamSourceLab hardware detached!");
                _callback->device_detach();
                _hot_detach = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } catch(...) {
        qDebug("Interrupt exception for hotplug thread was thrown.");
    }
   // qDebug("Hotplug thread exit!");
}

void SigSession::register_hotplug_callback()
{
    if (sr_listen_hotplug(_sr_ctx, hotplug_callback, NULL) != 0){
        qDebug() << "Error creating a hotplug callback,code:";
    }
}

void SigSession::deregister_hotplug_callback()
{
    sr_close_hotplug(_sr_ctx);
}

void SigSession::start_hotplug_work()
{ 
    // Begin the session
   // qDebug() << "Starting a hotplug thread...\n";
    _hot_attach = false;
    _hot_detach = false;

    if (_hotplug_thread.joinable()){
        return;
    } 
    _hotplug_thread =  std::thread(&SigSession::hotplug_proc, this);
}

void SigSession::stop_hotplug_work()
{  
    _bHotplugStop = true;
     if (_hotplug_thread.joinable()){
        _hotplug_thread.join();
    } 
}

uint16_t SigSession::get_ch_num(int type)
{
    uint16_t num_channels = 0;
    uint16_t logic_ch_num = 0;
    uint16_t dso_ch_num = 0;
    uint16_t analog_ch_num = 0;

    if (_dev_inst->dev_inst()) {
        for(auto &s : _signals)
        {
            assert(s);
            if (dynamic_cast<view::LogicSignal*>(s) && s->enabled()) {
                    logic_ch_num++;
            }
            if (dynamic_cast<view::DsoSignal*>(s) && s->enabled()) {
                    dso_ch_num++;
            }
            if (dynamic_cast<view::AnalogSignal*>(s) && s->enabled()) {
                    analog_ch_num++;
            }
        }
    }

    switch(type) {
        case SR_CHANNEL_LOGIC:
            num_channels = logic_ch_num; break;
        case SR_CHANNEL_DSO:
            num_channels = dso_ch_num; break;
        case SR_CHANNEL_ANALOG:
            num_channels = analog_ch_num; break;
        default:
            num_channels = logic_ch_num+dso_ch_num+analog_ch_num; break;
    }

    return num_channels;
}

bool SigSession::add_decoder(srd_decoder *const dec, bool silent, DecoderStatus *dstatus){
    return do_add_decoder(dec, silent, dstatus);
}

bool SigSession::do_add_decoder(srd_decoder *const dec, bool silent, DecoderStatus *dstatus)
{     
    try { 

        bool ret = false;

        // Create the decoder
        std::map<const srd_channel*, int> probes; 
        data::DecoderStack *decoder_stack = new data::DecoderStack(this, dec, dstatus);
        assert(decoder_stack);

        // Make a list of all the probes
        std::vector<const srd_channel*> all_probes;
        for(const GSList *i = dec->channels; i; i = i->next)
            all_probes.push_back((const srd_channel*)i->data);

        for(const GSList *i = dec->opt_channels; i; i = i->next)
            all_probes.push_back((const srd_channel*)i->data);
 
        decoder_stack->stack().front()->set_probes(probes);

        // Create the decode signal
        view::DecodeTrace *trace = new view::DecodeTrace(this, decoder_stack, _decode_traces.size());
        assert(trace);

        // set view early for decode start/end region setting
        for(auto &s : _signals) {
            if (s->get_view()) {
                trace->set_view(s->get_view());
                break;
            }
        }
        if (silent) { 
            ret = true;
        } else if (trace->create_popup()) { 
            ret = true;
        }

        if (ret)
        {
           _decode_traces.push_back(trace);
            add_decode_task(trace);
            signals_changed();
            data_updated();
        }
        else
        {
            delete trace;
        }

        return ret;

    } catch(...) {
         ds_debug("Starting a hotplug thread...\n");
    }
 
    return false;
}
 
std::vector<view::DecodeTrace*>& SigSession::get_decode_signals()
{ 
    return _decode_traces;
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

    if (isRunning){
        //destroy it in thread
        trace->_delete_flag = true;
    }
    else{
        delete trace;
        signals_changed();
    } 
}

void SigSession::rst_decoder(int index)
{
     auto trace = get_decoder_trace(index);
    
    if (trace && trace->create_popup() ){
        remove_decode_task(trace); //remove old task
        add_decode_task(trace);
        data_updated();
    }
}
  
pv::data::DecoderModel* SigSession::get_decoder_model()
{
    return _decoder_model;
}


void SigSession::spectrum_rebuild()
{
    bool has_dso_signal = false;
    for(auto &s : _signals) {
        view::DsoSignal *dsoSig = NULL;
        if ((dsoSig = dynamic_cast<view::DsoSignal*>(s))) {
            has_dso_signal = true;
            // check already have
            auto iter = _spectrum_traces.begin();
            for(unsigned int i = 0; i < _spectrum_traces.size(); i++, iter++)
                if ((*iter)->get_index() == dsoSig->get_index())
                    break;
            // if not, rebuild
            if (iter == _spectrum_traces.end()) {
                auto spectrum_stack = new data::SpectrumStack(this, dsoSig->get_index());
                auto spectrum_trace = new view::SpectrumTrace(this, spectrum_stack, dsoSig->get_index());
                _spectrum_traces.push_back(spectrum_trace);
            }
        }
    }

    if (!has_dso_signal){
        RELEASE_ARRAY(_spectrum_traces);
    }      

    signals_changed();
}

std::vector<view::SpectrumTrace*>& SigSession::get_spectrum_traces()
{ 
    return _spectrum_traces;
}

void SigSession::lissajous_rebuild(bool enable, int xindex, int yindex, double percent)
{
    DESTROY_OBJECT(_lissajous_trace);
    _lissajous_trace = new view::LissajousTrace(enable, _dso_data, xindex, yindex, percent);
    signals_changed();
}

void SigSession::lissajous_disable()
{
    if (_lissajous_trace)
        _lissajous_trace->set_enable(false);
}

view::LissajousTrace* SigSession::get_lissajous_trace()
{ 
    return _lissajous_trace;
}

void SigSession::math_rebuild(bool enable,view::DsoSignal *dsoSig1, 
                              view::DsoSignal *dsoSig2,
                              data::MathStack::MathType type)
{
    ds_lock_guard lock(_data_mutex);

    auto math_stack = new data::MathStack(this, dsoSig1, dsoSig2, type);
    DESTROY_OBJECT(_math_trace);
    _math_trace = new view::MathTrace(enable, math_stack, dsoSig1, dsoSig2);

    if (_math_trace && _math_trace->enabled()) {
        _math_trace->get_math_stack()->set_samplerate(_dev_inst->get_sample_rate());
        _math_trace->get_math_stack()->realloc(_dev_inst->get_sample_limit());
        _math_trace->get_math_stack()->calc_math();
    }
    signals_changed();
}

void SigSession::math_disable()
{
    if (_math_trace)
        _math_trace->set_enable(false);
}

view::MathTrace* SigSession::get_math_trace()
{
    return _math_trace;
}

void SigSession::set_session_time(QDateTime time)
{
    _session_time = time;
}

QDateTime SigSession::get_session_time()
{
    return _session_time;
}

uint64_t  SigSession::get_trigger_pos()
{
    return _trigger_pos;
}

bool SigSession::trigd()
{
    return _trigger_flag;
}

uint8_t SigSession::trigd_ch()
{
    return _trigger_ch;
}

void SigSession::nodata_timeout()
{
    GVariant *gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_TRIGGER_SOURCE);
    if (gvar == NULL)
        return;
    if (g_variant_get_byte(gvar) != DSO_TRIGGER_AUTO) {
        _callback->show_wait_trigger();
    }
}

void SigSession::feed_timeout()
{  
    data_unlock();
    if (!_data_updated) {
        if (++_noData_cnt >= (WaitShowTime/FeedInterval))
            nodata_timeout();
    }
}

data::Snapshot* SigSession::get_snapshot(int type)
{
    if (type == SR_CHANNEL_LOGIC)
        return _logic_data->snapshot();
    else if (type == SR_CHANNEL_ANALOG)
        return _analog_data->snapshot();
    else if (type == SR_CHANNEL_DSO)
        return _dso_data->snapshot();
    else
        return NULL;
}

SigSession::error_state SigSession::get_error()
{
    return _error;
}

void SigSession::set_error(error_state state)
{
    _error = state;
}

void SigSession::clear_error()
{
    _error_pattern = 0;
    _error = No_err;
}

uint64_t SigSession::get_error_pattern()
{
    return _error_pattern;
}

SigSession::run_mode SigSession::get_run_mode()
{
    return _run_mode;
}

void SigSession::set_run_mode(run_mode mode)
{
    _run_mode = mode;
}

int SigSession::get_repeat_intvl()
{
    return _repeat_intvl;
}

void SigSession::set_repeat_intvl(int interval)
{
    _repeat_intvl = interval;
}

void SigSession::set_repeating(bool repeat)
{
    _repeating = repeat;
    if (!_repeating)
        _repeat_hold_prg = 0;
}

bool SigSession::isRepeating()
{
    return _repeating;
}

bool SigSession::repeat_check()
{
    if (get_capture_state() != Stopped ||
        get_run_mode() != Repetitive ||
        !isRepeating()) {
        return false;
    }

    if (_dev_inst->dev_inst()->mode == LOGIC) {
        _repeat_hold_prg = 100;
        _callback->repeat_hold(_repeat_hold_prg); 
        _out_timer.TimeOut(_repeat_intvl*1000/RepeatHoldDiv, std::bind(&SigSession::repeat_update, this));
        return true;
    } else {
        return false;
    }
}

void SigSession::repeat_update()
{
    if (isRepeating()) {
        _repeat_hold_prg -= 100/RepeatHoldDiv;
        if (_repeat_hold_prg != 0){
            _out_timer.TimeOut(_repeat_intvl*1000/RepeatHoldDiv, std::bind(&SigSession::repeat_update, this));
        }
        _callback->repeat_hold(_repeat_hold_prg);
        if (_repeat_hold_prg == 0)
            repeat_resume();
    }
}

int SigSession::get_repeat_hold()
{
    if (isRepeating())
        return _repeat_hold_prg;
    else
        return 0;
}

void SigSession::set_map_zoom(int index)
{
    _map_zoom = index;
}

int SigSession::get_map_zoom()
{
    return _map_zoom;
}

void SigSession::auto_end()
{
    for(auto &s : _signals) {
        view::DsoSignal *dsoSig = NULL;
        if ((dsoSig = dynamic_cast<view::DsoSignal*>(s))) {
            dsoSig->auto_end();
        }
    }
}

void SigSession::set_save_start(uint64_t start)
{
    _save_start = start;
}

void SigSession::set_save_end(uint64_t end)
{
    _save_end = end;
}

uint64_t SigSession::get_save_start()
{
    return _save_start;
}

uint64_t SigSession::get_save_end()
{
    return _save_end;
}

bool SigSession::get_saving()
{
    return _saving;
}

void SigSession::set_saving(bool saving)
{
    _saving = saving;
}

void SigSession::exit_capture()
{
    set_repeating(false);
    bool wait_upload = false;
    if (get_run_mode() != SigSession::Repetitive) {
        GVariant *gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_WAIT_UPLOAD);
        if (gvar != NULL) {
            wait_upload = g_variant_get_boolean(gvar);
            g_variant_unref(gvar);
        }
    }
    if (!wait_upload) {
        stop_capture();
        capture_state_changed(SigSession::Stopped);
    }
}

float SigSession::stop_scale()
{
    return _stop_scale;
}

void SigSession::set_stop_scale(float scale)
{
    _stop_scale = scale;
}

 sr_dev_inst* SigSession::get_dev_inst_c()
 {
     if (_dev_inst != NULL){
         return _dev_inst->dev_inst();
     }
     return NULL;
 }

 void SigSession::Open()
 {
     register_hotplug_callback();
 }

 void SigSession::Close()
 { 
     if (_bClose)
        return; 

     _bClose = true;
 
     do_stop_capture(); //stop capture

     clear_all_decoder(); //clear all decode task, and stop decode thread

     ds_trigger_destroy();

     if (_dev_inst)
     {
         _dev_inst->release();
     }

     // TODO: This should not be necessary
     _session = NULL;

     stop_hotplug_work();

     deregister_hotplug_callback();
 }

//append a decode task, and try create a thread
 void SigSession::add_decode_task(view::DecodeTrace *trace)
 { 
     //qDebug()<<"add a decode task";

     std::lock_guard<std::mutex> lock(_decode_task_mutex);
     _decode_tasks.push_back(trace);

     if (!_bDecodeRunning)
     {
         if (_decode_thread.joinable())
             _decode_thread.join();

         _decode_thread = std::thread(&SigSession::decode_task_proc, this);
         _bDecodeRunning = true;
     }
 }

 void SigSession::remove_decode_task(view::DecodeTrace *trace)
 {
     std::lock_guard<std::mutex> lock(_decode_task_mutex);

     for (auto it = _decode_tasks.begin(); it != _decode_tasks.end(); it++){
         if ((*it) == trace){
              (*it)->decoder()->stop_decode_work();
             _decode_tasks.erase(it);
             qDebug()<<"remove a wait decode task";
             return;  
         }
     }

     //the task maybe is running 
  //   qDebug()<<"remove a running decode task";
     trace->decoder()->stop_decode_work();
 }

 void SigSession::clear_all_decoder()
 {
     //create the wait task deque
     int dex = -1;
     clear_all_decode_task(dex);

     view::DecodeTrace *runningTrace = NULL;
     if (dex != -1){
         runningTrace = _decode_traces[dex];
         runningTrace->_delete_flag = true;  //destroy it in thread
     }

     for (auto trace : _decode_traces)
     {
         if (trace != runningTrace){
             delete trace;
         }
     }     
     _decode_traces.clear();

    //wait thread end
     if (_decode_thread.joinable())
     {
       //  qDebug() << "wait the decode thread end";
         _decode_thread.join();
     }

     if (!is_closed())
        signals_changed();
 }

 void SigSession::clear_all_decode_task(int &runningDex)
 {
     std::lock_guard<std::mutex> lock(_decode_task_mutex);
  
     //remove wait task
      for (auto trace : _decode_tasks){
          trace->decoder()->stop_decode_work(); //set decode proc stop flag
      } 
      _decode_tasks.clear();

      //make sure the running task can stop
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
 }
 
 view::DecodeTrace* SigSession::get_decoder_trace(int index)
  { 
      int size = (int)_decode_traces.size();
      assert(index < size);
      return   _decode_traces[index];    
  }

  view::DecodeTrace* SigSession::get_top_decode_task()
  {
      std::lock_guard<std::mutex> lock(_decode_task_mutex);

      auto it = _decode_tasks.begin();
      if (it != _decode_tasks.end()){
          auto p = (*it);
          _decode_tasks.erase(it);
          return p;
      } 

      return NULL;
  }

  //the decode task thread proc
  void SigSession::decode_task_proc(){ 

      //qDebug()<<"decode thread start";
      auto task = get_top_decode_task();
      
      while (task != NULL)
      {
         // qDebug()<<"one decode task be actived";
 
          if (!task->_delete_flag){
              task->decoder()->begin_decode_work();
          }

          if (task->_delete_flag){
             qDebug()<<"destroy a decoder in task thread";

             DESTROY_QT_LATER(task);
             std::this_thread::sleep_for(std::chrono::milliseconds(100));
             if (!_bClose){
                  signals_changed();  
             }     
          } 

          task = get_top_decode_task();
      }  

     // qDebug()<<"decode thread end";
      _bDecodeRunning = false;
  }



} // namespace pv
