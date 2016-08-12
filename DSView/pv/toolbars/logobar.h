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


#ifndef DSVIEW_PV_TOOLBARS_LOGOBAR_H
#define DSVIEW_PV_TOOLBARS_LOGOBAR_H

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

    void dsl_connected(bool conn);

private:
    void session_error(
        const QString text, const QString info_text);
    void show_session_error(
        const QString text, const QString info_text);

signals:

private slots:
    void on_actionAbout_triggered();
    void on_actionWiki_triggered();

private:
    bool _enable;
    SigSession& _session;

    QToolButton _logo_button;

    QAction *_about;
    QAction *_wiki;

};

} // namespace toolbars
} // namespace pv

#endif // DSVIEW_PV_TOOLBARS_LOGOBAR_H
