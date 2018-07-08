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

#include <boost/bind.hpp>

#include <QDebug>
#include <QObject>

#include <stdint.h>

#include "probeoptions.h"

#include <pv/prop/bool.h>
#include <pv/prop/double.h>
#include <pv/prop/enum.h>
#include <pv/prop/int.h>

using namespace boost;
using namespace std;

namespace pv {
namespace prop {
namespace binding {

ProbeOptions::ProbeOptions(struct sr_dev_inst *sdi, 
			   struct sr_channel *probe) :
	_sdi(sdi),
	_probe(probe)
{
	GVariant *gvar_opts, *gvar_list;
	gsize num_opts;

    if ((sr_config_list(sdi->driver, sdi, NULL, SR_CONF_PROBE_CONFIGS,
        &gvar_opts) != SR_OK))
		/* Driver supports no device instance options. */
		return;

	const int *const options = (const int32_t *)g_variant_get_fixed_array(
		gvar_opts, &num_opts, sizeof(int32_t));
	for (unsigned int i = 0; i < num_opts; i++) {
		const struct sr_config_info *const info =
			sr_config_info_get(options[i]);

		if (!info)
			continue;

		const int key = info->key;

        if(sr_config_list(_sdi->driver, _sdi, NULL, key, &gvar_list) != SR_OK)
			gvar_list = NULL;

        const QString name(info->label);

		switch(key)
		{
        case SR_CONF_PROBE_VDIV:
            bind_vdiv(name, gvar_list);
			break;

        case SR_CONF_PROBE_MAP_MIN:
        case SR_CONF_PROBE_MAP_MAX:
            bind_double(name, key, "",
                        pair<double, double>(-999999.99, 999999.99), 2, 0.01);
            break;

        case SR_CONF_PROBE_COUPLING:
            bind_coupling(name, gvar_list);
            break;

        case SR_CONF_PROBE_MAP_UNIT:
            bind_enum(name, key, gvar_list);
			break;

        default:
            gvar_list = NULL;
		}

		if (gvar_list)
			g_variant_unref(gvar_list);
	}
	g_variant_unref(gvar_opts);
}

GVariant* ProbeOptions::config_getter(
    const struct sr_dev_inst *sdi,
    const struct sr_channel *probe, int key)
{
	GVariant *data = NULL;
    if (sr_config_get(sdi->driver, sdi, probe, NULL, key, &data) != SR_OK) {
		qDebug() <<
			"WARNING: Failed to get value of config id" << key;
		return NULL;
	}
	return data;
}

void ProbeOptions::config_setter(
    struct sr_dev_inst *sdi,
    struct sr_channel *probe, int key, GVariant* value)
{
    if (sr_config_set(sdi, probe, NULL, key, value) != SR_OK)
		qDebug() << "WARNING: Failed to set value of sample rate";
}

void ProbeOptions::bind_bool(const QString &name, int key)
{
	_properties.push_back(boost::shared_ptr<Property>(
        new Bool(name, bind(config_getter, _sdi, _probe, key),
            bind(config_setter, _sdi, _probe, key, _1))));
}

void ProbeOptions::bind_enum(const QString &name, int key,
    GVariant *const gvar_list, boost::function<QString (GVariant*)> printer)
{
	GVariant *gvar;
	GVariantIter iter;
	vector< pair<GVariant*, QString> > values;

	assert(gvar_list);

	g_variant_iter_init (&iter, gvar_list);
	while ((gvar = g_variant_iter_next_value (&iter)))
		values.push_back(make_pair(gvar, printer(gvar)));

	_properties.push_back(boost::shared_ptr<Property>(
        new Enum(name, values,
            bind(config_getter, _sdi, _probe, key),
            bind(config_setter, _sdi, _probe, key, _1))));
}

void ProbeOptions::bind_int(const QString &name, int key, QString suffix,
	optional< std::pair<int64_t, int64_t> > range)
{
	_properties.push_back(boost::shared_ptr<Property>(
		new Int(name, suffix, range,
            bind(config_getter, _sdi, _probe, key),
            bind(config_setter, _sdi, _probe, key, _1))));
}

void ProbeOptions::bind_double(const QString &name, int key, QString suffix,
    optional< std::pair<double, double> > range,
    int decimals, boost::optional<double> step)
{
    _properties.push_back(boost::shared_ptr<Property>(
        new Double(name, decimals, suffix, range, step,
            bind(config_getter, _sdi, _probe, key),
            bind(config_setter, _sdi, _probe, key, _1))));
}

void ProbeOptions::bind_vdiv(const QString &name,
	GVariant *const gvar_list)
{
    GVariant *gvar_list_vdivs;

	assert(gvar_list);

    if ((gvar_list_vdivs = g_variant_lookup_value(gvar_list,
            "vdivs", G_VARIANT_TYPE("at"))))
	{
        bind_enum(name, SR_CONF_PROBE_VDIV,
            gvar_list_vdivs, print_vdiv);
        g_variant_unref(gvar_list_vdivs);
	}
}

void ProbeOptions::bind_coupling(const QString &name,
    GVariant *const gvar_list)
{
    GVariant *gvar_list_coupling;

    assert(gvar_list);

    if ((gvar_list_coupling = g_variant_lookup_value(gvar_list,
            "coupling", G_VARIANT_TYPE("ay"))))
    {
        bind_enum(name, SR_CONF_PROBE_COUPLING,
            gvar_list_coupling, print_coupling);
        g_variant_unref(gvar_list_coupling);
    }
}

QString ProbeOptions::print_gvariant(GVariant *const gvar)
{
    QString s;

    if (g_variant_is_of_type(gvar, G_VARIANT_TYPE("s")))
        s = QString::fromUtf8(g_variant_get_string(gvar, NULL));
    else
    {
        gchar *const text = g_variant_print(gvar, FALSE);
        s = QString::fromUtf8(text);
        g_free(text);
    }

    return s;
}

QString ProbeOptions::print_vdiv(GVariant *const gvar)
{
    uint64_t p, q;
    g_variant_get(gvar, "t", &p);
    if (p < 1000ULL) {
        q = 1000;
    } else {
        q = 1;
        p /= 1000;
    }
	return QString(sr_voltage_string(p, q));
}

QString ProbeOptions::print_coupling(GVariant *const gvar)
{
    uint8_t coupling;
    g_variant_get(gvar, "y", &coupling);
    if (coupling == SR_DC_COUPLING) {
        return QString("DC");
    } else if (coupling == SR_AC_COUPLING) {
        return QString("AC");
    } else if (coupling == SR_GND_COUPLING) {
        return QString("GND");
    } else {
        return QString("Undefined");
    }
}

} // binding
} // prop
} // pv

