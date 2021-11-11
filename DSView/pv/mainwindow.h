/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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


#ifndef DSVIEW_PV_MAINWINDOW_H
#define DSVIEW_PV_MAINWINDOW_H

#include <list>
#include <QMainWindow>
#include <QTranslator> 
#include "dialogs/dsmessagebox.h"
#include "interface/icallbacks.h"
#include "eventobject.h"

class QAction;
class QMenuBar;
class QMenu;
class QVBoxLayout;
class QStatusBar;
class QToolBar;
class QWidget;
class QDockWidget;

class AppControl;

namespace pv {
 

namespace toolbars {
class SamplingBar;
class TrigBar;
class FileBar;
class LogoBar;
}

namespace dock{
class ProtocolDock;
class TriggerDock;
class DsoTriggerDock;
class MeasureDock;
class SearchDock;
}

namespace view {
class View;
}

namespace device{
    class DevInst;
}

using namespace pv::device;
 
//The mainwindow,referenced by MainFrame
//TODO: create graph view,toolbar,and show device list
class MainWindow : public QMainWindow, public ISessionCallback
{
	Q_OBJECT

private:
    static constexpr int Session_Version = 2;
   
public:
    explicit MainWindow(QWidget *parent = 0);

    void openDoc();
    void switchLanguage(int language);

private:
    void closeEvent(QCloseEvent *event);

	void setup_ui();
    void retranslateUi(); 
    bool eventFilter(QObject *object, QEvent *event);
    bool load_session_json(QJsonDocument json, bool file_dev,bool bDecoder=true);
    void update_device_list();

public slots: 
    void switchTheme(QString style);
    void restore_dock();

private slots:
	void on_load_file(QString file_name);
    void on_open_doc(); 
    void on_device_updated_reload();
 
	void on_run_stop();
    void on_instant_stop(); 
    void on_protocol(bool visible);
    void on_trigger(bool visible);
    void commit_trigger(bool instant);

    void on_measure(bool visible);
    void on_search(bool visible);
    void on_screenShot();
    void on_save();

    void on_export();
    bool on_load_session(QString name);  
    bool on_store_session(QString name);     
    void device_detach_post();
    void device_changed(bool close);
    void on_device_selected();      
    void on_setLanguage(int language);

    void on_capture_state_changed(int state);
    void on_data_updated();
    void on_device_attach();
    void on_device_detach();
    void on_show_error(QString str);
    void on_session_error();
    void on_signals_changed();
    void on_receive_trigger(quint64 trigger_pos);
    void on_frame_ended();
    void on_frame_began();
    void on_decode_done();
    void on_receive_data_len(quint64 len);
    void on_cur_snap_samplerate_changed();
  
signals:
    void prgRate(int progress);

//ISessionCallback
public:
    void session_save(); 

//ISessionCallback
private:
    void show_error(QString error);
    void session_error();
    void capture_state_changed(int state);
    void device_attach();
    void device_detach();

    void data_updated();
    void repeat_resume();
    void update_capture();
    void cur_snap_samplerate_changed();
    void device_setted();

    void signals_changed();
    void receive_trigger(quint64 trigger_pos);
    void frame_ended();
    void frame_began();
    void show_region(uint64_t start, uint64_t end, bool keep);

    void show_wait_trigger();
    void repeat_hold(int percent);
    void decode_done();
    void receive_data_len(quint64 len);
    void receive_header();    
    void data_received();

private:
    AppControl              *_control; 
    bool                    _hot_detach;

	pv::view::View          *_view;
    dialogs::DSMessageBox   *_msg;

	QMenuBar                *_menu_bar;
	QMenu                   *_menu_file;
	QAction                 *_action_open;
	QAction                 *_action_connect;
	QAction                 *_action_quit;

	QMenu                   *_menu_view;
	QAction                 *_action_view_zoom_in;
	QAction                 *_action_view_zoom_out;
	QAction                 *_action_view_show_cursors;

	QMenu                   *_menu_help;
	QAction                 *_action_about;

	QWidget                 *_central_widget;
	QVBoxLayout             *_vertical_layout;

	toolbars::SamplingBar   *_sampling_bar;
    toolbars::TrigBar       *_trig_bar;
    toolbars::FileBar       *_file_bar;
    toolbars::LogoBar       *_logo_bar; //help button, on top right


    QDockWidget             *_protocol_dock;
    dock::ProtocolDock      *_protocol_widget;
    QDockWidget             *_trigger_dock;
    QDockWidget             *_dso_trigger_dock;
    dock::TriggerDock       *_trigger_widget;
    dock::DsoTriggerDock    *_dso_trigger_widget;
    QDockWidget             *_measure_dock;
    dock::MeasureDock       *_measure_widget;
    QDockWidget             *_search_dock;
    dock::SearchDock        *_search_widget;

    QTranslator     _qtTrans;
    QTranslator     _myTrans;
    EventObject     _event;
};

} // namespace pv

#endif // DSVIEW_PV_MAINWINDOW_H
