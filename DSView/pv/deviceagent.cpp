/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 * 
 * Copyright (C) 2022 DreamSourceLab <support@dreamsourcelab.com>
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

#include "deviceagent.h"
#include <assert.h>
#include "log.h"


DeviceAgent::DeviceAgent()
{
    _dev_handle = NULL;
    _di = NULL;
    _dev_type = 0;
    _callback = NULL;
    _is_new_device = false;
}

void DeviceAgent::update()
{
    _dev_handle = NULL;
    _dev_name = "";
    _di = NULL;
    _dev_type = 0;

    ds_device_info info;

    if (ds_get_actived_device_info(&info) == SR_OK)
    {
        _dev_handle = info.handle;
        _dev_type = info.dev_type;
        _di = info.di;
        _dev_name = QString::fromLocal8Bit(info.name);
        _driver_name = QString::fromLocal8Bit(info.driver_name);

        if (is_in_history(_dev_handle) == false){
            _is_new_device = true;
        }
        else{
            _is_new_device = false; 
        }
    }
}

 sr_dev_inst* DeviceAgent::inst()
 {
    assert(_dev_handle);
    return _di;
 }

GVariant* DeviceAgent::get_config(const sr_channel *ch, const sr_channel_group *group, int key)
{
    assert(_dev_handle); 
    GVariant *data = NULL;
    if (ds_get_actived_device_config(ch, group, key, &data) != SR_OK)
    {
        if (is_hardware())
            dsv_warn("%s%d", "WARNING: Failed to get value of config id:", key);
        else
            dsv_detail("%s%d", "WARNING: Failed to get value of config id:", key);
    }
    return data;
}

bool DeviceAgent::set_config(sr_channel *ch, sr_channel_group *group, int key, GVariant *data)
{
    assert(_dev_handle);

    if (ds_set_actived_device_config(ch, group, key, data) != SR_OK)
    {   
        if (is_hardware())
            dsv_warn("%s%d", "WARNING: Failed to set value of config id:", key);
        else
            dsv_detail("%s%d", "WARNING: Failed to set value of config id:", key);
        return false;
    }

    config_changed();
    return true;
}

GVariant* DeviceAgent::get_config_list(const sr_channel_group *group, int key)
{
    assert(_dev_handle);

    GVariant *data = NULL;

    if (ds_get_actived_device_config_list(group, key, &data) != SR_OK){
        dsv_warn("%s%d", "WARNING: Failed to get config list, key:", key); 
        if (data != NULL){
            dsv_warn("%s%d", "WARNING: Failed to get config list, but data is not null. key:", key); 
        }
        data = NULL;
    }

    return data;
}

bool DeviceAgent::enable_probe(const sr_channel *probe, bool enable)
{
    assert(_dev_handle);

    if (ds_enable_device_channel(probe, enable) == SR_OK){
        config_changed();
        return true;
    }
    return false;
}

bool DeviceAgent::enable_probe(int probe_index, bool enable)
{
     assert(_dev_handle);

     if (ds_enable_device_channel_index(probe_index, enable) == SR_OK){
        config_changed();
        return true;
    }
    return false;
}

bool DeviceAgent::set_channel_name(int ch_index, const char *name)
{
    assert(_dev_handle);
    
    if (ds_set_device_channel_name(ch_index, name) == SR_OK){
        config_changed();
        return true;
    }
    return false;
}

uint64_t DeviceAgent::get_sample_limit()
{
    assert(_dev_handle);

    uint64_t v;
    GVariant* gvar = NULL;

    ds_get_actived_device_config(NULL, NULL, SR_CONF_LIMIT_SAMPLES, &gvar);

	if (gvar != NULL) {
        v = g_variant_get_uint64(gvar);
		g_variant_unref(gvar);
	}
    else {
		v = 0U;
	}

	return v;
}

uint64_t DeviceAgent::get_sample_rate()
{
    assert(_dev_handle);

    uint64_t v;
    GVariant* gvar = NULL;

    ds_get_actived_device_config(NULL, NULL, SR_CONF_SAMPLERATE, &gvar);

	if (gvar != NULL) {
        v = g_variant_get_uint64(gvar);
		g_variant_unref(gvar);
	}
    else {
		v = 0U;
	}

	return v;
}

uint64_t DeviceAgent::get_time_base()
{
    assert(_dev_handle);

    uint64_t v;
    GVariant* gvar = NULL;

    ds_get_actived_device_config(NULL, NULL, SR_CONF_TIMEBASE, &gvar);

	if (gvar != NULL) {
        v = g_variant_get_uint64(gvar);
		g_variant_unref(gvar);
	}
    else {
		v = 0U;
	}

	return v;
}

double DeviceAgent::get_sample_time()
{
    assert(_dev_handle);

    uint64_t sample_rate = get_sample_rate();
    uint64_t sample_limit = get_sample_limit();
    double sample_time;

    if (sample_rate == 0)
        sample_time = 0;
    else
        sample_time = sample_limit * 1.0 / sample_rate;

    return sample_time;
}

const GSList* DeviceAgent::get_device_mode_list()
{
    assert(_dev_handle);
    return ds_get_actived_device_mode_list();
}

bool DeviceAgent::is_trigger_enabled()
{
    assert(_dev_handle);
    if (ds_trigger_is_enabled() > 0){
        return true;
    }
    return false;
}

bool DeviceAgent::start()
{
    assert(_dev_handle);

    if (ds_start_collect() == SR_OK){
        return true;
    }
    return false;
}

bool DeviceAgent::stop()
{
    assert(_dev_handle);

    if (ds_stop_collect() == SR_OK){
        return true;
    }
    return false;
}

void DeviceAgent::release()
{
    ds_release_actived_device();
}

bool DeviceAgent::have_enabled_channel()
{
    assert(_dev_handle);
    return ds_channel_is_enabled() > 0;
}

bool DeviceAgent::get_status(struct sr_status &status, gboolean prg)
{
    assert(_dev_handle);

    if (ds_get_actived_device_status(&status, prg) == SR_OK){
        return true;
    }
    return false;
}

void DeviceAgent::config_changed()
{
    if (_callback != NULL){
        _callback->DeviceConfigChanged();
    }
}
 
//---------------device config-----------/

int DeviceAgent::get_work_mode()
{
    return ds_get_actived_device_mode();
}

const struct sr_config_info *DeviceAgent::get_config_info(int key)
{
    return ds_get_actived_device_config_info(key);
}

const struct sr_config_info *DeviceAgent::get_config_info_by_name(const char *optname)
{
    return ds_get_actived_device_config_info_by_name(optname);
}

bool DeviceAgent::get_device_status(struct sr_status &status, gboolean prg)
{
    if (ds_get_actived_device_status(&status, prg) == SR_OK)
    {
        return true;
    }
    return false;
}

struct sr_config *DeviceAgent::new_config(int key, GVariant *data)
{
    return ds_new_config(key, data);
}

void DeviceAgent::free_config(struct sr_config *src)
{
    ds_free_config(src);
}

bool DeviceAgent::is_collecting()
{
    return ds_is_collecting() > 0;
}

GSList *DeviceAgent::get_channels()
{
    assert(_dev_handle);
    return ds_get_actived_device_channels();
}

 bool DeviceAgent::is_in_history(ds_device_handle dev_handle)
 {
    for(ds_device_handle h : _history_handles){
        if (h == dev_handle){
            return true;
        }        
    }
    _history_handles.push_back(dev_handle);
    return false;
 }

//---------------device config end -----------/

