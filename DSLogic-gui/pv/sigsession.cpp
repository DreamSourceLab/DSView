/*
 * This file is part of the DSLogic-gui project.
 * DSLogic-gui is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2013 DreamSourceLab <dreamsourcelab@dreamsourcelab.com>
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


#include "sigsession.h"

#include "devicemanager.h"
#include "data/analog.h"
#include "data/analogsnapshot.h"
#include "data/dso.h"
#include "data/dsosnapshot.h"
#include "data/logic.h"
#include "data/logicsnapshot.h"
#include "data/group.h"
#include "data/groupsnapshot.h"
#include "view/analogsignal.h"
#include "view/dsosignal.h"
#include "view/logicsignal.h"
#include "view/groupsignal.h"
#include "view/protocolsignal.h"
#include "decoder/decoder.h"
#include "decoder/decoderfactory.h"

#include <assert.h>

#include <QDebug>
#include <QMessageBox>

#include <boost/foreach.hpp>

using namespace boost;
using namespace std;

namespace pv {

const float SigSession::Oversampling = 2.0f;

// TODO: This should not be necessary
SigSession* SigSession::_session = NULL;

SigSession::SigSession(DeviceManager &device_manager) :
	_device_manager(device_manager),
	_sdi(NULL),
    _capture_state(Init),
    _last_sample_rate(1),
    _total_sample_len(1)
{
	// TODO: This should not be necessary
	_session = this;
    _hot_attach = false;
    _hot_detach = false;
    _adv_trigger = false;
    _group_cnt = 0;
    _protocol_cnt = 0;
    _decoderFactory = new decoder::DecoderFactory();
    ds_trigger_init();

    _vDial_changed = false;
    _hDial_changed = false;
    _dso_ctrl_channel = 0;
	register_hotplug_callback();
}

SigSession::~SigSession()
{
    stop_capture();

    if (_sampling_thread.get())
	_sampling_thread->join();
    _sampling_thread.reset();

    if (_hotplug_handle) {
        stop_hotplug_proc();
        deregister_hotplug_callback();
    }

    ds_trigger_destroy();
    stop_dso_ctrl_proc();

	// TODO: This should not be necessary
	_session = NULL;
}

quint64 SigSession::get_last_sample_rate() const
{
    return _last_sample_rate;
}

quint64 SigSession::get_total_sample_len() const
{
    return _total_sample_len;
}

void SigSession::set_total_sample_len(quint64 length)
{
    _total_sample_len = length;
}

struct sr_dev_inst* SigSession::get_device() const
{
	return _sdi;
}

int SigSession::set_device(struct sr_dev_inst *sdi)
{
    int ret = SR_ERR;

    if (_sdi == NULL) {
        ret = _device_manager.use_device(sdi, this);
        _sdi = sdi;
        set_capture_state(Init);
    } else if (sdi != _sdi) {
        ret = _device_manager.use_device(sdi, this);
        _device_manager.release_device(_sdi);
        _sdi = sdi;
        set_capture_state(Init);
    } else {
        ret = SR_OK;
    }

    return ret;
}

void SigSession::release_device(struct sr_dev_inst *sdi)
{
	(void)sdi;

    assert(_capture_state != Running);
	_sdi = NULL;
}

void SigSession::save_file(const std::string &name){
    if (_sdi->mode == LOGIC) {
        const deque< boost::shared_ptr<pv::data::LogicSnapshot> > &snapshots =
            _logic_data->get_snapshots();
        if (snapshots.empty())
            return;

        const boost::shared_ptr<pv::data::LogicSnapshot> &snapshot =
            snapshots.front();

        sr_session_save(name.c_str(), _sdi,
                        (unsigned char*)snapshot->get_data(),
                        snapshot->get_unit_size(),
                        snapshot->get_sample_count());
    } else if (_sdi->mode == DSO){
        const deque< boost::shared_ptr<pv::data::DsoSnapshot> > &snapshots =
            _dso_data->get_snapshots();
        if (snapshots.empty())
            return;

        const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot =
            snapshots.front();

        sr_session_save(name.c_str(), _sdi,
                        (unsigned char*)snapshot->get_data(),
                        snapshot->get_unit_size(),
                        snapshot->get_sample_count());
    } else {
        const deque< boost::shared_ptr<pv::data::AnalogSnapshot> > &snapshots =
            _analog_data->get_snapshots();
        if (snapshots.empty())
            return;

        const boost::shared_ptr<pv::data::AnalogSnapshot> &snapshot =
            snapshots.front();

        sr_session_save(name.c_str(), _sdi,
                        (unsigned char*)snapshot->get_data(),
                        snapshot->get_unit_size(),
                        snapshot->get_sample_count());
    }
}

void SigSession::load_file(const string &name,
	boost::function<void (const QString)> error_handler)
{
	stop_capture();
	_sampling_thread.reset(new boost::thread(
		&SigSession::load_thread_proc, this, name,
		error_handler));
}

SigSession::capture_state SigSession::get_capture_state() const
{
	boost::lock_guard<boost::mutex> lock(_sampling_mutex);
	return _capture_state;
}

void SigSession::start_capture(uint64_t record_length,
	boost::function<void (const QString)> error_handler)
{
	stop_capture();

	// Check that a device instance has been selected.
	if (!_sdi) {
		qDebug() << "No device selected";
		return;
	}

	// Check that at least one probe is enabled
	const GSList *l;
	for (l = _sdi->probes; l; l = l->next) {
		sr_probe *const probe = (sr_probe*)l->data;
		assert(probe);
		if (probe->enabled)
			break;
	}
	if (!l) {
		error_handler(tr("No probes enabled."));
		return;
	}

    // Check that at least one signal channel is active under DSO mode
    if (_sdi->mode == DSO) {
        bool active = false;
        BOOST_FOREACH(const shared_ptr<view::Signal> s, _signals)
        {
            assert(s);
            if (s->get_active()) {
                active = true;
                break;
            }
        }
        if (!active) {
            error_handler(tr("No channels enabled."));
            return;
        }
    }

	// Begin the session
	_sampling_thread.reset(new boost::thread(
		&SigSession::sample_thread_proc, this, _sdi,
		record_length, error_handler));
}

void SigSession::stop_capture()
{
	if (get_capture_state() == Stopped)
		return;
	sr_session_stop();

	// Check that sampling stopped
	if (_sampling_thread.get())
		_sampling_thread->join();
	_sampling_thread.reset();
}

vector< boost::shared_ptr<view::Signal> > SigSession::get_signals()
{
	boost::lock_guard<boost::mutex> lock(_signals_mutex);
	return _signals;
}

vector< boost::shared_ptr<view::Signal> > SigSession::get_pro_signals()
{
    boost::lock_guard<boost::mutex> lock(_signals_mutex);
    return _protocol_signals;
}

int SigSession::get_logic_probe_cnt(const sr_dev_inst *sdi)
{
    unsigned int logic_probe_cnt = 0;
    // Detect what data types we will receive
    for (const GSList *l = sdi->probes; l; l = l->next) {
        const sr_probe *const probe = (const sr_probe *)l->data;
        if (!probe->enabled)
            continue;

        switch(probe->type) {
        case SR_PROBE_LOGIC:
            logic_probe_cnt++;
            break;
        }
    }

    return logic_probe_cnt;
}

int SigSession::get_dso_probe_cnt(const sr_dev_inst *sdi)
{
    unsigned int dso_probe_cnt = 0;
    for (const GSList *l = sdi->probes; l; l = l->next) {
        const sr_probe *const probe = (const sr_probe *)l->data;
        if (!probe->enabled)
            continue;

        switch(probe->type) {
        case SR_PROBE_DSO:
            dso_probe_cnt++;
            break;
        }
    }

    return dso_probe_cnt;
}

int SigSession::get_analog_probe_cnt(const sr_dev_inst *sdi)
{
    unsigned int analog_probe_cnt = 0;
    for (const GSList *l = sdi->probes; l; l = l->next) {
        const sr_probe *const probe = (const sr_probe *)l->data;
        if (!probe->enabled)
            continue;

        switch(probe->type) {
        case SR_PROBE_ANALOG:
            analog_probe_cnt++;
            break;
        }
    }

    return analog_probe_cnt;
}

boost::shared_ptr<data::Logic> SigSession::get_data()
{
	return _logic_data;
}

void* SigSession::get_buf(int& unit_size, uint64_t &length)
{
    if (_sdi->mode == LOGIC) {
        const deque< boost::shared_ptr<pv::data::LogicSnapshot> > &snapshots =
            _logic_data->get_snapshots();
        if (snapshots.empty())
            return NULL;

        const boost::shared_ptr<pv::data::LogicSnapshot> &snapshot =
            snapshots.front();

        unit_size = snapshot->get_unit_size();
        length = snapshot->get_sample_count();
        return snapshot->get_data();
    } else if (_sdi->mode == DSO) {
        const deque< boost::shared_ptr<pv::data::DsoSnapshot> > &snapshots =
            _dso_data->get_snapshots();
        if (snapshots.empty())
            return NULL;

        const boost::shared_ptr<pv::data::DsoSnapshot> &snapshot =
            snapshots.front();

        unit_size = snapshot->get_unit_size();
        length = snapshot->get_sample_count();
        return snapshot->get_data();
    } else {
        const deque< boost::shared_ptr<pv::data::AnalogSnapshot> > &snapshots =
            _analog_data->get_snapshots();
        if (snapshots.empty())
            return NULL;

        const boost::shared_ptr<pv::data::AnalogSnapshot> &snapshot =
            snapshots.front();

        unit_size = snapshot->get_unit_size();
        length = snapshot->get_sample_count();
        return snapshot->get_data();
    }
}

void SigSession::set_capture_state(capture_state state)
{
	boost::lock_guard<boost::mutex> lock(_sampling_mutex);
	_capture_state = state;
    data_updated();
	capture_state_changed(state);
}

void SigSession::load_thread_proc(const string name,
	boost::function<void (const QString)> error_handler)
{
    if (sr_session_load(name.c_str()) != SR_OK) {
        error_handler(tr("Failed to load file."));
        return;
    }

	sr_session_datafeed_callback_add(data_feed_in_proc, NULL);

	if (sr_session_start() != SR_OK) {
		error_handler(tr("Failed to start session."));
		return;
	}

	set_capture_state(Running);

	sr_session_run();
	sr_session_destroy();

	set_capture_state(Stopped);

	// Confirm that SR_DF_END was received
	assert(!_cur_logic_snapshot);
    assert(!_cur_dso_snapshot);
	assert(!_cur_analog_snapshot);
}

void SigSession::sample_thread_proc(struct sr_dev_inst *sdi,
	uint64_t record_length,
	boost::function<void (const QString)> error_handler)
{
    assert(sdi);
    assert(error_handler);

    if (!_adv_trigger) {
        /* simple trigger check trigger_enable */
        ds_trigger_set_en(false);
        BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _signals)
        {
            assert(s);
            if (s->get_trig() != 0) {
                ds_trigger_set_en(true);
                s->set_trig(s->get_trig());
            }
        }
    } else {
        /* advanced trigger check trigger_enable */
        ds_trigger_set_en(true);
    }

    sr_session_new();
    sr_session_datafeed_callback_add(data_feed_in_proc, NULL);

    if (sr_session_dev_add(sdi) != SR_OK) {
        error_handler(tr("Failed to use device."));
        sr_session_destroy();
        return;
    }

    // Set the sample limit
    if (sr_config_set(sdi, SR_CONF_LIMIT_SAMPLES,
        g_variant_new_uint64(record_length)) != SR_OK) {
        error_handler(tr("Failed to configure "
            "time-based sample limit."));
        sr_session_destroy();
        return;
    }

    receive_data(0);
    set_capture_state(Running);

    if (sr_session_start() != SR_OK) {
        error_handler(tr("Failed to start session."));
        set_capture_state(Stopped);
        return;
    }

    sr_session_run();
    sr_session_destroy();

    set_capture_state(Stopped);

    // Confirm that SR_DF_END was received
    assert(!_cur_logic_snapshot);
    assert(!_cur_dso_snapshot);
    assert(!_cur_analog_snapshot);
}

