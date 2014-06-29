/*
 * This file is part of the DSLogic-gui project.
 * DSLogic-gui is based on PulseView.
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


#include "dsotriggerdock.h"
#include "../sigsession.h"

#include <QObject>
#include <QLabel>
#include <QRadioButton>
#include <QPainter>
#include <QStyleOption>
#include <QMessageBox>

#include <QVector>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "libsigrok4DSLogic/libsigrok.h"

namespace pv {
namespace dock {

DsoTriggerDock::DsoTriggerDock(QWidget *parent, SigSession &session) :
    QWidget(parent),
    _session(session)
{
    QLabel *position_label = new QLabel("Trigger Position: ", this);
    position_spinBox = new QSpinBox(this);
    position_spinBox->setRange(0, 99);
    position_spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
    position_slider = new QSlider(Qt::Horizontal, this);
    position_slider->setRange(0, 99);
    connect(position_slider, SIGNAL(valueChanged(int)), position_spinBox, SLOT(setValue(int)));
    connect(position_spinBox, SIGNAL(valueChanged(int)), position_slider, SLOT(setValue(int)));
    connect(position_slider, SIGNAL(valueChanged(int)), this, SLOT(pos_changed(int)));

    QLabel *tSource_labe = new QLabel("Trigger Sources: ", this);
    QRadioButton *auto_radioButton = new QRadioButton("Auto");
    auto_radioButton->setChecked(true);
    QRadioButton *ch0_radioButton = new QRadioButton("Channel 0");
    QRadioButton *ch1_radioButton = new QRadioButton("Channel 1");
    QRadioButton *ch0a1_radioButton = new QRadioButton("Channel 0 && Channel 1");
    QRadioButton *ch0o1_radioButton = new QRadioButton("Channel 0 | Channel 1");
    connect(auto_radioButton, SIGNAL(clicked()), this, SLOT(source_changed()));
    connect(ch0_radioButton, SIGNAL(clicked()), this, SLOT(source_changed()));
    connect(ch1_radioButton, SIGNAL(clicked()), this, SLOT(source_changed()));
    connect(ch0a1_radioButton, SIGNAL(clicked()), this, SLOT(source_changed()));
    connect(ch0o1_radioButton, SIGNAL(clicked()), this, SLOT(source_changed()));

    QLabel *tType_labe = new QLabel("Trigger Types: ", this);
    QRadioButton *rising_radioButton = new QRadioButton("Rising Edge");
    rising_radioButton->setChecked(true);
    QRadioButton *falling_radioButton = new QRadioButton("Falling Edge");
    connect(rising_radioButton, SIGNAL(clicked()), this, SLOT(type_changed()));
    connect(falling_radioButton, SIGNAL(clicked()), this, SLOT(type_changed()));

    source_group=new QButtonGroup(this);
    type_group=new QButtonGroup(this);

    source_group->addButton(auto_radioButton);
    source_group->addButton(ch0_radioButton);
    source_group->addButton(ch1_radioButton);
    source_group->addButton(ch0a1_radioButton);
    source_group->addButton(ch0o1_radioButton);
    source_group->setId(auto_radioButton, DSO_TRIGGER_AUTO);
    source_group->setId(ch0_radioButton, DSO_TRIGGER_CH0);
    source_group->setId(ch1_radioButton, DSO_TRIGGER_CH1);
    source_group->setId(ch0a1_radioButton, DSO_TRIGGER_CH0A1);
    source_group->setId(ch0o1_radioButton, DSO_TRIGGER_CH0O1);

    type_group->addButton(rising_radioButton);
    type_group->addButton(falling_radioButton);
    type_group->setId(rising_radioButton, DSO_TRIGGER_RISING);
    type_group->setId(falling_radioButton, DSO_TRIGGER_FALLING);

    QVBoxLayout *layout = new QVBoxLayout(this);
    QGridLayout *gLayout = new QGridLayout();
    gLayout->addWidget(position_label, 0, 0);
    gLayout->addWidget(position_spinBox, 0, 1);
    gLayout->addWidget(new QLabel(this), 0, 2);
    gLayout->addWidget(position_slider, 1, 0, 1, 3);

    gLayout->addWidget(new QLabel(this), 2, 0);
    gLayout->addWidget(tSource_labe, 3, 0);
    gLayout->addWidget(auto_radioButton, 4, 0);
    gLayout->addWidget(ch0_radioButton, 5, 0);
    gLayout->addWidget(ch1_radioButton, 5, 1);
    gLayout->addWidget(ch0a1_radioButton, 6, 0);
    gLayout->addWidget(ch0o1_radioButton, 6, 1);

    gLayout->addWidget(new QLabel(this), 7, 0);
    gLayout->addWidget(tType_labe, 8, 0);
    gLayout->addWidget(rising_radioButton, 9, 0);
    gLayout->addWidget(falling_radioButton, 10, 0);

    gLayout->setColumnStretch(3, 1);

    layout->addLayout(gLayout);
    layout->addStretch(1);
    setLayout(layout);
}

DsoTriggerDock::~DsoTriggerDock()
{
}

void DsoTriggerDock::paintEvent(QPaintEvent *)
{
    QStyleOption opt;
    opt.init(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void DsoTriggerDock::pos_changed(int pos)
{
    int ret;
    quint32 real_pos;
    real_pos = pos*_session.get_total_sample_len()/100.0f;
    real_pos = (_session.get_last_sample_rate() > SR_MHZ(100)) ? real_pos/2 : real_pos;
    ret = sr_config_set(_session.get_device(), SR_CONF_HORIZ_TRIGGERPOS, g_variant_new_uint32(real_pos));
    if (ret != SR_OK) {
        QMessageBox msg(this);
        msg.setText("Trigger Setting Issue");
        msg.setInformativeText("Change horiz trigger position failed!");
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Warning);
        msg.exec();
    }
}

void DsoTriggerDock::source_changed()
{
    int id = source_group->checkedId();
    int ret;

    ret = sr_config_set(_session.get_device(), SR_CONF_TRIGGER_SOURCE, g_variant_new_byte(id));
    if (ret != SR_OK) {
        QMessageBox msg(this);
        msg.setText("Trigger Setting Issue");
        msg.setInformativeText("Change trigger source failed!");
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Warning);
        msg.exec();
    }
}

void DsoTriggerDock::type_changed()
{
    int id = type_group->checkedId();
    int ret;

    ret = sr_config_set(_session.get_device(), SR_CONF_TRIGGER_SLOPE, g_variant_new_byte(id));
    if (ret != SR_OK) {
        QMessageBox msg(this);
        msg.setText("Trigger Setting Issue");
        msg.setInformativeText("Change trigger type failed!");
        msg.setStandardButtons(QMessageBox::Ok);
        msg.setIcon(QMessageBox::Warning);
        msg.exec();
    }
}

void DsoTriggerDock::device_change()
{
    if (strcmp(_session.get_device()->driver->name, "DSLogic") != 0) {
        position_spinBox->setDisabled(true);
        position_slider->setDisabled(true);
    } else {
        position_spinBox->setDisabled(false);
        position_slider->setDisabled(false);
    }
}

} // namespace dock
} // namespace pv
