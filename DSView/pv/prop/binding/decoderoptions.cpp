/*
 * This file is part of the PulseView project.
 * DSView is based on PulseView.
 * 
 * Copyright (C) 2013 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include "decoderoptions.h"
#include <boost/bind.hpp> 
#include <boost/none_t.hpp>

#include "../../data/decoderstack.h"
#include "../../data/decode/decoder.h"
#include "../double.h"
#include "../enum.h"
#include "../int.h"
#include "../string.h"
#include "../../ui/langresource.h"
#include "../../config/appconfig.h"

using namespace boost;
using namespace std;
 
namespace pv {
namespace prop {
namespace binding {

DecoderOptions::DecoderOptions(pv::data::DecoderStack* decoder_stack, data::decode::Decoder *decoder) :
	Binding(),
	_decoder(decoder)
{
	assert(_decoder);
	(void)decoder_stack;

	const srd_decoder *const dec = _decoder->decoder();
	assert(dec);

	bool bLang = AppConfig::Instance().appOptions.transDecoderDlg;

	if (LangResource::Instance()->is_lang_en()){
        bLang = false;
    }

	for (GSList *l = dec->options; l; l = l->next)
	{ 
		const srd_decoder_option *const opt =
			(srd_decoder_option*)l->data;

		const char *desc_str = NULL;
		const char *lang_str = NULL;

        if (opt->idn != NULL && LangResource::Instance()->is_lang_en() == false){
            lang_str = LangResource::Instance()->get_lang_text(STR_PAGE_DECODER, opt->idn, opt->desc);
        }

		if (lang_str != NULL && bLang){
            desc_str = lang_str;
        }
        else{
            desc_str = opt->desc;
        }

		const QString name = QString::fromUtf8(desc_str);

		const Property::Getter getter = bind(
			&DecoderOptions::getter, this, opt->id);
		const Property::Setter setter = bind(
			&DecoderOptions::setter, this, opt->id, _1);

		Property *prop = NULL;

		if (opt->values)
            prop = bind_enum(name, opt, getter, setter);
		else if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("d")))
            prop = new Double(name, name, 2, "",none, none, getter, setter);
		else if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("x")))
			prop = new Int(name, name, "", none, getter, setter);
		else if (g_variant_is_of_type(opt->def, G_VARIANT_TYPE("s")))
			prop = new String(name, name, getter, setter);
		else
			continue;

		_properties.push_back(prop);
	}
}

Property* DecoderOptions::bind_enum(
	const QString &name, const srd_decoder_option *option,
	Property::Getter getter, Property::Setter setter)
{
    std::vector<std::pair<GVariant*, QString> > values;
	for (GSList *l = option->values; l; l = l->next) {
		GVariant *const var = (GVariant*)l->data;
		assert(var);
		values.push_back(make_pair(var, print_gvariant(var)));
	}

    return new Enum(name, name, values, getter, setter);
}

GVariant* DecoderOptions::getter(const char *id)
{
	GVariant *val = NULL;

	assert(_decoder);

	// Get the value from the hash table if it is already present
	const map<string, GVariant*>& options = _decoder->options();
	auto iter = options.find(id);

	if (iter != options.end())
		val = (*iter).second;
	else
	{
		assert(_decoder->decoder());

		// Get the default value if not
		for (GSList *l = _decoder->decoder()->options; l; l = l->next)
		{
			const srd_decoder_option *const opt =
				(srd_decoder_option*)l->data;
			if (strcmp(opt->id, id) == 0) {
				val = opt->def;
				break;
			}
		}
	}

	if (val)
		g_variant_ref(val);

	return val;
}

void DecoderOptions::setter(const char *id, GVariant *value)
{
	assert(_decoder);
	_decoder->set_option(id, value);
}

} // binding
} // prop
} // pv