void SigSession::feed_in_header(const sr_dev_inst *sdi)
{
	boost::shared_ptr<view::Signal> signal;
	GVariant *gvar;
	uint64_t sample_rate = 0;
	unsigned int logic_probe_count = 0;
    unsigned int dso_probe_count = 0;
    unsigned int dso_channel_count = 0;
	unsigned int analog_probe_count = 0;

	// Detect what data types we will receive
	for (const GSList *l = sdi->probes; l; l = l->next) {
		const sr_probe *const probe = (const sr_probe *)l->data;
		if (!probe->enabled)
			continue;

		switch(probe->type) {
		case SR_PROBE_LOGIC:
			logic_probe_count++;
			break;

        case SR_PROBE_DSO:
            dso_probe_count++;
            break;

		case SR_PROBE_ANALOG:
			analog_probe_count++;
			break;
		}
	}

	// Read out the sample rate
	assert(sdi->driver);

    int ret = sr_config_get(sdi->driver, SR_CONF_SAMPLERATE,
		&gvar, sdi);
	if (ret != SR_OK) {
		qDebug("Failed to get samplerate\n");
		return;
	}
	sample_rate = g_variant_get_uint64(gvar);
	g_variant_unref(gvar);

//    ret = sr_config_get(sdi->driver, SR_CONF_LIMIT_SAMPLES,
//        &gvar, sdi);
//    if (ret != SR_OK) {
//        qDebug("Failed to get total samples");
//        return;
//    }
//    if (g_variant_get_uint64(gvar) != 0)
//        _total_sample_len = g_variant_get_uint64(gvar);
//    g_variant_unref(gvar);

    if (sample_rate != _last_sample_rate) {
        _last_sample_rate = sample_rate;
        sample_rate_changed(sample_rate);
    }
    ret = sr_config_get(sdi->driver, SR_CONF_EN_CH0,
        &gvar, sdi);
    if (ret != SR_OK) {
        qDebug("Failed to get ENABLE of DSO channel 0\n");
        return;
    }
    dso_channel_count += g_variant_get_boolean(gvar);
    ret = sr_config_get(sdi->driver, SR_CONF_EN_CH1,
        &gvar, sdi);
    if (ret != SR_OK) {
        qDebug("Failed to get ENABLE of DSO channel 1\n");
        return;
    }
    dso_channel_count += g_variant_get_boolean(gvar);


	// Create data containers for the coming data snapshots
	{
		boost::lock_guard<boost::mutex> data_lock(_data_mutex);

		if (logic_probe_count != 0) {
			_logic_data.reset(new data::Logic(
				logic_probe_count, sample_rate));
			assert(_logic_data);

            _group_data.reset(new data::Group(logic_probe_count, sample_rate));
            assert(_group_data);
		}

        if (dso_probe_count != 0) {
            _dso_data.reset(new data::Dso(dso_channel_count, sample_rate));
            assert(_dso_data);
        }

		if (analog_probe_count != 0) {
            _analog_data.reset(new data::Analog(analog_probe_count, sample_rate));
			assert(_analog_data);
		}
	}

    // Set Signal data
    {
        BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _signals)
        {
            assert(s);
            s->set_data(_logic_data, _dso_data, _analog_data, _group_data);
        }

        receive_data(0);
        //signals_changed();
    }
}

