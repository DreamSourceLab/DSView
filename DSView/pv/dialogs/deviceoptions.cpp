/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include "deviceoptions.h"

#include <boost/foreach.hpp>

#include <QListWidget>
#include <QSpinBox>
#include <QDoubleSpinBox>

#include "dsmessagebox.h"
#include <pv/prop/property.h>

using namespace boost;
using namespace std;

namespace pv {
namespace dialogs {

DeviceOptions::DeviceOptions(QWidget *parent, boost::shared_ptr<pv::device::DevInst> dev_inst) :
    DSDialog(parent),
    _dev_inst(dev_inst),
    _button_box(QDialogButtonBox::Ok,
		Qt::Horizontal, this),
    _device_options_binding(_dev_inst->dev_inst())
{
    _props_box = new QGroupBox(tr("Mode"), this);
    _props_box->setLayout(get_property_form(_props_box));
    _layout.addWidget(_props_box);

    _dynamic_box = new QGroupBox(dynamic_widget(_dynamic_layout),
                                          this);
    _dynamic_box->setLayout(&_dynamic_layout);
    _layout.addWidget(_dynamic_box);
    _dynamic_box->setVisible(_dynamic_box->title() != NULL);

    _layout.addStretch(1);
	_layout.addWidget(&_button_box);

    layout()->addLayout(&_layout);
    setTitle(tr("Device Options"));

    connect(&_button_box, SIGNAL(accepted()), this, SLOT(accept()));
    //connect(&_button_box, SIGNAL(rejected()), this, SLOT(reject()));
    connect(_dev_inst.get(), SIGNAL(device_updated()), this, SLOT(reject()));

    GVariant* gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_OPERATION_MODE);
    if (gvar != NULL) {
        _mode = QString::fromUtf8(g_variant_get_string(gvar, NULL));
        g_variant_unref(gvar);
    }
    connect(&_mode_check, SIGNAL(timeout()), this, SLOT(mode_check()));
    _mode_check.setInterval(100);
    _mode_check.start();
}

void DeviceOptions::accept()
{
	using namespace Qt;
    bool hasEnabled = false;

	// Commit the properties
    const vector< boost::shared_ptr<pv::prop::Property> > &dev_props =
		_device_options_binding.properties();
    BOOST_FOREACH(boost::shared_ptr<pv::prop::Property> p, dev_props) {
		assert(p);
		p->commit();
	}

    // Commit the probes
    if (_dev_inst->dev_inst()->mode == LOGIC ||
            _dev_inst->dev_inst()->mode == ANALOG) {
        int index = 0;
        for (const GSList *l = _dev_inst->dev_inst()->channels; l; l = l->next) {
            sr_channel *const probe = (sr_channel*)l->data;
            assert(probe);
            probe->enabled = _probes_checkBox_list.at(index)->isChecked();
            index++;
            if (probe->enabled)
                hasEnabled = true;
        }
    } else {
        hasEnabled = true;
    }

    if (hasEnabled) {
        QVector<pv::prop::binding::ProbeOptions *>::iterator i = _probe_options_binding_list.begin();
        while(i != _probe_options_binding_list.end()) {
            const vector< boost::shared_ptr<pv::prop::Property> > &probe_props =
                    (*i)->properties();
            BOOST_FOREACH(boost::shared_ptr<pv::prop::Property> p, probe_props) {
                assert(p);
                p->commit();
            }
            i++;
        }

        QDialog::accept();
    } else {
        dialogs::DSMessageBox msg(this);
        msg.mBox()->setText(tr("Attention"));
        msg.mBox()->setInformativeText(tr("All channel disabled! Please enable at least one channel."));
        msg.mBox()->addButton(tr("Ok"), QMessageBox::AcceptRole);
        msg.mBox()->setIcon(QMessageBox::Warning);
        msg.exec();
    }
}

void DeviceOptions::reject()
{
    using namespace Qt;

    QDialog::reject();
}

QGridLayout * DeviceOptions::get_property_form(QWidget * parent)
{
    QGridLayout *const layout = new QGridLayout(parent);
    layout->setVerticalSpacing(5);

	const vector< boost::shared_ptr<pv::prop::Property> > &properties =
		_device_options_binding.properties();
    int i = 0;
    BOOST_FOREACH(boost::shared_ptr<pv::prop::Property> p, properties)
	{
		assert(p);
        const QString label = p->labeled_widget() ? QString() : p->name();
        layout->addWidget(new QLabel(label, parent), i, 0);
        if (label == tr("Operation Mode"))
            layout->addWidget(p->get_widget(parent, true), i, 1);
        else
            layout->addWidget(p->get_widget(parent), i, 1);
        i++;
	}

    return layout;
}

void DeviceOptions::logic_probes(QGridLayout &layout)
{
	using namespace Qt;

    int row0 = 0, row1 = 0, col = 0;
    int index = 0;
    QString ch_mode;
    int vld_ch_num = 0;
    int cur_ch_num = 0;

    while(layout.count() > 0)
    {
        //remove Widgets in QLayoutGrid
        QWidget* widget = layout.itemAt(0)->widget();
        layout.removeWidget(widget);
        delete widget;
    }
    _probes_label_list.clear();
    _probes_checkBox_list.clear();

    if (_dev_inst->dev_inst()->mode == LOGIC) {
        GVariant *gvar_opts;
        gsize num_opts;
        if (sr_config_list(_dev_inst->dev_inst()->driver, _dev_inst->dev_inst(), NULL, SR_CONF_CHANNEL_MODE,
            &gvar_opts) == SR_OK) {
            const char **const options = g_variant_get_strv(gvar_opts, &num_opts);
            GVariant* gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_CHANNEL_MODE);
            if (gvar != NULL) {
                ch_mode = QString::fromUtf8(g_variant_get_string(gvar, NULL));
                g_variant_unref(gvar);

                for (unsigned int i=0; i<num_opts; i++){
                    QRadioButton *ch_opts = new QRadioButton(options[i]);
                    layout.addWidget(ch_opts, row0, col, 1, 8);
                    connect(ch_opts, SIGNAL(pressed()), this, SLOT(channel_check()));
                    row0++;
                    if (QString::fromUtf8(options[i]) == ch_mode)
                        ch_opts->setChecked(true);
                }
            }
            g_variant_unref(gvar_opts);
        }
    }

