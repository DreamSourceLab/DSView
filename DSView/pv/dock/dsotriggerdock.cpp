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

    _position_label = new QLabel(_widget);
    _position_spinBox = new QSpinBox(_widget);
    _position_spinBox->setRange(1, 99);
    _position_spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
    _position_slider = new QSlider(Qt::Horizontal, _widget);
    _position_slider->setRange(1, 99);
    connect(_position_slider, SIGNAL(valueChanged(int)), _position_spinBox, SLOT(setValue(int)));
    connect(_position_spinBox, SIGNAL(valueChanged(int)), _position_slider, SLOT(setValue(int)));
    connect(_position_slider, SIGNAL(valueChanged(int)), this, SLOT(pos_changed(int)));

    _holdoff_label = new QLabel(_widget);
    _holdoff_comboBox = new QComboBox(_widget);
    _holdoff_comboBox->addItem(tr("uS"), QVariant::fromValue(1000));
    _holdoff_comboBox->addItem(tr("mS"), QVariant::fromValue(1000000));
    _holdoff_comboBox->addItem(tr("S"), QVariant::fromValue(1000000000));
    _holdoff_comboBox->setCurrentIndex(0);
    _holdoff_spinBox = new QSpinBox(_widget);
    _holdoff_spinBox->setRange(0, 999);
    _holdoff_spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
    _holdoff_slider = new QSlider(Qt::Horizontal, _widget);
    _holdoff_slider->setRange(0, 999);
    connect(_holdoff_slider, SIGNAL(valueChanged(int)), _holdoff_spinBox, SLOT(setValue(int)));
    connect(_holdoff_spinBox, SIGNAL(valueChanged(int)), _holdoff_slider, SLOT(setValue(int)));
    connect(_holdoff_slider, SIGNAL(valueChanged(int)), this, SLOT(hold_changed(int)));
    connect(_holdoff_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(hold_changed(int)));

    _margin_label = new QLabel(_widget);
    _margin_slider = new QSlider(Qt::Horizontal, _widget);
    _margin_slider->setRange(0, 15);
    connect(_margin_slider, SIGNAL(valueChanged(int)), this, SLOT(margin_changed(int)));


    _tSource_label = new QLabel(_widget);
    _auto_radioButton = new QRadioButton(_widget);
    _auto_radioButton->setChecked(true);
    _ch0_radioButton = new QRadioButton(_widget);
    _ch1_radioButton = new QRadioButton(_widget);
    _ch0a1_radioButton = new QRadioButton(_widget);
    _ch0o1_radioButton = new QRadioButton(_widget);
    connect(_auto_radioButton, SIGNAL(clicked()), this, SLOT(source_changed()));
    connect(_ch0_radioButton, SIGNAL(clicked()), this, SLOT(source_changed()));
    connect(_ch1_radioButton, SIGNAL(clicked()), this, SLOT(source_changed()));
    connect(_ch0a1_radioButton, SIGNAL(clicked()), this, SLOT(source_changed()));
    connect(_ch0o1_radioButton, SIGNAL(clicked()), this, SLOT(source_changed()));

    _tType_label = new QLabel(_widget);
    _rising_radioButton = new QRadioButton(_widget);
    _rising_radioButton->setChecked(true);
    _falling_radioButton = new QRadioButton(_widget);
    connect(_rising_radioButton, SIGNAL(clicked()), this, SLOT(type_changed()));
    connect(_falling_radioButton, SIGNAL(clicked()), this, SLOT(type_changed()));

    _source_group=new QButtonGroup(_widget);
    _channel_comboBox = new QComboBox(_widget);
    _type_group=new QButtonGroup(_widget);

    _source_group->addButton(_auto_radioButton);
    _source_group->addButton(_ch0_radioButton);
    _source_group->addButton(_ch1_radioButton);
    _source_group->addButton(_ch0a1_radioButton);
    _source_group->addButton(_ch0o1_radioButton);
    _source_group->setId(_auto_radioButton, DSO_TRIGGER_AUTO);
    _source_group->setId(_ch0_radioButton, DSO_TRIGGER_CH0);
    _source_group->setId(_ch1_radioButton, DSO_TRIGGER_CH1);
    _source_group->setId(_ch0a1_radioButton, DSO_TRIGGER_CH0A1);
    _source_group->setId(_ch0o1_radioButton, DSO_TRIGGER_CH0O1);

    _type_group->addButton(_rising_radioButton);
    _type_group->addButton(_falling_radioButton);
    _type_group->setId(_rising_radioButton, DSO_TRIGGER_RISING);
    _type_group->setId(_falling_radioButton, DSO_TRIGGER_FALLING);

    QVBoxLayout *layout = new QVBoxLayout(_widget);
    QGridLayout *gLayout = new QGridLayout();
    gLayout->setVerticalSpacing(5);
    gLayout->addWidget(_position_label, 0, 0);
    gLayout->addWidget(_position_spinBox, 0, 1);
    gLayout->addWidget(new QLabel(tr("%"), _widget), 0, 2);
    gLayout->addWidget(_position_slider, 1, 0, 1, 4);

    gLayout->addWidget(new QLabel(_widget), 2, 0);
    gLayout->addWidget(_tSource_label, 3, 0);
    gLayout->addWidget(_auto_radioButton, 4, 0);
    gLayout->addWidget(_channel_comboBox, 4, 1, 1, 3);
    gLayout->addWidget(_ch0_radioButton, 5, 0);
    gLayout->addWidget(_ch1_radioButton, 5, 1, 1, 3);
    gLayout->addWidget(_ch0a1_radioButton, 6, 0);
    gLayout->addWidget(_ch0o1_radioButton, 6, 1, 1, 3);

    gLayout->addWidget(new QLabel(_widget), 7, 0);
    gLayout->addWidget(_tType_label, 8, 0);
    gLayout->addWidget(_rising_radioButton, 9, 0);
    gLayout->addWidget(_falling_radioButton, 10, 0);

    gLayout->addWidget(new QLabel(_widget), 11, 0);
    gLayout->addWidget(_holdoff_label, 12, 0);
    gLayout->addWidget(_holdoff_spinBox, 12, 1);
    gLayout->addWidget(_holdoff_comboBox, 12, 2);
    gLayout->addWidget(_holdoff_slider, 13, 0, 1, 4);

    gLayout->addWidget(new QLabel(_widget), 14, 0);
    gLayout->addWidget(_margin_label, 15, 0);
    gLayout->addWidget(_margin_slider, 16, 0, 1, 4);

    gLayout->setColumnStretch(4, 1);

    layout->addLayout(gLayout);
    layout->addStretch(1);
    _widget->setLayout(layout);

    this->setWidget(_widget);
    //_widget->setGeometry(0, 0, sizeHint().width(), sizeHint().height());
    _widget->setObjectName("dsoTriggerWidget");

    retranslateUi();
}

