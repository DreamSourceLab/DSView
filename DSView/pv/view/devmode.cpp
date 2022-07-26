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
#include <QStyleOption>
#include <QMouseEvent>
#include <QPainter>
#include <QRect>
#include <QHBoxLayout>

#include "../config/appconfig.h"
#include "../ui/msgbox.h"


static const struct dev_mode_name dev_mode_name_list[] =
{
    {LOGIC, "Logic Analyzer", "逻辑分析仪",  "la.svg"},
    {ANALOG, "Data Acquisition", "数据记录仪", "daq.svg"},
    {DSO, "Oscilloscope", "示波器",  "osc.svg"},
};
  
namespace pv {
namespace view {

DevMode::DevMode(QWidget *parent, SigSession *session) :
    QWidget(parent),
    _session(session)    
{
    _bFile = false;

   QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(2, 0, 0, 0);

    _close_button = new QToolButton();
    _close_button->setObjectName("FileCloseButton");
    _close_button->setContentsMargins(0, 0, 0, 0);
    _close_button->setFixedWidth(10);
    _close_button->setFixedHeight(height());
    _close_button->setIconSize(QSize(10, 10));
    _close_button->setToolButtonStyle(Qt::ToolButtonIconOnly); 
    _close_button->setMinimumWidth(10);

    _mode_btn = new QToolButton();
    _mode_btn->setObjectName("ModeButton");
    _mode_btn->setIconSize(QSize(height() * 1.5, height()));
    _mode_btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    _mode_btn->setContentsMargins(0, 0, 0, 0);  
    _mode_btn->setPopupMode(QToolButton::InstantPopup);
    _mode_btn->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);

   // _mode_btn->setArrowType(Qt::NoArrow);

    _pop_menu = new QMenu(this);
    _pop_menu->setContentsMargins(15,0,0,0);
    _mode_btn->setMenu(_pop_menu);

    layout->addWidget(_close_button);
    layout->addWidget(_mode_btn); 

    layout->setStretch(1, 100); 
    setLayout(layout);

    connect(_close_button, SIGNAL(clicked()), this, SLOT(on_close()));
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
    DevInst* dev_inst = _session->get_device();
    assert(dev_inst);

    _bFile = false;
   
   //remove all action object
    for(std::map<QAction *, const sr_dev_mode *>::const_iterator i = _mode_list.begin();
        i != _mode_list.end(); i++) {
        (*i).first->setParent(NULL);
        _pop_menu->removeAction((*i).first);
        delete (*i).first;
    }
    _mode_list.clear();

    _close_button->setIcon(QIcon());
    _close_button->setDisabled(true); 

    AppConfig &app = AppConfig::Instance(); 
    int lan = app._frameOptions.language;

    QString iconPath = GetIconPath() + "/";

    auto dev_mode_list  = dev_inst->get_dev_mode_list();

    for (const GSList *l = dev_mode_list; l; l = l->next)
    {
        const sr_dev_mode *mode = (const sr_dev_mode *)l->data;
        auto *mode_name = get_mode_name(mode->mode);
        QString icon_name = QString::fromLocal8Bit(mode_name->_logo);

        QAction *action = new QAction(this);
        action->setIcon(QIcon(iconPath + "square-" + icon_name));
        if (lan == LAN_CN)
            action->setText(mode_name->_name_cn);
        else
            action->setText(mode_name->_name_en);

        connect(action, SIGNAL(triggered()), this, SLOT(on_mode_change()));

        _mode_list[action] = mode;
        if (dev_inst->dev_inst()->mode == _mode_list[action]->mode)
        {
            QString icon_fname = iconPath + icon_name;
            _mode_btn->setIcon(QIcon(icon_fname));
            if (lan == LAN_CN)
                _mode_btn->setText(mode_name->_name_cn);
            else
                _mode_btn->setText(mode_name->_name_en);
        }
        _pop_menu->addAction(action);
    }

    if ((dynamic_cast<File *>(dev_inst)))
    {
        _close_button->setDisabled(false);
        _close_button->setIcon(QIcon(iconPath + "/close.svg"));
        _bFile = true;
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
    DevInst* dev_inst = _session->get_device();
    assert(dev_inst);
    QAction *action = qobject_cast<QAction *>(sender());
    if (dev_inst->dev_inst()->mode == _mode_list[action]->mode)
        return;

    QString iconPath = GetIconPath();
    AppConfig &app = AppConfig::Instance(); 
    int lan = app._frameOptions.language;

    for(auto i = _mode_list.begin();i != _mode_list.end(); i++)
    {
        if ((*i).first == action) {
            if (dev_inst->dev_inst()->mode != (*i).second->mode) {
                _session->set_run_mode(SigSession::Single);
                _session->set_repeating(false);
                _session->stop_capture();
                _session->capture_state_changed(SigSession::Stopped);
                _session->session_save();
                dev_inst->set_config(NULL, NULL,
                                     SR_CONF_DEVICE_MODE,
                                     g_variant_new_int16((*i).second->mode));

                auto *mode_name = get_mode_name((*i).second->mode);
                QString icon_fname = iconPath + "/" + QString::fromLocal8Bit(mode_name->_logo);
             
                _mode_btn->setIcon(QIcon(icon_fname));
                if (lan == LAN_CN)
                    _mode_btn->setText(mode_name->_name_cn);
                else
                    _mode_btn->setText(mode_name->_name_en);
                dev_changed(false);
            }

            break;
        }
    }
}

void DevMode::on_close()
{
    DevInst *dev_inst = _session->get_device();
    assert(dev_inst);

    if (_bFile && MsgBox::Confirm(tr("are you sure to close the device?"))){
        _session->close_file(dev_inst);
        dev_changed(true);
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

const struct dev_mode_name* DevMode::get_mode_name(int mode) 
{
    for(auto &o : dev_mode_name_list)
        if (mode == o._mode){
            return &o;
    }
    assert(false);
}

} // namespace view
} // namespace pv
