/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
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


#include "dsomeasure.h"

#include <QCheckBox>
#include <QVariant>

#include <boost/foreach.hpp>

using namespace boost;
using namespace std;
using namespace pv::view;

namespace pv {
namespace dialogs {

DsoMeasure::DsoMeasure(QWidget *parent, boost::shared_ptr<DsoSignal> dsoSig) :
	QDialog(parent),
    _dsoSig(dsoSig),
    _layout(this),
    _button_box(QDialogButtonBox::Ok,
        Qt::Horizontal, this)
{
    setWindowTitle(tr("DSO Measure Options"));
    setLayout(&_layout);

    for (int i=DsoSignal::DSO_MS_BEGIN+1; i<DsoSignal::DSO_MS_END; i++) {
        QCheckBox *checkBox = new QCheckBox(_dsoSig->get_ms_string(i), this);
        checkBox->setProperty("id", QVariant(i));
        checkBox->setChecked(dsoSig->get_ms_en(i));
        _layout.addWidget(checkBox);
        connect(checkBox, SIGNAL(toggled(bool)), this, SLOT(set_measure(bool)));
    }

    _layout.addWidget(&_button_box);

    connect(&_button_box, SIGNAL(accepted()), this, SLOT(accept()));
    connect(&_button_box, SIGNAL(rejected()), this, SLOT(accept()));
}

void DsoMeasure::set_measure(bool en)
{
    (void)en;
    QCheckBox* sc=dynamic_cast<QCheckBox*>(sender());
    if(sc != NULL) {
        QVariant id = sc->property("id");
        _dsoSig->set_ms_en(id.toInt(), sc->isChecked());
    }
}

void DsoMeasure::accept()
{
	using namespace Qt;

	QDialog::accept();
}

void DsoMeasure::reject()
{
    accept();
}

} // namespace dialogs
} // namespace pv
