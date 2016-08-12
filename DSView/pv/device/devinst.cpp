/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include <cassert>

#include <QDebug>

#include <libsigrok4DSL/libsigrok.h>

#include "devinst.h"

#include <pv/sigsession.h>

namespace pv {
namespace device {

DevInst::DevInst() :
    _owner(NULL)
{
    _id = malloc(1);
}

DevInst::~DevInst()
{
    assert(_id);
    free(_id);
}

void* DevInst::get_id() const
{
    assert(_id);

    return _id;
}

void DevInst::use(SigSession *owner) throw(QString)
{
	assert(owner);
	assert(!_owner);
	_owner = owner;
}

void DevInst::release()
{
	if (_owner) {
		_owner->release_device(this);
		_owner = NULL;
	}
}

SigSession* DevInst::owner() const
{
	return _owner;
}

GVariant* DevInst::get_config(const sr_channel *ch, const sr_channel_group *group, int key)
{
	GVariant *data = NULL;
	assert(_owner);
	sr_dev_inst *const sdi = dev_inst();
	assert(sdi);
    if (sr_config_get(sdi->driver, sdi, ch, group, key, &data) != SR_OK)
		return NULL;
	return data;
}

bool DevInst::set_config(const sr_channel *ch, const sr_channel_group *group, int key, GVariant *data)
{
	assert(_owner);
	sr_dev_inst *const sdi = dev_inst();
	assert(sdi);
    if(sr_config_set(sdi, ch, group, key, data) == SR_OK) {
		config_changed();
		return true;
	}
	return false;
}

GVariant* DevInst::list_config(const sr_channel_group *group, int key)
{
	GVariant *data = NULL;
	assert(_owner);
	sr_dev_inst *const sdi = dev_inst();
	assert(sdi);
	if (sr_config_list(sdi->driver, sdi, group, key, &data) != SR_OK)
		return NULL;
	return data;
}

void DevInst::enable_probe(const sr_channel *probe, bool enable)
{
	assert(_owner);
	sr_dev_inst *const sdi = dev_inst();
	assert(sdi);
	for (const GSList *p = sdi->channels; p; p = p->next)
		if (probe == p->data) {
			const_cast<sr_channel*>(probe)->enabled = enable;
			config_changed();
			return;
		}

	// Probe was not found in the device
	assert(0);
}

uint64_t DevInst::get_sample_limit()
{
	uint64_t sample_limit;
    GVariant* gvar = get_config(NULL, NULL, SR_CONF_LIMIT_SAMPLES);
	if (gvar != NULL) {
		sample_limit = g_variant_get_uint64(gvar);
		g_variant_unref(gvar);
	} else {
		sample_limit = 0U;
	}
	return sample_limit;
}

uint64_t DevInst::get_sample_rate()
{
    uint64_t sample_rate;
    GVariant* gvar = get_config(NULL, NULL, SR_CONF_SAMPLERATE);
    if (gvar != NULL) {
        sample_rate = g_variant_get_uint64(gvar);
        g_variant_unref(gvar);
    } else {
        sample_rate = 0U;
    }
    return sample_rate;
}

uint64_t DevInst::get_time_base()
{
    uint64_t time_base;
    GVariant* gvar = get_config(NULL, NULL, SR_CONF_TIMEBASE);
    if (gvar != NULL) {
        time_base = g_variant_get_uint64(gvar);
        g_variant_unref(gvar);
    } else {
        time_base = 0U;
    }
    return time_base;
}


double DevInst::get_sample_time()
{
    uint64_t sample_rate = get_sample_rate();
    uint64_t sample_limit = get_sample_limit();
    double sample_time;

    if (sample_rate == 0)
        sample_time = 0;
    else
        sample_time = sample_limit * 1.0 / sample_rate;

    return sample_time;
}

GSList* DevInst::get_dev_mode_list()
{
    assert(_owner);
    sr_dev_inst *const sdi = dev_inst();
    assert(sdi);
    return sr_dev_mode_list(sdi);
}

QString DevInst::name()
{
    sr_dev_inst *const sdi = dev_inst();
    assert(sdi);
    return QString::fromLocal8Bit(sdi->driver->name);
}

bool DevInst::is_trigger_enabled() const
{
	return false;
}

void DevInst::start()
{
	if (sr_session_start() != SR_OK)
		throw tr("Failed to start session.");
}

void DevInst::run()
{
	sr_session_run();
}

} // device
} // pv
