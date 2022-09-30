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
#include <map>
#include <QString>

#include "data/decode/decoderstatus.h"
#include "dsvdef.h"
#include "log.h"
#include "config/appconfig.h"
#include "utility/path.h"
#include "ui/msgbox.h"

namespace pv
{

    // TODO: This should not be necessary
    SigSession *SigSession::_session = NULL;

    SigSession::SigSession()
    {
        // TODO: This should not be necessary
        _session = this;
        _group_cnt = 0;

        _map_zoom = 0;
        _repeat_hold_prg = 0;
        _repeat_intvl = 1;
        _error = No_err;
        _is_instant = false;
        _is_working = false;
        _is_repeat_mode = false;
        _is_saving = false;
        _device_status = ST_INIT;
        _noData_cnt = 0;
        _data_lock = false;
        _data_updated = false;

        this->add_msg_listener(this);

        _decoder_model = new pv::data::DecoderModel(NULL);

        _lissajous_trace = NULL;
        _math_trace = NULL;
        _dso_feed = false;
        _stop_scale = 1;
        _is_decoding = false;
        _bClose = false;
        _callback = NULL;

        _device_agent.set_callback(this);

        // Create snapshots & data containers
        _logic_data = new data::Logic(new data::LogicSnapshot());
        _dso_data = new data::Dso(new data::DsoSnapshot());
        _analog_data = new data::Analog(new data::AnalogSnapshot());
        _group_data = new data::Group();
        _group_cnt = 0;

        _feed_timer.Stop();
        _feed_timer.SetCallback(std::bind(&SigSession::feed_timeout, this));
        _repeate_timer.SetCallback(std::bind(&SigSession::repeat_capture_wait_timout, this));
    }

    SigSession::SigSession(SigSession &o)
    {
        (void)o;
    }

    SigSession::~SigSession()
    {
    }

