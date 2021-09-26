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
#include "../view/view.h"

#include <QObject>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QRegExpValidator>
#include <QSplitter>
#include <QInputMethodEvent>

#include "libsigrok4DSL/libsigrok.h"

namespace pv {
namespace dock {

const int TriggerDock::MinTrigPosition = 1;

TriggerDock::TriggerDock(QWidget *parent, SigSession &session) :
    QScrollArea(parent),
    _session(session)
{
    _cur_ch_num = 16;
    if (_session.get_device()) {
        GVariant *gvar = _session.get_device()->get_config(NULL, NULL, SR_CONF_TOTAL_CH_NUM);
        if (gvar != NULL) {
            _cur_ch_num = g_variant_get_int16(gvar);
            g_variant_unref(gvar);
        }
    }

    _widget = new QWidget(this);
    _simple_radioButton = new QRadioButton(_widget);
    _simple_radioButton->setChecked(true);
    _adv_radioButton = new QRadioButton(_widget);

    _position_label = new QLabel(_widget);
    _position_spinBox = new QSpinBox(_widget);
    _position_spinBox->setRange(MinTrigPosition, DS_MAX_TRIG_PERCENT);
    _position_spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
    _position_slider = new QSlider(Qt::Horizontal, _widget);
    _position_slider->setRange(MinTrigPosition, DS_MAX_TRIG_PERCENT);
    connect(_position_slider, SIGNAL(valueChanged(int)), _position_spinBox, SLOT(setValue(int)));
    connect(_position_spinBox, SIGNAL(valueChanged(int)), _position_slider, SLOT(setValue(int)));

    _stages_label = new QLabel(_widget);
    _stages_label->setDisabled(true);
    stages_comboBox = new QComboBox(_widget);
    for (int i = 1; i <= TriggerStages; i++)
        stages_comboBox->addItem(QString::number(i));
    //stages_comboBox->setCurrentIndex(stages_comboBox->count() - 1);
    stages_comboBox->setDisabled(true);

    _adv_tabWidget = new QTabWidget(_widget);
    _adv_tabWidget->setTabPosition(QTabWidget::North);
    _adv_tabWidget->setDisabled(true);
    setup_adv_tab();

    connect(_simple_radioButton, SIGNAL(clicked()), this, SLOT(simple_trigger()));
    connect(_adv_radioButton, SIGNAL(clicked()), this, SLOT(adv_trigger()));
    connect(stages_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(widget_enable(int)));


    QVBoxLayout *layout = new QVBoxLayout(_widget);
    QGridLayout *gLayout = new QGridLayout();
    gLayout->setVerticalSpacing(5);
    gLayout->addWidget(_simple_radioButton, 0, 0);
    gLayout->addWidget(_adv_radioButton, 1, 0);
    gLayout->addWidget(_position_label, 2, 0);
    gLayout->addWidget(_position_spinBox, 2, 1);
    gLayout->addWidget(new QLabel(tr("%"), _widget), 2, 2);
    gLayout->addWidget(_position_slider, 3, 0, 1, 3);
    gLayout->addWidget(_stages_label, 4, 0);
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

    retranslateUi();
}

TriggerDock::~TriggerDock()
{
}

void TriggerDock::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    else if (event->type() == QEvent::StyleChange)
        reStyle();
    QScrollArea::changeEvent(event);
}

void TriggerDock::retranslateUi()
{
    _simple_radioButton->setText(tr("Simple Trigger"));
    _adv_radioButton->setText(tr("Advanced Trigger"));
    _position_label->setText(tr("Trigger Position: "));
    _stages_label->setText(tr("Total Trigger Stages: "));
    _serial_start_label->setText(tr("Start Flag: "));
    _serial_stop_label->setText(tr("Stop Flag: "));
    _serial_edge_label->setText(tr("Clock Flag: "));
    _serial_data_label->setText(tr("Data Channel: "));
    _serial_value_label->setText(tr("Data Value: "));
    _serial_groupBox->setTitle(tr("Serial Trigger"));

    _adv_tabWidget->setTabText(0, tr("Stage Trigger"));
    _adv_tabWidget->setTabText(1, tr("Serial Trigger"));
    _serial_note_label->setText(tr("X: Don't care\n0: Low level\n1: High level\nR: Rising edge\nF: Falling edge\nC: Rising/Falling edge"));
    _data_bits_label->setText(tr("Data Bits"));

    for (int i = 0; i < _inv_exp_label_list.length(); i++)
        _inv_exp_label_list.at(i)->setText(tr("Inv"));

    for (int i = 0; i < _count_exp_label_list.length(); i++)
        _count_exp_label_list.at(i)->setText(tr("Counter"));

    for (int i = 0; i < _contiguous_label_list.length(); i++)
        _contiguous_label_list.at(i)->setText(tr("Contiguous"));

    for (int i = 0; i < _stage_groupBox_list.length(); i++)
        _stage_groupBox_list.at(i)->setTitle(tr("Stage")+QString::number(i));

    for (int i = 0; i < _stage_note_label_list.length(); i++)
        _stage_note_label_list.at(i)->setText(tr("X: Don't care\n0: Low level\n1: High level\nR: Rising edge\nF: Falling edge\nC: Rising/Falling edge"));
}

void TriggerDock::reStyle()
{
    //QString iconPath = ":/icons/" + qApp->property("Style").toString();
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
    _stages_label->setDisabled(true);
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
            _simple_radioButton->setChecked(true);
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
        _simple_radioButton->setChecked(true);
    }
}

