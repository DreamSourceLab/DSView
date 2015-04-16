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


#ifndef DSVIEW_PV_TOOLBARS_TRIGBAR_H
#define DSVIEW_PV_TOOLBARS_TRIGBAR_H

#include <QToolBar>
#include <QToolButton>

namespace pv {
namespace toolbars {

class TrigBar : public QToolBar
{
    Q_OBJECT
public:
    explicit TrigBar(QWidget *parent = 0);

    void enable_toggle(bool enable);
    void enable_protocol(bool enable);
    void close_all();

signals:
    void on_protocol(bool visible);
    void on_trigger(bool visible);
    void on_measure(bool visible);
    void on_search(bool visible);

public slots:
    void protocol_clicked();
    void trigger_clicked();
    void measure_clicked();
    void search_clicked();

private:
    bool _enable;
    QToolButton _trig_button;
    QToolButton _protocol_button;
    QToolButton _measure_button;
    QToolButton _search_button;

};

} // namespace toolbars
} // namespace pv

#endif // DSVIEW_PV_TOOLBARS_TRIGBAR_H
