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

#ifdef ENABLE_DECODE
#include <libsigrokdecode4DSL/libsigrokdecode.h>
#endif

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
#include <QProgressDialog>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>

#include <boost/foreach.hpp>

//using boost::dynamic_pointer_cast;
//using boost::function;
//using boost::lock_guard;
//using boost::mutex;
//using boost::shared_ptr;
//using std::list;
//using std::map;
//using std::set;
//using std::string;
//using std::vector;
//using std::deque;
//using std::min;

using namespace boost;
using namespace std;

namespace pv {

// TODO: This should not be necessary
SigSession* SigSession::_session = NULL;

SigSession::SigSession(DeviceManager &device_manager) :
	_device_manager(device_manager),
    _capture_state(Init),
    _instant(false),
    _error(No_err),
    _run_mode(Single),
    _repeat_intvl(1),
    _repeating(false),
    _repeat_hold_prg(0),
    _map_zoom(0)
{
	// TODO: This should not be necessary
	_session = this;
    _hot_attach = false;
    _hot_detach = false;
    _group_cnt = 0;
	register_hotplug_callback();
    _feed_timer.stop();
    _noData_cnt = 0;
    _data_lock = false;
    _data_updated = false;
    #ifdef ENABLE_DECODE
    _decoder_model = new pv::data::DecoderModel(this);
    #endif
    _lissajous_trace = NULL;
    _math_trace = NULL;
    _saving = false;
    _dso_feed = false;
    _stop_scale = 1;

    // Create snapshots & data containers
    _cur_logic_snapshot.reset(new data::LogicSnapshot());
    _logic_data.reset(new data::Logic());
    _logic_data->push_snapshot(_cur_logic_snapshot);
    _cur_dso_snapshot.reset(new data::DsoSnapshot());
    _dso_data.reset(new data::Dso());
    _dso_data->push_snapshot(_cur_dso_snapshot);
    _cur_analog_snapshot.reset(new data::AnalogSnapshot());
    _analog_data.reset(new data::Analog());
    _analog_data->push_snapshot(_cur_analog_snapshot);
    _group_data.reset(new data::Group());
    _group_cnt = 0;

    connect(&_feed_timer, SIGNAL(timeout()), this, SLOT(feed_timeout()));
}

SigSession::~SigSession()
{
	stop_capture();
		       
    ds_trigger_destroy();

    _dev_inst->release();

	// TODO: This should not be necessary
	_session = NULL;

    if (_hotplug_handle) {
        stop_hotplug_proc();
        deregister_hotplug_callback();
    }
}

boost::shared_ptr<device::DevInst> SigSession::get_device() const
{
    return _dev_inst;
}

void SigSession::set_device(boost::shared_ptr<device::DevInst> dev_inst)
{
    using pv::device::Device;

    // Ensure we are not capturing before setting the device
    //stop_capture();

    if (_dev_inst) {
        sr_session_datafeed_callback_remove_all();
        _dev_inst->release();
    }

    _dev_inst = dev_inst;
#ifdef ENABLE_DECODE
    _decode_traces.clear();
#endif
    _group_traces.clear();

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
        device_setted();
    }
}


void SigSession::set_file(QString name)
{
    // Deslect the old device, because file type detection in File::create
    // destorys the old session inside libsigrok.
    try {
        set_device(boost::shared_ptr<device::DevInst>());
    } catch(const QString e) {
        throw(e);
        return;
    }
    try {
        set_device(boost::shared_ptr<device::DevInst>(device::File::create(name)));
    } catch(const QString e) {
        throw(e);
        return;
    }
}

void SigSession::close_file(boost::shared_ptr<pv::device::DevInst> dev_inst)
{
    assert(dev_inst);
    try {
        dev_inst->device_updated();
        set_repeating(false);
        stop_capture();
        capture_state_changed(SigSession::Stopped);
        _device_manager.del_device(dev_inst);
    } catch(const QString e) {
        throw(e);
        return;
    }
}

void SigSession::set_default_device(boost::function<void (const QString)> error_handler)
{
    boost::shared_ptr<pv::device::DevInst> default_device;
    const list<boost::shared_ptr<device::DevInst> > &devices =
        _device_manager.devices();

    if (!devices.empty()) {
        // Fall back to the first device in the list.
        default_device = devices.front();

        // Try and find the DreamSourceLab device and select that by default
        BOOST_FOREACH (boost::shared_ptr<pv::device::DevInst> dev, devices)
            if (dev->dev_inst() &&
                !dev->name().contains("virtual")) {
                default_device = dev;
                break;
            }
        try {
            set_device(default_device);
        } catch(const QString e) {
            error_handler(e);
            return;
        }
    }
}

