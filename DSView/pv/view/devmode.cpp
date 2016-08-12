/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2014 DreamSourceLab <support@dreamsourcelab.com>
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


#include "devmode.h"
#include "view.h"
#include "trace.h"
#include "../sigsession.h"
#include "../device/devinst.h"

#include <assert.h>

#include <boost/foreach.hpp>

#include <QApplication>
#include <QStyleOption>
#include <QMouseEvent>
#include <QPainter>
#include <QRect>
#include <QDebug>

using boost::shared_ptr;
using namespace std;

namespace pv {
namespace view {

DevMode::DevMode(QWidget *parent, SigSession &session) :
    QWidget(parent),
    _session(session),
    _layout(new QGridLayout(this))
    
{
    _layout->setMargin(5);
    _layout->setSpacing(0);
    setLayout(_layout);
}

void DevMode::set_device()
{
    int index = 0;
    const boost::shared_ptr<device::DevInst> dev_inst = _session.get_device();

    assert(dev_inst);
   
    for(std::map<QPushButton *, sr_dev_mode *>::const_iterator i = _mode_button_list.begin();
        i != _mode_button_list.end(); i++) {
        (*i).first->setParent(NULL);
        _layout->removeWidget((*i).first);
        delete (*i).first;
    }
    _mode_button_list.clear();

    for (GSList *l = dev_inst->get_dev_mode_list();
         l; l = l->next) {
        sr_dev_mode *mode = (sr_dev_mode *)l->data;

        QPushButton *mode_button = new QPushButton(this);
        //mode_button->setFlat(true);
        mode_button->setMinimumWidth(32);
        mode_button->setText(mode->name);
        mode_button->setCheckable(true);

        _mode_button_list[mode_button] = mode;
        if (dev_inst->dev_inst()->mode == _mode_button_list[mode_button]->mode)
            mode_button->setChecked(true);

        connect(mode_button, SIGNAL(clicked()), this, SLOT(on_mode_change()));

        _layout->addWidget(mode_button, index / GRID_COLS, index % GRID_COLS);
        //layout->addWidget(new QWidget(), index / GRID_COLS, GRID_COLS);
        _layout->setColumnStretch(GRID_COLS, 1);
        index++;
    } 
    update();
}

void DevMode::paintEvent(QPaintEvent*)
{
    using pv::view::Trace;

    QStyleOption o;
    o.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &o, &painter, this);
}

void DevMode::on_mode_change()
{
    const boost::shared_ptr<device::DevInst> dev_inst = _session.get_device();
    assert(dev_inst);
    QPushButton *button = qobject_cast<QPushButton *>(sender());
    button->setChecked(true);
    if (dev_inst->dev_inst()->mode == _mode_button_list[button]->mode)
        return;

    for(std::map<QPushButton *, sr_dev_mode *>::const_iterator i = _mode_button_list.begin();
        i != _mode_button_list.end(); i++) {
        if ((*i).first == button) {
            if (dev_inst->dev_inst()->mode != (*i).second->mode) {
                _session.stop_capture();
                _session.on_mode_change();
                dev_inst->set_config(NULL, NULL,
                                     SR_CONF_DEVICE_MODE,
                                     g_variant_new_int16((*i).second->mode));
                mode_changed();
                button->setChecked(true);
            }
        } else {
            (*i).first->setChecked(false);
        }
    }
}

void DevMode::mousePressEvent(QMouseEvent *event)
{
	assert(event);
	(void)event;
}

void DevMode::mouseReleaseEvent(QMouseEvent *event)
{
	assert(event);
        (void)event;
}

void DevMode::mouseMoveEvent(QMouseEvent *event)
{
	assert(event);
	_mouse_point = event->pos();
	update();
}

void DevMode::leaveEvent(QEvent*)
{
	_mouse_point = QPoint(-1, -1);
	update();
}

} // namespace view
} // namespace pv
