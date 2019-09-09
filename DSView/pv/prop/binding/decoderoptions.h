/*
 * This file is part of the PulseView project.
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

#ifndef DSVIEW_PV_PROP_BINDING_DECODEROPTIONS_H
#define DSVIEW_PV_PROP_BINDING_DECODEROPTIONS_H

#include "binding.h"

#include <pv/prop/property.h>

struct srd_decoder_option;

namespace pv {

namespace data {
class DecoderStack;
namespace decode {
class Decoder;
}
}

namespace prop {
namespace binding {

class DecoderOptions : public Binding
{
public:
	DecoderOptions(boost::shared_ptr<pv::data::DecoderStack> decoder_stack,
		boost::shared_ptr<pv::data::decode::Decoder> decoder);

    GVariant* getter(const char *id);

    void setter(const char *id, GVariant *value);

private:
	static boost::shared_ptr<Property> bind_enum(const QString &name,
		const srd_decoder_option *option,
		Property::Getter getter, Property::Setter setter);



private:
	boost::shared_ptr<pv::data::DecoderStack> _decoder_stack;
	boost::shared_ptr<pv::data::decode::Decoder> _decoder;
};

} // binding
} // prop
} // pv

#endif // DSVIEW_PV_PROP_BINDING_DECODEROPTIONS_H