void SigSession::release_device(device::DevInst *dev_inst)
{
    (void)dev_inst;
    assert(_dev_inst.get() == dev_inst);

    assert(get_capture_state() != Running);
    _dev_inst = boost::shared_ptr<device::DevInst>();
    //_dev_inst.reset();
}

SigSession::capture_state SigSession::get_capture_state() const
{
    boost::lock_guard<boost::mutex> lock(_sampling_mutex);
	return _capture_state;
}

uint64_t SigSession::cur_samplelimits() const
{
    return _cur_samplelimits;
}

uint64_t SigSession::cur_samplerate() const
{
    // samplerate for current viewport
    if (_dev_inst->dev_inst()->mode == DSO)
        return _dev_inst->get_sample_rate();
    else
        return cur_snap_samplerate();
}

uint64_t SigSession::cur_snap_samplerate() const
{
    // samplerate for current snapshot
    return _cur_snap_samplerate;
}

double SigSession::cur_sampletime() const
{
    return  cur_samplelimits() * 1.0 / cur_samplerate();
}

double SigSession::cur_snap_sampletime() const
{
    return  cur_samplelimits() * 1.0 / cur_snap_samplerate();
}

double SigSession::cur_view_time() const
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

#ifdef ENABLE_DECODE
    // DecoderStack
    BOOST_FOREACH(const boost::shared_ptr<view::DecodeTrace> d, _decode_traces)
        d->decoder()->set_samplerate(_cur_snap_samplerate);
#endif
    // Math
    if (_math_trace && _math_trace->enabled())
        _math_trace->get_math_stack()->set_samplerate(_dev_inst->get_sample_rate());
    // SpectrumStack
    BOOST_FOREACH(const boost::shared_ptr<view::SpectrumTrace> m, _spectrum_traces)
        m->get_spectrum_stack()->set_samplerate(_cur_snap_samplerate);

    cur_snap_samplerate_changed();
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
    update_capture();

    set_cur_snap_samplerate(_dev_inst->get_sample_rate());
    set_cur_samplelimits(_dev_inst->get_sample_limit());
    set_stop_scale(1);
    _data_updated = false;
    _trigger_flag = false;
    _trigger_ch = 0;
    _hw_replied = false;
    if (_dev_inst->dev_inst()->mode != LOGIC)
        _feed_timer.start(FeedInterval);
    else
        _feed_timer.stop();

    _noData_cnt = 0;
    data_unlock();

    // container init
    container_init();

    // update current hw offset
    BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _signals)
    {
        assert(s);
        boost::shared_ptr<view::DsoSignal> dsoSig;
        if ((dsoSig = dynamic_pointer_cast<view::DsoSignal>(s))) {
            dsoSig->set_zero_ratio(dsoSig->get_zero_ratio());
        }
        boost::shared_ptr<view::AnalogSignal> analogSig;
        if ((analogSig = dynamic_pointer_cast<view::AnalogSignal>(s))) {
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
    BOOST_FOREACH(const boost::shared_ptr<view::SpectrumTrace> m, _spectrum_traces)
    {
        assert(m);
        m->get_spectrum_stack()->init();
    }

    if (_math_trace)
        _math_trace->get_math_stack()->init();

#ifdef ENABLE_DECODE
    // DecoderModel
    //pv::data::DecoderModel *decoder_model = get_decoder_model();
    //decoder_model->setDecoderStack(NULL);
    // DecoderStack
    BOOST_FOREACH(const boost::shared_ptr<view::DecodeTrace> d, _decode_traces)
    {
        assert(d);
        d->decoder()->init();
    }
#endif
}

void SigSession::start_capture(bool instant,
    boost::function<void (const QString)> error_handler)
{
    // Check that a device instance has been selected.
    if (!_dev_inst) {
        qDebug() << "No device selected";
        capture_state_changed(SigSession::Stopped);
        return;
    }
    assert(_dev_inst->dev_inst());

    if (!_dev_inst->is_usable()) {
        _error = Hw_err;
        session_error();
        capture_state_changed(SigSession::Stopped);
        return;
    }

    // stop previous capture
	stop_capture();
    // reset measure of dso signal
    BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _signals)
    {
        assert(s);
        boost::shared_ptr<view::DsoSignal> dsoSig;
        if ((dsoSig = dynamic_pointer_cast<view::DsoSignal>(s)))
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
		error_handler(tr("No probes enabled."));
        data_updated();
        set_repeating(false);
        capture_state_changed(SigSession::Stopped);
		return;
	}

	// Begin the session
	_sampling_thread.reset(new boost::thread(
        &SigSession::sample_thread_proc, this, _dev_inst,
        error_handler));
}

