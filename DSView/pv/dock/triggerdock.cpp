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


#include "triggerdock.h"
#include "../sigsession.h"
#include "../device/devinst.h"
#include "../dialogs/dsmessagebox.h"

#include <QObject>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QRegExpValidator>
#include <QSplitter>

#include "libsigrok4DSL/libsigrok.h"

namespace pv {
namespace dock {

const int TriggerDock::MinTrigPosition = 1;

TriggerDock::TriggerDock(QWidget *parent, SigSession &session) :
    QScrollArea(parent),
    _session(session)
{
    int i;

    _widget = new QWidget(this);

    QFont font("Monaco");
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);

    simple_radioButton = new QRadioButton(tr("Simple Trigger"), _widget);
    simple_radioButton->setChecked(true);
    adv_radioButton = new QRadioButton(tr("Advanced Trigger"), _widget);

    position_label = new QLabel(tr("Trigger Position: "), _widget);
    position_spinBox = new QSpinBox(_widget);
    position_spinBox->setRange(MinTrigPosition, 99);
    position_spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
    position_slider = new QSlider(Qt::Horizontal, _widget);
    position_slider->setRange(MinTrigPosition, 99);
    connect(position_slider, SIGNAL(valueChanged(int)), position_spinBox, SLOT(setValue(int)));
    connect(position_spinBox, SIGNAL(valueChanged(int)), position_slider, SLOT(setValue(int)));

    stages_label = new QLabel(tr("Total Trigger Stages: "), _widget);
    stages_label->setDisabled(true);
    stages_comboBox = new QComboBox(_widget);
    for (i = 1; i <= TriggerStages; i++)
        stages_comboBox->addItem(QString::number(i));
    //stages_comboBox->setCurrentIndex(stages_comboBox->count() - 1);
    stages_comboBox->setDisabled(true);

    stage_tabWidget = new QTabWidget(_widget);
    stage_tabWidget->setTabPosition(QTabWidget::East);
    //stage_tabWidget->setDisabled(true);
    stage_tabWidget->setUsesScrollButtons(false);

