/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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


#include <assert.h>

#include <QDoubleSpinBox>

#include "double.h"

using namespace std;
using namespace boost;

namespace pv {
namespace prop {

Double::Double(QString name,
	int decimals,
	QString suffix,
	optional< pair<double, double> > range,
	optional<double> step,
	Getter getter,
	Setter setter) :
	Property(name, getter, setter),
	_decimals(decimals),
	_suffix(suffix),
	_range(range),
	_step(step),
	_spin_box(NULL)
{
}

Double::~Double()
{
}

QWidget* Double::get_widget(QWidget *parent, bool auto_commit)
{
	if (_spin_box)
		return _spin_box;

	_spin_box = new QDoubleSpinBox(parent);
	_spin_box->setDecimals(_decimals);
	_spin_box->setSuffix(_suffix);
	if (_range)
		_spin_box->setRange(_range->first, _range->second);
	if (_step)
		_spin_box->setSingleStep(*_step);

	GVariant *const value = _getter ? _getter() : NULL;

	if (value) {
		_spin_box->setValue(g_variant_get_double(value));
		g_variant_unref(value);
	}

    if (auto_commit)
        connect(_spin_box, SIGNAL(valueChanged(double)),
            this, SLOT(on_value_changed(double)));

	return _spin_box;
}

void Double::commit()
{
	assert(_setter);

	if (!_spin_box)
		return;

	_setter(g_variant_new_double(_spin_box->value()));
}

void Double::on_value_changed(double)
{
    commit();
}

} // prop
} // pv
