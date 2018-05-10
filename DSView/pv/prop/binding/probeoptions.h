/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2018 DreamSourceLab <support@dreamsourcelab.com>
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


#ifndef DSVIEW_PV_PROP_BINDING_PROBEOPTIONS_H
#define DSVIEW_PV_PROP_BINDING_PROBEOPTIONS_H

#include <boost/function.hpp>
#include <boost/optional.hpp>

#include <QString>

#include <libsigrok4DSL/libsigrok.h>
#include "binding.h"

namespace pv {
namespace prop {
namespace binding {

class ProbeOptions : public Binding
{
public:
	ProbeOptions(struct sr_dev_inst *sdi, 
		     struct sr_channel *probe);

private:

	static GVariant* config_getter(
		const struct sr_dev_inst *sdi,
		const struct sr_channel *probe, int key);
	static void config_setter(
		struct sr_dev_inst *sdi,
        	struct sr_channel *probe, int key, GVariant* value);

	void bind_bool(const QString &name, int key);
	void bind_enum(const QString &name, int key,
		GVariant *const gvar_list,
		boost::function<QString (GVariant*)> printer = print_gvariant);
	void bind_int(const QString &name, int key, QString suffix,
		boost::optional< std::pair<int64_t, int64_t> > range);

    void bind_double(const QString &name, int key, QString suffix,
        boost::optional<std::pair<double, double> > range,
        int decimals, boost::optional<double> step);

	static QString print_gvariant(GVariant *const gvar);

    void bind_vdiv(const QString &name,
		GVariant *const gvar_list);
    void bind_coupling(const QString &name,
        GVariant *const gvar_list);

	static QString print_vdiv(GVariant *const gvar);
    static QString print_coupling(GVariant *const gvar);

protected:
	struct sr_dev_inst *const _sdi;
	struct sr_channel *const _probe;
};

} // binding
} // prop
} // pv

#endif // DSVIEW_PV_PROP_BINDING_DEVICEOPTIONS_H