    QRegExp value_rx("[10XRFCxrfc ]+");
    QValidator *value_validator = new QRegExpValidator(value_rx, _widget);
    for (i = 0; i < TriggerStages; i++) {
        QComboBox *_logic_comboBox = new QComboBox(_widget);
        _logic_comboBox->addItem(tr("Or"));
        _logic_comboBox->addItem(tr("And"));
        _logic_comboBox->setCurrentIndex(1);
        _logic_comboBox_list.push_back(_logic_comboBox);

        QLineEdit *_value0_lineEdit = new QLineEdit("X X X X X X X X X X X X X X X X", _widget);
        _value0_lineEdit->setFont(font);
        _value0_lineEdit->setValidator(value_validator);
        _value0_lineEdit->setMaxLength(TriggerProbes * 2 - 1);
        _value0_lineEdit->setInputMask("X X X X X X X X X X X X X X X X");
		_value0_lineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        _value0_lineEdit_list.push_back(_value0_lineEdit);
        QSpinBox *_count0_spinBox = new QSpinBox(_widget);
        _count0_spinBox->setRange(1, INT32_MAX);
        _count0_spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
        _count0_spinBox_list.push_back(_count0_spinBox);
        QComboBox *_inv0_comboBox = new QComboBox(_widget);
        _inv0_comboBox->addItem(tr("=="));
        _inv0_comboBox->addItem(tr("!="));
        _inv0_comboBox_list.push_back(_inv0_comboBox);

        QLineEdit *_value1_lineEdit = new QLineEdit("X X X X X X X X X X X X X X X X", _widget);
        _value1_lineEdit->setFont(font);
        _value1_lineEdit->setValidator(value_validator);
        _value1_lineEdit->setMaxLength(TriggerProbes * 2 - 1);
        _value1_lineEdit->setInputMask("X X X X X X X X X X X X X X X X");
		_value1_lineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        _value1_lineEdit_list.push_back(_value1_lineEdit);
        QSpinBox *_count1_spinBox = new QSpinBox(_widget);
        _count1_spinBox->setRange(1, INT32_MAX);
        _count1_spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
        _count1_spinBox_list.push_back(_count1_spinBox);
        QComboBox *_inv1_comboBox = new QComboBox(_widget);
        _inv1_comboBox->addItem(tr("=="));
        _inv1_comboBox->addItem(tr("!="));
        _inv1_comboBox_list.push_back(_inv1_comboBox);

        QLabel *value_exp_label = new QLabel("1 1 1 1 1 1\n5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 ", _widget);
        QLabel *inv_exp_label = new QLabel("Inv", _widget);
        QLabel *count_exp_label = new QLabel("Counter", _widget);
        value_exp_label->setFont(font);

        QVBoxLayout *stage_layout = new QVBoxLayout();
        QGridLayout *stage_glayout = new QGridLayout();
        stage_glayout->setVerticalSpacing(5);
        stage_glayout->addWidget(value_exp_label, 1, 0);
        stage_glayout->addWidget(inv_exp_label, 1, 1);
        stage_glayout->addWidget(count_exp_label, 1, 2);
        stage_glayout->addWidget(_value0_lineEdit, 2, 0);
        stage_glayout->addWidget(_inv0_comboBox, 2, 1);
        stage_glayout->addWidget(_count0_spinBox, 2, 2);
        stage_glayout->addWidget(_logic_comboBox, 2, 3);
        stage_glayout->addWidget(_value1_lineEdit, 3, 0);
        stage_glayout->addWidget(_inv1_comboBox, 3, 1);
        stage_glayout->addWidget(_count1_spinBox, 3, 2);
        stage_layout->addLayout(stage_glayout);
        stage_layout->addSpacing(20);
        stage_layout->addWidget(new QLabel(tr("X: Don't care\n0: Low level\n1: High level\nR: Rising edge\nF: Falling edge\nC: Rising/Falling edge")));
        stage_layout->addStretch(1);

        QGroupBox *_stage_groupBox = new QGroupBox(tr("Stage")+QString::number(i), _widget);
        _stage_groupBox->setFlat(true);
        _stage_groupBox->setLayout(stage_layout);
        _stage_groupBox_list.push_back(_stage_groupBox);

        stage_tabWidget->addTab((QWidget *)_stage_groupBox, QString::number(i));

        connect(_value0_lineEdit, SIGNAL(editingFinished()), this, SLOT(value_changed()));
        connect(_value1_lineEdit, SIGNAL(editingFinished()), this, SLOT(value_changed()));
    }

    _serial_start_label = new QLabel(tr("Start Flag: "), _widget);
    _serial_start_lineEdit = new QLineEdit("X X X X X X X X X X X X X X X X", _widget);
    _serial_start_lineEdit->setFont(font);
    _serial_start_lineEdit->setValidator(value_validator);
    _serial_start_lineEdit->setMaxLength(TriggerProbes * 2 - 1);
    _serial_start_lineEdit->setInputMask("X X X X X X X X X X X X X X X X");
    _serial_start_lineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    _serial_stop_label = new QLabel(tr("Stop Flag: "), _widget);
    _serial_stop_lineEdit = new QLineEdit("X X X X X X X X X X X X X X X X", _widget);
    _serial_stop_lineEdit->setFont(font);
    _serial_stop_lineEdit->setValidator(value_validator);
    _serial_stop_lineEdit->setMaxLength(TriggerProbes * 2 - 1);
    _serial_stop_lineEdit->setInputMask("X X X X X X X X X X X X X X X X");
    _serial_stop_lineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    _serial_edge_label = new QLabel(tr("Clock Flag: "), _widget);
    _serial_edge_lineEdit = new QLineEdit("X X X X X X X X X X X X X X X X", _widget);
    _serial_edge_lineEdit->setFont(font);
    _serial_edge_lineEdit->setValidator(value_validator);
    _serial_edge_lineEdit->setMaxLength(TriggerProbes * 2 - 1);
    _serial_edge_lineEdit->setInputMask("X X X X X X X X X X X X X X X X");
    _serial_edge_lineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    _serial_data_lable = new QLabel(tr("Data Channel: "), _widget);
    _serial_data_comboBox = new QComboBox(_widget);
    for(i = 0; i < TriggerProbes; i++)
        _serial_data_comboBox->addItem(QString::number(i));

