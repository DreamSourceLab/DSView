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


#ifndef DSVIEW_PV_PROP_DOUBLE_H
#define DSVIEW_PV_PROP_DOUBLE_H

#include <utility>

#include <boost/optional.hpp>

#include "property.h"

class QDoubleSpinBox;

namespace pv {
namespace prop {

class Double : public Property
{
    Q_OBJECT;

public:
	Double(QString name, int decimals, QString suffix,
		boost::optional< std::pair<double, double> > range,
		boost::optional<double> step,
		Getter getter,
		Setter setter);

	virtual ~Double();

    QWidget* get_widget(QWidget *parent, bool auto_commit);

	void commit();

private slots:
    void on_value_changed(double);

private:
	const int _decimals;
	const QString _suffix;
	const boost::optional< std::pair<double, double> > _range;
	const boost::optional<double> _step;

	QDoubleSpinBox *_spin_box;
};

} // prop
} // pv

#endif // DSVIEW_PV_PROP_DOUBLE_H
