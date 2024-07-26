/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2024 DreamSourceLab <support@dreamsourcelab.com>
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

#include "xspinbox.h"

XSpinBox::XSpinBox(QWidget *parent)
    : QSpinBox(parent)
{

}

XSpinBox::~XSpinBox()
{

}

QString XSpinBox::textFromValue(int val) const
{
    return QString::number(val);
}

//-------------------XDoubleSpinBox
XDoubleSpinBox::XDoubleSpinBox(QWidget *parent)
    : QDoubleSpinBox(parent)
{

}

XDoubleSpinBox::~XDoubleSpinBox()
{

}

QString XDoubleSpinBox::textFromValue(double val) const
{
    int digit = decimals();
    return QString::number(val, 'f', digit);
}