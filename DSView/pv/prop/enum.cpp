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

#include <QComboBox>
#include <QAbstractItemView>

#include "enum.h"

using namespace boost;
using namespace std;

namespace pv {
namespace prop {

Enum::Enum(QString name,
	vector<pair<GVariant*, QString> > values,
	Getter getter, Setter setter) :
	Property(name, getter, setter),
	_values(values),
	_selector(NULL)
{
    for (vector< pair<GVariant*, QString> >::const_iterator i =
        _values.begin(); i != _values.end(); i++)
        g_variant_ref((*i).first);
}

Enum::~Enum()
{
	for (unsigned int i = 0; i < _values.size(); i++)
		g_variant_unref(_values[i].first);
}

QWidget* Enum::get_widget(QWidget *parent, bool auto_commit)
{
	if (_selector)
		return _selector;

	GVariant *const value = _getter ? _getter() : NULL;
    if (!value)
        return NULL;

	_selector = new QComboBox(parent);
    _selector->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	for (unsigned int i = 0; i < _values.size(); i++) {
		const pair<GVariant*, QString> &v = _values[i];
		_selector->addItem(v.second, qVariantFromValue((void*)v.first));
		if (value && g_variant_compare(v.first, value) == 0)
			_selector->setCurrentIndex(i);
	}
    _selector->view()->setMinimumWidth(_selector->width()+30);

	g_variant_unref(value);

    if (auto_commit)
        connect(_selector, SIGNAL(currentIndexChanged(int)),
            this, SLOT(on_current_item_changed(int)));

	return _selector;
}

void Enum::commit()
{
	assert(_setter);

	if (!_selector)
		return;

	const int index = _selector->currentIndex();
	if (index < 0)
		return;

	_setter((GVariant*)_selector->itemData(index).value<void*>());
}

void Enum::on_current_item_changed(int)
{
    commit();
}

} // prop
} // pv