    bool SigSession::init()
    {
        ds_log_set_context(dsv_log_context());

        ds_set_event_callback(device_lib_event_callback);

        ds_set_datafeed_callback(data_feed_callback);

        // firmware resource directory
        QString resdir = GetResourceDir();
        std::string res_path = pv::path::ToUnicodePath(resdir);
        ds_set_firmware_resource_dir(res_path.c_str());

        if (ds_lib_init() != SR_OK)
        {
            dsv_err("%s", "DSView run ERROR: collect lib init failed.");
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
        assert(!_is_working);

        struct ds_device_base_info *array = NULL;
        int count = 0;

        dsv_info("%s", "Set default device.");

        if (ds_get_device_list(&array, &count) != SR_OK)
        {
            dsv_err("%s", "Get device list error!");
            return false;
        }
        if (count < 1 || array == NULL)
        {
            dsv_err("%s", "Error! Device list is empty, can't set default device.");
            return false;
        }

        struct ds_device_base_info *dev = (array + count - 1);
        ds_device_handle dev_handle = dev->handle;

        free(array);

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

        _callback->trigger_message(DSV_MSG_CURRENT_DEVICE_CHANGE_PREV);

        // Release the old device.
        _device_agent.release();

        if (ds_active_device(dev_handle) != SR_OK)
        {
            dsv_err("%s", "Switch device error!");
            return false;
        }

        _device_agent.update();

        if (_device_agent.is_file())
            dsv_info("Switch to file \"%s\" done.", _device_agent.name().toUtf8().data());
        else
            dsv_info("Switch to device \"%s\" done.", _device_agent.name().toUtf8().data());

        _device_status = ST_INIT;

        clear_all_decoder();

        RELEASE_ARRAY(_group_traces);        
        init_signals();

        _cur_snap_samplerate = _device_agent.get_sample_rate();
        _cur_samplelimits = _device_agent.get_sample_limit();

        if (_device_agent.get_work_mode() == DSO)
            _is_repeat_mode = true;
        else
            _is_repeat_mode = false;

        // The current device changed.
        _callback->trigger_message(DSV_MSG_CURRENT_DEVICE_CHANGED);

        return true;
    }

    bool SigSession::set_file(QString name)
    {
        assert(!_is_saving);
        assert(!_is_working);

        dsv_info("Load file:\"%s\"", name.toUtf8().data());

        std::string path = path::ToUnicodePath(name);

        if (ds_device_from_file(path.c_str()) != SR_OK)
        {
            dsv_err("%s", "Load file error!");
            return false;
        }

        return set_default_device();
    }

    void SigSession::close_file(ds_device_handle dev_handle)
    {
        assert(dev_handle);

        if (dev_handle == _device_agent.handle() && _is_working)
        {
            dsv_err("%s", "The virtual device is running, can't remove it.");
            return;
        }
        bool isCurrent = dev_handle == _device_agent.handle();

        if (ds_remove_device(dev_handle) != SR_OK)
        {
            dsv_err("%s", "Remove virtual deivice error!");
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
        return _cur_snap_samplerate;
    }

    double SigSession::cur_sampletime()
    {
        return cur_samplelimits() * 1.0 / cur_samplerate();
    }

    double SigSession::cur_snap_sampletime()
    {
        return cur_samplelimits() * 1.0 / cur_snap_samplerate();
    }

    double SigSession::cur_view_time()
    {
        return _device_agent.get_time_base() * DS_CONF_DSO_HDIVS * 1.0 / SR_SEC(1);
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
        for (auto &d : _decode_traces)
        {
            d->decoder()->set_samplerate(_cur_snap_samplerate);
        }

        // Math
        if (_math_trace && _math_trace->enabled())
            _math_trace->get_math_stack()->set_samplerate(_device_agent.get_sample_rate());
        // SpectrumStack
        for (auto &m : _spectrum_traces)
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
        // update instant setting
        _device_agent.set_config(NULL, NULL, SR_CONF_INSTANT, g_variant_new_boolean(_is_instant));
        _callback->update_capture();

        set_cur_snap_samplerate(_device_agent.get_sample_rate());
        set_cur_samplelimits(_device_agent.get_sample_limit());
        set_stop_scale(1);
        _data_updated = false;
        _trigger_flag = false;
        _trigger_ch = 0;
        _hw_replied = false;

        int work_mode = _device_agent.get_work_mode();
        if (work_mode == DSO || work_mode == ANALOG)
            _feed_timer.Start(FeedInterval);
        else
            _feed_timer.Stop();

        _noData_cnt = 0;
        data_unlock();

        // container init
        container_init();

        // update current hw offset
        for (auto &s : _signals)
        {
            assert(s);
            view::DsoSignal *dsoSig = NULL;
            if ((dsoSig = dynamic_cast<view::DsoSignal *>(s)))
            {
                dsoSig->set_zero_ratio(dsoSig->get_zero_ratio());
            }
            view::AnalogSignal *analogSig = NULL;
            if ((analogSig = dynamic_cast<view::AnalogSignal *>(s)))
            {
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
        for (auto &m : _spectrum_traces)
        {
            assert(m);
            m->get_spectrum_stack()->init();
        }

        if (_math_trace)
            _math_trace->get_math_stack()->init();

        // DecoderStack
        for (auto &d : _decode_traces)
        {
            d->decoder()->init();
        }
    }

    bool SigSession::start_capture(bool instant)
    {
        assert(_callback);

        dsv_info("%s", "Start collect.");

        if (_is_working)
        {
            dsv_err("%s", "Error! Is working now.");
            return false;
        }

        // Check that a device instance has been selected.
        if (_device_agent.have_instance() == false)
        {
            dsv_err("%s", "Error!No device selected");
            assert(false);
        }
        if (_device_agent.is_collecting())
        {
            dsv_err("%s", "Error!Device is running.");
            return false;
        }

        // update setting
        if (_device_agent.is_file())
            _is_instant = true;
        else
            _is_instant = instant;

        _callback->trigger_message(DSV_MSG_START_COLLECT_WORK_PREV);

        return exec_capture();
    }

    bool SigSession::exec_capture()
    {
        if (_device_agent.is_collecting())
        {
            dsv_err("%s", "Error!Device is running.");
            return false;
        }

        // Clear the previous decoder tasks.
        int run_dex = 0;
        clear_all_decode_task(run_dex);

        // reset measure of dso signal
        for (auto &s : _signals)
        {
            view::DsoSignal *dsoSig = NULL;
            if ((dsoSig = dynamic_cast<view::DsoSignal *>(s)))
                dsoSig->set_mValid(false);
        }

        if (_device_agent.have_enabled_channel() == false)
        {
            _callback->show_error("No probes enabled.");
            return false;
        }

        capture_init();

        if (_device_agent.start() == false)
        {
            dsv_err("%s", "Start collect error!");
            return false;
        }

        _is_working = true;
        _callback->trigger_message(DSV_MSG_START_COLLECT_WORK);
        return true;
    }

    void SigSession::stop_capture()
    {
        dsv_info("%s", "Stop collect.");

        if (!_is_working)
            return;

        if (_bClose)
        {
            _is_working = false;
            exit_capture();
            return;
        }

        bool wait_upload = false;
        if (!_is_repeat_mode)
        {
            GVariant *gvar = _device_agent.get_config(NULL, NULL, SR_CONF_WAIT_UPLOAD);
            if (gvar != NULL)
            {
                wait_upload = g_variant_get_boolean(gvar);
                g_variant_unref(gvar);
            }
        }

        if (!wait_upload)
        {
            _is_working = false;
            _callback->trigger_message(DSV_MSG_END_COLLECT_WORK_PREV);
            exit_capture();
        }
        else
        {
            dsv_info("%s", "Device is uploading.");
        }
    }

    void SigSession::exit_capture()
    {
        _is_instant = false;

        //_feed_timer
        _feed_timer.Stop();     

        if (_device_agent.is_collecting())
            _device_agent.stop();
    }

    bool SigSession::get_capture_status(bool &triggered, int &progress)
    {
        uint64_t sample_limits = cur_samplelimits();
        sr_status status;

        if (_device_agent.get_status(status, true))
        {
            triggered = status.trig_hit & 0x01;
            uint64_t captured_cnt = status.trig_hit >> 2;
            captured_cnt = ((uint64_t)status.captured_cnt0 +
                            ((uint64_t)status.captured_cnt1 << 8) +
                            ((uint64_t)status.captured_cnt2 << 16) +
                            ((uint64_t)status.captured_cnt3 << 24) +
                            (captured_cnt << 32));
            if (_device_agent.get_work_mode() == DSO)
                captured_cnt = captured_cnt * _signals.size() / get_ch_num(SR_CHANNEL_DSO);
            if (triggered)
                progress = (sample_limits - captured_cnt) * 100.0 / sample_limits;
            else
                progress = captured_cnt * 100.0 / sample_limits;
            return true;
        }
        return false;
    }

    std::vector<view::Signal *> &SigSession::get_signals()
    {
        return _signals;
    }

    std::vector<view::GroupSignal *> &SigSession::get_group_signals()
    {
        return _group_traces;
    }

    std::set<data::SignalData *> SigSession::get_data()
    {
        std::set<data::SignalData *> data;

        for (auto &sig : _signals)
        {
            assert(sig);
            data.insert(sig->data());
        }

        return data;
    }

    void SigSession::check_update()
    {
        ds_lock_guard lock(_data_mutex);

        if (_device_agent.is_collecting() == false)
            return;

        if (_data_updated)
        {
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

    void SigSession::add_group()
    {
        std::list<int> probe_index_list;

        auto i = _signals.begin();
        while (i != _signals.end())
        {
            if ((*i)->get_type() == SR_CHANNEL_LOGIC && (*i)->selected())
                probe_index_list.push_back((*i)->get_index());
            i++;
        }

        if (probe_index_list.size() > 1)
        {
            _group_data->init();
            _group_data->set_samplerate(_cur_snap_samplerate);
            auto signal = new view::GroupSignal("New Group", _group_data, probe_index_list, _group_cnt);
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

        while (i != _group_traces.end())
        {

            pv::view::GroupSignal *psig = *(i);

            if (psig->selected())
            {
                auto j = _group_traces.begin();
                while (j != _group_traces.end())
                {
                    if ((*j)->get_sec_index() > psig->get_sec_index())
                        (*j)->set_sec_index((*j)->get_sec_index() - 1);
                    j++;
                }

                auto &snapshots = _group_data->get_snapshots();
                if (!snapshots.empty())
                {
                    int dex = psig->get_sec_index();
                    pv::data::GroupSnapshot *pshot = _group_data->get_snapshots().at(dex);
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
        assert(!_is_working);

        if (_device_agent.have_instance() == false)
        {
            assert(false);
        }

        std::vector<view::Signal *> sigs;
        view::Signal *signal = NULL;
        unsigned int logic_probe_count = 0;
        unsigned int dso_probe_count = 0;
        unsigned int analog_probe_count = 0;

        _logic_data->clear();
        _dso_data->clear();
        _analog_data->clear();
        _group_data->clear();

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

        // Make the logic probe list
        RELEASE_ARRAY(_group_traces);

        std::vector<view::GroupSignal *>().swap(_group_traces);

        for (GSList *l = _device_agent.get_channels(); l; l = l->next)
        {
            sr_channel *probe =
                (sr_channel *)l->data;
            assert(probe);
            signal = NULL;

            switch (probe->type)
            {
            case SR_CHANNEL_LOGIC:
                if (probe->enabled)
                    signal = new view::LogicSignal(_logic_data, probe);
                break;

            case SR_CHANNEL_DSO:
                signal = new view::DsoSignal(_dso_data, probe);
                break;

            case SR_CHANNEL_ANALOG:
                if (probe->enabled)
                    signal = new view::AnalogSignal(_analog_data, probe);
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
        if (_device_agent.have_instance() == false)
        {
            assert(false);
        }

        if (_is_working)
            return;

        std::vector<view::Signal *> sigs;
        view::Signal *signal = NULL;

        // Make the logic probe list
        for (GSList *l = _device_agent.get_channels(); l; l = l->next)
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
                        signal = new view::LogicSignal(_logic_data, probe);
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
                        signal = new view::AnalogSignal(_analog_data, probe);
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

        if (_logic_data)
        {
            _logic_data->init();

            for (auto &d : _decode_traces)
            {
                d->decoder()->init();
            }
        }

        if (_dso_data)
        {
            _dso_data->init();
            // SpectrumStack
            for (auto &m : _spectrum_traces)
            {
                assert(m);
                m->get_spectrum_stack()->init();
            }

            if (_math_trace)
                _math_trace->get_math_stack()->init();
        }

        if (_analog_data)
        {
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
        _trigger_pos = 0;
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
                _trigger_pos = trigger_pos.real_pos;
                _callback->receive_trigger(_trigger_pos);
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
            _trigger_pos = trigger_pos.real_pos * probe_count / probe_en_count;
            _callback->receive_trigger(_trigger_pos);
        }
    }

    void SigSession::feed_in_logic(const sr_datafeed_logic &logic)
    {
        if (!_logic_data || _logic_data->snapshot()->memory_failed())
        {
            dsv_err("%s", "Unexpected logic packet");
            return;
        }

        if (logic.data_error == 1)
        {
            _error = Test_data_err;
            _error_pattern = logic.error_pattern;
            _callback->session_error();
        }

        if (_logic_data->snapshot()->last_ended())
        {
            _logic_data->snapshot()->first_payload(logic, _device_agent.get_sample_limit(), _device_agent.get_channels());
            // @todo Putting this here means that only listeners querying
            // for logic will be notified. Currently the only user of
            // frame_began is DecoderStack, but in future we need to signal
            // this after both analog and logic sweeps have begun.
            _callback->frame_began();
        }
        else
        {
            // Append to the existing data snapshot
            _logic_data->snapshot()->append_payload(logic);
        }

        if (_logic_data->snapshot()->memory_failed())
        {
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
        if (!_dso_data || _dso_data->snapshot()->memory_failed())
        {
            dsv_err("%s", "Unexpected dso packet");
            return; // This dso packet was not expected.
        }

        if (_dso_data->snapshot()->last_ended())
        {
            std::map<int, bool> sig_enable;
            // reset scale of dso signal
            for (auto &s : _signals)
            {
                assert(s);
                view::DsoSignal *dsoSig = NULL;
                if ((dsoSig = dynamic_cast<view::DsoSignal *>(s)))
                {
                    dsoSig->set_scale(dsoSig->get_view_rect().height());
                    sig_enable[dsoSig->get_index()] = dsoSig->enabled();
                }
            }

            // first payload
            _dso_data->snapshot()->first_payload(dso, _device_agent.get_sample_limit(), sig_enable, _is_instant);
        }
        else
        {
            // Append to the existing data snapshot
            _dso_data->snapshot()->append_payload(dso);
        }

        for (auto &s : _signals)
        {
            view::DsoSignal *dsoSig = NULL;
            if ((dsoSig = dynamic_cast<view::DsoSignal *>(s)) && (dsoSig->enabled()))
                dsoSig->paint_prepare();
        }

        if (dso.num_samples != 0)
        {
            // update current sample rate
            set_cur_snap_samplerate(_device_agent.get_sample_rate());
        }

        if (_dso_data->snapshot()->memory_failed())
        {
            _error = Malloc_err;
            _callback->session_error();
            return;
        }

        // calculate related spectrum results
        for (auto &m : _spectrum_traces)
        {
            assert(m);
            if (m->enabled())
                m->get_spectrum_stack()->calc_fft();
        }

        // calculate related math results
        if (_math_trace && _math_trace->enabled())
        {
            _math_trace->get_math_stack()->realloc(_device_agent.get_sample_limit());
            _math_trace->get_math_stack()->calc_math();
        }

        _trigger_flag = dso.trig_flag;
        _trigger_ch = dso.trig_ch;
        receive_data(dso.num_samples);

        if (!_is_instant)
            data_lock();

        _data_updated = true;
    }

    void SigSession::feed_in_analog(const sr_datafeed_analog &analog)
    {

        if (!_analog_data || _analog_data->snapshot()->memory_failed())
        {
            dsv_err("%s", "Unexpected analog packet");
            return; // This analog packet was not expected.
        }

        if (_analog_data->snapshot()->last_ended())
        {
            // reset scale of analog signal
            for (auto &s : _signals)
            {
                assert(s);
                view::AnalogSignal *analogSig = NULL;
                if ((analogSig = dynamic_cast<view::AnalogSignal *>(s)))
                {
                    analogSig->set_scale(analogSig->get_totalHeight());
                }
            }

            // first payload
            _analog_data->snapshot()->first_payload(analog, _device_agent.get_sample_limit(), _device_agent.get_channels());
        }
        else
        {
            // Append to the existing data snapshot
            _analog_data->snapshot()->append_payload(analog);
        }

        if (_analog_data->snapshot()->memory_failed())
        {
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
            feed_in_logic(*(const sr_datafeed_logic *)packet->payload);
            break;

        case SR_DF_DSO:
            assert(packet->payload);
            feed_in_dso(*(const sr_datafeed_dso *)packet->payload);
            break;

        case SR_DF_ANALOG:
            assert(packet->payload);
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

            for (auto trace : _decode_traces)
            {
                trace->decoder()->frame_ended();
                trace->frame_ended();
                add_decode_task(trace);
            }

            if (packet->status != SR_PKT_OK)
            {
                _error = Pkt_data_err;
                _callback->session_error();
            }

            _callback->frame_ended();

            if (_device_agent.get_work_mode() != LOGIC)
            {
                set_session_time(QDateTime::currentDateTime());
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
            for (auto &s : _signals)
            {
                assert(s);
                if (dynamic_cast<view::LogicSignal *>(s) && s->enabled())
                {
                    logic_ch_num++;
                }
                if (dynamic_cast<view::DsoSignal *>(s) && s->enabled())
                {
                    dso_ch_num++;
                }
                if (dynamic_cast<view::AnalogSignal *>(s) && s->enabled())
                {
                    analog_ch_num++;
                }
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
                                 std::list<pv::data::decode::Decoder *> &sub_decoders)
    {

        if (dec == NULL)
        {
            dsv_err("%s", "Decoder instance is null!");
            assert(false);
        }

        dsv_info("Create new decoder,name:\"%s\",id:\"%s\"", dec->name, dec->id);

        try
        {

            bool ret = false;

            // Create the decoder
            std::map<const srd_channel *, int> probes;
            data::DecoderStack *decoder_stack = new data::DecoderStack(this, dec, dstatus);
            assert(decoder_stack);

            // Make a list of all the probes
            std::vector<const srd_channel *> all_probes;
            for (const GSList *i = dec->channels; i; i = i->next)
                all_probes.push_back((const srd_channel *)i->data);

            for (const GSList *i = dec->opt_channels; i; i = i->next)
                all_probes.push_back((const srd_channel *)i->data);

            decoder_stack->stack().front()->set_probes(probes);

            // Create the decode signal
            view::DecodeTrace *trace = new view::DecodeTrace(this, decoder_stack, _decode_traces.size());
            assert(trace);

            // add sub decoder
            for (auto sub : sub_decoders)
            {
                trace->decoder()->add_sub_decoder(sub);
            }
            sub_decoders.clear();

            // set view early for decode start/end region setting
            for (auto &s : _signals)
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
                if (!silent)
                {
                    add_decode_task(trace);
                }

                signals_changed();
                data_updated();
            }
            else
            {
                delete trace;
            }

            return ret;
        }
        catch (...)
        {
            dsv_err("%s", "Error!add_decoder() throws an exception.");
        }

        return false;
    }

    std::vector<view::DecodeTrace *> &SigSession::get_decode_signals()
    {
        return _decode_traces;
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

        if (trace && trace->create_popup(false))
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

    pv::data::DecoderModel *SigSession::get_decoder_model()
    {
        return _decoder_model;
    }

    void SigSession::spectrum_rebuild()
    {
        bool has_dso_signal = false;
        for (auto &s : _signals)
        {
            view::DsoSignal *dsoSig = NULL;
            if ((dsoSig = dynamic_cast<view::DsoSignal *>(s)))
            {
                has_dso_signal = true;
                // check already have
                auto iter = _spectrum_traces.begin();
                for (unsigned int i = 0; i < _spectrum_traces.size(); i++, iter++)
                    if ((*iter)->get_index() == dsoSig->get_index())
                        break;
                // if not, rebuild
                if (iter == _spectrum_traces.end())
                {
                    auto spectrum_stack = new data::SpectrumStack(this, dsoSig->get_index());
                    auto spectrum_trace = new view::SpectrumTrace(this, spectrum_stack, dsoSig->get_index());
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

    std::vector<view::SpectrumTrace *> &SigSession::get_spectrum_traces()
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

    view::LissajousTrace *SigSession::get_lissajous_trace()
    {
        return _lissajous_trace;
    }

    void SigSession::math_rebuild(bool enable, view::DsoSignal *dsoSig1,
                                  view::DsoSignal *dsoSig2,
                                  data::MathStack::MathType type)
    {
        ds_lock_guard lock(_data_mutex);

        auto math_stack = new data::MathStack(this, dsoSig1, dsoSig2, type);
        DESTROY_OBJECT(_math_trace);
        _math_trace = new view::MathTrace(enable, math_stack, dsoSig1, dsoSig2);

        if (_math_trace && _math_trace->enabled())
        {
            _math_trace->get_math_stack()->set_samplerate(_dso_data->samplerate());
            _math_trace->get_math_stack()->realloc(_device_agent.get_sample_limit());
            _math_trace->get_math_stack()->calc_math();
        }
        signals_changed();
    }

    void SigSession::math_disable()
    {
        if (_math_trace)
            _math_trace->set_enable(false);
    }

    view::MathTrace *SigSession::get_math_trace()
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

    uint64_t SigSession::get_trigger_pos()
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
        GVariant *gvar = _device_agent.get_config(NULL, NULL, SR_CONF_TRIGGER_SOURCE);
        if (gvar == NULL)
            return;
        if (g_variant_get_byte(gvar) != DSO_TRIGGER_AUTO)
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

    double SigSession::get_repeat_intvl()
    {
        return _repeat_intvl;
    }

    void SigSession::set_repeat_intvl(double interval)
    {
        _repeat_intvl = interval;
    }

    bool SigSession::repeat_check()
    {
        if (!_is_working)
        {
            return false;
        }

        if (_device_agent.get_work_mode() == LOGIC)
        {
            _repeat_hold_prg = 100;
            _callback->repeat_hold(_repeat_hold_prg);
            _out_timer.TimeOut(_repeat_intvl * 1000 / RepeatHoldDiv, std::bind(&SigSession::repeat_update, this));
            return true;
        }
        else
        {
            return false;
        }
    }

    void SigSession::repeat_update()
    {
        if (!_is_instant && _is_working && _is_repeat_mode)
        {
            _repeat_hold_prg -= 100 / RepeatHoldDiv;
            if (_repeat_hold_prg != 0)
            {
                _out_timer.TimeOut(_repeat_intvl * 1000 / RepeatHoldDiv, std::bind(&SigSession::repeat_update, this));
            }
            _callback->repeat_hold(_repeat_hold_prg);
            if (_repeat_hold_prg == 0)
                repeat_resume();
        }
    }

    int SigSession::get_repeat_hold()
    {
        if (!_is_instant && _is_working && _is_repeat_mode)
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
        for (auto &s : _signals)
        {
            view::DsoSignal *dsoSig = NULL;
            if ((dsoSig = dynamic_cast<view::DsoSignal *>(s)))
            {
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

    float SigSession::stop_scale()
    {
        return _stop_scale;
    }

    void SigSession::set_stop_scale(float scale)
    {
        _stop_scale = scale;
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

        stop_capture();

        // TODO: This should not be necessary
        _session = NULL;
    }

    // append a decode task, and try create a thread
    void SigSession::add_decode_task(view::DecodeTrace *trace)
    {
        std::lock_guard<std::mutex> lock(_decode_task_mutex);
        _decode_tasks.push_back(trace);

        if (!_is_decoding)
        {
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
                dsv_info("%s", "remove a waiting decode task");
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
            {
                delete trace;
            }
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
        dsv_info("%s", "------->decode thread start");
        auto task = get_top_decode_task();

        while (task != NULL)
        {
            if (!task->_delete_flag)
            {
                task->decoder()->begin_decode_work();
            }

            if (task->_delete_flag)
            {
                dsv_info("%s", "destroy a decoder in task thread");

                DESTROY_QT_LATER(task);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (!_bClose)
                {
                    signals_changed();
                }
            }

            task = get_top_decode_task();
        }

        dsv_info("%s", "------->decode thread end");
        _is_decoding = false;
    }

    Snapshot *SigSession::get_signal_snapshot()
    {
        int mode = _device_agent.get_work_mode();
        if (mode == ANALOG)
            return _analog_data->snapshot();
        else if (mode == DSO)
            return _dso_data->snapshot();
        else
            return _logic_data->snapshot();
    }

    void SigSession::device_lib_event_callback(int event)
    {
        if (_session == NULL)
        {
            dsv_err("%s", "Error!Global variable \"_session\" is null.");
            return;
        }
        _session->on_device_lib_event(event);
    }

    void SigSession::on_device_lib_event(int event)
    {
        if (_callback == NULL)
        {
            dsv_detail("%s", "The callback is null, so the device event was ignored.");
            return;
        }

        switch (event)
        {
        case DS_EV_DEVICE_RUNNING:
            _device_status = ST_RUNNING;
            receive_data(0);
            break;

        case DS_EV_DEVICE_STOPPED:
            _device_status = ST_STOPPED;
            // Confirm that SR_DF_END was received
            if (!_logic_data->snapshot()->last_ended() || !_dso_data->snapshot()->last_ended() || !_analog_data->snapshot()->last_ended())
            {
                dsv_err("%s", "Error!The data is not completed.");
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

            if (_logic_data->snapshot()->last_ended() == false)
                dsv_err("%s", "The collected data is error!");
            if (_dso_data->snapshot()->last_ended() == false)
                dsv_err("%s", "The collected data is error!");
            if (_analog_data->snapshot()->last_ended() == false)
                dsv_err("%s", "The collected data is error!");

            // trigger next collect
            if (!_is_instant && _is_repeat_mode && _is_working && event == DS_EV_COLLECT_TASK_END)
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
        case DS_EV_CURRENT_DEVICE_DETACH:
        {
            if (_is_working)
                stop_capture();

            if (DS_EV_NEW_DEVICE_ATTACH == event)
                _callback->trigger_message(DSV_MSG_NEW_USB_DEVICE);
            else
                _callback->trigger_message(DSV_MSG_CURRENT_DEVICE_DETACHED);
        }
        break;

        case DS_EV_INACTIVE_DEVICE_DETACH:
            _callback->trigger_message(DSV_MSG_DEVICE_LIST_UPDATED); // Update list only.
            break;

        default:
            dsv_err("%s", "Error!Unknown device event.");
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

    void SigSession::set_repeat_mode(bool repeat)
    {
        assert(!_is_working);

        if (_is_repeat_mode != repeat)
        {
            _is_repeat_mode = repeat;
            _repeat_hold_prg = 0;
        }
    }

    void SigSession::repeat_capture_wait_timout()
    {
        exec_capture();
    }

    void SigSession::OnMessage(int msg)
    {
        switch (msg)
        {
        case DSV_MSG_DEVICE_OPTIONS_UPDATED:
            reload();
            break;

        case DSV_MSG_TRIG_NEXT_COLLECT:
            if (_repeat_intvl > 0)
                _repeate_timer.Start(_repeat_intvl * 1000);
            else
                exec_capture();
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

        if (_device_agent.get_work_mode() != mode)
        {
            GVariant *val = g_variant_new_int16(mode);
            _device_agent.set_config(NULL, NULL, SR_CONF_DEVICE_MODE, val);

            clear_all_decoder();
            init_signals();
            dsv_info("%s", "Work mode is changed.");
            broadcast_msg(DSV_MSG_DEVICE_MODE_CHANGED);
            return true;
        }
        return false;
    }

} // namespace pv
