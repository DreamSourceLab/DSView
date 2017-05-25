/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2016 DreamSourceLab <support@dreamsourcelab.com>
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

#include "interval.h"

#include <QGridLayout>

namespace pv {
namespace dialogs {

Interval::Interval(SigSession &session, QWidget *parent) :
    DSDialog(parent),
    _session(session),
    _button_box(QDialogButtonBox::Ok,
        Qt::Horizontal, this)
{
    setMinimumWidth(300);
    _interval_label = new QLabel(tr("Interval(s): "), this);
    _interval_spinBox = new QSpinBox(this);
    _interval_spinBox->setRange(1, 10);
    _interval_spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
    _interval_slider = new QSlider(Qt::Horizontal, this);
    _interval_slider->setRange(1, 10);
    connect(_interval_slider, SIGNAL(valueChanged(int)), _interval_spinBox, SLOT(setValue(int)));
    connect(_interval_spinBox, SIGNAL(valueChanged(int)), _interval_slider, SLOT(setValue(int)));

    _interval_slider->setValue(_session.get_repeat_intvl());

    QGridLayout *glayout = new QGridLayout(this);
    glayout->addWidget(_interval_label, 0, 0);
    glayout->addWidget(_interval_spinBox, 0, 1);
    glayout->addWidget(_interval_slider, 1, 0, 1, 3);
    glayout->addWidget(&_button_box, 2, 2);

    layout()->addLayout(glayout);
    setTitle(tr("Repetitive Interval"));

    connect(&_button_box, SIGNAL(accepted()), this, SLOT(accept()));
}

void Interval::accept()
{
    using namespace Qt;
    _session.set_repeat_intvl(_interval_slider->value());
    QDialog::accept();
}

void Interval::reject()
{
    using namespace Qt;

    QDialog::reject();
}


} // namespace dialogs
} // namespace pv
