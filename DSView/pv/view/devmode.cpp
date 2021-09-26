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
#include "../device/file.h"

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
    _session(session)
    
{
    _layout = new QHBoxLayout(this);
    _layout->setMargin(0);
    _layout->setSpacing(0);
    _layout->setContentsMargins(2, 0, 0, 0);

    _close_button = new QToolButton(this);
    _close_button->setObjectName("FileCloseButton");
    _close_button->setContentsMargins(0, 0, 0, 0);
    _close_button->setFixedWidth(10);
    _close_button->setFixedHeight(height());
    _close_button->setIconSize(QSize(10, 10));
    _close_button->setToolButtonStyle(Qt::ToolButtonIconOnly);

    _pop_menu = new QMenu(this);

    _mode_btn = new QToolButton(this);
    _mode_btn->setObjectName("ModeButton");
    _mode_btn->setIconSize(QSize(height()*2, height()));
    _mode_btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    _mode_btn->setContentsMargins(0, 0, 1000, 0);
    _mode_btn->setMenu(_pop_menu);
    _mode_btn->setPopupMode(QToolButton::InstantPopup);
    _mode_btn->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);

    _layout->addWidget(_close_button);
    _layout->addWidget(_mode_btn);
    //_layout->addWidget(new QWidget(this));
    _layout->setStretch(1, 100);
    setLayout(_layout);
}


void DevMode::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        set_device();
    else if (event->type() == QEvent::StyleChange)
        set_device();
    QWidget::changeEvent(event);
}

void DevMode::set_device()
{
    const boost::shared_ptr<device::DevInst> dev_inst = _session.get_device();
    assert(dev_inst);
   
    for(std::map<QAction *, const sr_dev_mode *>::const_iterator i = _mode_list.begin();
        i != _mode_list.end(); i++) {
        (*i).first->setParent(NULL);
        _pop_menu->removeAction((*i).first);
        delete (*i).first;
    }
    _mode_list.clear();
    _close_button->setIcon(QIcon());
    _close_button->setDisabled(true);
    disconnect(_close_button, SIGNAL(clicked()), this, SLOT(on_close()));

    if (!qApp->property("Style").isNull()) {
        QString iconPath = ":/icons/" + qApp->property("Style").toString() + "/";
        for (const GSList *l = dev_inst->get_dev_mode_list();
             l; l = l->next) {
            const sr_dev_mode *mode = (const sr_dev_mode *)l->data;
            QString icon_name = QString::fromLocal8Bit(mode->icon);

            QAction *action = new QAction(this);
            action->setIcon(QIcon(iconPath+"square-"+icon_name));
            if (qApp->property("Language") == QLocale::Chinese)
                action->setText(mode->name_cn);
            else
                action->setText(mode->name);
            connect(action, SIGNAL(triggered()), this, SLOT(on_mode_change()));

            _mode_list[action] = mode;
            if (dev_inst->dev_inst()->mode == _mode_list[action]->mode) {
                _mode_btn->setIcon(QIcon(iconPath+icon_name));
                if (qApp->property("Language") == QLocale::Chinese)
                    _mode_btn->setText(mode->name_cn);
                else
                    _mode_btn->setText(mode->name);
            }
            _pop_menu->addAction(action);
        }

        boost::shared_ptr<pv::device::File> file_dev;
        if((file_dev = dynamic_pointer_cast<pv::device::File>(dev_inst))) {
            _close_button->setDisabled(false);
            _close_button->setIcon(QIcon(iconPath+"/close.svg"));
            connect(_close_button, SIGNAL(clicked()), this, SLOT(on_close()));
        }
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
    QAction *action = qobject_cast<QAction *>(sender());
    if (dev_inst->dev_inst()->mode == _mode_list[action]->mode)
        return;

    QString iconPath = ":/icons/" + qApp->property("Style").toString();
    for(std::map<QAction *, const sr_dev_mode *>::const_iterator i = _mode_list.begin();
        i != _mode_list.end(); i++) {
        if ((*i).first == action) {
            if (dev_inst->dev_inst()->mode != (*i).second->mode) {
                _session.set_run_mode(SigSession::Single);
                _session.set_repeating(false);
                _session.stop_capture();
                _session.capture_state_changed(SigSession::Stopped);
                _session.session_save();
                dev_inst->set_config(NULL, NULL,
                                     SR_CONF_DEVICE_MODE,
                                     g_variant_new_int16((*i).second->mode));

                QString icon_name = "/" + QString::fromLocal8Bit((*i).second->icon);
                _mode_btn->setIcon(QIcon(iconPath+icon_name));
                if (qApp->property("Language") == QLocale::Chinese)
                    _mode_btn->setText((*i).second->name_cn);
                else
                    _mode_btn->setText((*i).second->name);
                dev_changed(false);
            }
        }
    }
}

void DevMode::on_close()
{
    const boost::shared_ptr<device::DevInst> dev_inst = _session.get_device();
    assert(dev_inst);

    _session.close_file(dev_inst);
    dev_changed(true);
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