    _serial_value_lable = new QLabel(tr("Data Value: "), _widget);
    _serial_value_lineEdit = new QLineEdit("X X X X X X X X X X X X X X X X", _widget);
    _serial_value_lineEdit->setFont(font);
    _serial_value_lineEdit->setValidator(value_validator);
    _serial_value_lineEdit->setMaxLength(TriggerProbes * 2 - 1);
    _serial_value_lineEdit->setInputMask("X X X X X X X X X X X X X X X X");
    _serial_value_lineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    _serial_vcnt_spinBox = new QSpinBox(_widget);
    _serial_vcnt_spinBox->setRange(1, INT32_MAX);
    _serial_vcnt_spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);

    QLabel *serial_value_exp_label = new QLabel("1 1 1 1 1 1\n5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0", _widget);
    serial_value_exp_label->setFont(font);

    QVBoxLayout *serial_layout = new QVBoxLayout();
    QGridLayout *serial_glayout = new QGridLayout();
    serial_glayout->setVerticalSpacing(5);
    serial_glayout->addWidget(serial_value_exp_label, 1, 1, 1, 3);
    serial_glayout->addWidget(_serial_start_label, 2, 0);
    serial_glayout->addWidget(_serial_start_lineEdit, 2, 1, 1, 3);
    serial_glayout->addWidget(new QLabel(_widget), 2, 4);
    serial_glayout->addWidget(_serial_stop_label, 3, 0);
    serial_glayout->addWidget(_serial_stop_lineEdit, 3, 1, 1, 3);
    serial_glayout->addWidget(_serial_edge_label, 4, 0);
    serial_glayout->addWidget(_serial_edge_lineEdit, 4, 1, 1, 3);

    serial_glayout->addWidget(new QLabel(_widget), 5, 0, 1, 5);
    serial_glayout->addWidget(_serial_data_lable, 6, 0);
    serial_glayout->addWidget(_serial_data_comboBox, 6, 1);
    serial_glayout->addWidget(new QLabel(tr("counter"), _widget), 6, 4);
    serial_glayout->addWidget(_serial_value_lable, 7, 0);
    serial_glayout->addWidget(_serial_value_lineEdit, 7, 1, 1, 3);
    serial_glayout->addWidget(_serial_vcnt_spinBox, 7, 4);
    serial_glayout->addWidget(new QLabel(_widget), 7, 5);
    serial_layout->addLayout(serial_glayout);
    serial_layout->addSpacing(20);
    serial_layout->addWidget(new QLabel(tr("X: Don't care\n0: Low level\n1: High level\nR: Rising edge\nF: Falling edge\nC: Rising/Falling edge")));
    serial_layout->addStretch(1);

    _serial_groupBox = new QGroupBox(tr("Serial Trigger"), _widget);
    _serial_groupBox->setFlat(true);
    _serial_groupBox->setLayout(serial_layout);
    //_serial_groupBox->setDisabled(true);

    connect(_serial_start_lineEdit, SIGNAL(editingFinished()), this, SLOT(value_changed()));
    connect(_serial_stop_lineEdit, SIGNAL(editingFinished()), this, SLOT(value_changed()));
    connect(_serial_edge_lineEdit, SIGNAL(editingFinished()), this, SLOT(value_changed()));
    connect(_serial_value_lineEdit, SIGNAL(editingFinished()), this, SLOT(value_changed()));


    _adv_tabWidget = new QTabWidget(_widget);
    _adv_tabWidget->setTabPosition(QTabWidget::North);
    _adv_tabWidget->setDisabled(true);
    _adv_tabWidget->addTab((QWidget *)stage_tabWidget, tr("Stage Trigger"));
    _adv_tabWidget->addTab((QWidget *)_serial_groupBox, tr("Serial Trigger"));

    connect(simple_radioButton, SIGNAL(clicked()), this, SLOT(simple_trigger()));
    connect(adv_radioButton, SIGNAL(clicked()), this, SLOT(adv_trigger()));
    connect(stages_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(widget_enable(int)));