DsoTriggerDock::~DsoTriggerDock()
{
}

void DsoTriggerDock::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    else if (event->type() == QEvent::StyleChange)
        reStyle();
    QScrollArea::changeEvent(event);
}

void DsoTriggerDock::retranslateUi()
{
    _position_label->setText(tr("Trigger Position: "));
    _holdoff_label->setText(tr("Hold Off Time: "));
    _margin_label->setText(tr("Noise Sensitivity: "));
    _tSource_label->setText(tr("Trigger Sources: "));
    _tType_label->setText(tr("Trigger Types: "));
    _rising_radioButton->setText(tr("Rising Edge"));
    _falling_radioButton->setText(tr("Falling Edge"));

    _auto_radioButton->setText(tr("Auto"));
    _ch0_radioButton->setText(tr("Channel 0"));
    _ch1_radioButton->setText(tr("Channel 1"));
    _ch0a1_radioButton->setText(tr("Channel 0 && 1"));
    _ch0o1_radioButton->setText(tr("Channel 0 | 1"));
}

void DsoTriggerDock::reStyle()
{
    //QString iconPath = ":/icons/" + qApp->property("Style").toString();
}

void DsoTriggerDock::paintEvent(QPaintEvent *)
{
//    QStyleOption opt;
//    opt.init(this);
//    QPainter p(this);
//    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void DsoTriggerDock::auto_trig(int index)
{
    _source_group->button(DSO_TRIGGER_AUTO)->setChecked(true);
    _channel_comboBox->setCurrentIndex(index);
    source_changed();
    channel_changed(index);
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
    if (_holdoff_comboBox->currentData().toDouble() == 1000000000) {
        _holdoff_slider->setRange(0, 10);
    } else {
        _holdoff_slider->setRange(0, 999);
    }
    holdoff = _holdoff_slider->value() * _holdoff_comboBox->currentData().toDouble() / 10;
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
    int id = _source_group->checkedId();
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
                                            g_variant_new_byte(_channel_comboBox->currentData().toInt()));
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
    int id = _type_group->checkedId();
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
        _position_spinBox->setDisabled(true);
        _position_slider->setDisabled(true);
    } else {
        _position_spinBox->setDisabled(false);
        _position_slider->setDisabled(false);
    }
}