void SigSession::add_group()
{
    std::list<int> probe_index_list;

    std::vector< boost::shared_ptr<view::Signal> >::iterator i = _signals.begin();
    while (i != _signals.end()) {
        if ((*i)->get_type() == view::Signal::DS_LOGIC && (*i)->selected())
            probe_index_list.push_back((*i)->get_index());
        i++;
    }

    if (probe_index_list.size() > 1) {
        //_group_data.reset(new data::Group(_last_sample_rate));
        const boost::shared_ptr<view::Signal> signal = boost::shared_ptr<view::Signal>(
                    new view::GroupSignal("New Group",
                                          _group_data, probe_index_list, _signals.size(), _group_cnt));
        _signals.push_back(signal);
        _group_cnt++;

        if (_capture_state == Stopped) {
            if (!_cur_group_snapshot)
            {
                // Create a new data snapshot
                _cur_group_snapshot = boost::shared_ptr<data::GroupSnapshot>(
                            new data::GroupSnapshot(_logic_data->get_snapshots().front(), signal->get_index_list()));
                //_cur_group_snapshot->append_payload();
                _group_data->push_snapshot(_cur_group_snapshot);
                _cur_group_snapshot.reset();
            }
        }

        signals_changed();
        data_updated();
    }
}

void SigSession::del_group()
{
    std::vector< boost::shared_ptr<view::Signal> >::iterator i = _signals.begin();
    while (i != _signals.end()) {
        if ((*i)->get_type() == view::Signal::DS_GROUP) {
            if ((*i)->selected()) {
                std::vector< boost::shared_ptr<view::Signal> >::iterator j = _signals.begin();
                while(j != _signals.end()) {
                    if ((*j)->get_order() > (*i)->get_order())
                        (*j)->set_order((*j)->get_order() - 1);
                    if ((*j)->get_sec_index() > (*i)->get_sec_index())
                        (*j)->set_sec_index((*j)->get_sec_index() - 1);
                    j++;
                }

                _group_data->get_snapshots().at((*i)->get_sec_index()).reset();
                std::deque< boost::shared_ptr<data::GroupSnapshot> >::iterator k = _group_data->get_snapshots().begin();
                k += (*i)->get_sec_index();
                _group_data->get_snapshots().erase(k);

                (*i).reset();
                i = _signals.erase(i);

                _group_cnt--;
                continue;
            }
        }
        i++;
    }

    signals_changed();
    data_updated();
}

