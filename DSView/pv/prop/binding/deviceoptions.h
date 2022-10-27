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


#ifndef DSVIEW_PV_PROP_BINDING_DEVICEOPTIONS_H
#define DSVIEW_PV_PROP_BINDING_DEVICEOPTIONS_H

#include <boost/function.hpp>
#include <boost/optional.hpp>

#include <QString>
#include <libsigrok.h> 
#include "binding.h" 

class DeviceAgent;

namespace pv {

namespace prop {
namespace binding {

class DeviceOptions : public Binding
{
public:
	DeviceOptions();

private:

	static GVariant* config_getter(int key);

	static void config_setter(int key, GVariant* value);

    void bind_bool(const QString &name, const QString label, int key);

    void bind_enum(const QString &name, const QString label, int key, GVariant *const gvar_list,
		boost::function<QString (GVariant*)> printer = print_gvariant);

	void bind_list(const QString &name, const QString label, int key, GVariant *const gvar_list);

    void bind_int(const QString &name, const QString label, int key, QString suffix,
		boost::optional< std::pair<int64_t, int64_t> > range);

    void bind_double(const QString &name, const QString label, int key, QString suffix,
        boost::optional<std::pair<double, double> > range,
        int decimals, boost::optional<double> step);

	static QString print_gvariant(GVariant *const gvar);

    void bind_samplerate(const QString &name, const QString label,GVariant *const gvar_list);

	static QString print_samplerate(GVariant *const gvar);

	static GVariant* samplerate_double_getter();

	static void samplerate_double_setter(GVariant *value);

	static QString print_timebase(GVariant *const gvar);

	static QString print_vdiv(GVariant *const gvar);

    void bind_bandwidths(const QString &name, const QString label, int key,GVariant *const gvar_list,
        boost::function<QString (GVariant*)> printer = print_gvariant);

private:
	DeviceAgent *_device_agent;
 
};

} // binding
} // prop
} // pv

#endif // DSVIEW_PV_PROP_BINDING_DEVICEOPTIONS_H
