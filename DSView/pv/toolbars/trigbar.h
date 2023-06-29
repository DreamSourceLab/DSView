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


#ifndef DSVIEW_PV_TOOLBARS_TRIGBAR_H
#define DSVIEW_PV_TOOLBARS_TRIGBAR_H

#include <QToolBar>
#include <QToolButton>
#include <QAction>
#include <QMenu>
#include "../interface/icallbacks.h"

class DockOptions;

namespace pv {

class SigSession;

namespace toolbars {

//boolbar, referenced by MainWindow
//TODO:show the property panel about protocol\trigger
class TrigBar : public QToolBar, public IFontForm
{
    Q_OBJECT

public:
    explicit TrigBar(SigSession *session, QWidget *parent = 0);
    void reload();
    void update_view_status();

private:
    void changeEvent(QEvent *event);
    void retranslateUi();
    void reStyle();
    DockOptions* getDockOptions();
    void update_checked_status();

    //IFontForm
    void update_font() override;

signals:
    void sig_setTheme(QString style);
    void sig_protocol(bool visible); //post decode button click event,to show or hide protocol property panel
    void sig_trigger(bool visible); //post decode button click event,to show or hide trigger property panel
    void sig_measure(bool visible);//post decode button click event,to show or hide measure property panel
    void sig_search(bool visible);
    void sig_show_lissajous(bool visible);

private slots:
    void on_actionDark_triggered();
    void on_actionLight_triggered();
    void on_actionLissajous_triggered();
    void on_actionFft_triggered();
    void on_actionMath_triggered();
    void on_display_setting();

public slots:
    void protocol_clicked();
    void trigger_clicked();
    void measure_clicked();
    void search_clicked();

private:
    SigSession  *_session;
    bool        _enable;
    QToolButton _trig_button;
    QToolButton _protocol_button;
    QToolButton _measure_button;
    QToolButton _search_button;
    QToolButton _function_button;
    QToolButton _setting_button;
    QAction     *_trig_action;
    QAction     *_protocol_action;
    QAction     *_measure_action;
    QAction     *_search_action;
    QAction     *_function_action; 
    QAction     *_display_action;

    QMenu       *_function_menu;
    QAction     *_action_fft;
    QAction     *_action_math;

    QMenu       *_display_menu;
    QMenu       *_themes;
    QAction     *_action_dispalyOptions;
    QAction     *_dark_style;
    QAction     *_light_style;
    QAction     *_action_lissajous;
};

} // namespace toolbars
} // namespace pv

#endif // DSVIEW_PV_TOOLBARS_TRIGBAR_H