    GVariant *gvar =  _dev_inst->get_config(NULL, NULL, SR_CONF_VLD_CH_NUM);
    if (gvar != NULL) {
        vld_ch_num = g_variant_get_int16(gvar);
        g_variant_unref(gvar);
    }

    for (const GSList *l = _dev_inst->dev_inst()->channels; l; l = l->next) {
		sr_channel *const probe = (sr_channel*)l->data;
		assert(probe);

        if (probe->enabled)
            cur_ch_num++;

        if (cur_ch_num > vld_ch_num)
            probe->enabled = false;

        QLabel *probe_label = new QLabel(QString::number(probe->index), this);
        QCheckBox *probe_checkBox = new QCheckBox(this);
        probe_checkBox->setCheckState(probe->enabled ? Qt::Checked : Qt::Unchecked);
        layout.addWidget(probe_label, row1 * 2 + row0, col);
        layout.addWidget(probe_checkBox, row1 * 2 + 1 + row0, col);
        _probes_label_list.push_back(probe_label);
        _probes_checkBox_list.push_back(probe_checkBox);

        index++;
        col = index % 8;
        row1 = index / 8;

        connect(probe_checkBox, SIGNAL(released()), this, SLOT(channel_enable()));
	}

    QPushButton *_enable_all_probes = new QPushButton(tr("Enable All"), this);
    QPushButton *_disable_all_probes = new QPushButton(tr("Disable All"), this);

    connect(_enable_all_probes, SIGNAL(clicked()),
        this, SLOT(enable_all_probes()));
    connect(_disable_all_probes, SIGNAL(clicked()),
        this, SLOT(disable_all_probes()));

    layout.addWidget(_enable_all_probes, (row1 + 1) * 2 + row0, 0, 1, 4);
    layout.addWidget(_disable_all_probes, (row1 + 1) * 2 + row0, 4, 1, 4);
}

void DeviceOptions::set_all_probes(bool set)
{
    QVector<QCheckBox *>::iterator i = _probes_checkBox_list.begin();
    while(i != _probes_checkBox_list.end()) {
        (*i)->setCheckState(set ? Qt::Checked : Qt::Unchecked);
        i++;
    }
}

void DeviceOptions::enable_max_probes() {
    int cur_ch_num = 0;
    QVector<QCheckBox *>::iterator iter = _probes_checkBox_list.begin();
    while(iter != _probes_checkBox_list.end()) {
        if ((*iter)->isChecked())
            cur_ch_num++;
        iter++;
    }

    GVariant* gvar =  _dev_inst->get_config(NULL, NULL, SR_CONF_VLD_CH_NUM);
    if (gvar == NULL)
        return;

    int vld_ch_num = g_variant_get_int16(gvar);
    g_variant_unref(gvar);
    iter = _probes_checkBox_list.begin();
    while(cur_ch_num < vld_ch_num &&
          iter != _probes_checkBox_list.end()) {
        if (!(*iter)->isChecked()) {
            (*iter)->setChecked(true);
            cur_ch_num++;
        }
        iter++;
    }
}

