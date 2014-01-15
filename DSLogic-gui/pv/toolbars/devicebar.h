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


#ifndef DSLOGIC_PV_TOOLBARS_DEVICEBAR_H
#define DSLOGIC_PV_TOOLBARS_DEVICEBAR_H

#include <stdint.h>

#include <list>

#include <QComboBox>
#include <QToolBar>
#include <QToolButton>

struct st_dev_inst;
class QAction;

namespace pv {
namespace toolbars {

class DeviceBar : public QToolBar
{
    Q_OBJECT

public:
    DeviceBar(QWidget *parent);

    void set_device_list(const std::list<struct sr_dev_inst*> &devices);

    struct sr_dev_inst* get_selected_device() const;
    void set_selected_device(struct sr_dev_inst *const sdi, bool discon);

    void enable_toggle(bool enable);

signals:
    void device_selected();

    void device_updated();

private slots:
    void on_device_selected();
    void on_configure();

private:
    bool _enable;
    QComboBox _device_selector;
    QToolButton _configure_button;
};

} // namespace toolbars
} // namespace pv

#endif // DSLOGIC_PV_TOOLBARS_DEVICEBAR_H
