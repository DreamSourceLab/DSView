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


#include <stdint.h>
#include <assert.h>
#include <math.h>

#include <QSpinBox>

#include "int.h"

using boost::optional;
using namespace std;

//#define INT8_MIN    (-0x7f - 1)
//#define INT16_MIN   (-0x7fff - 1)
//#define INT32_MIN   (-0x7fffffff - 1)
//#define INT64_MIN   (-0x7fffffffffffffff - 1)
//
//#define INT8_MAX    0x7f
//#define INT16_MAX   0x7fff
//#define INT32_MAX   0x7fffffff
//#define INT64_MAX   0x7fffffffffffffff
//
//#define UINT8_MAX   0xff
//#define UINT16_MAX  0xffff
//#define UINT32_MAX  0xffffffff
//#define UINT64_MAX  0xffffffffffffffff

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
    _value(NULL),
	_spin_box(NULL)
{
}

Int::~Int()
{
    if (_value)
        g_variant_unref(_value);
}

QWidget* Int::get_widget(QWidget *parent, bool auto_commit)
{
    int64_t int_val = 0, range_min = 0, range_max = 0;

    if (_spin_box)
        return _spin_box;

    if (_value)
        g_variant_unref(_value);

    _value = _getter ? _getter() : NULL;
    if (!_value)
        return NULL;

    _spin_box = new QSpinBox(parent);
    _spin_box->setSuffix(_suffix);

    const GVariantType *const type = g_variant_get_type(_value);
    assert(type);

    if (g_variant_type_equal(type, G_VARIANT_TYPE_BYTE))
    {
        int_val = g_variant_get_byte(_value);
        range_min = 0, range_max = UINT8_MAX;
    }
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT16))
    {
        int_val = g_variant_get_int16(_value);
        range_min = INT16_MIN, range_max = INT16_MAX;
    }
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT16))
    {
        int_val = g_variant_get_uint16(_value);
        range_min = 0, range_max = UINT16_MAX;
    }
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT32))
    {
        int_val = g_variant_get_int32(_value);
        range_min = INT32_MIN, range_max = INT32_MAX;
    }
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT32))
    {
        int_val = g_variant_get_uint32(_value);
        range_min = 0, range_max = UINT32_MAX;
    }
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT64))
    {
        int_val = g_variant_get_int64(_value);
        range_min = INT64_MIN, range_max = INT64_MAX;
    }
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT64))
    {
        int_val = g_variant_get_uint64(_value);
        range_min = 0, range_max = UINT64_MAX;
    }
    else
    {
        // Unexpected value type.
        assert(0);
    }

    // @todo Sigrok supports 64-bit quantities, but Qt does not have a
    // standard widget to allow the values to be modified over the full
    // 64-bit range on 32-bit machines. To solve the issue we need a
    // custom widget.

    range_min = max(range_min, (int64_t)INT_MIN);
    range_max = min(range_max, (int64_t)INT_MAX);

    if (_range)
        _spin_box->setRange((int)_range->first, (int)_range->second);
    else
        _spin_box->setRange((int)range_min, (int)range_max);

    _spin_box->setValue((int)int_val);

    if (auto_commit)
        connect(_spin_box, SIGNAL(valueChanged(int)),
            this, SLOT(on_value_changed(int)));

    return _spin_box;
}

void Int::commit()
{
    assert(_setter);

    if (!_spin_box)
        return;

    assert(_value);

    GVariant *new_value = NULL;
    const GVariantType *const type = g_variant_get_type(_value);
    assert(type);

    if (g_variant_type_equal(type, G_VARIANT_TYPE_BYTE))
        new_value = g_variant_new_byte(_spin_box->value());
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT16))
        new_value = g_variant_new_int16(_spin_box->value());
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT16))
        new_value = g_variant_new_uint16(_spin_box->value());
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT32))
        new_value = g_variant_new_int32(_spin_box->value());
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT32))
        new_value = g_variant_new_int32(_spin_box->value());
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_INT64))
        new_value = g_variant_new_int64(_spin_box->value());
    else if (g_variant_type_equal(type, G_VARIANT_TYPE_UINT64))
        new_value = g_variant_new_uint64(_spin_box->value());
    else
    {
        // Unexpected value type.
        assert(0);
    }

    assert(new_value);

    g_variant_unref(_value);
    g_variant_ref(new_value);
    _value = new_value;

    _setter(new_value);
}

void Int::on_value_changed(int)
{
    commit();
}

} // prop
} // pv