void DeviceOptions::enable_all_probes()
{
    GVariant* gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_STREAM);
    if (gvar != NULL) {
        bool stream_mode = g_variant_get_boolean(gvar);
        g_variant_unref(gvar);

        if (stream_mode) {
            enable_max_probes();
            return;
        }
    }

	set_all_probes(true);
}

void DeviceOptions::disable_all_probes()
{
	set_all_probes(false);
}

void DeviceOptions::zero_adj()
{
    using namespace Qt;
    QDialog::accept();

    dialogs::DSMessageBox msg(this);
    msg.mBox()->setText(tr("Information"));
    msg.mBox()->setInformativeText(tr("Auto Calibration program will be started. Please keep all channels out of singal input. It can take a while!"));
    //msg.mBox()->setStandardButtons(QMessageBox::);
    msg.mBox()->addButton(tr("Ok"), QMessageBox::AcceptRole);
    msg.mBox()->addButton(tr("Cancel"), QMessageBox::RejectRole);
    msg.mBox()->setIcon(QMessageBox::Information);
    if (msg.exec()) {
        _dev_inst->set_config(NULL, NULL, SR_CONF_ZERO, g_variant_new_boolean(true));
    }
}

void DeviceOptions::on_calibration()
{
    using namespace Qt;
    QDialog::accept();
    _dev_inst->set_config(NULL, NULL, SR_CONF_CALI, g_variant_new_boolean(true));
}

void DeviceOptions::mode_check()
{
    bool test;
    QString mode;
    GVariant* gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_OPERATION_MODE);
    if (gvar != NULL) {
        mode = QString::fromUtf8(g_variant_get_string(gvar, NULL));
        g_variant_unref(gvar);

        if (mode != _mode) {
            dynamic_widget(_dynamic_layout);
            _dynamic_box->setVisible(_dynamic_box->title() != NULL);
            _mode = mode;
        }
    }

    gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_TEST);
    if (gvar != NULL) {
        test = g_variant_get_boolean(gvar);
        g_variant_unref(gvar);

        if (test) {
            QVector<QCheckBox *>::iterator i = _probes_checkBox_list.begin();
            while(i != _probes_checkBox_list.end()) {
                (*i)->setCheckState(Qt::Checked);
                (*i)->setDisabled(TRUE);
                i++;
            }
        }
    }
}

void DeviceOptions::channel_check()
{
    QRadioButton* sc=dynamic_cast<QRadioButton*>(sender());
    QString text = sc->text();
    text.remove('&');
    if(sc != NULL)
        _dev_inst->set_config(NULL, NULL, SR_CONF_CHANNEL_MODE, g_variant_new_string(text.toUtf8().data()));
    dynamic_widget(_dynamic_layout);
    _dynamic_box->setVisible(_dynamic_box->title() != NULL);
}

void DeviceOptions::channel_enable()
{
    if (_dev_inst->dev_inst()->mode == LOGIC) {
        QCheckBox* sc=dynamic_cast<QCheckBox*>(sender());
        if (sc == NULL || !sc->isChecked())
            return;

        GVariant* gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_STREAM);
        if (gvar == NULL)
            return;

        bool stream_mode = g_variant_get_boolean(gvar);
        g_variant_unref(gvar);

        if (!stream_mode)
            return;

        int cur_ch_num = 0;
        QVector<QCheckBox *>::iterator i = _probes_checkBox_list.begin();
        while(i != _probes_checkBox_list.end()) {
            if ((*i)->isChecked())
                cur_ch_num++;
            i++;
        }

        gvar =  _dev_inst->get_config(NULL, NULL, SR_CONF_VLD_CH_NUM);
        if (gvar == NULL)
            return;

        int vld_ch_num = g_variant_get_int16(gvar);
        g_variant_unref(gvar);
        if (cur_ch_num > vld_ch_num) {
            dialogs::DSMessageBox msg(this);
            msg.mBox()->setText(tr("Information"));
            msg.mBox()->setInformativeText(tr("Current mode only suppport max ") + QString::number(vld_ch_num) + tr(" channels!"));
            msg.mBox()->addButton(tr("Ok"), QMessageBox::AcceptRole);
            msg.mBox()->setIcon(QMessageBox::Information);
            msg.exec();

            sc->setChecked(false);
        }
    } else if (_dev_inst->dev_inst()->mode == ANALOG) {
        QCheckBox* sc=dynamic_cast<QCheckBox*>(sender());
        if (sc != NULL) {
            QGridLayout *const layout = (QGridLayout *)sc->property("Layout").value<void *>();
            int i = layout->count();
            while(i--)
            {
                QWidget* w = layout->itemAt(i)->widget();
                if (w->property("Enable").isNull()) {
                    w->setEnabled(sc->isChecked());
                }
            }
            //dynamic_widget(_dynamic_layout);
        }
    }
}

