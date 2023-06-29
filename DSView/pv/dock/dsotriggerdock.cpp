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
#include "../dialogs/dsmessagebox.h"
#include "../view/dsosignal.h" 
#include <QObject>
#include <QLabel>
#include <QRadioButton>
#include <QPainter>
#include <QStyleOption>
#include <QVector>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QEvent>
#include "../ui/langresource.h"
#include "../log.h"
#include "../ui/msgbox.h"
#include "../config/appconfig.h"
#include "../ui/fn.h"

using namespace boost;
using namespace std;

namespace pv {
namespace dock {

DsoTriggerDock::DsoTriggerDock(QWidget *parent, SigSession *session) :
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
    _holdoff_comboBox = new DsComboBox(_widget);
    //tr
    _holdoff_comboBox->addItem(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_S), "S"), QVariant::fromValue(1000000000));
    _holdoff_comboBox->addItem(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_MS), "mS"), QVariant::fromValue(1000000));
    _holdoff_comboBox->addItem(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_US), "uS"), QVariant::fromValue(1000));
   
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
    _channel_comboBox = new DsComboBox(_widget);
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
    //tr
    gLayout->addWidget(new QLabel("%", _widget), 0, 2);
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

    update_font();
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
    _position_label->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_TRIGGER_POSITION), "Trigger Position: "));
    _holdoff_label->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_HOLD_OFF_TIME), "Hold Off Time: "));
    _margin_label->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_NOISE_SENSITIVITY), "Noise Sensitivity: "));
    _tSource_label->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_TRIGGER_SOURCES), "Trigger Sources: "));
    _tType_label->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_TRIGGER_TYPES), "Trigger Types: "));
    _rising_radioButton->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_RISING_EDGE), "Rising Edge"));
    _falling_radioButton->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_FALLING_EDGE), "Falling Edge"));

    _auto_radioButton->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_AUTO), "Auto"));
    _ch0_radioButton->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CHANNEL_0), "Channel 0"));
    _ch1_radioButton->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CHANNEL_1), "Channel 1"));
    _ch0a1_radioButton->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CHANNEL_0_AND_1), "Channel 0 && 1"));
    _ch0o1_radioButton->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CHANNEL_0_OR_1), "Channel 0 | 1"));
}

void DsoTriggerDock::reStyle()
{
     
}

void DsoTriggerDock::paintEvent(QPaintEvent *e)
{
    (void)e;
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
    ret = _session->get_device()->set_config_byte(
                                            SR_CONF_HORIZ_TRIGGERPOS,pos);
    if (!ret) {        
        if (_session->get_device()->is_hardware() || true){
            QString strMsg(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_CHANGE_HOR_TRI_POS_FAIL), 
                                       "Change horiz trigger position failed!"));
                                
            MsgBox::Show(strMsg);
        }
        
        return;
    }
    set_trig_pos(pos);
}

void DsoTriggerDock::hold_changed(int hold)
{ 
    (void)hold;
    int ret;
    uint64_t holdoff;

    if (_holdoff_comboBox->currentData().toDouble() == 1000000000)
        _holdoff_slider->setRange(0, 10);
    else
        _holdoff_slider->setRange(0, 999);

    holdoff = _holdoff_slider->value() * _holdoff_comboBox->currentData().toDouble() / 10;
    ret = _session->get_device()->set_config_uint64(
                                            SR_CONF_TRIGGER_HOLDOFF,holdoff);

    if (!ret) {
        QString strMsg(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_CHANGE_TRI_HOLDOFF_TIME_FAIL),
                                      "Change trigger hold off time failed!"));
        MsgBox::Show(strMsg);
    }
}

void DsoTriggerDock::margin_changed(int margin)
{
    int ret;
    ret = _session->get_device()->set_config_byte(SR_CONF_TRIGGER_MARGIN, margin);
    if (!ret) {
        QString strMsg(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_CHANGE_SENSITIVITY_FAIL), 
                                      "Change trigger value sensitivity failed!"));
        MsgBox::Show(strMsg);
    }
}

void DsoTriggerDock::source_changed()
{
    if (check_trig_channel() == false){
        _auto_radioButton->setChecked(true);
        _ch1_radioButton->setChecked(false);
        _ch0a1_radioButton->setChecked(false);
        _ch0o1_radioButton->setChecked(false); 

        QString msg(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_DISABLED_CHANNEL_TRIG), "Disabled channels cannot be used for triggering!"));
        MsgBox::Show(msg);        
    }

    int id = _source_group->checkedId();
    int ret;

    dsv_info("Set DSO trig type:%d", id);

    ret = _session->get_device()->set_config_byte(SR_CONF_TRIGGER_SOURCE, id);
    if (!ret) {
        QString strMsg(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_CHANGE_SOURCE_FAIL), 
                                      "Change trigger source failed!"));
        MsgBox::Show(strMsg);
    }
}

void DsoTriggerDock::check_setting()
{
    if (check_trig_channel() == false){
        _auto_radioButton->setChecked(true);
        _ch1_radioButton->setChecked(false);
        _ch0a1_radioButton->setChecked(false);
        _ch0o1_radioButton->setChecked(false);

        _session->get_device()->set_config_byte(SR_CONF_TRIGGER_SOURCE, int(DSO_TRIGGER_AUTO));
    }
}

bool DsoTriggerDock::check_trig_channel()
{
    int id = _source_group->checkedId();
    bool b0 = _session->get_device()->channel_is_enable(0);
    bool b1 = _session->get_device()->channel_is_enable(1);

    if (DSO_TRIGGER_CH0 == id && !b0){
        dsv_err("ERROR: The trigger channel is disabled");
        return false;
    }
    else if (DSO_TRIGGER_CH1 == id && !b1){
        dsv_err("ERROR: The trigger channel is disabled");
        return false;
    }
    else if (DSO_TRIGGER_CH0A1 == id && (!b0 || !b1)){
        dsv_err("ERROR: The trigger channel is disabled");
        return false;
    }

    return true;
}