void SigSession::add_protocol(std::list<int> probe_index_list, decoder::Decoder *decoder)
{
    assert(_logic_data);

    std::vector< boost::shared_ptr<view::Signal> >::iterator i = _signals.begin();
    while (i != _signals.end()) {
        (*i)->set_order((*i)->get_order() + 1);
        i++;
    }

    if (probe_index_list.size() > 0) {
        //_group_data.reset(new data::Group(_last_sample_rate));
        const boost::shared_ptr<view::Signal> signal = boost::shared_ptr<view::Signal>(
                    new view::ProtocolSignal(decoder->get_decode_name(),
                                          _logic_data, decoder, probe_index_list, 0, _protocol_cnt));
        _signals.push_back(signal);
        _protocol_cnt++;

        signals_changed();
        data_updated();
    }
}

void SigSession::del_protocol(int protocol_index)
{
    std::vector< boost::shared_ptr<view::Signal> >::iterator i = _signals.begin();
    while (i != _signals.end()) {
        if ((*i)->get_type() == view::Signal::DS_PROTOCOL) {
            if ((*i)->get_sec_index() == protocol_index) {
                std::vector< boost::shared_ptr<view::Signal> >::iterator j = _signals.begin();
                while(j != _signals.end()) {
                    if ((*j)->get_order() > (*i)->get_order())
                        (*j)->set_order((*j)->get_order() - 1);
                    if ((*j)->get_sec_index() > (*i)->get_sec_index())
                        (*j)->set_sec_index((*j)->get_sec_index() - 1);
                    j++;
                }

                (*i).reset();
                i = _signals.erase(i);

                _protocol_cnt--;
                break;
            }
        }
        i++;
    }

    signals_changed();
    data_updated();
}

void SigSession::del_signal(std::vector< boost::shared_ptr<view::Signal> >::iterator i)
{
    std::vector< boost::shared_ptr<view::Signal> >::iterator j = _signals.begin();
    while(j != _signals.end()) {
        if ((*j)->get_order() > (*i)->get_order())
            (*j)->set_order((*j)->get_order() - 1);
        j++;
    }

    (*i).reset();
    _signals.erase(i);
}

