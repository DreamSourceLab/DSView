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

#include <boost/foreach.hpp>

#include <QFormLayout>

#include <pv/prop/property.h>

#include "binding.h"

using boost::shared_ptr;

namespace pv {
namespace prop {
namespace binding {

const std::vector< boost::shared_ptr<Property> >& Binding::properties()
{
	return _properties;
}

void Binding::commit()
{
    BOOST_FOREACH(shared_ptr<pv::prop::Property> p, _properties) {
        assert(p);
        p->commit();
    }
}

void Binding::add_properties_to_form(QFormLayout *layout,
    bool auto_commit) const
{
    assert(layout);

    BOOST_FOREACH(shared_ptr<pv::prop::Property> p, _properties)
    {
        assert(p);

        QWidget *const widget = p->get_widget(layout->parentWidget(),
            auto_commit);
        if (p->labeled_widget())
            layout->addRow(widget);
        else
            layout->addRow(p->name(), widget);
    }
}

QWidget* Binding::get_property_form(QWidget *parent,
    bool auto_commit) const
{
    QWidget *const form = new QWidget(parent);
    QFormLayout *const layout = new QFormLayout(form);
    layout->setVerticalSpacing(5);
    layout->setFormAlignment(Qt::AlignLeft);
    layout->setLabelAlignment(Qt::AlignLeft);
    layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setLayout(layout);
    add_properties_to_form(layout, auto_commit);
    return form;
}

QString Binding::print_gvariant(GVariant *const gvar)
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

} // binding
} // prop
} // pv
