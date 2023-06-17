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

using namespace std;

namespace pv {
namespace dialogs {

const QString Calibration::VGAIN = QT_TR_NOOP(" VGAIN");
const QString Calibration::VOFF = QT_TR_NOOP(" VOFF");
const QString Calibration::VCOMB = QT_TR_NOOP(" VCOMB");

Calibration::Calibration(QWidget *parent) :
    DSDialog(parent)
{ 
    _save_btn = NULL;
    _abort_btn = NULL;
    _reset_btn = NULL;
    _exit_btn = NULL;
    _flayout = NULL;

#ifdef Q_OS_DARWIN
    Qt::WindowFlags flags = windowFlags();
    this->setWindowFlags(flags | Qt::Tool);
#endif
    this->setFixedSize(400, 250);
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

    connect(_save_btn, SIGNAL(clicked()), this, SLOT(on_save()));
    connect(_abort_btn, SIGNAL(clicked()), this, SLOT(on_abort()));
    connect(_reset_btn, SIGNAL(clicked()), this, SLOT(on_reset()));
    connect(_exit_btn, SIGNAL(clicked()), this, SLOT(reject()));

    retranslateUi();
}

Calibration::~Calibration(){ 
    DESTROY_QT_OBJECT(_save_btn);
    DESTROY_QT_OBJECT(_abort_btn);
    DESTROY_QT_OBJECT(_reset_btn);
    DESTROY_QT_OBJECT(_exit_btn);
    DESTROY_QT_OBJECT(_flayout);
}

void Calibration::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    DSDialog::changeEvent(event);
}

void Calibration::retranslateUi()
{
    _save_btn->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SAVE), "Save"));
    _abort_btn->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_ABORT), "Abort"));
    _reset_btn->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_RESET), "Reset"));
    _exit_btn->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_EXIT), "Exit"));

    setTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_MANUAL_CALIBRATION), "Manual Calibration"));
}

void Calibration::update_device_info()
{ 
    if (_device_agent->have_instance() == false){
        assert(false);
    }

    for(std::list<QSlider *>::const_iterator i = _slider_list.begin();
        i != _slider_list.end(); i++) {
        (*i)->setParent(NULL);
        _flayout->removeWidget((*i));
        delete (*i);
    }

    _slider_list.clear();
    for(std::list<QLabel *>::const_iterator i = _label_list.begin();
        i != _label_list.end(); i++) {
        (*i)->setParent(NULL);
        _flayout->removeWidget((*i));
        delete (*i);
    }
    _label_list.clear();

    for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
        sr_channel *const probe = (sr_channel*)l->data;
        assert(probe);

        uint64_t vgain = 0, vgain_default = 0;
        int vgain_range = 0;
        
        _device_agent->get_config_uint64(SR_CONF_PROBE_VGAIN, vgain, probe, NULL);
        _device_agent->get_config_uint64(SR_CONF_PROBE_VGAIN_DEFAULT, vgain_default, probe, NULL);   
        _device_agent->get_config_uint16(SR_CONF_PROBE_VGAIN_RANGE, vgain_range, probe, NULL);

        QSlider *gain_slider = new QSlider(Qt::Horizontal, this);
        gain_slider->setRange(-vgain_range/2, vgain_range/2);
        gain_slider->setValue(vgain - vgain_default);
        gain_slider->setObjectName(VGAIN+probe->index);
        QString gain_string = L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CHANNEL), "Channel") + QString::number(probe->index) + VGAIN;
        QLabel *gain_label = new QLabel(gain_string, this);
        _flayout->addRow(gain_label, gain_slider);
        _slider_list.push_back(gain_slider);
        _label_list.push_back(gain_label);

        uint64_t voff = 0;
        uint16_t voff_range = 0;
        int v;
 
        if (_device_agent->get_config_uint16(SR_CONF_PROBE_PREOFF, v, probe, NULL)) {
            voff = (uint64_t)v;
        }
        if (_device_agent->get_config_uint16(SR_CONF_PROBE_PREOFF_MARGIN, v, probe, NULL)) {
            voff_range = (uint16_t)v;
        }

        QSlider *off_slider = new QSlider(Qt::Horizontal, this);
        off_slider->setRange(0, voff_range);
        off_slider->setValue(voff);
        off_slider->setObjectName(VOFF+probe->index);
        QString off_string = L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CHANNEL), "Channel") + QString::number(probe->index) + VOFF;
        QLabel *off_label = new QLabel(off_string, this);
        _flayout->addRow(off_label, off_slider);
        _slider_list.push_back(off_slider);
        _label_list.push_back(off_label);

        bool comb_comp_en = false;
        _device_agent->get_config_bool(SR_CONF_PROBE_COMB_COMP_EN, comb_comp_en);

        if (comb_comp_en) {
            int comb_comp = 0;
           _device_agent->get_config_int16(SR_CONF_PROBE_COMB_COMP, comb_comp, probe, NULL);

            QSlider *comp_slider = new QSlider(Qt::Horizontal, this);
            comp_slider->setRange(-127, 127);
            comp_slider->setValue(comb_comp);
            comp_slider->setObjectName(VCOMB+probe->index);
            QString comp_string = L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CHANNEL), "Channel") + QString::number(probe->index) + VCOMB;
            QLabel *comp_label = new QLabel(comp_string, this);
            _flayout->addRow(comp_label, comp_slider);
            _slider_list.push_back(comp_slider);
            _label_list.push_back(comp_label);
            connect(comp_slider, SIGNAL(valueChanged(int)), this, SLOT(set_value(int)));
        }

        connect(gain_slider, SIGNAL(valueChanged(int)), this, SLOT(set_value(int)));
        connect(off_slider, SIGNAL(valueChanged(int)), this, SLOT(set_value(int)));
    }

    update();
}