void SigSession::init_signals(const sr_dev_inst *sdi)
{
    boost::shared_ptr<view::Signal> signal;
    GVariant *gvar;
    uint64_t sample_rate = 0;
    unsigned int logic_probe_count = 0;
    unsigned int dso_probe_count = 0;
    unsigned int dso_channel_count = 0;
    unsigned int analog_probe_count = 0;
    uint64_t vdiv;
    uint64_t timebase;
    bool coupling;
    bool active;
    int ret;

    // Detect what data types we will receive
    for (const GSList *l = sdi->probes; l; l = l->next) {
        const sr_probe *const probe = (const sr_probe *)l->data;
        if (!probe->enabled)
            continue;

        switch(probe->type) {
        case SR_PROBE_LOGIC:
            logic_probe_count++;
            break;

        case SR_PROBE_DSO:
            dso_probe_count++;
            break;

        case SR_PROBE_ANALOG:
            analog_probe_count++;
            break;
        }
    }

    // Read out the sample rate
    assert(sdi->driver);

    ret = sr_config_get(sdi->driver, SR_CONF_SAMPLERATE,
        &gvar, sdi);
    if (ret != SR_OK) {
        qDebug("Failed to get samplerate\n");
        return;
    }
    sample_rate = g_variant_get_uint64(gvar);

    if (sample_rate != _last_sample_rate) {
        _last_sample_rate = sample_rate;
        sample_rate_changed(sample_rate);
    }

    ret = sr_config_get(sdi->driver, SR_CONF_EN_CH0,
        &gvar, sdi);
    if (ret != SR_OK) {
        qDebug("Failed to get ENABLE of DSO channel 0\n");
        return;
    }
    dso_channel_count += g_variant_get_boolean(gvar);
    ret = sr_config_get(sdi->driver, SR_CONF_EN_CH1,
        &gvar, sdi);
    if (ret != SR_OK) {
        qDebug("Failed to get ENABLE of DSO channel 1\n");
        return;
    }
    dso_channel_count += g_variant_get_boolean(gvar);

    // Create data containers for the coming data snapshots
    {
        if (logic_probe_count != 0) {
            _logic_data.reset(new data::Logic(
                logic_probe_count, sample_rate));
            assert(_logic_data);

            _group_data.reset(new data::Group(logic_probe_count, sample_rate));
            assert(_group_data);
            _group_cnt = 0;
        }

        if (dso_probe_count != 0) {
            _dso_data.reset(new data::Dso(dso_channel_count, sample_rate));
            assert(_dso_data);
        }

        if (analog_probe_count != 0) {
            _analog_data.reset(new data::Analog(analog_probe_count, sample_rate));
            assert(_analog_data);
        }
    }

    // Make the logic probe list
    {
        _signals.clear();

        for (const GSList *l = sdi->probes; l; l = l->next) {
            const sr_probe *const probe =
                (const sr_probe *)l->data;
            assert(probe);
            if (!probe->enabled)
                continue;

            switch(probe->type) {
            case SR_PROBE_LOGIC:
                signal = boost::shared_ptr<view::Signal>(
                    new view::LogicSignal(probe->name,
                        _logic_data, probe->index, _signals.size()));
                break;

            case SR_PROBE_DSO:
                if (probe->index == 0) {
                    ret = sr_config_get(sdi->driver, SR_CONF_VDIV0,
                        &gvar, sdi);
                    if (ret != SR_OK) {
                        qDebug("Failed to get VDIV of channel 0\n");
                        return;
                    }
                    vdiv = g_variant_get_uint64(gvar);
                    ret = sr_config_get(sdi->driver, SR_CONF_TIMEBASE,
                        &gvar, sdi);
                    if (ret != SR_OK) {
                        qDebug("Failed to get TIMEBASE\n");
                        return;
                    }
                    timebase = g_variant_get_uint64(gvar);
                    ret = sr_config_get(sdi->driver, SR_CONF_COUPLING0,
                        &gvar, sdi);
                    if (ret != SR_OK) {
                        qDebug("Failed to get AC COUPLING of channel 0\n");
                        return;
                    }
                    coupling = g_variant_get_boolean(gvar);
                    ret = sr_config_get(sdi->driver, SR_CONF_EN_CH0,
                        &gvar, sdi);
                    if (ret != SR_OK) {
                        qDebug("Failed to get ENABLE of channel 0\n");
                        return;
                    }
                    active = g_variant_get_boolean(gvar);
                } else if (probe->index == 1) {
                    ret = sr_config_get(sdi->driver, SR_CONF_VDIV1,
                        &gvar, sdi);
                    if (ret != SR_OK) {
                        qDebug("Failed to get VDIV of channel 1\n");
                        return;
                    }
                    vdiv = g_variant_get_uint64(gvar);
                    ret = sr_config_get(sdi->driver, SR_CONF_TIMEBASE,
                        &gvar, sdi);
                    if (ret != SR_OK) {
                        qDebug("Failed to get TIMEBASE\n");
                        return;
                    }
                    timebase = g_variant_get_uint64(gvar);
                    ret = sr_config_get(sdi->driver, SR_CONF_COUPLING1,
                        &gvar, sdi);
                    if (ret != SR_OK) {
                        qDebug("Failed to get AC COUPLING of channel 1\n");
                        return;
                    }
                    coupling = g_variant_get_boolean(gvar);
                    ret = sr_config_get(sdi->driver, SR_CONF_EN_CH1,
                        &gvar, sdi);
                    if (ret != SR_OK) {
                        qDebug("Failed to get ENABLE of channel 1\n");
                        return;
                    }
                    active = g_variant_get_boolean(gvar);
                }
                signal = boost::shared_ptr<view::Signal>(
                    new view::DsoSignal(probe->name,
                        _dso_data, probe->index, _signals.size(), vdiv, timebase, coupling, active));
                break;

            case SR_PROBE_ANALOG:
                signal = boost::shared_ptr<view::Signal>(
                    new view::AnalogSignal(probe->name,
                        _analog_data, probe->index, _signals.size()));
                break;
            }

            _signals.push_back(signal);
        }
        signals_changed();
        data_updated();
    }
    g_variant_unref(gvar);
}

