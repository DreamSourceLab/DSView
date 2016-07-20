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


#ifndef DSVIEW_PV_TOOLBARS_FILEBAR_H
#define DSVIEW_PV_TOOLBARS_FILEBAR_H

#include <QToolBar>
#include <QToolButton>
#include <QAction>
#include <QMenu>

#include "../sigsession.h"

namespace pv {
namespace toolbars {

class FileBar : public QToolBar
{
    Q_OBJECT

public:
    explicit FileBar(SigSession &session, QWidget *parent = 0);

    void enable_toggle(bool enable);

    void set_settings_en(bool enable);

private:

    void session_error(
        const QString text, const QString info_text);
    void show_session_error(
        const QString text, const QString info_text);

signals:
    void load_file(QString);
    void save();
    void on_screenShot();
    void load_session(QString);
    void store_session(QString);

private slots:
    void on_actionLoad_triggered();
    void on_actionStore_triggered();
    void on_actionDefault_triggered();
    void on_actionOpen_triggered();
    void on_actionSave_triggered();
    void on_actionCapture_triggered();
    void on_actionExport_triggered();

private:
    bool _enable;
    SigSession& _session;

    QToolButton _file_button;

    QMenu *_menu;

    QMenu *_menu_session;
    QAction *_action_load;
    QAction *_action_store;
    QAction *_action_default;

    QAction *_action_open;
    QAction *_action_save;
    QAction *_action_export;
    QAction *_action_capture;

    QTimer _screenshot_timer;

};

} // namespace toolbars
} // namespace pv

#endif // DSVIEW_PV_TOOLBARS_FILEBAR_H
