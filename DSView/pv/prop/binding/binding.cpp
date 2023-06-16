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
 

#include <QFormLayout>
#include <QLabel>

#include "../property.h"
#include "binding.h"
 

namespace pv {
namespace prop {
namespace binding {

const std::vector<Property*>& Binding::properties()
{
	return _properties;
}

Binding::Binding(){
    _row_num = 0;    
}

void Binding::commit()
{
    for(auto p : _properties) {
        p->commit();
    }
}

void Binding::add_properties_to_form(QFormLayout *layout, bool auto_commit, QFont font)
{
    assert(layout);

    for(auto p : _properties)
    {
        QWidget *const widget = p->get_widget(layout->parentWidget(), auto_commit);

        if (p->labeled_widget()){
            layout->addRow(widget);
            widget->setFont(font);
            _row_num++;
        }
        else{
            const QString &lbstr = p->label();
            //remove data format options
            if (lbstr == "Data format"){
                continue;                
            }   
            QLabel *lb = new QLabel(p->label());
            lb->setFont(font);
            widget->setFont(font);
            layout->addRow(lb, widget);
            _row_num++;
        } 
    }
}

std::map<Property*,GVariant*> Binding::get_property_value()
{
    std::map <Property*,GVariant*> pvalue;
            
    for(auto p : _properties)
    {
        pvalue[p] = p->get_value();
    }

    return pvalue;
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
