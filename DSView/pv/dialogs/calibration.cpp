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

#include "../view/trace.h"
#include "../dialogs/dsmessagebox.h"

using namespace boost;
using namespace std;

namespace pv {
namespace dialogs {

const QString Calibration::VGAIN = QT_TR_NOOP(" VGAIN");
const QString Calibration::VOFF = QT_TR_NOOP(" VOFF");
const QString Calibration::VCOMB = QT_TR_NOOP(" VCOMB");

Calibration::Calibration(QWidget *parent) :
    DSDialog(parent)
{
#ifdef Q_OS_DARWIN
    Qt::WindowFlags flags = windowFlags();
    this->setWindowFlags(flags | Qt::Tool);
#endif
    this->setFixedSize(400, 250);
    this->setWindowOpacity(0.7);
    this->setModal(false);

    _dev_inst = NULL;
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

void Calibration::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    DSDialog::changeEvent(event);
}

void Calibration::retranslateUi()
{
    _save_btn->setText(tr("Save"));
    _abort_btn->setText(tr("Abort"));
    _reset_btn->setText(tr("Reset"));
    _exit_btn->setText(tr("Exit"));

    setTitle(tr("Manual Calibration"));
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

        uint64_t vgain = 0, vgain_default = 0;
        uint16_t vgain_range = 0;
        GVariant* gvar = _dev_inst->get_config(probe, NULL, SR_CONF_PROBE_VGAIN);
        if (gvar != NULL) {
            vgain = g_variant_get_uint64(gvar);
            g_variant_unref(gvar);
        }
        gvar = _dev_inst->get_config(probe, NULL, SR_CONF_PROBE_VGAIN_DEFAULT);
        if (gvar != NULL) {
            vgain_default = g_variant_get_uint64(gvar);
            g_variant_unref(gvar);
        }
        gvar = _dev_inst->get_config(probe, NULL, SR_CONF_PROBE_VGAIN_RANGE);
        if (gvar != NULL) {
            vgain_range = g_variant_get_uint16(gvar);
            g_variant_unref(gvar);
        }

        QSlider *gain_slider = new QSlider(Qt::Horizontal, this);
        gain_slider->setRange(-vgain_range/2, vgain_range/2);
        gain_slider->setValue(vgain - vgain_default);
        gain_slider->setObjectName(VGAIN+probe->index);
        QString gain_string = tr("Channel") + QString::number(probe->index) + VGAIN;
        QLabel *gain_label = new QLabel(gain_string, this);
        _flayout->addRow(gain_label, gain_slider);
        _slider_list.push_back(gain_slider);
        _label_list.push_back(gain_label);

        uint64_t voff = 0;
        uint16_t voff_range = 0;
        gvar = _dev_inst->get_config(probe, NULL, SR_CONF_PROBE_PREOFF);
        if (gvar != NULL) {
            voff = g_variant_get_uint16(gvar);
            g_variant_unref(gvar);
        }
        gvar = _dev_inst->get_config(probe, NULL, SR_CONF_PROBE_PREOFF_MARGIN);
        if (gvar != NULL) {
            voff_range = g_variant_get_uint16(gvar);
            g_variant_unref(gvar);
        }
        QSlider *off_slider = new QSlider(Qt::Horizontal, this);
        off_slider->setRange(0, voff_range);
        off_slider->setValue(voff);
        off_slider->setObjectName(VOFF+probe->index);
        QString off_string = tr("Channel") + QString::number(probe->index) + VOFF;
        QLabel *off_label = new QLabel(off_string, this);
        _flayout->addRow(off_label, off_slider);
        _slider_list.push_back(off_slider);
        _label_list.push_back(off_label);

        bool comb_comp_en = false;
        gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_PROBE_COMB_COMP_EN);
        if (gvar != NULL) {
            comb_comp_en = g_variant_get_boolean(gvar);
            g_variant_unref(gvar);
        }
        if (comb_comp_en) {
            int16_t comb_comp = 0;
            gvar = _dev_inst->get_config(probe, NULL, SR_CONF_PROBE_COMB_COMP);
            if (gvar != NULL) {
                comb_comp = g_variant_get_int16(gvar);
                g_variant_unref(gvar);
            }
            QSlider *comp_slider = new QSlider(Qt::Horizontal, this);
            comp_slider->setRange(-127, 127);
            comp_slider->setValue(comb_comp);
            comp_slider->setObjectName(VCOMB+probe->index);
            QString comp_string = tr("Channel") + QString::number(probe->index) + VCOMB;
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
            GVariant* gvar = _dev_inst->get_config(probe, NULL, SR_CONF_PROBE_VGAIN_DEFAULT);
            if (gvar != NULL) {
                vgain_default = g_variant_get_uint64(gvar);
                g_variant_unref(gvar);
                _dev_inst->set_config(probe, NULL, SR_CONF_PROBE_VGAIN,
                                      g_variant_new_uint64(value+vgain_default));
            }
            break;
        } else if (sc->objectName() == VOFF+probe->index) {
            _dev_inst->set_config(probe, NULL, SR_CONF_PROBE_PREOFF,
                                  g_variant_new_uint16(value));
            break;
        } else if (sc->objectName() == VCOMB+probe->index) {
            _dev_inst->set_config(probe, NULL, SR_CONF_PROBE_COMB_COMP,
                                  g_variant_new_int16(value));
            break;
        }
    }
}

