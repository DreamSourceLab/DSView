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

#include "calibration.h"
  
#include <QGridLayout>
#include <QFuture>
#include <QProgressDialog>
#include <QtConcurrent/QtConcurrent>
#include <QTime>
#include <assert.h>

#include "../view/trace.h"
#include "../dialogs/dsmessagebox.h"
#include "../dsvdef.h"
#include "../appcontrol.h"
#include "../sigsession.h"
#include "../ui/langresource.h"
#include "../ui/msgbox.h"
#include "../ui/fn.h"
#include "../config/appconfig.h"

using namespace std;

namespace pv {
namespace dialogs {

namespace
{
    QString VGAIN = "VGAIN";
    QString VOFF =  "VOFF";
    QString VCOMB = "VCOMB";
    QString CHANNEL_LABEL = "Channel";
}

Calibration::Calibration(QWidget *parent) :
    DSDialog(parent)
{ 
    _save_btn = NULL;
    _abort_btn = NULL;
    _reset_btn = NULL;
    _exit_btn = NULL;
    _flayout = NULL;
    _is_setting = false;

#ifdef Q_OS_DARWIN
    Qt::WindowFlags flags = windowFlags();
    this->setWindowFlags(flags | Qt::Tool);
#endif
    this->setFixedSize(450, 300);
    this->setWindowOpacity(0.7);
    this->setModal(false);

    _device_agent = AppControl::Instance()->GetSession()->get_device();

    _save_btn = new QPushButton(this);
    _abort_btn = new QPushButton(this);
    _reset_btn = new QPushButton(this);
    _exit_btn = new QPushButton(this);

    _flayout = new QFormLayout();
    _flayout->setVerticalSpacing(10);
    _flayout->setFormAlignment(Qt::AlignLeft);
    _flayout->setLabelAlignment(Qt::AlignLeft);
    _flayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    QGridLayout *glayout = new QGridLayout();
    glayout->setVerticalSpacing(5);

    glayout->addLayout(_flayout, 1, 0, 1, 7);
    glayout->addWidget(_save_btn, 2, 0);
    glayout->addWidget(new QWidget(this), 2, 1);
    glayout->setColumnStretch(1, 1);
    glayout->addWidget(_abort_btn, 2, 2);
    glayout->addWidget(new QWidget(this), 2, 3);
    glayout->setColumnStretch(3, 1);
    glayout->addWidget(_reset_btn, 2, 4);
    glayout->addWidget(new QWidget(this), 2, 5);
    glayout->setColumnStretch(5, 1);
    glayout->addWidget(_exit_btn, 2, 6);

    layout()->addLayout(glayout);

    BuildUI();

    connect(_save_btn, SIGNAL(clicked()), this, SLOT(on_save()));
    connect(_abort_btn, SIGNAL(clicked()), this, SLOT(on_abort()));
    connect(_reset_btn, SIGNAL(clicked()), this, SLOT(on_reset()));
    connect(_exit_btn, SIGNAL(clicked()), this, SLOT(reject()));

    ADD_UI(this);
}

Calibration::~Calibration()
{ 
    DESTROY_QT_OBJECT(_save_btn);
    DESTROY_QT_OBJECT(_abort_btn);
    DESTROY_QT_OBJECT(_reset_btn);
    DESTROY_QT_OBJECT(_exit_btn);
    DESTROY_QT_OBJECT(_flayout);

    REMOVE_UI(this);
}

void Calibration::retranslateUi()
{
    _save_btn->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SAVE), "Save"));
    _abort_btn->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_ABORT), "Abort"));
    _reset_btn->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_RESET), "Reset"));
    _exit_btn->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_EXIT), "Exit"));

    setTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_MANUAL_CALIBRATION), "Manual Calibration"));
}

