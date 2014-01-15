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


#include <boost/foreach.hpp>

#include "connect.h"

#include "pv/devicemanager.h"

extern "C" {
/* __STDC_FORMAT_MACROS is required for PRIu64 and friends (in C++). */
#define __STDC_FORMAT_MACROS
#include <glib.h>
#include <libsigrok4DSLogic/libsigrok.h>
}

using namespace std;

extern sr_context *sr_ctx;

namespace pv {
namespace dialogs {

Connect::Connect(QWidget *parent, pv::DeviceManager &device_manager) :
	QDialog(parent),
	_device_manager(device_manager),
	_layout(this),
	_form(this),
	_form_layout(&_form),
	_drivers(&_form),
	_serial_device(&_form),
	_scan_button(tr("Scan for Devices"), this),
	_device_list(this),
	_button_box(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
		Qt::Horizontal, this)
{
	setWindowTitle(tr("Connect to Device"));

	connect(&_button_box, SIGNAL(accepted()), this, SLOT(accept()));
	connect(&_button_box, SIGNAL(rejected()), this, SLOT(reject()));

	populate_drivers();
	connect(&_drivers, SIGNAL(activated(int)),
		this, SLOT(device_selected(int)));

	_form.setLayout(&_form_layout);
	_form_layout.addRow(tr("Driver"), &_drivers);

	_form_layout.addRow(tr("Serial Port"), &_serial_device);

	unset_connection();

	connect(&_scan_button, SIGNAL(pressed()),
		this, SLOT(scan_pressed()));

	setLayout(&_layout);
	_layout.addWidget(&_form);
	_layout.addWidget(&_scan_button);
	_layout.addWidget(&_device_list);
	_layout.addWidget(&_button_box);
}

struct sr_dev_inst* Connect::get_selected_device() const
{
	const QListWidgetItem *const item = _device_list.currentItem();
	if (!item)
		return NULL;

	return (sr_dev_inst*)item->data(Qt::UserRole).value<void*>();
}

void Connect::populate_drivers()
{
	gsize num_opts = 0;
	const int32_t *hwopts;
	struct sr_dev_driver **drivers = sr_driver_list();
	GVariant *gvar_opts;

	for (int i = 0; drivers[i]; ++i) {
		/**
		 * We currently only support devices that can deliver
		 * samples at a fixed samplerate i.e. oscilloscopes and
		 * logic analysers.
		 * @todo Add support for non-monotonic devices i.e. DMMs
		 * and sensors.
		 */
		bool supported_device = false;
		if ((sr_config_list(drivers[i], SR_CONF_DEVICE_OPTIONS,
				&gvar_opts, NULL) == SR_OK)) {
			hwopts = (const int32_t *)g_variant_get_fixed_array(gvar_opts,
					&num_opts, sizeof(int32_t));
			for (unsigned int j = 0; j < num_opts; j++)
				if (hwopts[j] == SR_CONF_SAMPLERATE) {
					supported_device = true;
					break;
				}
		}

		if (supported_device)
			_drivers.addItem(QString("%1 (%2)").arg(
				drivers[i]->longname).arg(drivers[i]->name),
				qVariantFromValue((void*)drivers[i]));
	}
}

void Connect::unset_connection()
{
	_device_list.clear();
	_serial_device.hide();
	_form_layout.labelForField(&_serial_device)->hide();
	_button_box.button(QDialogButtonBox::Ok)->setDisabled(true);
}

void Connect::set_serial_connection()
{
	_serial_device.show();
	_form_layout.labelForField(&_serial_device)->show();
}

void Connect::scan_pressed()
{
	_device_list.clear();

	const int index = _drivers.currentIndex();
	if (index == -1)
		return;

	sr_dev_driver *const driver = (sr_dev_driver*)_drivers.itemData(
		index).value<void*>();

	GSList *drvopts = NULL;

	if (_serial_device.isVisible()) {
		sr_config *const src = (sr_config*)g_try_malloc(sizeof(sr_config));
		src->key = SR_CONF_CONN;
		const QByteArray byteArray = _serial_device.text().toUtf8();
		src->data = g_variant_new_string((const gchar*)byteArray.constData());
		drvopts = g_slist_append(drvopts, src);
	}

	const list<sr_dev_inst*> devices = _device_manager.driver_scan(
		driver, drvopts);

	g_slist_free_full(drvopts, (GDestroyNotify)free_drvopts);

	BOOST_FOREACH(sr_dev_inst *const sdi, devices)
	{
		const string title = DeviceManager::format_device_title(sdi);
		QString text(title.c_str());
		if (sdi->probes) {
			text += QString(" with %1 probes").arg(
				g_slist_length(sdi->probes));
		}

		QListWidgetItem *const item = new QListWidgetItem(text,
			&_device_list);
		item->setData(Qt::UserRole, qVariantFromValue((void*)sdi));
		_device_list.addItem(item);
	}

	_device_list.setCurrentRow(0);
	_button_box.button(QDialogButtonBox::Ok)->setDisabled(false);
}

void Connect::device_selected(int index)
{
	gsize num_opts = 0;
	const int32_t *hwopts;
	GVariant *gvar_list;
	sr_dev_driver *const driver = (sr_dev_driver*)_drivers.itemData(
		index).value<void*>();

	unset_connection();

	if ((sr_config_list(driver, SR_CONF_SCAN_OPTIONS,
				&gvar_list, NULL) == SR_OK)) {
		hwopts = (const int32_t *)g_variant_get_fixed_array(gvar_list,
				&num_opts, sizeof(int32_t));

		for (unsigned int i = 0; i < num_opts; i++) {
			switch(hwopts[i]) {
			case SR_CONF_SERIALCOMM:
				set_serial_connection();
				break;

			default:
				continue;
			}

			break;
		}
		g_variant_unref(gvar_list);
	}
}

void Connect::free_drvopts(struct sr_config *src)
{
	g_variant_unref(src->data);
	g_free(src);
}

} // namespace dialogs
} // namespace pv
