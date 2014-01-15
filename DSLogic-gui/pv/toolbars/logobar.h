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


#ifndef DSLOGIC_PV_TOOLBARS_LOGOBAR_H
#define DSLOGIC_PV_TOOLBARS_LOGOBAR_H

#include <QToolBar>
#include <QToolButton>
#include <QAction>

#include "../sigsession.h"

namespace pv {
namespace toolbars {

class LogoBar : public QToolBar
{
    Q_OBJECT

public:
    explicit LogoBar(SigSession &session, QWidget *parent = 0);

    void enable_toggle(bool enable);

    void dslogic_connected(bool conn);

private:
    void load_file(QString file_name);
    void session_error(
        const QString text, const QString info_text);
    void show_session_error(
        const QString text, const QString info_text);

signals:

private slots:
    void on_actionAbout_triggered();

private:
    bool _enable;
    SigSession& _session;

    QToolButton _logo_button;

    QAction *_about;

};

} // namespace toolbars
} // namespace pv

#endif // DSLOGIC_PV_TOOLBARS_LOGOBAR_H
