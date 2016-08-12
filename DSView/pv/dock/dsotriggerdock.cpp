/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
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


#include "dsotriggerdock.h"
#include "../sigsession.h"
#include "../device/devinst.h"
#include "../dialogs/dsmessagebox.h"
#include "../view/dsosignal.h"

#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>

#include <QObject>
#include <QLabel>
#include <QRadioButton>
#include <QPainter>
#include <QStyleOption>

#include <QVector>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include "libsigrok4DSL/libsigrok.h"

using namespace boost;
using namespace std;

namespace pv {
namespace dock {

DsoTriggerDock::DsoTriggerDock(QWidget *parent, SigSession &session) :
    QScrollArea(parent),
    _session(session)
{
    this->setWidgetResizable(true);
    _widget = new QWidget(this);

    QLabel *position_label = new QLabel(tr("Trigger Position: "), _widget);
    position_spinBox = new QSpinBox(_widget);
    position_spinBox->setRange(0, 99);
    position_spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
    position_slider = new QSlider(Qt::Horizontal, _widget);
    position_slider->setRange(0, 99);
    connect(position_slider, SIGNAL(valueChanged(int)), position_spinBox, SLOT(setValue(int)));
    connect(position_spinBox, SIGNAL(valueChanged(int)), position_slider, SLOT(setValue(int)));
    connect(position_slider, SIGNAL(valueChanged(int)), this, SLOT(pos_changed(int)));

    QLabel *holdoff_label = new QLabel(tr("Hold Off Time: "), _widget);
    holdoff_comboBox = new QComboBox(_widget);
    holdoff_comboBox->addItem(tr("uS"), qVariantFromValue(1000));
    holdoff_comboBox->addItem(tr("mS"), qVariantFromValue(1000000));
    holdoff_comboBox->addItem(tr("S"), qVariantFromValue(1000000000));
    holdoff_comboBox->setCurrentIndex(0);
    holdoff_spinBox = new QSpinBox(_widget);
    holdoff_spinBox->setRange(0, 999);
    holdoff_spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
    holdoff_slider = new QSlider(Qt::Horizontal, _widget);
    holdoff_slider->setRange(0, 999);
    connect(holdoff_slider, SIGNAL(valueChanged(int)), holdoff_spinBox, SLOT(setValue(int)));
    connect(holdoff_spinBox, SIGNAL(valueChanged(int)), holdoff_slider, SLOT(setValue(int)));
    connect(holdoff_slider, SIGNAL(valueChanged(int)), this, SLOT(hold_changed(int)));
    connect(holdoff_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(hold_changed(int)));

    QLabel *margin_label = new QLabel(tr("Noise Sensitivity: "), _widget);
    margin_slider = new QSlider(Qt::Horizontal, _widget);
    margin_slider->setRange(0, 15);
    connect(margin_slider, SIGNAL(valueChanged(int)), this, SLOT(margin_changed(int)));


    QLabel *tSource_labe = new QLabel(tr("Trigger Sources: "), _widget);
    QRadioButton *auto_radioButton = new QRadioButton(tr("Auto"));
    auto_radioButton->setChecked(true);
    QRadioButton *ch0_radioButton = new QRadioButton(tr("Channel 0"));
    QRadioButton *ch1_radioButton = new QRadioButton(tr("Channel 1"));
    QRadioButton *ch0a1_radioButton = new QRadioButton(tr("Channel 0 && 1"));
    QRadioButton *ch0o1_radioButton = new QRadioButton(tr("Channel 0 | 1"));
    connect(auto_radioButton, SIGNAL(clicked()), this, SLOT(source_changed()));
    connect(ch0_radioButton, SIGNAL(clicked()), this, SLOT(source_changed()));
    connect(ch1_radioButton, SIGNAL(clicked()), this, SLOT(source_changed()));
    connect(ch0a1_radioButton, SIGNAL(clicked()), this, SLOT(source_changed()));
    connect(ch0o1_radioButton, SIGNAL(clicked()), this, SLOT(source_changed()));

    QLabel *tType_labe = new QLabel(tr("Trigger Types: "), _widget);
    QRadioButton *rising_radioButton = new QRadioButton(tr("Rising Edge"));
    rising_radioButton->setChecked(true);
    QRadioButton *falling_radioButton = new QRadioButton(tr("Falling Edge"));
    connect(rising_radioButton, SIGNAL(clicked()), this, SLOT(type_changed()));
    connect(falling_radioButton, SIGNAL(clicked()), this, SLOT(type_changed()));

    source_group=new QButtonGroup(_widget);
    channel_comboBox = new QComboBox(_widget);
    type_group=new QButtonGroup(_widget);

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

    QVBoxLayout *layout = new QVBoxLayout(_widget);
    QGridLayout *gLayout = new QGridLayout();
    gLayout->setVerticalSpacing(5);
    gLayout->addWidget(position_label, 0, 0);
    gLayout->addWidget(position_spinBox, 0, 1);
    gLayout->addWidget(new QLabel(tr("%"), _widget), 0, 2);
    gLayout->addWidget(position_slider, 1, 0, 1, 4);

    gLayout->addWidget(new QLabel(_widget), 2, 0);
    gLayout->addWidget(tSource_labe, 3, 0);
    gLayout->addWidget(auto_radioButton, 4, 0);
    gLayout->addWidget(channel_comboBox, 4, 1, 1, 3);
    gLayout->addWidget(ch0_radioButton, 5, 0);
    gLayout->addWidget(ch1_radioButton, 5, 1, 1, 3);
    gLayout->addWidget(ch0a1_radioButton, 6, 0);
    gLayout->addWidget(ch0o1_radioButton, 6, 1, 1, 3);

    gLayout->addWidget(new QLabel(_widget), 7, 0);
    gLayout->addWidget(tType_labe, 8, 0);
    gLayout->addWidget(rising_radioButton, 9, 0);
    gLayout->addWidget(falling_radioButton, 10, 0);

    gLayout->addWidget(new QLabel(_widget), 11, 0);
    gLayout->addWidget(holdoff_label, 12, 0);
    gLayout->addWidget(holdoff_spinBox, 12, 1);
    gLayout->addWidget(holdoff_comboBox, 12, 2);
    gLayout->addWidget(holdoff_slider, 13, 0, 1, 4);

    gLayout->addWidget(new QLabel(_widget), 14, 0);
    gLayout->addWidget(margin_label, 15, 0);
    gLayout->addWidget(margin_slider, 16, 0, 1, 4);

    gLayout->setColumnStretch(4, 1);

    layout->addLayout(gLayout);
    layout->addStretch(1);
    _widget->setLayout(layout);

    this->setWidget(_widget);
    //_widget->setGeometry(0, 0, sizeHint().width(), 500);
    _widget->setObjectName("dsoTriggerWidget");
}

DsoTriggerDock::~DsoTriggerDock()
{
}

void DsoTriggerDock::paintEvent(QPaintEvent *)
{
//    QStyleOption opt;
//    opt.init(this);
//    QPainter p(this);
//    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void DsoTriggerDock::pos_changed(int pos)
{
    int ret;
    ret = _session.get_device()->set_config(NULL, NULL,
                                            SR_CONF_HORIZ_TRIGGERPOS,
                                            g_variant_new_byte((uint8_t)pos));
    if (!ret) {
        dialogs::DSMessageBox msg(this);
        msg.mBox()->setText(tr("Trigger Setting Issue"));
        msg.mBox()->setInformativeText(tr("Change horiz trigger position failed!"));
        msg.mBox()->setStandardButtons(QMessageBox::Ok);
        msg.mBox()->setIcon(QMessageBox::Warning);
        msg.exec();
    }
    set_trig_pos(pos);
}

void DsoTriggerDock::hold_changed(int hold)
{
    (void)hold;
    int ret;
    uint64_t holdoff;
    if (holdoff_comboBox->currentData().toDouble() == 1000000000) {
        holdoff_slider->setRange(0, 10);
    } else {
        holdoff_slider->setRange(0, 999);
    }
    holdoff = holdoff_slider->value() * holdoff_comboBox->currentData().toDouble() / 10;
    ret = _session.get_device()->set_config(NULL, NULL,
                                            SR_CONF_TRIGGER_HOLDOFF,
                                            g_variant_new_uint64(holdoff));

    if (!ret) {
        dialogs::DSMessageBox msg(this);
        msg.mBox()->setText(tr("Trigger Setting Issue"));
        msg.mBox()->setInformativeText(tr("Change trigger hold off time failed!"));
        msg.mBox()->setStandardButtons(QMessageBox::Ok);
        msg.mBox()->setIcon(QMessageBox::Warning);
        msg.exec();
    }
}

void DsoTriggerDock::margin_changed(int margin)
{
    int ret;
    ret = _session.get_device()->set_config(NULL, NULL,
                                            SR_CONF_TRIGGER_MARGIN,
                                            g_variant_new_byte(margin));
    if (!ret) {
        dialogs::DSMessageBox msg(this);
        msg.mBox()->setText(tr("Trigger Setting Issue"));
        msg.mBox()->setInformativeText(tr("Change trigger value sensitivity failed!"));
        msg.mBox()->setStandardButtons(QMessageBox::Ok);
        msg.mBox()->setIcon(QMessageBox::Warning);
        msg.exec();
    }
}

void DsoTriggerDock::source_changed()
{
    int id = source_group->checkedId();
    int ret;

    ret = _session.get_device()->set_config(NULL, NULL,
                                            SR_CONF_TRIGGER_SOURCE,
                                            g_variant_new_byte(id));
    if (!ret) {
        dialogs::DSMessageBox msg(this);
        msg.mBox()->setText(tr("Trigger Setting Issue"));
        msg.mBox()->setInformativeText(tr("Change trigger source failed!"));
        msg.mBox()->setStandardButtons(QMessageBox::Ok);
        msg.mBox()->setIcon(QMessageBox::Warning);
        msg.exec();
    }
}

void DsoTriggerDock::channel_changed(int ch)
{
    (void)ch;
    int ret;

    ret = _session.get_device()->set_config(NULL, NULL,
                                            SR_CONF_TRIGGER_CHANNEL,
                                            g_variant_new_byte(channel_comboBox->currentData().toInt()));
    if (!ret) {
        dialogs::DSMessageBox msg(this);
        msg.mBox()->setText(tr("Trigger Setting Issue"));
        msg.mBox()->setInformativeText(tr("Change trigger channel failed!"));
        msg.mBox()->setStandardButtons(QMessageBox::Ok);
        msg.mBox()->setIcon(QMessageBox::Warning);
        msg.exec();
    }
}

void DsoTriggerDock::type_changed()
{
    int id = type_group->checkedId();
    int ret;

    ret = _session.get_device()->set_config(NULL, NULL,
                                            SR_CONF_TRIGGER_SLOPE,
                                            g_variant_new_byte(id));
    if (!ret) {
        dialogs::DSMessageBox msg(this);
        msg.mBox()->setText(tr("Trigger Setting Issue"));
        msg.mBox()->setInformativeText(tr("Change trigger type failed!"));
        msg.mBox()->setStandardButtons(QMessageBox::Ok);
        msg.mBox()->setIcon(QMessageBox::Warning);
        msg.exec();
    }
}

void DsoTriggerDock::device_change()
{
    if (_session.get_device()->name() != "DSLogic") {
        position_spinBox->setDisabled(true);
        position_slider->setDisabled(true);
    } else {
        position_spinBox->setDisabled(false);
        position_slider->setDisabled(false);
    }
}

void DsoTriggerDock::init()
{
    if (_session.get_device()->name().contains("virtual")) {
        foreach(QAbstractButton * btn, source_group->buttons())
            btn->setDisabled(true);
        foreach(QAbstractButton * btn, type_group->buttons())
            btn->setDisabled(true);
        holdoff_slider->setDisabled(true);
        holdoff_spinBox->setDisabled(true);
        holdoff_comboBox->setDisabled(true);
        margin_slider->setDisabled(true);
        channel_comboBox->setDisabled(true);
        return;
    } else {
        foreach(QAbstractButton * btn, source_group->buttons())
            btn->setDisabled(false);
        foreach(QAbstractButton * btn, type_group->buttons())
            btn->setDisabled(false);
        holdoff_slider->setDisabled(false);
        holdoff_spinBox->setDisabled(false);
        holdoff_comboBox->setDisabled(false);
        margin_slider->setDisabled(false);
        channel_comboBox->setDisabled(false);
    }

    // TRIGGERPOS
    GVariant* gvar = _session.get_device()->get_config(NULL, NULL,
                                            SR_CONF_HORIZ_TRIGGERPOS);
    if (gvar != NULL) {
        uint16_t pos = g_variant_get_byte(gvar);
        g_variant_unref(gvar);
        position_slider->setValue(pos);
    }

    gvar = _session.get_device()->get_config(NULL, NULL,
                                                SR_CONF_TRIGGER_SOURCE);
    if (gvar != NULL) {
        uint8_t src = g_variant_get_byte(gvar);
        g_variant_unref(gvar);
        source_group->button(src)->setChecked(true);
    }

    // setup channel_comboBox
    disconnect(channel_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(channel_changed(int)));
    channel_comboBox->clear();
    BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _session.get_signals()) {
        boost::shared_ptr<view::DsoSignal> dsoSig;
        if (dsoSig = dynamic_pointer_cast<view::DsoSignal>(s)) {
            channel_comboBox->addItem(dsoSig->get_name(), qVariantFromValue(dsoSig->get_index()));
        }
    }
    gvar = _session.get_device()->get_config(NULL, NULL,
                                                SR_CONF_TRIGGER_CHANNEL);
    if (gvar != NULL) {
        uint8_t src = g_variant_get_byte(gvar);
        g_variant_unref(gvar);
        for (int i = 0; i < channel_comboBox->count(); i++) {
            if (src == channel_comboBox->itemData(i).toInt()) {
                channel_comboBox->setCurrentIndex(i);
                break;
            }
        }
    }
    connect(channel_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(channel_changed(int)));

    gvar = _session.get_device()->get_config(NULL, NULL,
                                                SR_CONF_TRIGGER_SLOPE);
    if (gvar != NULL) {
        uint8_t slope = g_variant_get_byte(gvar);
        g_variant_unref(gvar);
        type_group->button(slope)->setChecked(true);
    }

    disconnect(holdoff_slider, SIGNAL(valueChanged(int)), this, SLOT(hold_changed(int)));
    disconnect(holdoff_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(hold_changed(int)));
    gvar = _session.get_device()->get_config(NULL, NULL,
                                                SR_CONF_TRIGGER_HOLDOFF);
    if (gvar != NULL) {
        uint64_t holdoff = g_variant_get_uint64(gvar);
        g_variant_unref(gvar);
        for (int i = holdoff_comboBox->count()-1; i >= 0; i--) {
            if (holdoff >= holdoff_comboBox->itemData(i).toDouble()) {
                holdoff_comboBox->setCurrentIndex(i);
                break;
            }
        }
        if (holdoff_comboBox->currentData().toDouble() == 1000000000) {
            holdoff_slider->setRange(0, 10);
        } else {
            holdoff_slider->setRange(0, 999);
        }
        holdoff_spinBox->setValue(holdoff * 10.0/holdoff_comboBox->currentData().toDouble());
    }
    connect(holdoff_slider, SIGNAL(valueChanged(int)), this, SLOT(hold_changed(int)));
    connect(holdoff_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(hold_changed(int)));

    disconnect(margin_slider, SIGNAL(valueChanged(int)), this, SLOT(margin_changed(int)));
    gvar = _session.get_device()->get_config(NULL, NULL,
                                                SR_CONF_TRIGGER_MARGIN);
    if (gvar != NULL) {
        uint8_t margin = g_variant_get_byte(gvar);
        g_variant_unref(gvar);
        margin_slider->setValue(margin);
    }
    connect(margin_slider, SIGNAL(valueChanged(int)), this, SLOT(margin_changed(int)));
}

} // namespace dock
} // namespace pv
