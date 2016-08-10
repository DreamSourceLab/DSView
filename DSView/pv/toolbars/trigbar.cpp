/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
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


#include "trigbar.h"
#include "../sigsession.h"
#include "../device/devinst.h"
#include "../dialogs/fftoptions.h"

#include <QApplication>

namespace pv {
namespace toolbars {

TrigBar::TrigBar(SigSession &session, QWidget *parent) :
    QToolBar("Trig Bar", parent),
    _session(session),
    _enable(true),
    _trig_button(this),
    _protocol_button(this),
    _measure_button(this),
    _search_button(this),
    _math_button(this)
{
    setMovable(false);
    setContentsMargins(0,0,0,0);

    connect(&_trig_button, SIGNAL(clicked()),
        this, SLOT(trigger_clicked()));
    connect(&_protocol_button, SIGNAL(clicked()),
        this, SLOT(protocol_clicked()));
    connect(&_measure_button, SIGNAL(clicked()),
            this, SLOT(measure_clicked()));
    connect(&_search_button, SIGNAL(clicked()),
            this, SLOT(search_clicked()));

    _trig_button.setIcon(QIcon::fromTheme("trig",
        QIcon(":/icons/trigger.png")));
    _trig_button.setCheckable(true);
    _protocol_button.setIcon(QIcon::fromTheme("trig",
        QIcon(":/icons/protocol.png")));
#ifdef ENABLE_DECODE
    _protocol_button.setCheckable(true);
#endif
    _measure_button.setIcon(QIcon::fromTheme("trig",
        QIcon(":/icons/measure.png")));
    _measure_button.setCheckable(true);
    _search_button.setIcon(QIcon::fromTheme("trig",
        QIcon(":/icons/search-bar.png")));
    _search_button.setCheckable(true);
    _math_button.setIcon(QIcon::fromTheme("trig",
        QIcon(":/icons/math.png")));

    _action_fft = new QAction(this);
    _action_fft->setText(QApplication::translate(
        "Math", "&FFT", 0));
    _action_fft->setIcon(QIcon::fromTheme("Math",
        QIcon(":/icons/fft.png")));
    _action_fft->setObjectName(QString::fromUtf8("actionFft"));
    connect(_action_fft, SIGNAL(triggered()), this, SLOT(on_actionFft_triggered()));

    _math_menu = new QMenu(this);
    _math_menu->setContentsMargins(0,0,0,0);
    _math_menu->addAction(_action_fft);
    _math_button.setPopupMode(QToolButton::InstantPopup);
    _math_button.setMenu(_math_menu);

    _trig_action = addWidget(&_trig_button);
    _protocol_action = addWidget(&_protocol_button);
    _measure_action = addWidget(&_measure_button);
    _search_action = addWidget(&_search_button);
    _math_action = addWidget(&_math_button);
}

void TrigBar::protocol_clicked()
{
    on_protocol(_protocol_button.isChecked());
}

void TrigBar::trigger_clicked()
{
    on_trigger(_trig_button.isChecked());
}

void TrigBar::update_trig_btn(bool checked)
{
    _trig_button.setChecked(checked);
}

void TrigBar::measure_clicked()
{
    on_measure(_measure_button.isChecked());
}

void TrigBar::search_clicked()
{
    on_search(_search_button.isChecked());
}

void TrigBar::enable_toggle(bool enable)
{
    _trig_button.setDisabled(!enable);
    _protocol_button.setDisabled(!enable);
    _measure_button.setDisabled(!enable);
    _search_button.setDisabled(!enable);
    _math_button.setDisabled(!enable);

    _trig_button.setIcon(enable ? QIcon::fromTheme("trig", QIcon(":/icons/trigger.png")) :
                                  QIcon::fromTheme("trig", QIcon(":/icons/trigger_dis.png")));
    _protocol_button.setIcon(enable ? QIcon::fromTheme("trig", QIcon(":/icons/protocol.png")) :
                                  QIcon::fromTheme("trig", QIcon(":/icons/protocol_dis.png")));
    _measure_button.setIcon(enable ? QIcon::fromTheme("trig", QIcon(":/icons/measure.png")) :
                                  QIcon::fromTheme("trig", QIcon(":/icons/measure_dis.png")));
    _search_button.setIcon(enable ? QIcon::fromTheme("trig", QIcon(":/icons/search-bar.png")) :
                                  QIcon::fromTheme("trig", QIcon(":/icons/search-bar_dis.png")));
    _math_button.setIcon(enable ? QIcon::fromTheme("trig", QIcon(":/icons/math.png")) :
                                  QIcon::fromTheme("trig", QIcon(":/icons/math_dis.png")));
}

void TrigBar::enable_protocol(bool enable)
{
    _protocol_button.setDisabled(!enable);
    _protocol_button.setIcon(enable ? QIcon::fromTheme("trig", QIcon(":/icons/protocol.png")) :
                                  QIcon::fromTheme("trig", QIcon(":/icons/protocol_dis.png")));
}

void TrigBar::close_all()
{
    if (_trig_button.isChecked()) {
        _trig_button.setChecked(false);
        on_trigger(false);
    }
    if (_protocol_button.isChecked()) {
        _protocol_button.setChecked(false);
        on_protocol(false);
    }
    if (_measure_button.isChecked()) {
        _measure_button.setChecked(false);
        on_measure(false);
    }
    if(_search_button.isChecked()) {
        _search_button.setChecked(false);
        on_search(false);
    }
}

void TrigBar::reload()
{
    close_all();
    if (_session.get_device()->dev_inst()->mode == LOGIC) {
        _trig_action->setVisible(true);
        _protocol_action->setVisible(true);
        _measure_action->setVisible(true);
        _search_action->setVisible(true);
        _math_action->setVisible(false);
    } else if (_session.get_device()->dev_inst()->mode == ANALOG) {
        _trig_action->setVisible(false);
        _protocol_action->setVisible(false);
        _measure_action->setVisible(true);
        _search_action->setVisible(false);
        _math_action->setVisible(false);
    } else if (_session.get_device()->dev_inst()->mode == DSO) {
        _trig_action->setVisible(true);
        _protocol_action->setVisible(false);
        _measure_action->setVisible(true);
        _search_action->setVisible(false);
        _math_action->setVisible(true);
    }
    enable_toggle(true);
    update();
}

void TrigBar::on_actionFft_triggered()
{
    pv::dialogs::FftOptions fft_dlg(this, _session);
    fft_dlg.exec();
}

} // namespace toolbars
} // namespace pv
