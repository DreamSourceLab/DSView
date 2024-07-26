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
#include "../ui/langresource.h"
#include "../ui/xspinbox.h"

namespace pv {
namespace dialogs {

Interval::Interval(QWidget *parent) :
    DSDialog(parent),
    _button_box(QDialogButtonBox::Ok,
        Qt::Horizontal, this)
{
    _interval_label = NULL;
    _interval_spinBox = NULL;
    _interval_slider = NULL;
    _bSetting = false;
    _bDone = false;

    setMinimumWidth(300);
    _interval_label = new QLabel(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_INTERVAL_S), "Interval(s): "), this);
    _interval_spinBox = new XDoubleSpinBox(this);
    _interval_spinBox->setRange(0.1, 10);
    _interval_spinBox->setDecimals(1);
    _interval_spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
    _interval_slider = new QSlider(Qt::Horizontal, this);
    _interval_slider->setRange(0, 10);

    QGridLayout *glayout = new QGridLayout(this);
    glayout->addWidget(_interval_label, 0, 0);
    glayout->addWidget(_interval_spinBox, 0, 1);
    glayout->addWidget(_interval_slider, 1, 0, 1, 3);
    glayout->addWidget(&_button_box, 2, 2);

    layout()->addLayout(glayout);
    setTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_REPETITIVE_INTERVAL), "Repetitive Interval"));

    connect(&_button_box, SIGNAL(accepted()), this, SLOT(accept()));
    connect(_interval_slider, SIGNAL(valueChanged(int)), this, SLOT(on_slider_changed(int)));
    connect(_interval_spinBox, SIGNAL(valueChanged(double)), this, SLOT(on_inputbox_changed(double)));
}

void Interval::accept()
{
    using namespace Qt;
    _bDone = true;
    QDialog::accept();
}

void Interval::reject()
{
    using namespace Qt;

    QDialog::reject();
}

void Interval::set_interval(double value)
{
    _interval_slider->setValue((int)value);
    _interval_spinBox->setValue(value);
}

double Interval::get_interval()
{
    return _interval_spinBox->value();
}

void Interval::on_slider_changed(int value)
{
    if (!_bSetting){
        _bSetting = true;
        _interval_spinBox->setValue((double)value);
        _bSetting = false;
    }    
}

void Interval::on_inputbox_changed(double value)
{
    if (!_bSetting){
        _bSetting = true;
        _interval_slider->setValue((int)value);
        _bSetting = false;
    }    
}

} // namespace dialogs
} // namespace pv