void TriggerDock::widget_enable(int index)
{
    (void) index;

    int enable_stages;
    _stages_label->setDisabled(false);
    stages_comboBox->setVisible(true);
    stages_comboBox->setDisabled(false);
    _adv_tabWidget->setDisabled(false);
    enable_stages = stages_comboBox->currentText().toInt();
    for (int i = 0; i < enable_stages; i++) {
        _stage_tabWidget->setTabEnabled(i, true);
    }
    for (int i = enable_stages; i < TriggerStages; i++) {
          _stage_tabWidget->setTabEnabled(i, false);
    }
}

void TriggerDock::value_changed()
{
    QLineEdit* sc=dynamic_cast<QLineEdit*>(sender());
    if(sc != NULL) {
        for (int i = 0; i < TriggerProbes*2-1; i++) {
            if ((i >= sc->text().size()) || (i % 2 == 0 && sc->text().at(i) == ' ')) {
                sc->setText(sc->text().insert(i, 'X'));
                i++;
            }
        }
        sc->setText(sc->text().toUpper());
        lineEdit_highlight(sc);
    }
}

void TriggerDock::device_updated()
{
    uint64_t hw_depth;
    bool stream = false;
    uint8_t maxRange;
    uint64_t sample_limits;
    GVariant *gvar = _session.get_device()->get_config(NULL, NULL, SR_CONF_HW_DEPTH);
    if (gvar != NULL) {
        hw_depth = g_variant_get_uint64(gvar);
        g_variant_unref(gvar);

        if (_session.get_device()->dev_inst()->mode == LOGIC) {

            gvar = _session.get_device()->get_config(NULL, NULL, SR_CONF_STREAM);
            if (gvar != NULL) {
                stream = g_variant_get_boolean(gvar);
                g_variant_unref(gvar);
            }

            sample_limits = _session.get_device()->get_sample_limit();
            if (stream)
                maxRange = 1;
            else if (hw_depth >= sample_limits)
                maxRange = DS_MAX_TRIG_PERCENT;
            else
                maxRange = ceil(hw_depth * DS_MAX_TRIG_PERCENT / sample_limits);
            _position_spinBox->setRange(MinTrigPosition, maxRange);
            _position_slider->setRange(MinTrigPosition, maxRange);

            if (_session.get_device()->name().contains("virtual") ||
                stream) {
                _simple_radioButton->setChecked(true);
                simple_trigger();
            }
        }
    }

    gvar = _session.get_device()->get_config(NULL, NULL, SR_CONF_TOTAL_CH_NUM);
    if (gvar != NULL) {
        int ch_num = g_variant_get_int16(gvar);
        g_variant_unref(gvar);

        if (ch_num != _cur_ch_num) {
            _cur_ch_num = ch_num;
            setup_adv_tab();
            retranslateUi();
        }
    }
}

