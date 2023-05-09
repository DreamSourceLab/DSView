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
#include <QJsonDocument>
#include <chrono>

class QAction;
class QMenuBar;
class QMenu;
class QVBoxLayout;
class QStatusBar;
class QToolBar;
class QWidget;
class QDockWidget;
class AppControl;
class DeviceAgent;

using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

namespace pv {
 
class SigSession;

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
 
//The mainwindow,referenced by MainFrame
//TODO: create graph view,toolbar,and show device list
class MainWindow : 
    public QMainWindow,
    public ISessionCallback,
    public IMainForm,
    public ISessionDataGetter,
    public IMessageListener
{
	Q_OBJECT

public:
    static const int Min_Width  = 800;
    static const int Min_Height = 520;
    static const int Base_Height = 150;
    static const int Per_Chan_Height = 35;
     
public:
    explicit MainWindow(QWidget *parent = 0);

    void openDoc();

public slots: 
    void switchTheme(QString style);
    void restore_dock();

private slots:
	void on_load_file(QString file_name);
    void on_open_doc();  
    void on_protocol(bool visible);
    void on_trigger(bool visible);
    void on_measure(bool visible);
    void on_search(bool visible);
    void on_screenShot();
    void on_save();

    void on_export();
    bool on_load_session(QString name);  
    bool on_store_session(QString name); 
    void on_data_updated();
 
    void on_session_error();
    void on_signals_changed();
    void on_receive_trigger(quint64 trigger_pos);
    void on_frame_ended();
    void on_frame_began();
    void on_decode_done();
    void on_receive_data_len(quint64 len);
    void on_cur_snap_samplerate_changed();
    void on_trigger_message(int msg);
  
signals:
    void prgRate(int progress);

public:
    //IMainForm
    void switchLanguage(int language);
    bool able_to_close();
   
private: 
	void setup_ui();
    void retranslateUi(); 
    bool eventFilter(QObject *object, QEvent *event);

    void check_usb_device_speed();
    void reset_all_view();
    bool confirm_to_store_data();
    void update_toolbar_view_status();
    bool load_session_json(QJsonDocument json, bool &haveDecoder);
    QString genSessionFileName(bool isNewFormat);
    bool gen_session_json(QJsonObject &sessionVar);
    void check_session_file_version();
    void load_device_config();
    QJsonDocument get_session_json_from_file(QString file);
    QJsonArray get_decoder_json_from_file(QString file);
    void calc_min_height();
   
private:
    //ISessionCallback
    void show_error(QString error);
    void session_error();
    void data_updated();
    void update_capture();
    void cur_snap_samplerate_changed();
      
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
    void trigger_message(int msg);
    void session_save();

    //ISessionDataGetter
    bool genSessionData(std::string &str);

    //IMessageListener
    void OnMessage(int msg);

private: 
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
    SigSession      *_session;
    DeviceAgent     *_device_agent;
    bool            _is_auto_switch_device;
    high_resolution_clock::time_point _last_key_press_time;
    bool            _is_save_confirm_msg;
    std::string         _pattern_mode;
    QWidget         *_frame;

    int _key_value;
    bool _key_vaild;
};

} // namespace pv

#endif // DSVIEW_PV_MAINWINDOW_H
