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

#include <QFormLayout>
#include <QListWidget>

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

    if (_dev_inst->dev_inst()->mode != DSO) {
        _probes_box = new QGroupBox(tr("Channels"), this);
        setup_probes();
        _probes_box->setLayout(&_probes_box_layout);
        _layout.addWidget(_probes_box);
    } else if (_dev_inst->name().contains("DSCope")){
        _config_button = new QPushButton(tr("Zero Adjustment"), this);
        _layout.addWidget(_config_button);
        connect(_config_button, SIGNAL(clicked()), this, SLOT(zero_adj()));

        _cali_button = new QPushButton(tr("Manual Calibration"), this);
        _layout.addWidget(_cali_button);
        connect(_cali_button, SIGNAL(clicked()), this, SLOT(on_calibration()));
    }

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

	QDialog::accept();

	// Commit the properties
	const vector< boost::shared_ptr<pv::prop::Property> > &properties =
		_device_options_binding.properties();
	BOOST_FOREACH(boost::shared_ptr<pv::prop::Property> p, properties) {
		assert(p);
		p->commit();
	}

    // Commit the probes
    if (_dev_inst->dev_inst()->mode != DSO) {
        int index = 0;
        for (const GSList *l = _dev_inst->dev_inst()->channels; l; l = l->next) {
            sr_channel *const probe = (sr_channel*)l->data;
            assert(probe);

            probe->enabled = (_probes_checkBox_list.at(index)->checkState() == Qt::Checked);
            index++;
        }
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

void DeviceOptions::setup_probes()
{
	using namespace Qt;

    int row0 = 0, row1 = 0, col = 0;
    int index = 0;
    QString ch_mode;

    while(_probes_box_layout.count() > 0)
    {
        //remove Widgets in QLayoutGrid
        QWidget* widget = _probes_box_layout.itemAt(0)->widget();
        _probes_box_layout.removeWidget(widget);
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
                    _probes_box_layout.addWidget(ch_opts, row0, col, 1, 8);
                    connect(ch_opts, SIGNAL(pressed()), this, SLOT(channel_check()));
                    row0++;
                    if (QString::fromUtf8(options[i]) == ch_mode)
                        ch_opts->setChecked(true);
                }
            }
            g_variant_unref(gvar_opts);
        }
    }

    for (const GSList *l = _dev_inst->dev_inst()->channels; l; l = l->next) {
		sr_channel *const probe = (sr_channel*)l->data;
		assert(probe);

        QLabel *probe_label = new QLabel(QString::number(probe->index), this);
        QCheckBox *probe_checkBox = new QCheckBox(this);
        probe_checkBox->setCheckState(probe->enabled ? Qt::Checked : Qt::Unchecked);
        _probes_box_layout.addWidget(probe_label, row1 * 2 + row0, col);
        _probes_box_layout.addWidget(probe_checkBox, row1 * 2 + 1 + row0, col);
        _probes_label_list.push_back(probe_label);
        _probes_checkBox_list.push_back(probe_checkBox);

        index++;
        col = index % 8;
        row1 = index / 8;
	}

    QPushButton *_enable_all_probes = new QPushButton(tr("Enable All"), this);
    QPushButton *_disable_all_probes = new QPushButton(tr("Disable All"), this);

    connect(_enable_all_probes, SIGNAL(clicked()),
        this, SLOT(enable_all_probes()));
    connect(_disable_all_probes, SIGNAL(clicked()),
        this, SLOT(disable_all_probes()));

    _probes_box_layout.addWidget(_enable_all_probes, (row1 + 1) * 2 + row0, 0, 1, 4);
    _probes_box_layout.addWidget(_disable_all_probes, (row1 + 1) * 2 + row0, 4, 1, 4);
}

void DeviceOptions::set_all_probes(bool set)
{
    QVector<QCheckBox *>::iterator i = _probes_checkBox_list.begin();
    while(i != _probes_checkBox_list.end()) {
        (*i)->setCheckState(set ? Qt::Checked : Qt::Unchecked);
        i++;
    }
}

void DeviceOptions::enable_all_probes()
{
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
    msg.mBox()->setInformativeText(tr("Zero adjustment program will be started. Please keep all channels out of singal input. It can take a while!"));
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
            setup_probes();
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
    if(sc != NULL)
        _dev_inst->set_config(NULL, NULL, SR_CONF_CHANNEL_MODE, g_variant_new_string(sc->text().toUtf8().data()));
    setup_probes();
}

} // namespace dialogs
} // namespace pv