bool TriggerDock::commit_trigger()
{
    // trigger position update
    ds_trigger_set_pos(_position_slider->value());

    // trigger mode update
    if (_simple_radioButton->isChecked()) {
        ds_trigger_set_mode(SIMPLE_TRIGGER);
        return 0;
    } else {
        ds_trigger_set_en(true);
        if (_adv_tabWidget->currentIndex() == 0)
            ds_trigger_set_mode(ADV_TRIGGER);
        else if (_adv_tabWidget->currentIndex() == 1)
            ds_trigger_set_mode(SERIAL_TRIGGER);

        // trigger stage update
        ds_trigger_set_stage(stages_comboBox->currentText().toInt() - 1);

        // trigger value update
        if (_adv_tabWidget->currentIndex() == 0) {
            for (int i = 0; i < stages_comboBox->currentText().toInt(); i++) {
                QString value0_str, value1_str;
                if (_cur_ch_num == 32) {
                    value0_str = _value0_ext32_lineEdit_list.at(i)->text() + " " + _value0_lineEdit_list.at(i)->text();
                    value1_str = _value1_ext32_lineEdit_list.at(i)->text() + " " + _value1_lineEdit_list.at(i)->text();
                } else {
                    value0_str = _value0_lineEdit_list.at(i)->text();
                    value1_str = _value1_lineEdit_list.at(i)->text();
                }
                ds_trigger_stage_set_value(i, _cur_ch_num, value0_str.toLocal8Bit().data(),
                                                           value1_str.toLocal8Bit().data());
            }
        } else if(_adv_tabWidget->currentIndex() == 1){
            QString start_str, stop_str, edge_str, comp_str;
            if (_cur_ch_num == 32) {
                start_str = _serial_start_ext32_lineEdit->text() + " " + _serial_start_lineEdit->text();
                stop_str = _serial_stop_ext32_lineEdit->text() + " " + _serial_stop_lineEdit->text();
                edge_str = _serial_edge_ext32_lineEdit->text() + " " + _serial_edge_lineEdit->text();
                comp_str = _value1_ext32_lineEdit_list.at(1)->text() + " " + _value1_lineEdit_list.at(1)->text();
            } else {
                start_str = _serial_start_lineEdit->text();
                stop_str = _serial_stop_lineEdit->text();
                edge_str = _serial_edge_lineEdit->text();
                comp_str = _value1_lineEdit_list.at(1)->text();
            }
            ds_trigger_stage_set_value(0, _cur_ch_num, start_str.toLocal8Bit().data(),
                                                       stop_str.toLocal8Bit().data());
            ds_trigger_stage_set_value(1, _cur_ch_num, edge_str.toLocal8Bit().data(),
                                                       comp_str.toLocal8Bit().data());

            //_serial_data_comboBox
            const int data_channel = _serial_data_comboBox->currentText().toInt();
            QString channel = "X X X X X X X X X X X X X X X X";
            QString channel_ext32 = "X X X X X X X X X X X X X X X X";
            if (_cur_ch_num == 32) {
                if (data_channel < 16)
                    channel.replace(30 - 2*data_channel, 1, '0');
                else
                    channel_ext32.replace(30 - 2*(data_channel - 16), 1, '0');
            } else {
                channel.replace(30 - 2*data_channel, 1, '0');
            }
            ds_trigger_stage_set_value(2, TriggerProbes,
                                 channel.toLocal8Bit().data(),
                                 channel_ext32.toLocal8Bit().data());
            ds_trigger_stage_set_value(STriggerDataStage, TriggerProbes,
                                 _serial_value_lineEdit->text().toLocal8Bit().data(),
                                 _value1_lineEdit_list.at(3)->text().toLocal8Bit().data());
        }

        // trigger logic update
        for (int i = 0; i < stages_comboBox->currentText().toInt(); i++) {
            const char logic = (_contiguous_checkbox_list.at(i)->isChecked() << 1) +
                               _logic_comboBox_list.at(i)->currentIndex();
            ds_trigger_stage_set_logic(i, TriggerProbes,
                                 logic);
        }

        // trigger inv update
        for (int i = 0; i < stages_comboBox->currentText().toInt(); i++) {
            ds_trigger_stage_set_inv(i, TriggerProbes,
                                 _inv0_comboBox_list.at(i)->currentIndex(),
                                 _inv1_comboBox_list.at(i)->currentIndex());
        }

        // trigger count update
        if (_adv_tabWidget->currentIndex() == 0) {
            for (int i = 0; i < stages_comboBox->currentText().toInt(); i++) {
                ds_trigger_stage_set_count(i, TriggerProbes,
                                           _count_spinBox_list.at(i)->value(),
                                           0);
            }
        } else if(_adv_tabWidget->currentIndex() == 1){
            ds_trigger_stage_set_count(1, TriggerProbes,
                                       1,
                                       0);
            ds_trigger_stage_set_count(3, TriggerProbes,
                                       _serial_bits_comboBox->currentText().toInt() - 1,
                                       0);
        }
        return 1;
    }
}

void TriggerDock::init()
{
    // TRIGGERPOS
    //uint16_t pos = ds_trigger_get_pos();
    //_position_slider->setValue(pos);
}

QJsonObject TriggerDock::get_session()
{
    QJsonObject trigSes;
    trigSes["advTriggerMode"] = _adv_radioButton->isChecked();
    trigSes["triggerPos"] = _position_slider->value();
    trigSes["triggerStages"] = stages_comboBox->currentIndex();
    trigSes["triggerTab"] = _adv_tabWidget->currentIndex();

    for (int i = 0; i < stages_comboBox->count(); i++) {
        QString value0_str = "stageTriggerValue0" + QString::number(i);
        QString inv0_str = "stageTriggerInv0" + QString::number(i);
        QString value1_str = "stageTriggerValue1" + QString::number(i);
        QString inv1_str = "stageTriggerInv1" + QString::number(i);

        QString logic_str = "stageTriggerLogic" + QString::number(i);
        QString count_str = "stageTriggerCount" + QString::number(i);
        QString conti_str = "stageTriggerContiguous" + QString::number(i);

        trigSes[value0_str] = _value0_lineEdit_list.at(i)->text();
        trigSes[value1_str] = _value1_lineEdit_list.at(i)->text();
        trigSes[inv0_str] = _inv0_comboBox_list.at(i)->currentIndex();
        trigSes[inv1_str] = _inv1_comboBox_list.at(i)->currentIndex();

        trigSes[logic_str] = _logic_comboBox_list.at(i)->currentIndex();
        trigSes[count_str] = _count_spinBox_list.at(i)->value();
        trigSes[conti_str] = _contiguous_checkbox_list.at(i)->isChecked();

        if (_cur_ch_num == 32) {
            QString value0_ext32_str = "stageTriggerExt32Value0" + QString::number(i);
            QString value1_ext32_str = "stageTriggerExt32Value1" + QString::number(i);

            trigSes[value0_ext32_str] = _value0_ext32_lineEdit_list.at(i)->text();
            trigSes[value1_ext32_str] = _value1_ext32_lineEdit_list.at(i)->text();
        }
    }

    trigSes["serialTriggerStart"] = _serial_start_lineEdit->text();
    trigSes["serialTriggerStop"] = _serial_stop_lineEdit->text();
    trigSes["serialTriggerClock"] = _serial_edge_lineEdit->text();
    trigSes["serialTriggerChannel"] = _serial_data_comboBox->currentIndex();
    trigSes["serialTriggerData"] = _serial_value_lineEdit->text();
    trigSes["serialTriggerBits"] = _serial_bits_comboBox->currentIndex();

    if (_cur_ch_num == 32) {
        trigSes["serialTriggerExt32Start"] = _serial_start_ext32_lineEdit->text();
        trigSes["serialTriggerExt32Stop"] = _serial_stop_ext32_lineEdit->text();
        trigSes["serialTriggerExt32Clock"] = _serial_edge_ext32_lineEdit->text();
    }

    return trigSes;
}