QString DeviceOptions::dynamic_widget(QGridLayout& inner_layout) {
    if (_dev_inst->dev_inst()->mode == LOGIC) {
        logic_probes(inner_layout);
        return tr("Channels");
    } else if (_dev_inst->dev_inst()->mode == DSO) {
        GVariant* gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_HAVE_ZERO);
        if (gvar != NULL) {
            bool have_zero = g_variant_get_boolean(gvar);
            g_variant_unref(gvar);

            if (have_zero) {
                _config_button = new QPushButton(tr("Auto Calibration"), this);
                inner_layout.addWidget(_config_button, 0, 0, 1, 1);
                connect(_config_button, SIGNAL(clicked()), this, SLOT(zero_adj()));
                _cali_button = new QPushButton(tr("Manual Calibration"), this);
                inner_layout.addWidget(_cali_button, 1, 0, 1, 1);
                connect(_cali_button, SIGNAL(clicked()), this, SLOT(on_calibration()));

                return tr("Calibration");
            }
        }
    } else if (_dev_inst->dev_inst()->mode == ANALOG) {
        analog_probes(inner_layout);
        return tr("Channels");
    }
    return NULL;
}

void DeviceOptions::analog_probes(QGridLayout &layout)
{
    using namespace Qt;

    while(layout.count() > 0)
    {
        //remove Widgets in QLayoutGrid
        QWidget* widget = layout.itemAt(0)->widget();
        layout.removeWidget(widget);
        delete widget;
    }
    _probe_widget_list.clear();
    _probes_checkBox_list.clear();
    _probe_options_binding_list.clear();

    QTabWidget *tabWidget = new QTabWidget(this);
    tabWidget->setTabPosition(QTabWidget::North);
    tabWidget->setUsesScrollButtons(false);
    for (const GSList *l = _dev_inst->dev_inst()->channels; l; l = l->next) {
        sr_channel *const probe = (sr_channel*)l->data;
        assert(probe);

        QWidget *probe_widget = new QWidget(tabWidget);
        QGridLayout *probe_layout = new QGridLayout(probe_widget);
        probe_widget->setLayout(probe_layout);
        _probe_widget_list.push_back(probe_widget);

        QCheckBox *probe_checkBox = new QCheckBox(this);
        QVariant vlayout = QVariant::fromValue((void *)probe_layout);
        probe_checkBox->setProperty("Layout", vlayout);
        probe_checkBox->setProperty("Enable", true);
        probe_checkBox->setCheckState(probe->enabled ? Qt::Checked : Qt::Unchecked);
        _probes_checkBox_list.push_back(probe_checkBox);

        QLabel *en_label = new QLabel(tr("Enable: "), this);
        en_label->setProperty("Enable", true);
        probe_layout->addWidget(en_label, 0, 0, 1, 1);
        probe_layout->addWidget(probe_checkBox, 0, 1, 1, 3);


        pv::prop::binding::ProbeOptions *probe_options_binding =
                new pv::prop::binding::ProbeOptions(_dev_inst->dev_inst(), probe);
        const vector< boost::shared_ptr<pv::prop::Property> > &properties =
            probe_options_binding->properties();
        int i = 1;
        BOOST_FOREACH(boost::shared_ptr<pv::prop::Property> p, properties)
        {
            assert(p);
            probe_layout->addWidget(new QLabel(p->name(), probe_widget), i, 0, 1, 1);
            QWidget *pow = p->get_widget(probe_widget);
            pow->setEnabled(probe_checkBox->isChecked());
            probe_layout->addWidget(pow, i, 1, 1, 3);
            i++;
        }
        _probe_options_binding_list.push_back(probe_options_binding);

        connect(probe_checkBox, SIGNAL(released()), this, SLOT(channel_enable()));

        tabWidget->addTab(probe_widget, QString::fromUtf8(probe->name));
    }

    layout.addWidget(tabWidget, 0, 0, 1, 1);
}

} // namespace dialogs
} // namespace pv
