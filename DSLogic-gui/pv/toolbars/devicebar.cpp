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


#include <extdef.h>

#include <assert.h>

#include <boost/foreach.hpp>

#include <libsigrok4DSLogic/libsigrok.h>

#include <QAction>
#include <QDebug>
#include <QMessageBox>

#include "devicebar.h"

#include <pv/devicemanager.h>
#include <pv/dialogs/deviceoptions.h>

using namespace std;

namespace pv {
namespace toolbars {

DeviceBar::DeviceBar(QWidget *parent) :
    QToolBar("Device Bar", parent),
    _enable(true),
    _device_selector(this),
    _configure_button(this)
{
    setMovable(false);

    connect(&_device_selector, SIGNAL(currentIndexChanged (int)),
        this, SLOT(on_device_selected()));
    connect(&_configure_button, SIGNAL(clicked()),
        this, SLOT(on_configure()));

    _configure_button.setIcon(QIcon::fromTheme("configure",
        QIcon(":/icons/params.png")));

    addWidget(&_device_selector);
    addWidget(&_configure_button);
}

void DeviceBar::set_device_list(
    const std::list<struct sr_dev_inst*> &devices)
{
    disconnect(&_device_selector, SIGNAL(currentIndexChanged (int)),
        this, SLOT(on_device_selected()));
    _device_selector.clear();

    BOOST_FOREACH (sr_dev_inst *sdi, devices) {
        const string title = DeviceManager::format_device_title(sdi);
        _device_selector.addItem(title.c_str(),
            qVariantFromValue((void*)sdi));
    }
    connect(&_device_selector, SIGNAL(currentIndexChanged (int)),
        this, SLOT(on_device_selected()));
}

struct sr_dev_inst* DeviceBar::get_selected_device() const
{
    const int index = _device_selector.currentIndex();
    if (index < 0)
        return NULL;

    return (sr_dev_inst*)_device_selector.itemData(
        index).value<void*>();
}

void DeviceBar::set_selected_device(struct sr_dev_inst *const sdi, bool discon)
{
    if (discon)
        disconnect(&_device_selector, SIGNAL(currentIndexChanged (int)),
            this, SLOT(on_device_selected()));

    for (int i = 0; i < _device_selector.count(); i++)
        if (sdi == _device_selector.itemData(i).value<void*>()) {
            _device_selector.setCurrentIndex(i);
            if (!discon)
                device_selected();
            break;
        }

    if (discon)
        connect(&_device_selector, SIGNAL(currentIndexChanged (int)),
            this, SLOT(on_device_selected()));
}

void DeviceBar::on_device_selected()
{
    device_selected();
}

void DeviceBar::on_configure()
{
    int  dev_mode, ret;
    sr_dev_inst *const sdi = get_selected_device();
    assert(sdi);

    dev_mode = sdi->mode;

    pv::dialogs::DeviceOptions dlg(this, sdi);
    ret = dlg.exec();
    if (ret == QDialog::Accepted) {
        if (dev_mode != sdi->mode) {
            device_selected();
        } else {
            device_updated();
        }
    }
}

void DeviceBar::enable_toggle(bool enable)
{
    _device_selector.setDisabled(!enable);
    _configure_button.setDisabled(!enable);
}

} // namespace toolbars
} // namespace pv
