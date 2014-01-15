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


#ifndef DSLOGIC_PV_CONNECT_H
#define DSLOGIC_PV_CONNECT_H

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

struct sr_config;
struct sr_dev_inst;

namespace pv {

class DeviceManager;

namespace dialogs {

class Connect : public QDialog
{
	Q_OBJECT

public:
	Connect(QWidget *parent, pv::DeviceManager &device_manager);

	struct sr_dev_inst* get_selected_device() const;

private:
	void populate_drivers();

	void unset_connection();

	void set_serial_connection();

private slots:
	void device_selected(int index);

	void scan_pressed();

private:
	static void free_drvopts(sr_config *src);

private:
	pv::DeviceManager &_device_manager;

	QVBoxLayout _layout;

	QWidget _form;
	QFormLayout _form_layout;

	QComboBox _drivers;

	QLineEdit _serial_device;

	QPushButton _scan_button;
	QListWidget _device_list;

	QDialogButtonBox _button_box;
};

} // namespace dialogs
} // namespace pv

#endif // DSLOGIC_PV_CONNECT_H