void SigSession::update_signals(const sr_dev_inst *sdi)
{
    boost::shared_ptr<view::Signal> signal;
    QMap<int, bool> probes_en_table;
    QMap<int, bool> signals_en_table;
    int index = 0;
    GVariant *gvar;
    uint64_t vdiv;
    uint64_t timebase;
    bool coupling;
    bool active;
    int ret;

    std::vector< boost::shared_ptr<view::Signal> >::iterator i = _signals.begin();
    while (i != _signals.end()) {
        if (((*i)->get_type() == view::Signal::DS_LOGIC ||
             (*i)->get_type() == view::Signal::DS_DSO ||
             (*i)->get_type() == view::Signal::DS_ANALOG))
            signals_en_table.insert((*i)->get_index(), 1);
        i++;
    }

    index = 0;
    for (const GSList *l = sdi->probes; l; l = l->next) {
        const sr_probe *const probe =
            (const sr_probe *)l->data;
        assert(probe);
        probes_en_table.insert(index, probe->enabled);
        if (probe->enabled && !signals_en_table.contains(index)) {
            i = _signals.begin();
            while (i != _signals.end()) {
                (*i)->set_order((*i)->get_order() + 1);
                i++;
            }

            switch(probe->type) {
            case SR_PROBE_LOGIC:
                signal = boost::shared_ptr<view::Signal>(
                    new view::LogicSignal(probe->name,
                        _logic_data, probe->index, 0));
                break;

            case SR_PROBE_DSO:
                if (probe->index == 0) {
                    ret = sr_config_get(sdi->driver, SR_CONF_VDIV0,
                        &gvar, sdi);
                    if (ret != SR_OK) {
                        qDebug("Failed to get VDIV of channel 0\n");
                        return;
                    }
                    vdiv = g_variant_get_uint64(gvar);
                    ret = sr_config_get(sdi->driver, SR_CONF_TIMEBASE,
                        &gvar, sdi);
                    if (ret != SR_OK) {
                        qDebug("Failed to get TIMEBASE\n");
                        return;
                    }
                    timebase = g_variant_get_uint64(gvar);
                    ret = sr_config_get(sdi->driver, SR_CONF_COUPLING0,
                        &gvar, sdi);
                    if (ret != SR_OK) {
                        qDebug("Failed to get AC COUPLING of channel 0\n");
                        return;
                    }
                    coupling = g_variant_get_boolean(gvar);
                    ret = sr_config_get(sdi->driver, SR_CONF_EN_CH0,
                        &gvar, sdi);
                    if (ret != SR_OK) {
                        qDebug("Failed to get ENABLE of channel 0\n");
                        return;
                    }
                    active = g_variant_get_boolean(gvar);
                } else if (probe->index == 1) {
                    ret = sr_config_get(sdi->driver, SR_CONF_VDIV1,
                        &gvar, sdi);
                    if (ret != SR_OK) {
                        qDebug("Failed to get VDIV of channel 1\n");
                        return;
                    }
                    vdiv = g_variant_get_uint64(gvar);
                    ret = sr_config_get(sdi->driver, SR_CONF_TIMEBASE,
                        &gvar, sdi);
                    if (ret != SR_OK) {
                        qDebug("Failed to get TIMEBASE\n");
                        return;
                    }
                    timebase = g_variant_get_uint64(gvar);
                    ret = sr_config_get(sdi->driver, SR_CONF_COUPLING1,
                        &gvar, sdi);
                    if (ret != SR_OK) {
                        qDebug("Failed to get AC COUPLING of channel 1\n");
                        return;
                    }
                    coupling = g_variant_get_boolean(gvar);
                    ret = sr_config_get(sdi->driver, SR_CONF_EN_CH1,
                        &gvar, sdi);
                    if (ret != SR_OK) {
                        qDebug("Failed to get ENABLE of channel 1\n");
                        return;
                    }
                    active = g_variant_get_boolean(gvar);
                }
                signal = boost::shared_ptr<view::Signal>(
                    new view::DsoSignal(probe->name,
                        _dso_data, probe->index, _signals.size(), vdiv, timebase, coupling, active));
                break;

            case SR_PROBE_ANALOG:
                signal = boost::shared_ptr<view::Signal>(
                    new view::AnalogSignal(probe->name,
                        _analog_data, probe->index, 0));
                break;
            }
            _signals.push_back(signal);
        }
        index++;
    }

    i = _signals.begin();
    while (i != _signals.end()) {
        if (((*i)->get_type() == view::Signal::DS_LOGIC ||
             (*i)->get_type() == view::Signal::DS_DSO ||
             (*i)->get_type() == view::Signal::DS_ANALOG) &&
            probes_en_table.value((*i)->get_index()) == false) {
            std::vector< boost::shared_ptr<view::Signal> >::iterator j = _signals.begin();
            while(j != _signals.end()) {
                if ((*j)->get_order() > (*i)->get_order())
                    (*j)->set_order((*j)->get_order() - 1);
                j++;
            }

            (*i).reset();
            i = _signals.erase(i);
            continue;
        }
        i++;
    }

    signals_changed();
    data_updated();
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
    receive_trigger(trigger_pos.real_pos);
}

