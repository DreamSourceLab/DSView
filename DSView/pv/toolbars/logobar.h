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
#include <QMenu>
#include <libsigrok.h> 
#include <QPushButton>

#include "../sigsession.h"
#include "../interface/icallbacks.h"

namespace pv {
namespace toolbars {

//The tool button for help,is a ui class,referenced by MainWindow
//TODO: switch language,submit bug descript,
class LogoBar : public QToolBar, public IFontForm
{
    Q_OBJECT

public:
    explicit LogoBar(SigSession *session, QWidget *parent = 0);

    void enable_toggle(bool enable);

   //show the hardware device conneted status with logo picture
    void dsl_connected(bool conn);

    inline void set_mainform_callback(IMainForm *callback){
        _mainForm = callback;
    }

private:
    void changeEvent(QEvent *event);
    void retranslateUi();
    void reStyle();

    //IFontForm
    void update_font() override;

signals: 
    //post event message to open user help document, MainWindow class receive it
    void sig_open_doc(); 

private slots:
    void on_actionEn_triggered();
    void on_actionCn_triggered();
    void on_actionAbout_triggered();
    void on_actionManual_triggered();
    void on_actionIssue_triggered();
    void on_action_update();
    void on_action_setting_log();
    void on_open_log_file();
    void on_clear_log_file();

private:
    bool _enable;
    bool _connected;
    SigSession* _session;

    QToolButton _logo_button;

    QMenu *_menu;

    QMenu *_language;
    QAction *_action_en;
    QAction *_action_cn;

    QAction *_about;
    QAction *_manual;
    QAction *_issue;
    QAction *_update;
    QAction *_log;

    QPushButton *_log_open_bt;
    QPushButton *_log_clear_bt;

    IMainForm *_mainForm;
};

} // namespace toolbars
} // namespace pv

#endif // DSVIEW_PV_TOOLBARS_LOGOBAR_H
