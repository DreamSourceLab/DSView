/*
 * This file is part of the DSLogic-gui project.
 * DSLogic-gui is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2013 DreamSourceLab <dreamsourcelab@dreamsourcelab.com>
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


#ifndef DSLOGIC_PV_DEVICEOPTIONS_H
#define DSLOGIC_PV_DEVICEOPTIONS_H

#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QVector>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QRadioButton>

#include <pv/prop/binding/deviceoptions.h>

namespace pv {
namespace dialogs {

class DeviceOptions : public QDialog
{
	Q_OBJECT

public:
	DeviceOptions(QWidget *parent, struct sr_dev_inst *sdi);

protected:
	void accept();
    void reject();

private:

	QWidget* get_property_form();

	void setup_probes();

	void set_all_probes(bool set);

private slots:
	void enable_all_probes();
	void disable_all_probes();

private:
	struct sr_dev_inst *const _sdi;
	QVBoxLayout _layout;

	QGroupBox _probes_box;
    QGridLayout _probes_box_layout;
    QVector <QLabel *> _probes_label_list;
    QVector <QCheckBox *> _probes_checkBox_list;

	QGroupBox _props_box;
    QVBoxLayout _props_box_layout;

	QDialogButtonBox _button_box;

	pv::prop::binding::DeviceOptions _device_options_binding;
};

} // namespace dialogs
} // namespace pv

#endif // DSLOGIC_PV_DEVICEOPTIONS_H