void Calibration::accept()
{
    using namespace Qt;
    _device_agent->set_config_bool(SR_CONF_CALI, false);
    QDialog::accept();
}

void Calibration::reject()
{
    using namespace Qt;
    _device_agent->set_config_bool(SR_CONF_CALI, false);
    QDialog::reject();
}

void Calibration::set_value(int value)
{
    QSlider* sc = dynamic_cast<QSlider *>(sender());

    for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
        sr_channel *const probe = (sr_channel*)l->data;
        assert(probe);
        if (sc->objectName() == VGAIN+probe->index) {
            uint64_t vgain_default;
            if (_device_agent->get_config_uint64(SR_CONF_PROBE_VGAIN_DEFAULT, vgain_default, probe))
            {
                _device_agent->set_config_uint64(SR_CONF_PROBE_VGAIN, value+vgain_default, probe);          
            }
            break;
        }
        else if (sc->objectName() == VOFF+probe->index) {
           _device_agent->set_config_uint16(SR_CONF_PROBE_PREOFF, value, probe); 
            break;
        } else if (sc->objectName() == VCOMB+probe->index) {
            _device_agent->set_config_int16(SR_CONF_PROBE_COMB_COMP, value, probe);
            break;
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
    for (const GSList *l = _device_agent->get_channels(); l; l = l->next) {
        sr_channel *const probe = (sr_channel*)l->data;
        assert(probe);

        uint64_t vgain = 0, vgain_default = 0;
        int vgain_range = 0;
        _device_agent->get_config_uint64(SR_CONF_PROBE_VGAIN, vgain, probe, NULL);
        _device_agent->get_config_uint64(SR_CONF_PROBE_VGAIN_DEFAULT, vgain_default, probe, NULL);   
        _device_agent->get_config_uint16(SR_CONF_PROBE_VGAIN_RANGE, vgain_range, probe, NULL);

        int voff = 0;
        int voff_range = 0;

        _device_agent->get_config_uint16(SR_CONF_PROBE_PREOFF, voff, probe, NULL);
        _device_agent->get_config_uint16(SR_CONF_PROBE_PREOFF_MARGIN, voff_range, probe, NULL);

        for(std::list<QSlider*>::iterator i = _slider_list.begin();
            i != _slider_list.end(); i++) {
            if ((*i)->objectName() == VGAIN+probe->index) {
                (*i)->setRange(-vgain_range/2, vgain_range/2);
                (*i)->setValue(vgain - vgain_default);
            } else if ((*i)->objectName() == VOFF+probe->index) {
                (*i)->setRange(0, voff_range);
                (*i)->setValue(voff);
            }
        }
    }
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

} // namespace dialogs
} // namespace pv
