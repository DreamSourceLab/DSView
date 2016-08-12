/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2013 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2014 DreamSourceLab <support@dreamsourcelab.com>
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

#include <libsigrok4DSL/libsigrok.h>
#include <libsigrokdecode/libsigrokdecode.h>

#include "decoder.h"

#include <pv/view/logicsignal.h>

using boost::shared_ptr;
using std::set;
using std::map;
using std::string;

namespace pv {
namespace data {
namespace decode {

Decoder::Decoder(const srd_decoder *const dec) :
	_decoder(dec),
    _shown(true),
    _setted(true)
{
}

Decoder::~Decoder()
{
	for (map<string, GVariant*>::const_iterator i = _options.begin();
		i != _options.end(); i++)
		g_variant_unref((*i).second);

    for (map<string, GVariant*>::const_iterator i = _options_back.begin();
        i != _options_back.end(); i++)
        g_variant_unref((*i).second);
}

const srd_decoder* Decoder::decoder() const
{
	return _decoder;
}

bool Decoder::shown() const
{
	return _shown;
}

void Decoder::show(bool show)
{
    _shown = show;
}

const map<const srd_channel*, shared_ptr<view::LogicSignal> >&
Decoder::channels() const
{
	return _probes;
}

void Decoder::set_probes(std::map<const srd_channel*,
	boost::shared_ptr<view::LogicSignal> > probes)
{
    _probes_back = probes;
    _setted = true;
}

const std::map<std::string, GVariant*>& Decoder::options() const
{
	return _options;
}

void Decoder::set_option(const char *id, GVariant *value)
{
	assert(value);
	g_variant_ref(value);
    _options_back[id] = value;
    _setted = true;
}

void Decoder::set_decode_region(uint64_t start, uint64_t end)
{
    _decode_start_back = start;
    _decode_end_back = end;
    if (_decode_start != start ||
        _decode_end != end)
        _setted = true;
}

uint64_t Decoder::decode_start() const
{
    return _decode_start;
}

uint64_t Decoder::decode_end() const
{
    return _decode_end;
}

bool Decoder::commit()
{
    if (_setted) {
        _probes = _probes_back;
        _options = _options_back;
        _decode_start = _decode_start_back;
        _decode_end = _decode_end_back;
        _setted = false;
        return true;
    } else {
        return false;
    }
}

bool Decoder::have_required_probes() const
{
	for (GSList *l = _decoder->channels; l; l = l->next) {
		const srd_channel *const pdch = (const srd_channel*)l->data;
		assert(pdch);
		if (_probes.find(pdch) == _probes.end())
			return false;
	}

	return true;
}

set< shared_ptr<pv::data::Logic> > Decoder::get_data()
{
	set< shared_ptr<pv::data::Logic> > data;
	for(map<const srd_channel*, shared_ptr<view::LogicSignal> >::
		const_iterator i = _probes.begin();
		i != _probes.end(); i++)
	{
		shared_ptr<view::LogicSignal> signal((*i).second);
		assert(signal);
		data.insert(signal->logic_data());
	}

	return data;
}

srd_decoder_inst* Decoder::create_decoder_inst(srd_session *session, int unit_size) const
{
    (void)unit_size;
	GHashTable *const opt_hash = g_hash_table_new_full(g_str_hash,
		g_str_equal, g_free, (GDestroyNotify)g_variant_unref);

	for (map<string, GVariant*>::const_iterator i = _options.begin();
		i != _options.end(); i++)
	{
		GVariant *const value = (*i).second;
		g_variant_ref(value);
		g_hash_table_replace(opt_hash, (void*)g_strdup(
			(*i).first.c_str()), value);
	}

	srd_decoder_inst *const decoder_inst = srd_inst_new(
		session, _decoder->id, opt_hash);
	g_hash_table_destroy(opt_hash);

	if(!decoder_inst)
		return NULL;

	// Setup the probes
	GHashTable *const probes = g_hash_table_new_full(g_str_hash,
		g_str_equal, g_free, (GDestroyNotify)g_variant_unref);

	for(map<const srd_channel*, shared_ptr<view::LogicSignal> >::
		const_iterator i = _probes.begin();
		i != _probes.end(); i++)
	{
		shared_ptr<view::LogicSignal> signal((*i).second);
		GVariant *const gvar = g_variant_new_int32(
			signal->probe()->index);
		g_variant_ref_sink(gvar);
		g_hash_table_insert(probes, (*i).first->id, gvar);
	}

    srd_inst_channel_set_all(decoder_inst, probes);

	return decoder_inst;
}

} // decode
} // data
} // pv