void TriggerDock::set_session(QJsonObject ses)
{
    _position_slider->setValue(ses["triggerPos"].toDouble());
    stages_comboBox->setCurrentIndex(ses["triggerStages"].toDouble());
    _adv_tabWidget->setCurrentIndex(ses["triggerTab"].toDouble());
    if (ses["advTriggerMode"].toBool())
        _adv_radioButton->click();
    else
        _simple_radioButton->click();

    for (int i = 0; i < stages_comboBox->count(); i++) {
        QString value0_str = "stageTriggerValue0" + QString::number(i);
        QString inv0_str = "stageTriggerInv0" + QString::number(i);
        QString value1_str = "stageTriggerValue1" + QString::number(i);
        QString inv1_str = "stageTriggerInv1" + QString::number(i);

        QString logic_str = "stageTriggerLogic" + QString::number(i);
        QString count_str = "stageTriggerCount" + QString::number(i);
        QString conti_str = "stageTriggerContiguous" + QString::number(i);

        _value0_lineEdit_list.at(i)->setText(ses[value0_str].toString());
        lineEdit_highlight(_value0_lineEdit_list.at(i));
        _value1_lineEdit_list.at(i)->setText(ses[value1_str].toString());
        lineEdit_highlight(_value1_lineEdit_list.at(i));
        _inv0_comboBox_list.at(i)->setCurrentIndex(ses[inv0_str].toDouble());
        _inv1_comboBox_list.at(i)->setCurrentIndex(ses[inv1_str].toDouble());

        _logic_comboBox_list.at(i)->setCurrentIndex(ses[logic_str].toDouble());
        _count_spinBox_list.at(i)->setValue(ses[count_str].toDouble());
        _contiguous_checkbox_list.at(i)->setChecked(ses[conti_str].toBool());

        if (_cur_ch_num == 32) {
            QString value0_ext32_str = "stageTriggerExt32Value0" + QString::number(i);
            QString value1_ext32_str = "stageTriggerExt32Value1" + QString::number(i);

            if (ses.contains(value0_ext32_str)) {
                _value0_ext32_lineEdit_list.at(i)->setText(ses[value0_ext32_str].toString());
                lineEdit_highlight(_value0_ext32_lineEdit_list.at(i));
            }
            if (ses.contains(value1_ext32_str)) {
                _value1_ext32_lineEdit_list.at(i)->setText(ses[value1_ext32_str].toString());
                lineEdit_highlight(_value1_ext32_lineEdit_list.at(i));
            }
        }
    }

    _serial_start_lineEdit->setText(ses["serialTriggerStart"].toString());
    lineEdit_highlight(_serial_start_lineEdit);
    _serial_stop_lineEdit->setText(ses["serialTriggerStop"].toString());
    lineEdit_highlight(_serial_stop_lineEdit);
    _serial_edge_lineEdit->setText(ses["serialTriggerClock"].toString());
    lineEdit_highlight(_serial_edge_lineEdit);
    _serial_data_comboBox->setCurrentIndex(ses["serialTriggerChannel"].toDouble());
    _serial_value_lineEdit->setText(ses["serialTriggerData"].toString());
    lineEdit_highlight(_serial_value_lineEdit);
    _serial_bits_comboBox->setCurrentIndex(ses["serialTriggerBits"].toDouble());

    if (_cur_ch_num == 32) {
        if (ses.contains("serialTriggerExt32Start")) {
            _serial_start_ext32_lineEdit->setText(ses["serialTriggerExt32Start"].toString());
            lineEdit_highlight(_serial_start_ext32_lineEdit);
        }
        if (ses.contains("serialTriggerExt32Stop")) {
            _serial_stop_ext32_lineEdit->setText(ses["serialTriggerExt32Stop"].toString());
            lineEdit_highlight(_serial_stop_ext32_lineEdit);
        }
        if (ses.contains("serialTriggerExt32Clock")) {
            _serial_edge_ext32_lineEdit->setText(ses["serialTriggerExt32Clock"].toString());
            lineEdit_highlight(_serial_edge_ext32_lineEdit);
        }
    }
}