void SigSession::stop_capture()
{
    data_unlock();
#ifdef ENABLE_DECODE
    for (vector< boost::shared_ptr<view::DecodeTrace> >::iterator i =
        _decode_traces.begin();
        i != _decode_traces.end();
        i++)
        (*i)->decoder()->stop_decode();
#endif
    if (get_capture_state() != Running)
		return;

	sr_session_stop();

	// Check that sampling stopped
    if (_sampling_thread.get())
        _sampling_thread->join();
    _sampling_thread.reset();
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

vector< boost::shared_ptr<view::Signal> > SigSession::get_signals()
{
    //boost::lock_guard<boost::mutex> lock(_signals_mutex);
	return _signals;
}

vector< boost::shared_ptr<view::GroupSignal> > SigSession::get_group_signals()
{
    //boost::lock_guard<boost::mutex> lock(_signals_mutex);
    return _group_traces;
}

set< boost::shared_ptr<data::SignalData> > SigSession::get_data() const
{
    //lock_guard<mutex> lock(_signals_mutex);
    set< boost::shared_ptr<data::SignalData> > data;
    BOOST_FOREACH(const boost::shared_ptr<view::Signal> sig, _signals) {
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
    boost::lock_guard<boost::mutex> lock(_sampling_mutex);
	_capture_state = state;
    data_updated();
	capture_state_changed(state);
}

void SigSession::sample_thread_proc(boost::shared_ptr<device::DevInst> dev_inst,
                                    boost::function<void (const QString)> error_handler)
{
    assert(dev_inst);
    assert(dev_inst->dev_inst());
    assert(error_handler);

    try {
        dev_inst->start();
    } catch(const QString e) {
        error_handler(e);
        return;
    }

    receive_data(0);
    set_capture_state(Running);

    dev_inst->run();

    set_capture_state(Stopped);

    // Confirm that SR_DF_END was received
    assert(_cur_logic_snapshot->last_ended());
    assert(_cur_dso_snapshot->last_ended());
    assert(_cur_analog_snapshot->last_ended());
}

void SigSession::check_update()
{
    boost::lock_guard<boost::mutex> lock(_data_mutex);

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

    std::vector< boost::shared_ptr<view::Signal> >::iterator i = _signals.begin();
    while (i != _signals.end()) {
        if ((*i)->get_type() == SR_CHANNEL_LOGIC && (*i)->selected())
            probe_index_list.push_back((*i)->get_index());
        i++;
    }

    if (probe_index_list.size() > 1) {
        //_group_data.reset(new data::Group(_last_sample_rate));
//        if (_group_data->get_snapshots().empty())
//            _group_data->set_samplerate(_dev_inst->get_sample_rate());
        _group_data->init();
        _group_data->set_samplerate(_cur_snap_samplerate);
        const boost::shared_ptr<view::GroupSignal> signal(
                    new view::GroupSignal("New Group",
                                          _group_data, probe_index_list, _group_cnt));
        _group_traces.push_back(signal);
        _group_cnt++;

        const deque< boost::shared_ptr<data::LogicSnapshot> > &snapshots =
            _logic_data->get_snapshots();
        if (!snapshots.empty()) {
            //if (!_cur_group_snapshot)
            //{
                // Create a new data snapshot
                _cur_group_snapshot = boost::shared_ptr<data::GroupSnapshot>(
                            new data::GroupSnapshot(snapshots.front(), signal->get_index_list()));
                //_cur_group_snapshot->append_payload();
                _group_data->push_snapshot(_cur_group_snapshot);
                _cur_group_snapshot.reset();
            //}
        }

        signals_changed();
        data_updated();
    }
}

void SigSession::del_group()
{
    std::vector< boost::shared_ptr<view::GroupSignal> >::iterator i = _group_traces.begin();
    while (i != _group_traces.end()) {
        if ((*i)->selected()) {
            std::vector< boost::shared_ptr<view::GroupSignal> >::iterator j = _group_traces.begin();
            while(j != _group_traces.end()) {
                if ((*j)->get_sec_index() > (*i)->get_sec_index())
                    (*j)->set_sec_index((*j)->get_sec_index() - 1);
                j++;
            }

            std::deque< boost::shared_ptr<data::GroupSnapshot> > &snapshots = _group_data->get_snapshots();
            if (!snapshots.empty()) {
                _group_data->get_snapshots().at((*i)->get_sec_index()).reset();
                std::deque< boost::shared_ptr<data::GroupSnapshot> >::iterator k = snapshots.begin();
                k += (*i)->get_sec_index();
                _group_data->get_snapshots().erase(k);
            }

            (*i).reset();
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

    vector< boost::shared_ptr<view::Signal> > sigs;
    boost::shared_ptr<view::Signal> signal;
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

#ifdef ENABLE_DECODE
    // Clear the decode traces
    _decode_traces.clear();
#endif

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
    {
        _group_traces.clear();
        vector< boost::shared_ptr<view::GroupSignal> >().swap(_group_traces);

        for (GSList *l = _dev_inst->dev_inst()->channels; l; l = l->next) {
            sr_channel *probe =
                ( sr_channel *)l->data;
            assert(probe);
            signal.reset();
            switch(probe->type) {
            case SR_CHANNEL_LOGIC:
                if (probe->enabled)
                    signal = boost::shared_ptr<view::Signal>(
                        new view::LogicSignal(_dev_inst, _logic_data, probe));
                break;

            case SR_CHANNEL_DSO:
                signal = boost::shared_ptr<view::Signal>(
                    new view::DsoSignal(_dev_inst, _dso_data, probe));
                break;

            case SR_CHANNEL_ANALOG:
                if (probe->enabled)
                    signal = boost::shared_ptr<view::Signal>(
                        new view::AnalogSignal(_dev_inst, _analog_data, probe));
                break;
            }
            if(signal.get())
                sigs.push_back(signal);
        }

        _signals.clear();
        vector< boost::shared_ptr<view::Signal> >().swap(_signals);
        _signals = sigs;
    }

    spectrum_rebuild();
    lissajous_disable();
    math_disable();
    //data_updated();
}

void SigSession::reload()
{
    assert(_dev_inst);

    if (_capture_state == Running)
        stop_capture();

    //refresh(0);
    vector< boost::shared_ptr<view::Signal> > sigs;
    boost::shared_ptr<view::Signal> signal;

    // Make the logic probe list
    {
        for (GSList *l = _dev_inst->dev_inst()->channels; l; l = l->next) {
            sr_channel *probe =
                (sr_channel *)l->data;
            assert(probe);
            signal.reset();
            switch(probe->type) {
            case SR_CHANNEL_LOGIC:
                if (probe->enabled) {
                    std::vector< boost::shared_ptr<view::Signal> >::iterator i = _signals.begin();
                    while (i != _signals.end()) {
                        if ((*i)->get_index() == probe->index) {
                            boost::shared_ptr<view::LogicSignal> logicSig;
                            if ((logicSig = dynamic_pointer_cast<view::LogicSignal>(*i)))
                                signal = boost::shared_ptr<view::Signal>(
                                    new view::LogicSignal(logicSig, _logic_data, probe));
                            break;
                        }
                        i++;
                    }
                    if (!signal.get())
                        signal = boost::shared_ptr<view::Signal>(
                            new view::LogicSignal(_dev_inst, _logic_data, probe));
                }
                break;

//            case SR_CHANNEL_DSO:
//                signal = boost::shared_ptr<view::Signal>(
//                    new view::DsoSignal(_dev_inst,_dso_data, probe));
//                break;

            case SR_CHANNEL_ANALOG:
                if (probe->enabled) {
                    std::vector< boost::shared_ptr<view::Signal> >::iterator i = _signals.begin();
                    while (i != _signals.end()) {
                        if ((*i)->get_index() == probe->index) {
                            boost::shared_ptr<view::AnalogSignal> analogSig;
                            if ((analogSig = dynamic_pointer_cast<view::AnalogSignal>(*i)))
                                signal = boost::shared_ptr<view::Signal>(
                                    new view::AnalogSignal(analogSig, _analog_data, probe));
                            break;
                        }
                        i++;
                    }
                    if (!signal.get())
                        signal = boost::shared_ptr<view::Signal>(
                            new view::AnalogSignal(_dev_inst, _analog_data, probe));
                }
                break;
            }
            if (signal.get())
                sigs.push_back(signal);
        }

        if (!sigs.empty()) {
            _signals.clear();
            vector< boost::shared_ptr<view::Signal> >().swap(_signals);
            _signals = sigs;
        }
    }

    spectrum_rebuild();
}

void SigSession::refresh(int holdtime)
{
    boost::lock_guard<boost::mutex> lock(_data_mutex);

    data_lock();

    if (_logic_data) {
        _logic_data->init();
        //_cur_logic_snapshot.reset();
#ifdef ENABLE_DECODE
        BOOST_FOREACH(const boost::shared_ptr<view::DecodeTrace> d, _decode_traces)
        {
            assert(d);
            d->decoder()->init();
        }
#endif
    }
    if (_dso_data) {
        _dso_data->init();
        // SpectrumStack
        BOOST_FOREACH(const boost::shared_ptr<view::SpectrumTrace> m, _spectrum_traces)
        {
            assert(m);
            m->get_spectrum_stack()->init();
        }
        if (_math_trace)
            _math_trace->get_math_stack()->init();
    }
    if (_analog_data) {
        _analog_data->init();
        //_cur_analog_snapshot.reset();
    }

    QTimer::singleShot(holdtime, this, SLOT(feed_timeout()));
    //data_updated();
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
    receive_header();
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
            receive_trigger(_trigger_pos);
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
        receive_trigger(_trigger_pos);
    }
}

void SigSession::feed_in_logic(const sr_datafeed_logic &logic)
{
    //boost::lock_guard<boost::mutex> lock(_data_mutex);
    if (!_logic_data || _cur_logic_snapshot->memory_failed()) {
		qDebug() << "Unexpected logic packet";
		return;
	}

    if (logic.data_error == 1) {
        _error = Test_data_err;
        _error_pattern = logic.error_pattern;
        session_error();
    }

    if (_cur_logic_snapshot->last_ended()) {
        _cur_logic_snapshot->first_payload(logic, _dev_inst->get_sample_limit(), _dev_inst->dev_inst()->channels);
        // @todo Putting this here means that only listeners querying
        // for logic will be notified. Currently the only user of
        // frame_began is DecoderStack, but in future we need to signal
        // this after both analog and logic sweeps have begun.
        frame_began();
    } else {
		// Append to the existing data snapshot
        _cur_logic_snapshot->append_payload(logic);
    }

    if (_cur_logic_snapshot->memory_failed()) {
        _error = Malloc_err;
        session_error();
        return;
    }

    emit receive_data(logic.length * 8 / get_ch_num(SR_CHANNEL_LOGIC));
    data_received();
    //data_updated();
    _data_updated = true;
}

void SigSession::feed_in_dso(const sr_datafeed_dso &dso)
{
    //boost::lock_guard<boost::mutex> lock(_data_mutex);

    if(!_dso_data || _cur_dso_snapshot->memory_failed())
    {
        qDebug() << "Unexpected dso packet";
        return;	// This dso packet was not expected.
    }

    if (_cur_dso_snapshot->last_ended())
    {
        std::map<int, bool> sig_enable;
        // reset scale of dso signal
        BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _signals)
        {
            assert(s);
            boost::shared_ptr<view::DsoSignal> dsoSig;
            if ((dsoSig = dynamic_pointer_cast<view::DsoSignal>(s))) {
                dsoSig->set_scale(dsoSig->get_view_rect().height());
                sig_enable[dsoSig->get_index()] = dsoSig->enabled();
            }
        }

        // first payload
        _cur_dso_snapshot->first_payload(dso, _dev_inst->get_sample_limit(), sig_enable, _instant);
    } else {
        // Append to the existing data snapshot
        _cur_dso_snapshot->append_payload(dso);
    }

    BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _signals) {
        boost::shared_ptr<view::DsoSignal> dsoSig;
        if ((dsoSig = dynamic_pointer_cast<view::DsoSignal>(s)) && (dsoSig->enabled()))
            dsoSig->paint_prepare();
    }

    if (dso.num_samples != 0) {
        // update current sample rate
        set_cur_snap_samplerate(_dev_inst->get_sample_rate());
//        // reset measure of dso signal
//        BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _signals)
//        {
//            assert(s);
//            boost::shared_ptr<view::DsoSignal> dsoSig;
//            if ((dsoSig = dynamic_pointer_cast<view::DsoSignal>(s)))
//                dsoSig->set_mValid(false);
//        }
    }

    if (_cur_dso_snapshot->memory_failed()) {
        _error = Malloc_err;
        session_error();
        return;
    }

    // calculate related spectrum results
    BOOST_FOREACH(const boost::shared_ptr<view::SpectrumTrace> m, _spectrum_traces)
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
    //boost::lock_guard<boost::mutex> lock(_data_mutex);

    if(!_analog_data || _cur_analog_snapshot->memory_failed())
	{
		qDebug() << "Unexpected analog packet";
		return;	// This analog packet was not expected.
	}

    if (_cur_analog_snapshot->last_ended())
	{
        // reset scale of analog signal
        BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _signals)
        {
            assert(s);
            boost::shared_ptr<view::AnalogSignal> analogSig;
            if ((analogSig = dynamic_pointer_cast<view::AnalogSignal>(s))) {
                analogSig->set_scale(analogSig->get_totalHeight());
            }
        }

        // first payload
        _cur_analog_snapshot->first_payload(analog, _dev_inst->get_sample_limit(), _dev_inst->dev_inst()->channels);
    } else {
		// Append to the existing data snapshot
		_cur_analog_snapshot->append_payload(analog);
    }

    if (_cur_analog_snapshot->memory_failed()) {
        _error = Malloc_err;
        session_error();
        return;
    }

    receive_data(analog.num_samples);
    //data_updated();
    _data_updated = true;
}

