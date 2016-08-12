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


#ifndef DSVIEW_PV_PROP_PROPERTY_H
#define DSVIEW_PV_PROP_PROPERTY_H

#include <glib.h>

#include <boost/function.hpp>

#include <QString>
#include <QWidget>

class QWidget;

namespace pv {
namespace prop {

class Property : public QObject
{
    Q_OBJECT;

public:
	typedef boost::function<GVariant* ()> Getter;
	typedef boost::function<void (GVariant*)> Setter;

protected:
    Property(QString name, Getter getter, Setter setter);

public:
    const QString& name() const;

    virtual QWidget* get_widget(QWidget *parent,
        bool auto_commit = false) = 0;
	virtual bool labeled_widget() const;

	virtual void commit() = 0;

protected:
	const Getter _getter;
	const Setter _setter;

private:
    QString _name;
};

} // prop
} // pv

#endif // DSVIEW_PV_PROP_PROPERTY_H