void SigSession::feed_in_logic(const sr_datafeed_logic &logic)
{
	boost::lock_guard<boost::mutex> lock(_data_mutex);

	if (!_logic_data)
	{
		qDebug() << "Unexpected logic packet";
		return;
	}

    if (logic.data_error == 1) {
        test_data_error();
    }

	if (!_cur_logic_snapshot)
	{
		// Create a new data snapshot
		_cur_logic_snapshot = boost::shared_ptr<data::LogicSnapshot>(
            new data::LogicSnapshot(logic, _total_sample_len, 1));
        if (_cur_logic_snapshot->buf_null())
            stop_capture();
        else
            _logic_data->push_snapshot(_cur_logic_snapshot);
	}
	else
	{
		// Append to the existing data snapshot
		_cur_logic_snapshot->append_payload(logic);
	}

    receive_data(logic.length/logic.unitsize);
    //data_updated();
}

void SigSession::feed_in_dso(const sr_datafeed_dso &dso)
{
    boost::lock_guard<boost::mutex> lock(_data_mutex);

    if(!_dso_data)
    {
        qDebug() << "Unexpected dso packet";
        return;	// This dso packet was not expected.
    }

    if (!_cur_dso_snapshot)
    {
        // Create a new data snapshot
        _cur_dso_snapshot = boost::shared_ptr<data::DsoSnapshot>(
            new data::DsoSnapshot(dso, _total_sample_len, _dso_data->get_num_probes()));
        if (_cur_dso_snapshot->buf_null())
            stop_capture();
        else
            _dso_data->push_snapshot(_cur_dso_snapshot);
    }
    else
    {
        // Append to the existing data snapshot
        _cur_dso_snapshot->append_payload(dso);
    }

    receive_data(dso.num_samples);
    data_updated();
}

void SigSession::feed_in_analog(const sr_datafeed_analog &analog)
{
	boost::lock_guard<boost::mutex> lock(_data_mutex);

	if(!_analog_data)
	{
		qDebug() << "Unexpected analog packet";
		return;	// This analog packet was not expected.
	}

	if (!_cur_analog_snapshot)
	{
		// Create a new data snapshot
		_cur_analog_snapshot = boost::shared_ptr<data::AnalogSnapshot>(
            new data::AnalogSnapshot(analog, _total_sample_len, _analog_data->get_num_probes()));
        if (_cur_analog_snapshot->buf_null())
            stop_capture();
        else
            _analog_data->push_snapshot(_cur_analog_snapshot);
	}
	else
	{
		// Append to the existing data snapshot
		_cur_analog_snapshot->append_payload(analog);
	}

    receive_data(analog.num_samples);
    data_updated();
}

void SigSession::data_feed_in(const struct sr_dev_inst *sdi,
    const struct sr_datafeed_packet *packet)
{
	assert(sdi);
	assert(packet);

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

	case SR_DF_END:
	{
		{
            boost::lock_guard<boost::mutex> lock(_data_mutex);
            BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _signals)
            {
                assert(s);
                if (s->get_type() == view::Signal::DS_GROUP) {
                    _cur_group_snapshot = boost::shared_ptr<data::GroupSnapshot>(
                                new data::GroupSnapshot(_logic_data->get_snapshots().front(), s->get_index_list()));
                    //_cur_group_snapshot->append_payload();
                    _group_data->push_snapshot(_cur_group_snapshot);
                    _cur_group_snapshot.reset();
                }
                if (s->get_type() == view::Signal::DS_PROTOCOL) {
                    s->get_decoder()->decode();
                }
            }
			_cur_logic_snapshot.reset();
            _cur_dso_snapshot.reset();
            _cur_analog_snapshot.reset();
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

QVector<std::pair<decoder::Decoder *, std::list<int> > > SigSession::get_decoders() const
{
    return _decoders;
}

void SigSession::add_protocol_analyzer(int decoder_index, std::list <int > _sel_probes,
                                       QMap <QString, QVariant>& _options, QMap <QString, int> _options_index)
{
    decoder::Decoder *decoder;

    // new different docoder according to protocol_list in decoder.h
    decoder = _decoderFactory->createDecoder(decoder_index, _logic_data, _sel_probes, _options, _options_index);

    // if current data is valid, do decode
    if (_logic_data)
        decoder->decode();
    _decoders.push_back(std::pair<decoder::Decoder *, std::list<int> >(decoder, _sel_probes));

//    // config signal's attribute for display
//    BOOST_FOREACH(const int _index, _sel_probes) {
//        _signals.at(_index)->set_decoder(decoder);
//    }

    // add protocol decoder signal
    add_protocol(_sel_probes, decoder);
}

void SigSession::rst_protocol_analyzer(int rst_index, std::list <int > _sel_probes,
                                       QMap <QString, QVariant>& _options, QMap <QString, int> _options_index)
{
    // if current data is valid, redo decode
    if (_logic_data)
        _decoders.at(rst_index).first->recode(_sel_probes, _options, _options_index);

    BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _signals)
    {
        assert(s);
        if (s->get_decoder() == _decoders.at(rst_index).first) {
            s->set_index_list(s->get_decoder()->get_probes());
            break;
        }
    }

    // update protocol signal
    signals_changed();
    data_updated();
}

void SigSession::del_protocol_analyzer(int protocol_index)
{
    assert(protocol_index < _decoders.size());
    //delete (_decoders.at(protocol_index)).first;

//    BOOST_FOREACH(const int _index, (_decoders.at(protocol_index)).second) {
//        _signals.at(_index)->del_decoder();
//    }
    del_protocol(protocol_index);

    _decoders.remove(protocol_index);
}

std::list<int> SigSession::get_decode_probes(int decode_index)
{
    assert(decode_index >= 0);
    assert(decode_index < _decoders.size());
    return _decoders.at(decode_index).first->get_probes();
}

QMap<QString, int> SigSession::get_decode_options_index(int decode_index)
{
    assert(decode_index >= 0);
    assert(decode_index < _decoders.size());
    return _decoders.at(decode_index).first->get_options_index();
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
        qDebug("DSLogic attaced!\n");
    }else if (LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT == event) {
        _session->_hot_detach = true;
        qDebug("DSLogic dettaced!\n");
    }else{
        qDebug("Unhandled event %d\n", event);
    }

    return 0;
}