void Calibration::BuildUI()
{
    if (_device_agent->have_instance() == false){
        assert(false);
    }
    assert(_params.empty());

    _flayout->setSpacing(15);

    for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
        sr_channel *const probe = (sr_channel*)l->data;
        assert(probe);

        QSlider *gain_slider = new QSlider(Qt::Horizontal, this);
        QLabel *gain_label = new QLabel("gain", this);
        gain_label->setAlignment(Qt::AlignVCenter);   
        _flayout->addRow(gain_label, gain_slider);
 
        QSlider *off_slider = new QSlider(Qt::Horizontal, this);
        QLabel *off_label = new QLabel("off", this);
        off_label->setAlignment(Qt::AlignVCenter);   
        _flayout->addRow(off_label, off_slider);

        QSlider *comp_slider = NULL;
        QLabel *comp_label = NULL;

         bool comb_comp_en = false;
        _device_agent->get_config_bool(SR_CONF_PROBE_COMB_COMP_EN, comb_comp_en);

        if (comb_comp_en){
            comp_slider = new QSlider(Qt::Horizontal, this);
            comp_label = new QLabel("comp", this);
            comp_label->setAlignment(Qt::AlignVCenter);
            _flayout->addRow(comp_label, comp_slider);            
        } 

        channel_param_widget form;
        form.gain.lable = gain_label;
        form.gain.slider = gain_slider;
        form.off.lable = off_label;
        form.off.slider = off_slider;
        form.comp.lable = comp_label;
        form.comp.slider = comp_slider;
        form.probe = probe;

        _params.push_back(form);
 
        connect(gain_slider, SIGNAL(valueChanged(int)), this, SLOT(set_value(int)));
        connect(off_slider, SIGNAL(valueChanged(int)), this, SLOT(set_value(int))); 

        if (comp_slider != NULL){
            connect(comp_slider, SIGNAL(valueChanged(int)), this, SLOT(set_value(int)));
        }
    }

    QWidget *spaceLine = new QWidget();
    spaceLine->setFixedHeight(10);
    _flayout->addRow(spaceLine);
}

void Calibration::update_device_info()
{ 
    if (_device_agent->have_instance() == false){
        assert(false);
    }

    int dex = -1;
    _is_setting = true;
 
    for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
        sr_channel *const probe = (sr_channel*)l->data;
        dex++;
        assert(dex < _params.size());
        auto *form = &_params[dex];

        assert(form->probe == probe);

        uint64_t vgain = 0, vgain_default = 0;
        int vgain_range = 0;
        
        _device_agent->get_config_uint64(SR_CONF_PROBE_VGAIN, vgain, probe, NULL);
        _device_agent->get_config_uint64(SR_CONF_PROBE_VGAIN_DEFAULT, vgain_default, probe, NULL);   
        _device_agent->get_config_uint16(SR_CONF_PROBE_VGAIN_RANGE, vgain_range, probe, NULL);
    
        form->gain.slider->setRange(-vgain_range/2, vgain_range/2);
        form->gain.slider->setValue(vgain - vgain_default);
         
        uint64_t voff = 0;
        uint16_t voff_range = 0;
        int v;
 
        if (_device_agent->get_config_uint16(SR_CONF_PROBE_PREOFF, v, probe, NULL)) {
            voff = (uint64_t)v;
        }
        if (_device_agent->get_config_uint16(SR_CONF_PROBE_PREOFF_MARGIN, v, probe, NULL)) {
            voff_range = (uint16_t)v;
        }
 
        form->off.slider->setRange(0, voff_range);
        form->off.slider->setValue(voff);
         
        if (form->comp.slider != NULL) {
            int comb_comp = 0;
           _device_agent->get_config_int16(SR_CONF_PROBE_COMB_COMP, comb_comp, probe, NULL);

            form->comp.slider->setRange(-127, 127);
            form->comp.slider->setValue(comb_comp);
        } 
    }
 
    update();

    _is_setting = false;
}

void Calibration::reject()
{ 
    _device_agent->set_config_bool(SR_CONF_CALI, false);
    sig_closed();
    close();
}

void Calibration::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        reject();
    }
    else { 
        DSDialog::keyPressEvent(event);
    }
}

void Calibration::set_value(int value)
{
    if (_is_setting){
        return;
    }

    const QSlider* sc = dynamic_cast<QSlider *>(sender());

    for (auto &form : _params)
    {
        if (form.gain.slider == sc){
            uint64_t vgain_default;
            if (_device_agent->get_config_uint64(SR_CONF_PROBE_VGAIN_DEFAULT, vgain_default, form.probe)){
                _device_agent->set_config_uint64(SR_CONF_PROBE_VGAIN, value+vgain_default, form.probe);          
            }
        }
        else if (form.off.slider == sc){
            _device_agent->set_config_uint16(SR_CONF_PROBE_PREOFF, value, form.probe); 
        }
        else if (form.comp.slider == sc){
            _device_agent->set_config_int16(SR_CONF_PROBE_COMB_COMP, value, form.probe);
        } 
    }
}