void Calibration::on_save()
{
    this->hide();
    QFuture<void> future;
    future = QtConcurrent::run([&]{
        //QTime dieTime = QTime::currentTime().addSecs(1);
        _dev_inst->set_config(NULL, NULL, SR_CONF_ZERO_SET,
                              g_variant_new_boolean(true));
        //while( QTime::currentTime() < dieTime );
    });
    Qt::WindowFlags flags = Qt::CustomizeWindowHint;
    QProgressDialog dlg(tr("Save calibration results... It can take a while."),
                        tr("Cancel"),0,0,this,flags);
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
        //QTime dieTime = QTime::currentTime().addSecs(1);
        _dev_inst->set_config(NULL, NULL, SR_CONF_ZERO_LOAD,
                              g_variant_new_boolean(true));
        reload_value();
        //while( QTime::currentTime() < dieTime );
    });
    Qt::WindowFlags flags = Qt::CustomizeWindowHint;
    QProgressDialog dlg(tr("Reload last calibration results... It can take a while."),
                        tr("Cancel"),0,0,this,flags);
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
    for (const GSList *l = _dev_inst->dev_inst()->channels; l; l = l->next) {
        sr_channel *const probe = (sr_channel*)l->data;
        assert(probe);

        uint64_t vgain = 0, vgain_default = 0;
        uint16_t vgain_range = 0;
        GVariant* gvar = _dev_inst->get_config(probe, NULL, SR_CONF_PROBE_VGAIN);
        if (gvar != NULL) {
            vgain = g_variant_get_uint64(gvar);
            g_variant_unref(gvar);
        }
        gvar = _dev_inst->get_config(probe, NULL, SR_CONF_PROBE_VGAIN_DEFAULT);
        if (gvar != NULL) {
            vgain_default = g_variant_get_uint64(gvar);
            g_variant_unref(gvar);
        }
        gvar = _dev_inst->get_config(probe, NULL, SR_CONF_PROBE_VGAIN_RANGE);
        if (gvar != NULL) {
            vgain_range = g_variant_get_uint16(gvar);
            g_variant_unref(gvar);
        }

        uint64_t voff = 0;
        uint16_t voff_range = 0;
        gvar = _dev_inst->get_config(probe, NULL, SR_CONF_PROBE_PREOFF);
        if (gvar != NULL) {
            voff = g_variant_get_uint16(gvar);
            g_variant_unref(gvar);
        }
        gvar = _dev_inst->get_config(probe, NULL, SR_CONF_PROBE_PREOFF_MARGIN);
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

void Calibration::on_reset()
{

    dialogs::DSMessageBox msg(this);
    msg.mBox()->setText(tr("Attention"));
    msg.mBox()->setInformativeText(tr("All calibration settings will become the defualt values!"));
    msg.mBox()->addButton(tr("Ok"), QMessageBox::AcceptRole);
    msg.mBox()->addButton(tr("Cancel"), QMessageBox::RejectRole);
    msg.mBox()->setIcon(QMessageBox::Warning);
    if (msg.exec()) {
        _dev_inst->set_config(NULL, NULL, SR_CONF_ZERO_DEFAULT,
                              g_variant_new_boolean(true));
        reload_value();
    }
}

} // namespace dialogs
} // namespace pv