void DsoTriggerDock::init()
{
    if (_session.get_device()->name().contains("virtual")) {
        foreach(QAbstractButton * btn, _source_group->buttons())
            btn->setDisabled(true);
        foreach(QAbstractButton * btn, _type_group->buttons())
            btn->setDisabled(true);
        _holdoff_slider->setDisabled(true);
        _holdoff_spinBox->setDisabled(true);
        _holdoff_comboBox->setDisabled(true);
        _margin_slider->setDisabled(true);
        _channel_comboBox->setDisabled(true);
        return;
    } else {
        foreach(QAbstractButton * btn, _source_group->buttons())
            btn->setDisabled(false);
        foreach(QAbstractButton * btn, _type_group->buttons())
            btn->setDisabled(false);
        _holdoff_slider->setDisabled(false);
        _holdoff_spinBox->setDisabled(false);
        _holdoff_comboBox->setDisabled(false);
        _margin_slider->setDisabled(false);
        _channel_comboBox->setDisabled(false);
    }

    // TRIGGERPOS
    GVariant* gvar = _session.get_device()->get_config(NULL, NULL,
                                            SR_CONF_HORIZ_TRIGGERPOS);
    if (gvar != NULL) {
        uint16_t pos = g_variant_get_byte(gvar);
        g_variant_unref(gvar);
        _position_slider->setValue(pos);
    }

    gvar = _session.get_device()->get_config(NULL, NULL,
                                                SR_CONF_TRIGGER_SOURCE);
    if (gvar != NULL) {
        uint8_t src = g_variant_get_byte(gvar);
        g_variant_unref(gvar);
        _source_group->button(src)->setChecked(true);
    }

    // setup _channel_comboBox
    disconnect(_channel_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(channel_changed(int)));
    _channel_comboBox->clear();
    BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _session.get_signals()) {
        boost::shared_ptr<view::DsoSignal> dsoSig;
        if ((dsoSig = dynamic_pointer_cast<view::DsoSignal>(s))) {
            _channel_comboBox->addItem(dsoSig->get_name(), QVariant::fromValue(dsoSig->get_index()));
        }
    }
    gvar = _session.get_device()->get_config(NULL, NULL,
                                                SR_CONF_TRIGGER_CHANNEL);
    if (gvar != NULL) {
        uint8_t src = g_variant_get_byte(gvar);
        g_variant_unref(gvar);
        for (int i = 0; i < _channel_comboBox->count(); i++) {
            if (src == _channel_comboBox->itemData(i).toInt()) {
                _channel_comboBox->setCurrentIndex(i);
                break;
            }
        }
    }
    connect(_channel_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(channel_changed(int)));

    gvar = _session.get_device()->get_config(NULL, NULL,
                                                SR_CONF_TRIGGER_SLOPE);
    if (gvar != NULL) {
        uint8_t slope = g_variant_get_byte(gvar);
        g_variant_unref(gvar);
        _type_group->button(slope)->setChecked(true);
    }

    disconnect(_holdoff_slider, SIGNAL(valueChanged(int)), this, SLOT(hold_changed(int)));
    disconnect(_holdoff_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(hold_changed(int)));
    gvar = _session.get_device()->get_config(NULL, NULL,
                                                SR_CONF_TRIGGER_HOLDOFF);
    if (gvar != NULL) {
        uint64_t holdoff = g_variant_get_uint64(gvar);
        g_variant_unref(gvar);
        for (int i = _holdoff_comboBox->count()-1; i >= 0; i--) {
            if (holdoff >= _holdoff_comboBox->itemData(i).toDouble()) {
                _holdoff_comboBox->setCurrentIndex(i);
                break;
            }
        }
        if (_holdoff_comboBox->currentData().toDouble() == 1000000000) {
            _holdoff_slider->setRange(0, 10);
        } else {
            _holdoff_slider->setRange(0, 999);
        }
        _holdoff_spinBox->setValue(holdoff * 10.0/_holdoff_comboBox->currentData().toDouble());
    }
    connect(_holdoff_slider, SIGNAL(valueChanged(int)), this, SLOT(hold_changed(int)));
    connect(_holdoff_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(hold_changed(int)));

    disconnect(_margin_slider, SIGNAL(valueChanged(int)), this, SLOT(margin_changed(int)));
    gvar = _session.get_device()->get_config(NULL, NULL,
                                                SR_CONF_TRIGGER_MARGIN);
    if (gvar != NULL) {
        uint8_t margin = g_variant_get_byte(gvar);
        g_variant_unref(gvar);
        _margin_slider->setValue(margin);
    }
    connect(_margin_slider, SIGNAL(valueChanged(int)), this, SLOT(margin_changed(int)));
}

} // namespace dock
} // namespace pv
