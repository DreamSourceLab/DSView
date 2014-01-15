/*
 * This file is part of the DSLogic-gui project.
 * DSLogic-gui is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2013 DreamSourceLab <dreamsourcelab@dreamsourcelab.com>
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

#include <QSpinBox>

#include "int.h"

using namespace std;
using namespace boost;

namespace pv {
namespace prop {

Int::Int(QString name,
	QString suffix,
	optional< pair<int64_t, int64_t> > range,
	Getter getter,
	Setter setter) :
	Property(name, getter, setter),
	_suffix(suffix),
	_range(range),
	_spin_box(NULL)
{
}

Int::~Int()
{
}

QWidget* Int::get_widget(QWidget *parent)
{
	if (_spin_box)
		return _spin_box;

	_spin_box = new QSpinBox(parent);
	_spin_box->setSuffix(_suffix);
	if (_range)
		_spin_box->setRange((int)_range->first, (int)_range->second);

	GVariant *const value = _getter ? _getter() : NULL;
	if (value) {
		_spin_box->setValue((int)g_variant_get_int64(value));
		g_variant_unref(value);
	}

	return _spin_box;
}

void Int::commit()
{
	assert(_setter);

	if (!_spin_box)
		return;

	_setter(g_variant_new_int64(_spin_box->value()));
}

} // prop
} // pv
