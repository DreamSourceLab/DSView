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

#include <boost/foreach.hpp>

#include <QGridLayout>
#include <QFuture>
#include <QProgressDialog>
#include <QtConcurrent/QtConcurrent>
#include <QTime>

#include "libsigrok4DSL/libsigrok.h"
#include "../view/trace.h"

using namespace boost;
using namespace std;

namespace pv {
namespace dialogs {

const QString Calibration::VGAIN = tr(" VGAIN");
const QString Calibration::VOFF = tr(" VOFF");

Calibration::Calibration(QWidget *parent) :
    QDialog(parent)
{
    this->setFixedSize(400, 200);
    this->setWindowOpacity(0.7);
    this->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    this->setModal(false);

    _dev_inst = NULL;
    _save_btn = new QPushButton(tr("Save"), this);
    _reset_btn = new QPushButton(tr("Reset"), this);
    _exit_btn = new QPushButton(tr("Exit"), this);

    _flayout = new QFormLayout();
    QGridLayout *glayout = new QGridLayout(this);
    glayout->addLayout(_flayout, 0, 0, 1, 5);
    glayout->addWidget(_save_btn, 1, 0);
    glayout->addWidget(new QWidget(this), 1, 1);
    glayout->setColumnStretch(1, 1);
    glayout->addWidget(_reset_btn, 1, 2);
    glayout->addWidget(new QWidget(this), 1, 3);
    glayout->setColumnStretch(3, 1);
    glayout->addWidget(_exit_btn, 1, 4);
    setLayout(glayout);

    connect(_save_btn, SIGNAL(clicked()), this, SLOT(on_save()));
    connect(_reset_btn, SIGNAL(clicked()), this, SLOT(on_reset()));
    connect(_exit_btn, SIGNAL(clicked()), this, SLOT(reject()));
}

void Calibration::set_device(boost::shared_ptr<device::DevInst> dev_inst)
{
    assert(dev_inst);
    _dev_inst = dev_inst;

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

    for (const GSList *l = _dev_inst->dev_inst()->channels; l; l = l->next) {
        sr_channel *const probe = (sr_channel*)l->data;
        assert(probe);

        uint64_t vgain, vgain_default;
        uint16_t vgain_range;
        GVariant* gvar = _dev_inst->get_config(probe, NULL, SR_CONF_VGAIN);
        if (gvar != NULL) {
            vgain = g_variant_get_uint64(gvar);
            g_variant_unref(gvar);
        }
        gvar = _dev_inst->get_config(probe, NULL, SR_CONF_VGAIN_DEFAULT);
        if (gvar != NULL) {
            vgain_default = g_variant_get_uint64(gvar);
            g_variant_unref(gvar);
        }
        gvar = _dev_inst->get_config(probe, NULL, SR_CONF_VGAIN_RANGE);
        if (gvar != NULL) {
            vgain_range = g_variant_get_uint16(gvar);
            g_variant_unref(gvar);
        }

        QSlider *gain_slider = new QSlider(Qt::Horizontal, this);
        gain_slider->setRange(-vgain_range/2, vgain_range/2);
        gain_slider->setValue(vgain - vgain_default);
        gain_slider->setObjectName(VGAIN+probe->index);
        QString gain_string = "Channel" + QString::number(probe->index) + VGAIN;
        QLabel *gain_label = new QLabel(gain_string, this);
        _flayout->addRow(gain_label, gain_slider);
        _slider_list.push_back(gain_slider);
        _label_list.push_back(gain_label);

        uint64_t voff, voff_default;
        uint16_t voff_range;
        gvar = _dev_inst->get_config(probe, NULL, SR_CONF_VOFF);
        if (gvar != NULL) {
            voff = g_variant_get_uint16(gvar);
            g_variant_unref(gvar);
        }
        gvar = _dev_inst->get_config(probe, NULL, SR_CONF_VOFF_DEFAULT);
        if (gvar != NULL) {
            voff_default = g_variant_get_uint16(gvar);
            g_variant_unref(gvar);
        }
        gvar = _dev_inst->get_config(probe, NULL, SR_CONF_VOFF_RANGE);
        if (gvar != NULL) {
            voff_range = g_variant_get_uint16(gvar);
            g_variant_unref(gvar);
        }
        QSlider *off_slider = new QSlider(Qt::Horizontal, this);
        off_slider->setRange(0, voff_range);
        off_slider->setValue(voff);
        off_slider->setObjectName(VOFF+probe->index);
        QString off_string = "Channel" + QString::number(probe->index) + VOFF;
        QLabel *off_label = new QLabel(off_string, this);
        _flayout->addRow(off_label, off_slider);
        _slider_list.push_back(off_slider);
        _label_list.push_back(off_label);

        connect(gain_slider, SIGNAL(valueChanged(int)), this, SLOT(set_value(int)));
        connect(off_slider, SIGNAL(valueChanged(int)), this, SLOT(set_value(int)));
    }

    update();
}

void Calibration::accept()
{
    using namespace Qt;
    _dev_inst->set_config(NULL, NULL, SR_CONF_CALI, g_variant_new_boolean(false));
    QDialog::accept();
}

void Calibration::reject()
{
    using namespace Qt;
    _dev_inst->set_config(NULL, NULL, SR_CONF_CALI, g_variant_new_boolean(false));
    QDialog::reject();
}

void Calibration::set_value(int value)
{
    QSlider* sc = dynamic_cast<QSlider *>(sender());

    for (const GSList *l = _dev_inst->dev_inst()->channels; l; l = l->next) {
        sr_channel *const probe = (sr_channel*)l->data;
        assert(probe);
        if (sc->objectName() == VGAIN+probe->index) {
            uint64_t vgain_default;
            GVariant* gvar = _dev_inst->get_config(probe, NULL, SR_CONF_VGAIN_DEFAULT);
            if (gvar != NULL) {
                vgain_default = g_variant_get_uint64(gvar);
                g_variant_unref(gvar);
                _dev_inst->set_config(probe, NULL, SR_CONF_VGAIN,
                                      g_variant_new_uint64(value+vgain_default));
            }
            break;
        } else if (sc->objectName() == VOFF+probe->index) {
            _dev_inst->set_config(probe, NULL, SR_CONF_VOFF,
                                  g_variant_new_uint16(value));
            break;
        }
    }
}

void Calibration::on_save()
{
    this->hide();
    QFuture<void> future;
    future = QtConcurrent::run([&]{
        QTime dieTime = QTime::currentTime().addSecs(1);
        _dev_inst->set_config(NULL, NULL, SR_CONF_ZERO_SET,
                              g_variant_new_boolean(true));
        while( QTime::currentTime() < dieTime );
    });
    Qt::WindowFlags flags = Qt::CustomizeWindowHint;
    QProgressDialog dlg(tr("Save Calibration Result... It can take a while."),
                        tr("Cancel"),0,0,this,flags);
    dlg.setWindowModality(Qt::WindowModal);
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dlg.setCancelButton(NULL);

    QFutureWatcher<void> watcher;
    connect(&watcher,SIGNAL(finished()),&dlg,SLOT(cancel()));
    watcher.setFuture(future);

    dlg.exec();
    this->show();
}

void Calibration::on_reset()
{
    this->hide();
    QFuture<void> future;
    future = QtConcurrent::run([&]{
        QTime dieTime = QTime::currentTime().addSecs(1);
        _dev_inst->set_config(NULL, NULL, SR_CONF_ZERO_LOAD,
                              g_variant_new_boolean(true));
        reload_value();
        while( QTime::currentTime() < dieTime );
    });
    Qt::WindowFlags flags = Qt::CustomizeWindowHint;
    QProgressDialog dlg(tr("Reset Calibration Result... It can take a while."),
                        tr("Cancel"),0,0,this,flags);
    dlg.setWindowModality(Qt::WindowModal);
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dlg.setCancelButton(NULL);

    QFutureWatcher<void> watcher;
    connect(&watcher,SIGNAL(finished()),&dlg,SLOT(cancel()));
    watcher.setFuture(future);

    dlg.exec();
    this->show();
}

void Calibration::reload_value()
{
    for (const GSList *l = _dev_inst->dev_inst()->channels; l; l = l->next) {
        sr_channel *const probe = (sr_channel*)l->data;
        assert(probe);

        uint64_t vgain, vgain_default;
        uint16_t vgain_range;
        GVariant* gvar = _dev_inst->get_config(probe, NULL, SR_CONF_VGAIN);
        if (gvar != NULL) {
            vgain = g_variant_get_uint64(gvar);
            g_variant_unref(gvar);
        }
        gvar = _dev_inst->get_config(probe, NULL, SR_CONF_VGAIN_DEFAULT);
        if (gvar != NULL) {
            vgain_default = g_variant_get_uint64(gvar);
            g_variant_unref(gvar);
        }
        gvar = _dev_inst->get_config(probe, NULL, SR_CONF_VGAIN_RANGE);
        if (gvar != NULL) {
            vgain_range = g_variant_get_uint16(gvar);
            g_variant_unref(gvar);
        }

        uint64_t voff;
        uint16_t voff_range;
        gvar = _dev_inst->get_config(probe, NULL, SR_CONF_VOFF);
        if (gvar != NULL) {
            voff = g_variant_get_uint16(gvar);
            g_variant_unref(gvar);
        }
        gvar = _dev_inst->get_config(probe, NULL, SR_CONF_VOFF_RANGE);
        if (gvar != NULL) {
            voff_range = g_variant_get_uint16(gvar);
            g_variant_unref(gvar);
        }

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

} // namespace dialogs
} // namespace pv