    QVBoxLayout *layout = new QVBoxLayout(_widget);
    QGridLayout *gLayout = new QGridLayout();
    gLayout->setVerticalSpacing(5);
    gLayout->addWidget(simple_radioButton, 0, 0);
    gLayout->addWidget(adv_radioButton, 1, 0);
    gLayout->addWidget(position_label, 2, 0);
    gLayout->addWidget(position_spinBox, 2, 1);
    gLayout->addWidget(new QLabel(_widget), 2, 2);
    gLayout->addWidget(position_slider, 3, 0, 1, 3);
    gLayout->addWidget(stages_label, 4, 0);
    gLayout->addWidget(stages_comboBox, 4, 1);
    gLayout->addWidget(new QLabel(_widget), 4, 2);
    gLayout->setColumnStretch(2, 1);

    layout->addLayout(gLayout);
    layout->addWidget(_adv_tabWidget);
    layout->addStretch(1);
    _widget->setLayout(layout);

    this->setWidget(_widget);
    //_widget->setGeometry(0, 0, sizeHint().width(), 1000);
    _widget->setObjectName("triggerWidget");
}

TriggerDock::~TriggerDock()
{
}

void TriggerDock::paintEvent(QPaintEvent *)
{
//    QStyleOption opt;
//    opt.init(this);
//    QPainter p(this);
//    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void TriggerDock::simple_trigger()
{
    stages_label->setDisabled(true);
    stages_comboBox->setDisabled(true);
    _adv_tabWidget->setDisabled(true);
}

void TriggerDock::adv_trigger()
{
    if (_session.get_device()->name() == "DSLogic") {
        bool stream = false;
        GVariant *gvar = _session.get_device()->get_config(NULL, NULL, SR_CONF_STREAM);
        if (gvar != NULL) {
            stream = g_variant_get_boolean(gvar);
            g_variant_unref(gvar);
        }
        if (stream) {
            dialogs::DSMessageBox msg(this);
            msg.mBox()->setText(tr("Trigger"));
            msg.mBox()->setInformativeText(tr("Stream Mode Don't Support Advanced Trigger!"));
            msg.mBox()->setStandardButtons(QMessageBox::Ok);
            msg.mBox()->setIcon(QMessageBox::Warning);
            msg.exec();
            simple_radioButton->setChecked(true);
        } else {
            widget_enable(0);
        }
    } else {
        dialogs::DSMessageBox msg(this);
        msg.mBox()->setText(tr("Trigger"));
        msg.mBox()->setInformativeText(tr("Advanced Trigger need DSLogic Hardware Support!"));
        msg.mBox()->setStandardButtons(QMessageBox::Ok);
        msg.mBox()->setIcon(QMessageBox::Warning);
        msg.exec();
        simple_radioButton->setChecked(true);
    }
}

void TriggerDock::widget_enable(int index)
{
    (void) index;

    int i;
    int enable_stages;
    stages_label->setDisabled(false);
    stages_comboBox->setVisible(true);
    stages_comboBox->setDisabled(false);
    _adv_tabWidget->setDisabled(false);
    enable_stages = stages_comboBox->currentText().toInt();
    for (i = 0; i < enable_stages; i++) {
        stage_tabWidget->setTabEnabled(i, true);
    }
    for (i = enable_stages; i < TriggerStages; i++) {
          stage_tabWidget->setTabEnabled(i, false);
    }
}

void TriggerDock::value_changed()
{
    QLineEdit* sc=dynamic_cast<QLineEdit*>(sender());
    if(sc != NULL)
        sc->setText(sc->text().toUpper());
}

void TriggerDock::device_change()
{
    uint64_t max_hd_depth;
    bool stream = false;
    uint8_t maxRange;
    uint64_t sample_limits;
    GVariant *gvar = _session.get_device()->get_config(NULL, NULL, SR_CONF_MAX_LOGIC_SAMPLELIMITS);
    if (gvar != NULL) {
        max_hd_depth = g_variant_get_uint64(gvar);
        g_variant_unref(gvar);

        if (_session.get_device()->dev_inst()->mode == LOGIC) {
            sample_limits = _session.get_device()->get_sample_limit();
            if (max_hd_depth >= sample_limits)
                maxRange = 99;
            else
                maxRange = max_hd_depth*70 / sample_limits;
            position_spinBox->setRange(MinTrigPosition, maxRange);
            position_slider->setRange(MinTrigPosition, maxRange);



            gvar = _session.get_device()->get_config(NULL, NULL, SR_CONF_STREAM);
            if (gvar != NULL) {
                stream = g_variant_get_boolean(gvar);
                g_variant_unref(gvar);
            }

            if (_session.get_device()->name().contains("virtual") ||
                stream) {
                simple_radioButton->setChecked(true);
                simple_trigger();
            }
        }
    }
}

bool TriggerDock::commit_trigger()
{
    // trigger position update
    ds_trigger_set_pos(position_slider->value());

    // trigger mode update
    if (simple_radioButton->isChecked()) {
        ds_trigger_set_mode(SIMPLE_TRIGGER);
        return 0;
    } else {
        ds_trigger_set_en(true);
        if (_adv_tabWidget->currentIndex() == 0)
            ds_trigger_set_mode(ADV_TRIGGER);
        else if (_adv_tabWidget->currentIndex() == 1)
            ds_trigger_set_mode(SERIAL_TRIGGER);

        // trigger stage update
        ds_trigger_set_stage(stages_comboBox->currentText().toInt());

        int i;
        // trigger value update
        if (_adv_tabWidget->currentIndex() == 0) {
            for (i = 0; i < stages_comboBox->currentText().toInt(); i++) {
                ds_trigger_stage_set_value(i, TriggerProbes,
                                     _value0_lineEdit_list.at(i)->text().toLocal8Bit().data(),
                                     _value1_lineEdit_list.at(i)->text().toLocal8Bit().data());
            }
        } else if(_adv_tabWidget->currentIndex() == 1){
            ds_trigger_stage_set_value(0, TriggerProbes,
                                 _serial_start_lineEdit->text().toLocal8Bit().data(),
                                 _value1_lineEdit_list.at(0)->text().toLocal8Bit().data());
            ds_trigger_stage_set_value(1, TriggerProbes,
                                 _serial_stop_lineEdit->text().toLocal8Bit().data(),
                                 _value1_lineEdit_list.at(1)->text().toLocal8Bit().data());
            ds_trigger_stage_set_value(2, TriggerProbes,
                                 _serial_edge_lineEdit->text().toLocal8Bit().data(),
                                 _value1_lineEdit_list.at(2)->text().toLocal8Bit().data());
            //_serial_data_comboBox
            const int data_channel = _serial_data_comboBox->currentText().toInt();
            char channel[31];
            for(i = 0; i < 31; i++){
                if (i == (30 - 2*data_channel))
                    channel[i] = '1';
                else if (i%2 == 0)
                    channel[i] = '0';
                else
                    channel[i] = ' ';
            }
            ds_trigger_stage_set_value(3, TriggerProbes,
                                 channel,
                                 _value1_lineEdit_list.at(3)->text().toLocal8Bit().data());
            ds_trigger_stage_set_value(4, TriggerProbes,
                                 _serial_value_lineEdit->text().toLocal8Bit().data(),
                                 _value1_lineEdit_list.at(4)->text().toLocal8Bit().data());
        }

        // trigger logic update
        for (i = 0; i < stages_comboBox->currentText().toInt(); i++) {
            ds_trigger_stage_set_logic(i, TriggerProbes,
                                 _logic_comboBox_list.at(i)->currentIndex());
        }

        // trigger inv update
        for (i = 0; i < stages_comboBox->currentText().toInt(); i++) {
            ds_trigger_stage_set_inv(i, TriggerProbes,
                                 _inv0_comboBox_list.at(i)->currentIndex(),
                                 _inv1_comboBox_list.at(i)->currentIndex());
        }

        // trigger count update
        if (_adv_tabWidget->currentIndex() == 0) {
            for (i = 0; i < stages_comboBox->currentText().toInt(); i++) {
                ds_trigger_stage_set_count(i, TriggerProbes,
                                           _count0_spinBox_list.at(i)->value() - 1,
                                           _count1_spinBox_list.at(i)->value() - 1);
            }
        } else if(_adv_tabWidget->currentIndex() == 1){
            ds_trigger_stage_set_count(4, TriggerProbes,
                                       _serial_vcnt_spinBox->value() - 1,
                                       0);
        }
        return 1;
    }
}

void TriggerDock::init()
{
    // TRIGGERPOS
    //uint16_t pos = ds_trigger_get_pos();
    //position_slider->setValue(pos);
}

QJsonObject TriggerDock::get_session()
{
    QJsonObject trigSes;
    trigSes["triggerMode"] = adv_radioButton->isChecked() ? 1 : 0;
    trigSes["triggerPos"] = position_slider->value();
    trigSes["triggerStages"] = stages_comboBox->currentIndex();
    trigSes["triggerSerial"] = _adv_tabWidget->currentIndex();

    for (int i = 0; i < stages_comboBox->count(); i++) {
        QString value0_str = "triggerValue0" + QString::number(i);
        QString inv0_str = "triggerInv0" + QString::number(i);
        QString count0_str = "triggerCount0" + QString::number(i);
        QString value1_str = "triggerValue1" + QString::number(i);
        QString inv1_str = "triggerInv1" + QString::number(i);
        QString count1_str = "triggerCount1" + QString::number(i);
        QString logic_str = "triggerLogic" + QString::number(i);
        trigSes[value0_str] = _value0_lineEdit_list.at(i)->text();
        trigSes[value1_str] = _value1_lineEdit_list.at(i)->text();
        trigSes[inv0_str] = _inv0_comboBox_list.at(i)->currentIndex();
        trigSes[inv1_str] = _inv1_comboBox_list.at(i)->currentIndex();
        trigSes[count0_str] = _count0_spinBox_list.at(i)->value();
        trigSes[count1_str] = _count1_spinBox_list.at(i)->value();
        trigSes[logic_str] = _logic_comboBox_list.at(i)->currentIndex();
    }

    trigSes["triggerStart"] = _serial_start_lineEdit->text();
    trigSes["triggerStop"] = _serial_stop_lineEdit->text();
    trigSes["triggerClock"] = _serial_edge_lineEdit->text();
    trigSes["triggerChannel"] = _serial_data_comboBox->currentIndex();
    trigSes["triggerData"] = _serial_value_lineEdit->text();
    trigSes["triggerVcnt"] = _serial_vcnt_spinBox->value();

    return trigSes;
}

void TriggerDock::set_session(QJsonObject ses)
{
    position_slider->setValue(ses["triggerPos"].toDouble());
    stages_comboBox->setCurrentIndex(ses["triggerStages"].toDouble());
    _adv_tabWidget->setCurrentIndex(ses["triggerSerial"].toDouble());
    if (ses["triggerMode"].toDouble() == 0)
        simple_radioButton->click();
    else
        adv_radioButton->click();

    for (int i = 0; i < stages_comboBox->count(); i++) {
        QString value0_str = "triggerValue0" + QString::number(i);
        QString inv0_str = "triggerInv0" + QString::number(i);
        QString count0_str = "triggerCount0" + QString::number(i);
        QString value1_str = "triggerValue1" + QString::number(i);
        QString inv1_str = "triggerInv1" + QString::number(i);
        QString count1_str = "triggerCount1" + QString::number(i);
        QString logic_str = "triggerLogic" + QString::number(i);
        _value0_lineEdit_list.at(i)->setText(ses[value0_str].toString());
        _value1_lineEdit_list.at(i)->setText(ses[value1_str].toString());
        _inv0_comboBox_list.at(i)->setCurrentIndex(ses[inv0_str].toDouble());
        _inv1_comboBox_list.at(i)->setCurrentIndex(ses[inv1_str].toDouble());
        _count0_spinBox_list.at(i)->setValue(ses[count0_str].toDouble());
        _count1_spinBox_list.at(i)->setValue(ses[count1_str].toDouble());
        _logic_comboBox_list.at(i)->setCurrentIndex(ses[logic_str].toDouble());
    }

    _serial_start_lineEdit->setText(ses["triggerStart"].toString());
    _serial_stop_lineEdit->setText(ses["triggerStop"].toString());
    _serial_edge_lineEdit->setText(ses["triggerClock"].toString());
    _serial_data_comboBox->setCurrentIndex(ses["triggerChannel"].toDouble());
    _serial_value_lineEdit->setText(ses["triggerData"].toString());
    _serial_vcnt_spinBox->setValue(ses["triggerVcnt"].toDouble());
}

} // namespace dock
} // namespace pv