void Calibration::on_save()
{
    this->hide();
    QFuture<void> future;

    future = QtConcurrent::run([&]{
        _device_agent->set_config_bool( SR_CONF_ZERO_SET, true);
    });

    Qt::WindowFlags flags = Qt::CustomizeWindowHint;
    QProgressDialog dlg(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SAVE_CALIBRATION_RESULTS), 
                        "Save calibration results... It can take a while."),
                        L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CANCEL), "Cancel"),
                        0,0,this,flags);
    dlg.setWindowModality(Qt::WindowModal);
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint |
                       Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
    dlg.setCancelButton(NULL);

    QFutureWatcher<void> watcher;
    connect(&watcher,SIGNAL(finished()),&dlg,SLOT(cancel()));
    watcher.setFuture(future);

    dlg.exec();
    this->show();
}

void Calibration::on_abort()
{
    this->hide();
    QFuture<void> future;

    future = QtConcurrent::run([&]{ 
        _device_agent->set_config_bool(SR_CONF_ZERO_LOAD, true);
        reload_value();
    });

    Qt::WindowFlags flags = Qt::CustomizeWindowHint;
    QProgressDialog dlg(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_RELOAD_CALIBRATION_RESULTS), 
                        "Reload last calibration results... It can take a while."),
                        L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CANCEL), "Cancel"),0,0,this,flags);
    dlg.setWindowModality(Qt::WindowModal);
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint |
                       Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
    dlg.setCancelButton(NULL);

    QFutureWatcher<void> watcher;
    connect(&watcher,SIGNAL(finished()),&dlg,SLOT(cancel()));
    watcher.setFuture(future);

    dlg.exec();
    this->show();
}

void Calibration::reload_value()
{
    update_device_info();
}

void Calibration::on_reset()
{
    QString strMsg(L_S(STR_PAGE_MSG, S_ID(IDS_MSG_SET_DEF_CAL_SETTING), 
        "All calibration settings will become the defualt values!"));

    if (MsgBox::Confirm(strMsg)) {
        _device_agent->set_config_bool(SR_CONF_ZERO_DEFAULT, true);
        reload_value();
    }
}

void Calibration::updateLangText()
{
    QString sp = " ";
    VGAIN = sp + L_S(STR_PAGE_DLG, S_ID(IDS_CALIB_VGAIN), "VGAIN");
    VOFF = sp + L_S(STR_PAGE_DLG, S_ID(IDS_CALIB_VOFF), "VOFF");
    VCOMB = sp + L_S(STR_PAGE_DLG, S_ID(IDS_CALIB_VCOMB), "VCOMB");
    CHANNEL_LABEL = L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CHANNEL), "Channel");

    for(auto &form : _params)
    {
        QString gain_string = CHANNEL_LABEL + QString::number(form.probe->index) + VGAIN;
        QString off_string = CHANNEL_LABEL + QString::number(form.probe->index) + VOFF;
        QString comp_string = CHANNEL_LABEL + QString::number(form.probe->index) + VCOMB;

        form.gain.lable->setText(gain_string);
        form.off.lable->setText(off_string);

        if (form.comp.lable != NULL){
            form.comp.lable->setText(comp_string);
        }

        {
            auto lable = form.gain.lable;
            auto slider = form.gain.slider;
            ui::adjust_label_size(lable, ui::ADJUST_HEIGHT);
            slider->setFixedHeight(lable->size().height());
        }

        {
            auto lable = form.off.lable;
            auto slider = form.off.slider;
            ui::adjust_label_size(lable, ui::ADJUST_HEIGHT);
            slider->setFixedHeight(lable->size().height());
        }

        if (form.comp.lable != NULL)
        {
            auto lable = form.comp.lable;
            auto slider = form.comp.slider;
            ui::adjust_label_size(lable, ui::ADJUST_HEIGHT);
            slider->setFixedHeight(lable->size().height());
        } 
    }
}

void Calibration::UpdateLanguage()
{
    updateLangText();
    retranslateUi();

    if (this->isVisible()){
        update_device_info();
    }
}

void Calibration::UpdateTheme()
{ 
}

void Calibration::UpdateFont()
{
    QFont font = this->font();
    font.setPointSizeF(AppConfig::Instance().appOptions.fontSize);
    ui::set_form_font(this, font);
}

} // namespace dialogs
} // namespace pv