void SigSession::hotplug_proc(boost::function<void (const QString)> error_handler)
{
    struct timeval tv;

    (void)error_handler;

    if (!_sdi)
        return;

    tv.tv_sec = tv.tv_usec = 0;
    try {
        while(_session) {
            libusb_handle_events_timeout(NULL, &tv);
            if (_hot_attach) {
                qDebug("DSLogic hardware attached!");
                device_attach();
                _hot_attach = false;
                break;
            }
            if (_hot_detach) {
                qDebug("DSLogic hardware detached!");
                device_detach();
                _logic_data.reset();
                _dso_data.reset();
                _analog_data.reset();
                _hot_detach = false;
                break;
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
                                           (libusb_hotplug_flag)LIBUSB_HOTPLUG_ENUMERATE, 0x2A0E, 0x0001,
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


/*
 * Tigger
 */
void SigSession::set_adv_trigger(bool adv_trigger)
{
    _adv_trigger = adv_trigger;
}


/*
 * oscilloscope control
 */
void SigSession::start_dso_ctrl_proc(boost::function<void (const QString)> error_handler)
{

    // Begin the dso control thread
    _dso_ctrl_thread.reset(new boost::thread(
        &SigSession::dso_ctrl_proc, this, error_handler));

}

void SigSession::stop_dso_ctrl_proc()
{

    if (_dso_ctrl_thread.get()) {
        _dso_ctrl_thread->interrupt();
        _dso_ctrl_thread->join();
    }
    _dso_ctrl_thread.reset();
}

int SigSession::set_dso_ctrl(int key)
{
    int ret;

    if (key== SR_CONF_TIMEBASE) {
        uint64_t timebase = _signals.at(0)->get_hDialValue();
        ret = sr_config_set(_sdi, key, g_variant_new_uint64(timebase));
    } else if (key == SR_CONF_VDIV0) {
        uint64_t vdiv = _signals.at(0)->get_vDialValue();
        ret = sr_config_set(_sdi, key, g_variant_new_uint64(vdiv));
    } else if (key == SR_CONF_VDIV1) {
        uint64_t vdiv = _signals.at(1)->get_vDialValue();
        ret = sr_config_set(_sdi, key, g_variant_new_uint64(vdiv));
    } else if (key == SR_CONF_COUPLING0) {
        bool acdc = _signals.at(0)->get_acCoupling();
        ret = sr_config_set(_sdi, key, g_variant_new_boolean(acdc));
    } else if (key == SR_CONF_COUPLING1) {
        bool acdc = _signals.at(1)->get_acCoupling();
        ret = sr_config_set(_sdi, key, g_variant_new_boolean(acdc));
    } else if (key == SR_CONF_EN_CH0) {
        bool enable = _signals.at(0)->get_active();
        ret = sr_config_set(_sdi, key, g_variant_new_boolean(enable));
        dso_ch_changed(get_dso_ch_num());
    } else if (key == SR_CONF_EN_CH1) {
        bool enable = _signals.at(1)->get_active();
        ret = sr_config_set(_sdi, key, g_variant_new_boolean(enable));
        dso_ch_changed(get_dso_ch_num());
    }

    return ret;
}

uint16_t SigSession::get_dso_ch_num()
{
    uint16_t num_channels = 0;
    BOOST_FOREACH(const shared_ptr<view::Signal> s, _signals)
    {
        assert(s);
        if (s->get_active()) {
            num_channels++;
        }
    }
    return num_channels;
}

void SigSession::dso_ctrl_proc(boost::function<void (const QString)> error_handler)
{
    (void)error_handler;
    try {
        while(_session) {
            if (!_sdi) {
                // do nothing
            } else if (strcmp(_sdi->driver->name, "Demo") == 0) {

            } else if (strcmp(_sdi->driver->name, "DSLogic") == 0) {

            }
            boost::this_thread::sleep(boost::posix_time::millisec(100));
        }
    } catch(...) {
        qDebug("Interrupt exception for oscilloscope control thread was thrown.");
    }
    qDebug("Oscilloscope control thread exit!");
}

} // namespace pv