void SigSession::data_feed_in(const struct sr_dev_inst *sdi,
    const struct sr_datafeed_packet *packet)
{
	assert(sdi);
	assert(packet);

    boost::lock_guard<boost::mutex> lock(_data_mutex);

    if (_data_lock && packet->type != SR_DF_END)
        return;
    if (packet->type != SR_DF_END &&
        packet->status != SR_PKT_OK) {
        _error = Pkt_data_err;
        session_error();
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
            session_error();
        }
        break;
    }
	case SR_DF_END:
	{
		{
            //boost::lock_guard<boost::mutex> lock(_data_mutex);
            if (!_cur_logic_snapshot->empty()) {
                BOOST_FOREACH(const boost::shared_ptr<view::GroupSignal> g, _group_traces)
                {
                    assert(g);

                    _cur_group_snapshot = boost::shared_ptr<data::GroupSnapshot>(
                                new data::GroupSnapshot(_logic_data->get_snapshots().front(), g->get_index_list()));
                    _group_data->push_snapshot(_cur_group_snapshot);
                    _cur_group_snapshot.reset();
                }
            }
            _cur_logic_snapshot->capture_ended();
            _cur_dso_snapshot->capture_ended();
            _cur_analog_snapshot->capture_ended();
#ifdef ENABLE_DECODE
            BOOST_FOREACH(const boost::shared_ptr<view::DecodeTrace> d, _decode_traces)
                d->frame_ended();
#endif
		}

        if (packet->status != SR_PKT_OK) {
            _error = Pkt_data_err;
            session_error();
        }
        frame_ended();
        if (get_device()->dev_inst()->mode != LOGIC)
            set_session_time(QDateTime::currentDateTime());
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
int SigSession::hotplug_callback(struct libusb_context *ctx, struct libusb_device *dev, 
                                  libusb_hotplug_event event, void *user_data) {

    (void)ctx;
    (void)dev;
    (void)user_data;

    if (LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED == event) {
        _session->_hot_attach = true;
        qDebug("DreamSourceLab Hardware Attached!\n");
    }else if (LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT == event) {
        _session->_hot_detach = true;
        qDebug("DreamSourceLab Hardware Detached!\n");
    }else{
        qDebug("Unhandled event %d\n", event);
    }

    return 0;
}

void SigSession::hotplug_proc(boost::function<void (const QString)> error_handler)
{
    struct timeval tv;

    (void)error_handler;

    if (!_dev_inst)
        return;

    tv.tv_sec = tv.tv_usec = 0;
    try {
        while(_session) {
            libusb_handle_events_timeout(NULL, &tv);
            if (_hot_attach) {
                qDebug("DreamSourceLab hardware attached!");
                device_attach();
                _hot_attach = false;
            }
            if (_hot_detach) {
                qDebug("DreamSourceLab hardware detached!");
                device_detach();
                _hot_detach = false;
            }
            boost::this_thread::sleep(boost::posix_time::millisec(100));
        }
    } catch(...) {
        qDebug("Interrupt exception for hotplug thread was thrown.");
    }
    qDebug("Hotplug thread exit!");
}

void SigSession::register_hotplug_callback()
{
    int ret;

    ret = libusb_hotplug_register_callback(NULL, (libusb_hotplug_event)(LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
                                           LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT), 
                                           (libusb_hotplug_flag)LIBUSB_HOTPLUG_ENUMERATE, 0x2A0E, LIBUSB_HOTPLUG_MATCH_ANY,
                                           LIBUSB_HOTPLUG_MATCH_ANY, hotplug_callback, NULL,
                                           &_hotplug_handle);
    if (LIBUSB_SUCCESS != ret){
	qDebug() << "Error creating a hotplug callback\n";
    }
}

void SigSession::deregister_hotplug_callback()
{
    libusb_hotplug_deregister_callback(NULL, _hotplug_handle);
}

void SigSession::start_hotplug_proc(boost::function<void (const QString)> error_handler)
{

    // Begin the session
    qDebug() << "Starting a hotplug thread...\n";
    _hot_attach = false;
    _hot_detach = false;
    _hotplug.reset(new boost::thread(
        &SigSession::hotplug_proc, this, error_handler));

}

void SigSession::stop_hotplug_proc()
{
    if (_hotplug.get()) {
        _hotplug->interrupt();
        _hotplug->join();
    }
    _hotplug.reset();
}

uint16_t SigSession::get_ch_num(int type)
{
    uint16_t num_channels = 0;
    uint16_t logic_ch_num = 0;
    uint16_t dso_ch_num = 0;
    uint16_t analog_ch_num = 0;
    if (_dev_inst->dev_inst()) {
        BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _signals)
        {
            assert(s);
            if (dynamic_pointer_cast<view::LogicSignal>(s) && s->enabled()) {
            //if (dynamic_pointer_cast<view::LogicSignal>(s)) {
                    logic_ch_num++;
            }
            if (dynamic_pointer_cast<view::DsoSignal>(s) && s->enabled()) {
            //if (dynamic_pointer_cast<view::DsoSignal>(s)) {
                    dso_ch_num++;
            }
            if (dynamic_pointer_cast<view::AnalogSignal>(s) && s->enabled()) {
            //if (dynamic_pointer_cast<view::AnalogSignal>(s)) {
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

#ifdef ENABLE_DECODE
bool SigSession::add_decoder(srd_decoder *const dec, bool silent)
{
    bool ret = false;
    map<const srd_channel*, int> probes;
    boost::shared_ptr<data::DecoderStack> decoder_stack;

    try {
        //lock_guard<mutex> lock(_signals_mutex);

        // Create the decoder
        decoder_stack = boost::shared_ptr<data::DecoderStack>(
            new data::DecoderStack(*this, dec));

        // Make a list of all the probes
        std::vector<const srd_channel*> all_probes;
        for(const GSList *i = dec->channels; i; i = i->next)
            all_probes.push_back((const srd_channel*)i->data);
        for(const GSList *i = dec->opt_channels; i; i = i->next)
            all_probes.push_back((const srd_channel*)i->data);

        assert(decoder_stack);
        assert(!decoder_stack->stack().empty());
        assert(decoder_stack->stack().front());
        decoder_stack->stack().front()->set_probes(probes);

        // Create the decode signal
        boost::shared_ptr<view::DecodeTrace> d(
            new view::DecodeTrace(*this, decoder_stack,
                _decode_traces.size()));
        // set view early for decode start/end region setting
        BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _signals) {
            if (s->get_view()) {
                d->set_view(s->get_view());
                break;
            }
        }
        if (silent) {
            _decode_traces.push_back(d);
            ret = true;
        } else if (d->create_popup()) {
            _decode_traces.push_back(d);
            ret = true;
        }
    } catch(const std::runtime_error &e) {
        return false;
    }

    if (ret) {
        signals_changed();
        // Do an initial decode
        decoder_stack->begin_decode();
        data_updated();
    }

    return ret;
}

vector< boost::shared_ptr<view::DecodeTrace> > SigSession::get_decode_signals() const
{
    //lock_guard<mutex> lock(_signals_mutex);
    return _decode_traces;
}

void SigSession::remove_decode_signal(view::DecodeTrace *signal)
{
    for (vector< boost::shared_ptr<view::DecodeTrace> >::iterator i =
        _decode_traces.begin();
        i != _decode_traces.end();
        i++)
        if ((*i).get() == signal)
        {
            _decode_traces.erase(i);
            signals_changed();
            return;
        }
}

void SigSession::remove_decode_signal(int index)
{
    int cur_index = 0;
    for (vector< boost::shared_ptr<view::DecodeTrace> >::iterator i =
        _decode_traces.begin();
        i != _decode_traces.end();
        i++)
    {
        if (cur_index == index)
        {
            _decode_traces.erase(i);
            signals_changed();
            return;
        }
        cur_index++;
    }
}

void SigSession::rst_decoder(int index)
{
    int cur_index = 0;
    for (vector< boost::shared_ptr<view::DecodeTrace> >::iterator i =
        _decode_traces.begin();
        i != _decode_traces.end();
        i++)
    {
        if (cur_index == index)
        {
            if ((*i)->create_popup())
            {
                (*i)->decoder()->stop_decode();
                (*i)->decoder()->begin_decode();
                data_updated();
            }
            return;
        }
        cur_index++;
    }
}

void SigSession::rst_decoder(view::DecodeTrace *signal)
{
    for (vector< boost::shared_ptr<view::DecodeTrace> >::iterator i =
        _decode_traces.begin();
        i != _decode_traces.end();
        i++)
        if ((*i).get() == signal)
        {
            if ((*i)->create_popup())
            {
                (*i)->decoder()->stop_decode();
                (*i)->decoder()->begin_decode();
                data_updated();
            }
            return;
        }
}

pv::data::DecoderModel* SigSession::get_decoder_model() const
{
    return _decoder_model;
}
#endif

void SigSession::spectrum_rebuild()
{
    bool has_dso_signal = false;
    BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _signals) {
        boost::shared_ptr<view::DsoSignal> dsoSig;
        if ((dsoSig = dynamic_pointer_cast<view::DsoSignal>(s))) {
            has_dso_signal = true;
            // check already have
            std::vector< boost::shared_ptr<view::SpectrumTrace> >::iterator iter = _spectrum_traces.begin();
            for(unsigned int i = 0; i < _spectrum_traces.size(); i++, iter++)
                if ((*iter)->get_index() == dsoSig->get_index())
                    break;
            // if not, rebuild
            if (iter == _spectrum_traces.end()) {
                boost::shared_ptr<data::SpectrumStack> spectrum_stack(
                    new data::SpectrumStack(*this, dsoSig->get_index()));
                boost::shared_ptr<view::SpectrumTrace> spectrum_trace(
                    new view::SpectrumTrace(*this, spectrum_stack, dsoSig->get_index()));
                _spectrum_traces.push_back(spectrum_trace);
            }
        }
    }

    if (!has_dso_signal)
        _spectrum_traces.clear();

    signals_changed();
}

vector< boost::shared_ptr<view::SpectrumTrace> > SigSession::get_spectrum_traces()
{
    //lock_guard<mutex> lock(_signals_mutex);
    return _spectrum_traces;
}

void SigSession::lissajous_rebuild(bool enable, int xindex, int yindex, double percent)
{
    _lissajous_trace.reset(new view::LissajousTrace(enable, _dso_data, xindex, yindex, percent));
    signals_changed();
}

void SigSession::lissajous_disable()
{
    if (_lissajous_trace)
        _lissajous_trace->set_enable(false);
}

boost::shared_ptr<view::LissajousTrace> SigSession::get_lissajous_trace()
{
    //lock_guard<mutex> lock(_signals_mutex);
    return _lissajous_trace;
}

void SigSession::math_rebuild(bool enable,
                              boost::shared_ptr<view::DsoSignal> dsoSig1,
                              boost::shared_ptr<view::DsoSignal> dsoSig2,
                              data::MathStack::MathType type)
{
    boost::lock_guard<boost::mutex> lock(_data_mutex);
    boost::shared_ptr<data::MathStack> math_stack(
        new data::MathStack(*this, dsoSig1, dsoSig2, type));
    _math_trace.reset(new view::MathTrace(enable, math_stack, dsoSig1, dsoSig2));
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

boost::shared_ptr<view::MathTrace> SigSession::get_math_trace()
{
    //lock_guard<mutex> lock(_signals_mutex);
    return _math_trace;
}

void SigSession::set_session_time(QDateTime time)
{
    _session_time = time;
}

QDateTime SigSession::get_session_time() const
{
    return _session_time;
}

uint64_t  SigSession::get_trigger_pos() const
{
    return _trigger_pos;
}

bool SigSession::trigd() const
{
    return _trigger_flag;
}

uint8_t SigSession::trigd_ch() const
{
    return _trigger_ch;
}

void SigSession::nodata_timeout()
{
    GVariant *gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_TRIGGER_SOURCE);
    if (gvar == NULL)
        return;
    if (g_variant_get_byte(gvar) != DSO_TRIGGER_AUTO) {
        show_wait_trigger();
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

boost::shared_ptr<data::Snapshot> SigSession::get_snapshot(int type)
{
    if (type == SR_CHANNEL_LOGIC)
        return _cur_logic_snapshot;
    else if (type == SR_CHANNEL_ANALOG)
        return _cur_analog_snapshot;
    else if (type == SR_CHANNEL_DSO)
        return _cur_dso_snapshot;
    else
        return NULL;
}

SigSession::error_state SigSession::get_error() const
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

uint64_t SigSession::get_error_pattern() const
{
    return _error_pattern;
}

SigSession::run_mode SigSession::get_run_mode() const
{
    return _run_mode;
}

void SigSession::set_run_mode(run_mode mode)
{
    _run_mode = mode;
}

int SigSession::get_repeat_intvl() const
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

bool SigSession::isRepeating() const
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
        repeat_hold(_repeat_hold_prg);
        QTimer::singleShot(_repeat_intvl*1000/RepeatHoldDiv, this, SLOT(repeat_update()));
        return true;
    } else {
        return false;
    }
}

void SigSession::repeat_update()
{
    if (isRepeating()) {
        _repeat_hold_prg -= 100/RepeatHoldDiv;
        if (_repeat_hold_prg != 0)
            QTimer::singleShot(_repeat_intvl*1000/RepeatHoldDiv, this, SLOT(repeat_update()));
        repeat_hold(_repeat_hold_prg);
        if (_repeat_hold_prg == 0)
            repeat_resume();
    }
}

int SigSession::get_repeat_hold() const
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

int SigSession::get_map_zoom() const
{
    return _map_zoom;
}

void SigSession::auto_end()
{
    BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _signals) {
        boost::shared_ptr<view::DsoSignal> dsoSig;
        if ((dsoSig = dynamic_pointer_cast<view::DsoSignal>(s))) {
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

uint64_t SigSession::get_save_start() const
{
    return _save_start;
}

uint64_t SigSession::get_save_end() const
{
    return _save_end;
}

bool SigSession::get_saving() const
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

float SigSession::stop_scale() const
{
    return _stop_scale;
}

void SigSession::set_stop_scale(float scale)
{
    _stop_scale = scale;
}

} // namespace pv