void TriggerDock::setup_adv_tab()
{
    int row;

    for (int i = _adv_tabWidget->count(); i > 0; i--)
    {
        _adv_tabWidget->widget(i-1)->deleteLater();
        _adv_tabWidget->removeTab(i-1);
    }
    _logic_comboBox_list.clear();
    _value0_lineEdit_list.clear();
    _count_spinBox_list.clear();
    _inv0_comboBox_list.clear();
    _value1_lineEdit_list.clear();
    _inv1_comboBox_list.clear();
    _contiguous_checkbox_list.clear();
    _inv_exp_label_list.clear();
    _count_exp_label_list.clear();
    _contiguous_label_list.clear();
    _stage_note_label_list.clear();
    _stage_groupBox_list.clear();

    _value0_ext32_lineEdit_list.clear();
    _value1_ext32_lineEdit_list.clear();

    QFont font("Monaco");
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);

    _stage_tabWidget = new QTabWidget(_widget);
    _stage_tabWidget->setTabPosition(QTabWidget::East);
    //_stage_tabWidget->setDisabled(true);
    _stage_tabWidget->setUsesScrollButtons(false);

    const QString mask = "N N N N N N N N N N N N N N N N";
    QRegExp value_rx("[10XRFCxrfc ]+");
    QValidator *value_validator = new QRegExpValidator(value_rx, _stage_tabWidget);
    for (int i = 0; i < TriggerStages; i++) {
        QComboBox *_logic_comboBox = new QComboBox(_stage_tabWidget);
        _logic_comboBox->addItem(tr("Or"));
        _logic_comboBox->addItem(tr("And"));
        _logic_comboBox->setCurrentIndex(1);
        _logic_comboBox_list.push_back(_logic_comboBox);

        QLineEdit *_value0_lineEdit = new QLineEdit("X X X X X X X X X X X X X X X X", _stage_tabWidget);
        _value0_lineEdit->setFont(font);
        _value0_lineEdit->setValidator(value_validator);
        _value0_lineEdit->setMaxLength(TriggerProbes * 2 - 1);
        _value0_lineEdit->setInputMask(mask);
        _value0_lineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        _value0_lineEdit_list.push_back(_value0_lineEdit);
        QSpinBox *_count_spinBox = new QSpinBox(_stage_tabWidget);
        _count_spinBox->setRange(1, INT32_MAX);
        _count_spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
        _count_spinBox_list.push_back(_count_spinBox);
        QComboBox *_inv0_comboBox = new QComboBox(_stage_tabWidget);
        _inv0_comboBox->addItem(tr("=="));
        _inv0_comboBox->addItem(tr("!="));
        _inv0_comboBox_list.push_back(_inv0_comboBox);

        QLineEdit *_value1_lineEdit = new QLineEdit("X X X X X X X X X X X X X X X X", _stage_tabWidget);
        _value1_lineEdit->setFont(font);
        _value1_lineEdit->setValidator(value_validator);
        _value1_lineEdit->setMaxLength(TriggerProbes * 2 - 1);
        _value1_lineEdit->setInputMask(mask);
        _value1_lineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        _value1_lineEdit_list.push_back(_value1_lineEdit);
        QComboBox *_inv1_comboBox = new QComboBox(_stage_tabWidget);
        _inv1_comboBox->addItem(tr("=="));
        _inv1_comboBox->addItem(tr("!="));
        _inv1_comboBox_list.push_back(_inv1_comboBox);

        connect(_value0_lineEdit, SIGNAL(editingFinished()), this, SLOT(value_changed()));
        connect(_value1_lineEdit, SIGNAL(editingFinished()), this, SLOT(value_changed()));

        QCheckBox *_contiguous_checkbox = new QCheckBox(_stage_tabWidget);
        _contiguous_checkbox_list.push_back(_contiguous_checkbox);

        QLabel *value0_exp_label = new QLabel("15 ---------- 8 7 ----------- 0 ", _stage_tabWidget);
        value0_exp_label->setFont(font);
        QLabel *inv0_exp_label = new QLabel(_stage_tabWidget);
        _inv_exp_label_list.push_back(inv0_exp_label);
        QLabel *value1_exp_label = new QLabel("15 ---------- 8 7 ----------- 0 ", _stage_tabWidget);
        value1_exp_label->setFont(font);
        QLabel *inv1_exp_label = new QLabel(_stage_tabWidget);
        _inv_exp_label_list.push_back(inv1_exp_label);

        QLabel *count_exp_label = new QLabel(_stage_tabWidget);
        _count_exp_label_list.push_back(count_exp_label);

        QVBoxLayout *stage_layout = new QVBoxLayout();
        QGridLayout *stage_glayout = new QGridLayout();
        stage_glayout->setVerticalSpacing(5);

        row = 1;
        if (_cur_ch_num == 32) {
            QLineEdit *_value0_ext32_lineEdit = new QLineEdit("X X X X X X X X X X X X X X X X", _stage_tabWidget);
            _value0_ext32_lineEdit->setFont(font);
            _value0_ext32_lineEdit->setValidator(value_validator);
            _value0_ext32_lineEdit->setMaxLength(TriggerProbes * 2 - 1);
            _value0_ext32_lineEdit->setInputMask(mask);
            _value0_ext32_lineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
            _value0_ext32_lineEdit_list.push_back(_value0_ext32_lineEdit);

            QLineEdit *_value1_ext32_lineEdit = new QLineEdit("X X X X X X X X X X X X X X X X", _stage_tabWidget);
            _value1_ext32_lineEdit->setFont(font);
            _value1_ext32_lineEdit->setValidator(value_validator);
            _value1_ext32_lineEdit->setMaxLength(TriggerProbes * 2 - 1);
            _value1_ext32_lineEdit->setInputMask(mask);
            _value1_ext32_lineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
            _value1_ext32_lineEdit_list.push_back(_value1_ext32_lineEdit);

            QLabel *value0_ext32_exp_label = new QLabel("31 --------- 24 23 ---------- 16", _stage_tabWidget);
            value0_ext32_exp_label->setFont(font);
            QLabel *value1_ext32_exp_label = new QLabel("31 --------- 24 23 ---------- 16", _stage_tabWidget);
            value1_ext32_exp_label->setFont(font);

            stage_glayout->addWidget(value0_ext32_exp_label, row++, 0);
            stage_glayout->addWidget(_value0_ext32_lineEdit, row++, 0);
            stage_glayout->addWidget(value0_exp_label, row, 0);
            stage_glayout->addWidget(inv0_exp_label, row++, 1);
            stage_glayout->addWidget(_value0_lineEdit, row, 0);
            stage_glayout->addWidget(_inv0_comboBox, row, 1);
            stage_glayout->addWidget(_logic_comboBox, row++, 2);

            stage_glayout->addWidget(new QLabel(_stage_tabWidget), row++, 0);

            stage_glayout->addWidget(value1_ext32_exp_label, row++, 0);
            stage_glayout->addWidget(_value1_ext32_lineEdit, row++, 0);
            stage_glayout->addWidget(value1_exp_label, row, 0);
            stage_glayout->addWidget(inv1_exp_label, row++, 1);
            stage_glayout->addWidget(_value1_lineEdit, row, 0);
            stage_glayout->addWidget(_inv1_comboBox, row++, 1);

            connect(_value0_ext32_lineEdit, SIGNAL(editingFinished()), this, SLOT(value_changed()));
            connect(_value1_ext32_lineEdit, SIGNAL(editingFinished()), this, SLOT(value_changed()));
        } else {
            stage_glayout->addWidget(value0_exp_label, row, 0);
            stage_glayout->addWidget(inv0_exp_label, row++, 1);
            stage_glayout->addWidget(_value0_lineEdit, row, 0);
            stage_glayout->addWidget(_inv0_comboBox, row, 1);
            stage_glayout->addWidget(_logic_comboBox, row++, 2);

            stage_glayout->addWidget(new QLabel(_stage_tabWidget), row++, 0);

            stage_glayout->addWidget(value1_exp_label, row, 0);
            stage_glayout->addWidget(inv1_exp_label, row++, 1);
            stage_glayout->addWidget(_value1_lineEdit, row, 0);
            stage_glayout->addWidget(_inv1_comboBox, row++, 1);
        }

        stage_glayout->addWidget(new QLabel(_stage_tabWidget), row++, 0);

        QLabel *contiguous_label = new QLabel(_stage_tabWidget);
        _contiguous_label_list.push_back(contiguous_label);
        stage_glayout->addWidget(contiguous_label, row, 1, 1, 2);
        stage_glayout->addWidget(_contiguous_checkbox, row++, 0, 1, 1, Qt::AlignRight);
        stage_glayout->addWidget(count_exp_label, row, 1, 1, 2);
        stage_glayout->addWidget(_count_spinBox, row++, 0);

        stage_layout->addLayout(stage_glayout);
        stage_layout->addSpacing(20);
        QLabel *stage_note_label = new QLabel(_stage_tabWidget);
        _stage_note_label_list.push_back(stage_note_label);
        stage_layout->addWidget(stage_note_label);
        stage_layout->addStretch(1);

        QGroupBox *stage_groupBox = new QGroupBox(_stage_tabWidget);
        stage_groupBox->setFlat(true);
        stage_groupBox->setLayout(stage_layout);
        _stage_groupBox_list.push_back(stage_groupBox);

        _stage_tabWidget->addTab((QWidget *)stage_groupBox, QString::number(i));
    }

    _serial_groupBox = new QGroupBox(_widget);
    _serial_groupBox->setFlat(true);

    _serial_start_label = new QLabel(_serial_groupBox);
    _serial_start_lineEdit = new QLineEdit("X X X X X X X X X X X X X X X X", _serial_groupBox);
    _serial_start_lineEdit->setFont(font);
    _serial_start_lineEdit->setValidator(value_validator);
    _serial_start_lineEdit->setMaxLength(TriggerProbes * 2 - 1);
    _serial_start_lineEdit->setInputMask(mask);
    _serial_start_lineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    _serial_stop_label = new QLabel(_serial_groupBox);
    _serial_stop_lineEdit = new QLineEdit("X X X X X X X X X X X X X X X X", _serial_groupBox);
    _serial_stop_lineEdit->setFont(font);
    _serial_stop_lineEdit->setValidator(value_validator);
    _serial_stop_lineEdit->setMaxLength(TriggerProbes * 2 - 1);
    _serial_stop_lineEdit->setInputMask(mask);
    _serial_stop_lineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    _serial_edge_label = new QLabel(_serial_groupBox);
    _serial_edge_lineEdit = new QLineEdit("X X X X X X X X X X X X X X X X", _serial_groupBox);
    _serial_edge_lineEdit->setFont(font);
    _serial_edge_lineEdit->setValidator(value_validator);
    _serial_edge_lineEdit->setMaxLength(TriggerProbes * 2 - 1);
    _serial_edge_lineEdit->setInputMask(mask);
    _serial_edge_lineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    _serial_data_label = new QLabel(_serial_groupBox);
    _serial_data_comboBox = new QComboBox(_serial_groupBox);
    for(int i = 0; i < _cur_ch_num; i++)
        _serial_data_comboBox->addItem(QString::number(i));

    _serial_value_label = new QLabel(_serial_groupBox);
    _serial_value_lineEdit = new QLineEdit("X X X X X X X X X X X X X X X X", _serial_groupBox);
    _serial_value_lineEdit->setFont(font);
    _serial_value_lineEdit->setValidator(value_validator);
    _serial_value_lineEdit->setMaxLength(TriggerProbes * 2 - 1);
    _serial_value_lineEdit->setInputMask(mask);
    _serial_value_lineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    _serial_bits_comboBox = new QComboBox(_serial_groupBox);
    for(int i = 1; i <= 16; i++)
        _serial_bits_comboBox->addItem(QString::number(i));


    QVBoxLayout *serial_layout = new QVBoxLayout();
    QGridLayout *serial_glayout = new QGridLayout();
    serial_glayout->setVerticalSpacing(5);

    row = 1;
    if (_cur_ch_num == 32) {
        _serial_start_ext32_lineEdit = new QLineEdit("X X X X X X X X X X X X X X X X", _serial_groupBox);
        _serial_start_ext32_lineEdit->setFont(font);
        _serial_start_ext32_lineEdit->setValidator(value_validator);
        _serial_start_ext32_lineEdit->setMaxLength(TriggerProbes * 2 - 1);
        _serial_start_ext32_lineEdit->setInputMask(mask);
        _serial_start_ext32_lineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

        _serial_stop_ext32_lineEdit = new QLineEdit("X X X X X X X X X X X X X X X X", _serial_groupBox);
        _serial_stop_ext32_lineEdit->setFont(font);
        _serial_stop_ext32_lineEdit->setValidator(value_validator);
        _serial_stop_ext32_lineEdit->setMaxLength(TriggerProbes * 2 - 1);
        _serial_stop_ext32_lineEdit->setInputMask(mask);
        _serial_stop_ext32_lineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

        _serial_edge_ext32_lineEdit = new QLineEdit("X X X X X X X X X X X X X X X X", _serial_groupBox);
        _serial_edge_ext32_lineEdit->setFont(font);
        _serial_edge_ext32_lineEdit->setValidator(value_validator);
        _serial_edge_ext32_lineEdit->setMaxLength(TriggerProbes * 2 - 1);
        _serial_edge_ext32_lineEdit->setInputMask(mask);
        _serial_edge_ext32_lineEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

        connect(_serial_start_ext32_lineEdit, SIGNAL(editingFinished()), this, SLOT(value_changed()));
        connect(_serial_stop_ext32_lineEdit, SIGNAL(editingFinished()), this, SLOT(value_changed()));
        connect(_serial_edge_ext32_lineEdit, SIGNAL(editingFinished()), this, SLOT(value_changed()));

        QLabel *serial0_value_exp_label = new QLabel("31 --------- 24 23 ---------- 16", _serial_groupBox);
        serial0_value_exp_label->setFont(font);
        serial_glayout->addWidget(serial0_value_exp_label, row++, 1, 1, 3);
        serial_glayout->addWidget(_serial_start_ext32_lineEdit, row, 1, 1, 3);
        serial_glayout->addWidget(new QLabel(_serial_groupBox), row++, 4);
        QLabel *serial1_value_exp_label = new QLabel("15 ---------- 8 7 ----------- 0 ", _serial_groupBox);
        serial1_value_exp_label->setFont(font);
        serial_glayout->addWidget(serial1_value_exp_label, row++, 1, 1, 3);
        serial_glayout->addWidget(_serial_start_label, row, 0);
        serial_glayout->addWidget(_serial_start_lineEdit, row, 1, 1, 3);
        serial_glayout->addWidget(new QLabel(_serial_groupBox), row++, 4);

        serial_glayout->addWidget(new QLabel(_stage_tabWidget), row++, 0);

        QLabel *serial2_value_exp_label = new QLabel("31 --------- 24 23 ---------- 16", _serial_groupBox);
        serial2_value_exp_label->setFont(font);
        serial_glayout->addWidget(serial2_value_exp_label, row++, 1, 1, 3);
        serial_glayout->addWidget(_serial_stop_ext32_lineEdit, row++, 1, 1, 3);
        QLabel *serial3_value_exp_label = new QLabel("15 ---------- 8 7 ----------- 0 ", _serial_groupBox);
        serial3_value_exp_label->setFont(font);
        serial_glayout->addWidget(serial3_value_exp_label, row++, 1, 1, 3);
        serial_glayout->addWidget(_serial_stop_label, row, 0);
        serial_glayout->addWidget(_serial_stop_lineEdit, row++, 1, 1, 3);

        serial_glayout->addWidget(new QLabel(_stage_tabWidget), row++, 0);

        QLabel *serial4_value_exp_label = new QLabel("31 --------- 24 23 ---------- 16", _serial_groupBox);
        serial4_value_exp_label->setFont(font);
        serial_glayout->addWidget(serial4_value_exp_label, row++, 1, 1, 3);
        serial_glayout->addWidget(_serial_edge_ext32_lineEdit, row++, 1, 1, 3);
        QLabel *serial5_value_exp_label = new QLabel("15 ---------- 8 7 ----------- 0 ", _serial_groupBox);
        serial5_value_exp_label->setFont(font);
        serial_glayout->addWidget(serial5_value_exp_label, row++, 1, 1, 3);
        serial_glayout->addWidget(_serial_edge_label, row, 0);
        serial_glayout->addWidget(_serial_edge_lineEdit, row++, 1, 1, 3);
    } else {
        QLabel *serial0_value_exp_label = new QLabel("15 ---------- 8 7 ----------- 0 ", _serial_groupBox);
        serial0_value_exp_label->setFont(font);
        serial_glayout->addWidget(serial0_value_exp_label, row++, 1, 1, 3);
        serial_glayout->addWidget(_serial_start_label, row, 0);
        serial_glayout->addWidget(_serial_start_lineEdit, row, 1, 1, 3);
        serial_glayout->addWidget(new QLabel(_serial_groupBox), row++, 4);

        serial_glayout->addWidget(new QLabel(_stage_tabWidget), row++, 0);

        QLabel *serial1_value_exp_label = new QLabel("15 ---------- 8 7 ----------- 0 ", _serial_groupBox);
        serial1_value_exp_label->setFont(font);
        serial_glayout->addWidget(serial1_value_exp_label, row++, 1, 1, 3);
        serial_glayout->addWidget(_serial_stop_label, row, 0);
        serial_glayout->addWidget(_serial_stop_lineEdit, row++, 1, 1, 3);

        serial_glayout->addWidget(new QLabel(_stage_tabWidget), row++, 0);

        QLabel *serial2_value_exp_label = new QLabel("15 ---------- 8 7 ----------- 0 ", _serial_groupBox);
        serial2_value_exp_label->setFont(font);
        serial_glayout->addWidget(serial2_value_exp_label, row++, 1, 1, 3);
        serial_glayout->addWidget(_serial_edge_label, row, 0);
        serial_glayout->addWidget(_serial_edge_lineEdit, row++, 1, 1, 3);
    }

    serial_glayout->addWidget(new QLabel(_serial_groupBox), row++, 0, 1, 5);
    serial_glayout->addWidget(_serial_data_label, row, 0);
    serial_glayout->addWidget(_serial_data_comboBox, row++, 1);
    _data_bits_label = new QLabel(_serial_groupBox);
    serial_glayout->addWidget(_data_bits_label, row, 0);
    serial_glayout->addWidget(_serial_bits_comboBox, row++, 1);
    serial_glayout->addWidget(_serial_value_label, row, 0);
    serial_glayout->addWidget(_serial_value_lineEdit, row++, 1, 1, 3);

    _serial_note_label = new QLabel(_serial_groupBox);
    serial_layout->addLayout(serial_glayout);
    serial_layout->addSpacing(20);
    serial_layout->addWidget(_serial_note_label);
    serial_layout->addStretch(1);

    _serial_groupBox->setLayout(serial_layout);

    connect(_serial_start_lineEdit, SIGNAL(editingFinished()), this, SLOT(value_changed()));
    connect(_serial_stop_lineEdit, SIGNAL(editingFinished()), this, SLOT(value_changed()));
    connect(_serial_edge_lineEdit, SIGNAL(editingFinished()), this, SLOT(value_changed()));
    connect(_serial_value_lineEdit, SIGNAL(editingFinished()), this, SLOT(value_changed()));

    _adv_tabWidget->addTab((QWidget *)_stage_tabWidget, tr("Stage Trigger"));
    _adv_tabWidget->addTab((QWidget *)_serial_groupBox, tr("Serial Trigger"));
}

void TriggerDock::lineEdit_highlight(QLineEdit *dst) {
    if (dst == NULL)
        return;

    QTextCharFormat fmt;
    fmt.setForeground(view::View::Red);
    QList<QInputMethodEvent::Attribute> attributes;
    for (int i = 0; i < dst->text().size(); i++) {
        if (dst->text().at(i) != 'X' && dst->text().at(i) != ' ')
            attributes.append(QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat, i-dst->cursorPosition(), 1, fmt));
    }
    QInputMethodEvent event(QString(), attributes);
    QCoreApplication::sendEvent(dst, &event);
}

} // namespace dock
} // namespace pv
