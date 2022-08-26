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
}

void DeviceAgent::update()
{
    _dev_handle = NULL;
    _dev_name = "";

    ds_device_info info;

    if (ds_get_actived_device_info(&info) == SR_OK)
    {
        _dev_handle = info.handle;
        _dev_type = info.dev_type;
        _dev_name = QString::fromLocal8Bit(info.name);
    }
}

GVariant* DeviceAgent::get_config(const sr_channel *ch, const sr_channel_group *group, int key)
{
    assert(_dev_handle);

    GVariant *data = NULL;
    if (ds_set_actived_device_config(ch, group, key, data) != SR_OK)
    {
        dsv_err("%s", "Get device config error!");
    }
    return data;
}

bool DeviceAgent::set_config(sr_channel *ch, sr_channel_group *group, int key, GVariant *data)
{
    assert(_dev_handle);

    if (ds_set_actived_device_config(ch, group, key, data) != SR_OK)
    {   
        dsv_err("%s", "Set device config error!");
        return false;
    }

    config_changed();
    return true;
}

GVariant* DeviceAgent::list_config(const sr_channel_group *group, int key)
{
    assert(_dev_handle);

    GVariant *data = NULL;

    if (ds_get_actived_device_config_list(group, key, &data) != SR_OK){
        dsv_err("%s", "Get device config list error!");    
    }

    return data;
}

void DeviceAgent::enable_probe(const sr_channel *probe, bool enable = true)
{
    assert(_dev_handle);

    if (ds_enable_device_channel(probe, enable) == SR_OK){
        config_changed();
    }
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
 
//---------------device config-----------/

  int DeviceAgent::get_work_mode()
  {
      return ds_get_actived_device_mode();
  }

  bool DeviceAgent::get_device_info(struct ds_device_info &info)
  {
     if (ds_get_actived_device_info(&info) == SR_OK){
        return info.handle != NULL;
     }
     return false;
  }

  bool DeviceAgent::get_device_config(const struct sr_channel *ch,
                         const struct sr_channel_group *cg,
                         int key, GVariant **data)
  {
    
    if (ds_get_actived_device_config(ch, cg, key, data) == SR_OK){
        return true;
    }

    return false;
  }

  bool DeviceAgent::set_device_config(const struct sr_channel *ch,
                         const struct sr_channel_group *cg,
                         int key, GVariant *data)
   {

     if (ds_set_actived_device_config(ch, cg, key, data) == SR_OK){
            return true;
     }

     return false;
   }

    bool DeviceAgent::get_device_config_list(const struct sr_channel_group *cg,
                          int key, GVariant **data)
    {

        if (ds_get_actived_device_config_list(cg, key, data) == SR_OK){
            return true;
        }
        return false;
    }

    const struct sr_config_info* DeviceAgent::get_device_config_info(int key)
    {
        return ds_get_actived_device_config_info(key);
    }

    const struct sr_config_info* DeviceAgent::get_device_config_info_by_name(const char *optname)
    {
        return ds_get_actived_device_config_info_by_name(optname);
    }

    bool DeviceAgent::get_device_status(struct sr_status &status, gboolean prg)
    {
        if (ds_get_actived_device_status(&status, prg) == SR_OK){
            return true;
        }
        return false;
    }

    struct sr_config* DeviceAgent::new_config(int key, GVariant *data)
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

    GSList* DeviceAgent::get_channels()
    {
        assert(_dev_handle);
        return ds_get_actived_device_channels();
    }

//---------------device config end -----------/
