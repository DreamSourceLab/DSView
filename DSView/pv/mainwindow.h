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
#include <QTimer>

#include "sigsession.h"

class QAction;
class QMenuBar;
class QMenu;
class QVBoxLayout;
class QStatusBar;
class QToolBar;
class QWidget;
class QDockWidget;

namespace pv {

class DeviceManager;

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

namespace dialogs{
class Calibration;
}

namespace view {
class View;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(DeviceManager &device_manager,
		const char *open_file_name = NULL,
		QWidget *parent = 0);

protected:
    void closeEvent(QCloseEvent *event);

private:
	void setup_ui();

	void session_error(const QString text, const QString info_text);

    bool eventFilter(QObject *object, QEvent *event);

public slots:
    void session_save();

private slots:
	void load_file(QString file_name);

    /**
     * Updates the device list in the sampling bar, and updates the
     * selection.
     * @param selected_device The device to select, or NULL if the
     * first device in the device list should be selected.
     */
    void update_device_list();

    void mode_changed();

    void reload();

	void show_session_error(
		const QString text, const QString info_text);

	void run_stop();

    void instant_stop();

    void test_data_error();

    void malloc_error();

    void capture_state_changed(int state);

    void on_protocol(bool visible);

    void on_trigger(bool visible);

    void commit_trigger(bool instant);

    void on_measure(bool visible);

    void on_search(bool visible);

    void on_screenShot();

    void on_save();

    bool load_session(QString name);
    bool store_session(QString name);

    /*
     * hotplug slot function
     */
    void device_attach();
    void device_detach();

    /*
     * errors
     */
    void hardware_connect_failed();

private:
	DeviceManager &_device_manager;

	SigSession _session;

	pv::view::View *_view;

	QMenuBar *_menu_bar;
	QMenu *_menu_file;
	QAction *_action_open;
	QAction *_action_connect;
	QAction *_action_quit;

	QMenu *_menu_view;
	QAction *_action_view_zoom_in;
	QAction *_action_view_zoom_out;
	QAction *_action_view_show_cursors;

	QMenu *_menu_help;
	QAction *_action_about;

	QWidget *_central_widget;
	QVBoxLayout *_vertical_layout;

	toolbars::SamplingBar *_sampling_bar;
    toolbars::TrigBar *_trig_bar;
    toolbars::FileBar *_file_bar;
    toolbars::LogoBar *_logo_bar;

#ifdef ENABLE_DECODE
    QDockWidget *_protocol_dock;
    dock::ProtocolDock *_protocol_widget;
#endif

    QDockWidget *_trigger_dock;
    QDockWidget *_dso_trigger_dock;
    dock::TriggerDock *_trigger_widget;
    dock::DsoTriggerDock *_dso_trigger_widget;
    QDockWidget *_measure_dock;
    dock::MeasureDock *_measure_widget;
    QDockWidget *_search_dock;
    dock::SearchDock * _search_widget;

    QTimer test_timer;
    bool test_timer_linked;
};

} // namespace pv

#endif // DSVIEW_PV_MAINWINDOW_H