void DsoTriggerDock::channel_changed(int ch)
{
    (void)ch;
    int ret;

    ret = _session->get_device()->set_config_byte(
                                            SR_CONF_TRIGGER_CHANNEL,
                                            int(_channel_comboBox->currentData().toInt()));
    if (!ret) {
        QString strMsg(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_CHANGE_CHANNEL_FAIL), 
                                      "Change trigger channel failed!"));
        MsgBox::Show(strMsg);
    }
}

void DsoTriggerDock::type_changed()
{
    int id = _type_group->checkedId();
    int ret;

    ret = _session->get_device()->set_config_byte(
                                            SR_CONF_TRIGGER_SLOPE,
                                            id);
    if (!ret) {
        QString strMsg(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_CHANGE_TYPE_FAIL), 
                                      "Change trigger type failed!"));
        MsgBox::Show(strMsg);
    }
}

void DsoTriggerDock::device_change()
{
    bool bDisable = _session->get_device()->is_file();
    _position_spinBox->setDisabled(bDisable);
    _position_slider->setDisabled(bDisable);
}

void DsoTriggerDock::update_view()
{
    bool bDisable = _session->get_device()->is_file();
    bool ret;

    if (_session->get_device()->is_demo()){
        bDisable = true;
    }

    for(QAbstractButton * btn :  _source_group->buttons()){
        btn->setDisabled(bDisable);
    }

    for(QAbstractButton * btn : _type_group->buttons()){
        btn->setDisabled(bDisable);
    }

    _holdoff_slider->setDisabled(bDisable);
    _holdoff_spinBox->setDisabled(bDisable);
    _holdoff_comboBox->setDisabled(bDisable);
    _margin_slider->setDisabled(bDisable);
    _channel_comboBox->setDisabled(bDisable);

    _position_spinBox->setDisabled(bDisable);
    _position_slider->setDisabled(bDisable);

    if (_session->get_device()->is_demo()){
        _position_spinBox->setDisabled(false);
        _position_slider->setDisabled(false);
    }

    if (_session->get_device()->is_file()){
        return;
    }

    int pos;
    int src;
    int slope;

    // TRIGGERPOS 
    if (_session->get_device()->get_config_byte(
                                            SR_CONF_HORIZ_TRIGGERPOS, pos)) {
        _position_slider->setValue(pos);
    }

    if (_session->get_device()->get_config_byte(SR_CONF_TRIGGER_SOURCE, src)) {
        _source_group->button(src)->setChecked(true);
    }

    // setup _channel_comboBox
    disconnect(_channel_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(channel_changed(int)));
    _channel_comboBox->clear();
    
    for(auto s : _session->get_signals()) {
        if (s->signal_type() == SR_CHANNEL_DSO) {
            view::DsoSignal *dsoSig = (view::DsoSignal*)s;
            _channel_comboBox->addItem(dsoSig->get_name(), QVariant::fromValue(dsoSig->get_index()));
        }
    }
    ret = _session->get_device()->get_config_byte(
                                                SR_CONF_TRIGGER_CHANNEL, src);
    if (ret) { 
        for (int i = 0; i < _channel_comboBox->count(); i++) {
            if (src == _channel_comboBox->itemData(i).toInt()) {
                _channel_comboBox->setCurrentIndex(i);
                break;
            }
        }
    }
    connect(_channel_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(channel_changed(int)));

    ret = _session->get_device()->get_config_byte(
                                                SR_CONF_TRIGGER_SLOPE, slope);
    if (ret) {
        _type_group->button(slope)->setChecked(true);
    }

    disconnect(_holdoff_slider, SIGNAL(valueChanged(int)), this, SLOT(hold_changed(int)));
    disconnect(_holdoff_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(hold_changed(int)));

    uint64_t holdoff;
    ret = _session->get_device()->get_config_uint64(
                                                SR_CONF_TRIGGER_HOLDOFF, holdoff);
    if (ret) {        
        auto v = holdoff * 10.0;

        for (int i=0; i<_holdoff_comboBox->count(); i++)
        {
            if (v >= _holdoff_comboBox->itemData(i).toDouble()) {
                _holdoff_comboBox->setCurrentIndex(i);
                break;
            }
        }

        if (_holdoff_comboBox->currentData().toDouble() == 1000000000)
            _holdoff_slider->setRange(0, 10);
        else 
            _holdoff_slider->setRange(0, 999);
        
        auto v1 = holdoff * 10.0 / _holdoff_comboBox->currentData().toDouble();
        _holdoff_spinBox->setValue(v1);
    }

    connect(_holdoff_slider, SIGNAL(valueChanged(int)), this, SLOT(hold_changed(int)));
    connect(_holdoff_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(hold_changed(int)));

    disconnect(_margin_slider, SIGNAL(valueChanged(int)), this, SLOT(margin_changed(int)));
    
    int  margin;
    ret = _session->get_device()->get_config_byte(
                                                SR_CONF_TRIGGER_MARGIN, margin);
    if (ret) {
        _margin_slider->setValue(margin);
    }
    connect(_margin_slider, SIGNAL(valueChanged(int)), this, SLOT(margin_changed(int)));
}

void DsoTriggerDock::update_font()
{
    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);
    ui::set_form_font(this, font);
    font.setPointSizeF(font.pointSizeF() + 1);
    this->parentWidget()->setFont(font);
}

} // namespace dock
} // namespace pv
