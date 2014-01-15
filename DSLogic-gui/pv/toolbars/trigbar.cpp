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


#include "trigbar.h"

namespace pv {
namespace toolbars {

TrigBar::TrigBar(QWidget *parent) :
    QToolBar("Trig Bar", parent),
    _enable(true),
    _trig_button(this),
    _protocol_button(this),
    _measure_button(this),
    _search_button(this)
{
    setMovable(false);

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
    _protocol_button.setCheckable(true);
    _measure_button.setIcon(QIcon::fromTheme("trig",
        QIcon(":/icons/measure.png")));
    _measure_button.setCheckable(true);
    _search_button.setIcon(QIcon::fromTheme("trig",
        QIcon(":/icons/search-bar.png")));
    _search_button.setCheckable(true);

    addWidget(&_trig_button);
    addWidget(&_protocol_button);
    addWidget(&_measure_button);
    addWidget(&_search_button);
}

void TrigBar::protocol_clicked()
{
    on_protocol(_protocol_button.isChecked());
}

void TrigBar::trigger_clicked()
{
    on_trigger(_trig_button.isChecked());
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
}

} // namespace toolbars
} // namespace pv
