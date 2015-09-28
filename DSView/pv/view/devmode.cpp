/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2014 DreamSourceLab <dreamsourcelab@dreamsourcelab.com>
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
#include <QMessageBox>
#include <QDebug>

using boost::shared_ptr;
using namespace std;

namespace pv {
namespace view {

DevMode::DevMode(View &parent) :
	QWidget(&parent),
    _view(parent),
    layout(new QGridLayout(this))
    
{
    setLayout(layout);
}

void DevMode::set_device()
{
    int index = 0;
    const boost::shared_ptr<device::DevInst> dev_inst = _view.session().get_device();

    assert(dev_inst);
   
    _mode_button_list.clear();
    delete layout;
    layout = new QGridLayout(this);

    for (GSList *l = dev_inst->get_dev_mode_list();
         l; l = l->next) {
        sr_dev_mode *mode = (sr_dev_mode *)l->data;

        boost::shared_ptr<QPushButton> mode_button = boost::shared_ptr<QPushButton>(new QPushButton(NULL));
        mode_button->setFlat(true);
        mode_button->setText(mode->name);

        _mode_button_list[mode_button] = mode;

        connect(mode_button.get(), SIGNAL(clicked()), this, SLOT(on_mode_change()));

        layout->addWidget(mode_button.get(), index / GRID_COLS, index % GRID_COLS);
        layout->addWidget(new QWidget(), index / GRID_COLS, GRID_COLS);
        layout->setColumnStretch(GRID_COLS, 1);
        index++;
    }

    setLayout(layout);  
    update();
}

void DevMode::paintEvent(QPaintEvent*)
{
    using pv::view::Trace;

    QStyleOption o;
    o.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &o, &painter, this);

    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    for(std::map<boost::shared_ptr<QPushButton>, sr_dev_mode *>::const_iterator i = _mode_button_list.begin();
        i != _mode_button_list.end(); i++) {
        const boost::shared_ptr<device::DevInst> dev_inst = _view.session().get_device();
        assert(dev_inst);
        if (dev_inst->dev_inst()->mode == (*i).second->mode)
            painter.setBrush(Trace::dsBlue);
        else
            painter.setBrush(Trace::dsGray);

        painter.drawRoundedRect((*i).first->geometry(), 4, 4);
    }

    painter.end();
}

void DevMode::on_mode_change()
{
    const boost::shared_ptr<device::DevInst> dev_inst = _view.session().get_device();
    assert(dev_inst);
    QPushButton *button = qobject_cast<QPushButton *>(sender());

    for(std::map<boost::shared_ptr<QPushButton>, sr_dev_mode *>::const_iterator i = _mode_button_list.begin();
        i != _mode_button_list.end(); i++) {
        if ((*i).first.get() == button) {
            if (dev_inst->dev_inst()->mode != (*i).second->mode) {
                _view.session().stop_capture();
                dev_inst->set_config(NULL, NULL,
                                     SR_CONF_DEVICE_MODE,
                                     g_variant_new_int16((*i).second->mode));
                mode_changed();
            }
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
