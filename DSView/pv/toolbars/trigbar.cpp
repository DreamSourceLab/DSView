/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
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

#ifdef LANGUAGE_ZH_CN
    _trig_button.setIcon(QIcon::fromTheme("trig",
        QIcon(":/icons/trigger_cn.png")));
    _trig_button.setCheckable(true);
    _protocol_button.setIcon(QIcon::fromTheme("trig",
        QIcon(":/icons/protocol_cn.png")));
#ifdef ENABLE_DECODE
    _protocol_button.setCheckable(true);
#endif
    _measure_button.setIcon(QIcon::fromTheme("trig",
        QIcon(":/icons/measure_cn.png")));
    _measure_button.setCheckable(true);
    _search_button.setIcon(QIcon::fromTheme("trig",
        QIcon(":/icons/search-bar_cn.png")));
    _search_button.setCheckable(true);
#else
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
#endif

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

#ifdef LANGUAGE_ZH_CN
    _trig_button.setIcon(enable ? QIcon::fromTheme("trig", QIcon(":/icons/trigger_cn.png")) :
                                  QIcon::fromTheme("trig", QIcon(":/icons/trigger_dis_cn.png")));
    _protocol_button.setIcon(enable ? QIcon::fromTheme("trig", QIcon(":/icons/protocol_cn.png")) :
                                  QIcon::fromTheme("trig", QIcon(":/icons/protocol_dis_cn.png")));
    _measure_button.setIcon(enable ? QIcon::fromTheme("trig", QIcon(":/icons/measure_cn.png")) :
                                  QIcon::fromTheme("trig", QIcon(":/icons/measure_dis_cn.png")));
    _search_button.setIcon(enable ? QIcon::fromTheme("trig", QIcon(":/icons/search-bar_cn.png")) :
                                  QIcon::fromTheme("trig", QIcon(":/icons/search-bar_dis_cn.png")));
#else
    _trig_button.setIcon(enable ? QIcon::fromTheme("trig", QIcon(":/icons/trigger.png")) :
                                  QIcon::fromTheme("trig", QIcon(":/icons/trigger_dis.png")));
    _protocol_button.setIcon(enable ? QIcon::fromTheme("trig", QIcon(":/icons/protocol.png")) :
                                  QIcon::fromTheme("trig", QIcon(":/icons/protocol_dis.png")));
    _measure_button.setIcon(enable ? QIcon::fromTheme("trig", QIcon(":/icons/measure.png")) :
                                  QIcon::fromTheme("trig", QIcon(":/icons/measure_dis.png")));
    _search_button.setIcon(enable ? QIcon::fromTheme("trig", QIcon(":/icons/search-bar.png")) :
                                  QIcon::fromTheme("trig", QIcon(":/icons/search-bar_dis.png")));
#endif
}

void TrigBar::enable_protocol(bool enable)
{
    _protocol_button.setDisabled(!enable);
#ifdef LANGUAGE_ZH_CN
    _protocol_button.setIcon(enable ? QIcon::fromTheme("trig", QIcon(":/icons/protocol_cn.png")) :
                                  QIcon::fromTheme("trig", QIcon(":/icons/protocol_dis_cn.png")));
#else
    _protocol_button.setIcon(enable ? QIcon::fromTheme("trig", QIcon(":/icons/protocol.png")) :
                                  QIcon::fromTheme("trig", QIcon(":/icons/protocol_dis.png")));
#endif
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

} // namespace toolbars
} // namespace pv
