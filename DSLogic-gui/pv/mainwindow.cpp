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


#ifdef ENABLE_SIGROKDECODE
#include <libsigrokdecode/libsigrokdecode.h>
#endif

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QFileDialog>
#include <QMessageBox>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QDockWidget>
#include <QDebug>
#include <QDesktopWidget>

#include "mainwindow.h"

#include "devicemanager.h"

#include "dialogs/about.h"
#include "dialogs/connect.h"

#include "toolbars/samplingbar.h"
#include "toolbars/devicebar.h"
#include "toolbars/trigbar.h"
#include "toolbars/filebar.h"
#include "toolbars/logobar.h"

#include "dock/protocoldock.h"
#include "dock/triggerdock.h"
#include "dock/dsotriggerdock.h"
#include "dock/measuredock.h"
#include "dock/searchdock.h"

#include "view/view.h"

/* __STDC_FORMAT_MACROS is required for PRIu64 and friends (in C++). */
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdint.h>
#include <stdarg.h>
#include <glib.h>
#include <libsigrok4DSLogic/libsigrok.h>

using namespace std;

namespace pv {

MainWindow::MainWindow(DeviceManager &device_manager,
	const char *open_file_name,
	QWidget *parent) :
    QMainWindow(parent),
    _device_manager(device_manager),
    _session(device_manager)
{
	setup_ui();
	if (open_file_name) {
		const QString s(QString::fromUtf8(open_file_name));
		QMetaObject::invokeMethod(this, "load_file",
			Qt::QueuedConnection,
			Q_ARG(QString, s));
	}
}

void MainWindow::setup_ui()
{
	setObjectName(QString::fromUtf8("MainWindow"));
    setMinimumHeight(680);
	resize(1024, 768);

	// Set the window icon
	QIcon icon;
    icon.addFile(QString::fromUtf8(":/icons/logo.png"),
		QSize(), QIcon::Normal, QIcon::Off);
	setWindowIcon(icon);

	// Setup the central widget
	_central_widget = new QWidget(this);
	_vertical_layout = new QVBoxLayout(_central_widget);
	_vertical_layout->setSpacing(6);
	_vertical_layout->setContentsMargins(0, 0, 0, 0);
	setCentralWidget(_central_widget);

//	// Setup the menu bar
//	_menu_bar = new QMenuBar(this);
//	_menu_bar->setGeometry(QRect(0, 0, 400, 25));

//	// File Menu
//	_menu_file = new QMenu(_menu_bar);
//	_menu_file->setTitle(QApplication::translate(
//		"MainWindow", "&File", 0, QApplication::UnicodeUTF8));

//	_action_open = new QAction(this);
//	_action_open->setText(QApplication::translate(
//		"MainWindow", "&Open...", 0, QApplication::UnicodeUTF8));
//	_action_open->setIcon(QIcon::fromTheme("document-open",
//		QIcon(":/icons/document-open.png")));
//	_action_open->setObjectName(QString::fromUtf8("actionOpen"));
//	_menu_file->addAction(_action_open);

//	_menu_file->addSeparator();

//	_action_connect = new QAction(this);
//	_action_connect->setText(QApplication::translate(
//		"MainWindow", "&Connect to Device...", 0,
//		QApplication::UnicodeUTF8));
//	_action_connect->setObjectName(QString::fromUtf8("actionConnect"));
//	_menu_file->addAction(_action_connect);

//	_menu_file->addSeparator();

//	_action_quit = new QAction(this);
//	_action_quit->setText(QApplication::translate(
//		"MainWindow", "&Quit", 0, QApplication::UnicodeUTF8));
//	_action_quit->setIcon(QIcon::fromTheme("application-exit",
//		QIcon(":/icons/application-exit.png")));
//	_action_quit->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
//	_action_quit->setObjectName(QString::fromUtf8("actionQuit"));
//	_menu_file->addAction(_action_quit);

//	// View Menu
//	_menu_view = new QMenu(_menu_bar);
//	_menu_view->setTitle(QApplication::translate(
//		"MainWindow", "&View", 0, QApplication::UnicodeUTF8));

//	_action_view_zoom_in = new QAction(this);
//	_action_view_zoom_in->setText(QApplication::translate(
//		"MainWindow", "Zoom &In", 0, QApplication::UnicodeUTF8));
//	_action_view_zoom_in->setIcon(QIcon::fromTheme("zoom-in",
//		QIcon(":/icons/zoom-in.png")));
//	_action_view_zoom_in->setObjectName(
//		QString::fromUtf8("actionViewZoomIn"));
//	_menu_view->addAction(_action_view_zoom_in);

//	_action_view_zoom_out = new QAction(this);
//	_action_view_zoom_out->setText(QApplication::translate(
//		"MainWindow", "Zoom &Out", 0, QApplication::UnicodeUTF8));
//	_action_view_zoom_out->setIcon(QIcon::fromTheme("zoom-out",
//		QIcon(":/icons/zoom-out.png")));
//	_action_view_zoom_out->setObjectName(
//		QString::fromUtf8("actionViewZoomOut"));
//	_menu_view->addAction(_action_view_zoom_out);

//	_menu_view->addSeparator();

//	_action_view_show_cursors = new QAction(this);
//	_action_view_show_cursors->setCheckable(true);
//	_action_view_show_cursors->setChecked(_view->cursors_shown());
//	_action_view_show_cursors->setShortcut(QKeySequence(Qt::Key_C));
//	_action_view_show_cursors->setObjectName(
//		QString::fromUtf8("actionViewShowCursors"));
//	_action_view_show_cursors->setText(QApplication::translate(
//		"MainWindow", "Show &Cursors", 0, QApplication::UnicodeUTF8));
//	_menu_view->addAction(_action_view_show_cursors);

//	// Help Menu
//	_menu_help = new QMenu(_menu_bar);
//	_menu_help->setTitle(QApplication::translate(
//		"MainWindow", "&Help", 0, QApplication::UnicodeUTF8));

//	_action_about = new QAction(this);
//	_action_about->setObjectName(QString::fromUtf8("actionAbout"));
//	_action_about->setText(QApplication::translate(
//		"MainWindow", "&About...", 0, QApplication::UnicodeUTF8));
//	_menu_help->addAction(_action_about);

//	_menu_bar->addAction(_menu_file->menuAction());
//	_menu_bar->addAction(_menu_view->menuAction());
//	_menu_bar->addAction(_menu_help->menuAction());

    //setMenuBar(_menu_bar);
    //QMenuBar *_void_menu = new QMenuBar(this);
    //setMenuBar(_void_menu);
	//QMetaObject::connectSlotsByName(this);

	// Setup the sampling bar
	_sampling_bar = new toolbars::SamplingBar(this);
    _trig_bar = new toolbars::TrigBar(this);
    _file_bar = new toolbars::FileBar(_session, this);
    _device_bar = new toolbars::DeviceBar(this);
    _logo_bar = new toolbars::LogoBar(_session, this);

    connect(_trig_bar, SIGNAL(on_protocol(bool)), this,
            SLOT(on_protocol(bool)));
    connect(_trig_bar, SIGNAL(on_trigger(bool)), this,
            SLOT(on_trigger(bool)));
    connect(_trig_bar, SIGNAL(on_measure(bool)), this,
            SLOT(on_measure(bool)));
    connect(_trig_bar, SIGNAL(on_search(bool)), this,
            SLOT(on_search(bool)));
    connect(_file_bar, SIGNAL(on_screenShot()), this,
            SLOT(on_screenShot()));

    // protocol dock
    _protocol_dock=new QDockWidget(tr("Protocol"),this);
    _protocol_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    _protocol_dock->setAllowedAreas(Qt::RightDockWidgetArea);
    _protocol_dock->setVisible(false);
    //dock::ProtocolDock *_protocol_widget = new dock::ProtocolDock(_protocol_dock, _session);
    _protocol_widget = new dock::ProtocolDock(_protocol_dock, _session);
    _protocol_dock->setWidget(_protocol_widget);
    // trigger dock
    _trigger_dock=new QDockWidget(tr("Trigger Setting..."),this);
    _trigger_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    _trigger_dock->setAllowedAreas(Qt::RightDockWidgetArea);
    _trigger_dock->setVisible(false);
    _trigger_widget = new dock::TriggerDock(_trigger_dock, _session);
    _trigger_dock->setWidget(_trigger_widget);

    _dso_trigger_dock=new QDockWidget(tr("Trigger Setting..."),this);
    _dso_trigger_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    _dso_trigger_dock->setAllowedAreas(Qt::RightDockWidgetArea);
    _dso_trigger_dock->setVisible(false);
    _dso_trigger_widget = new dock::DsoTriggerDock(_dso_trigger_dock, _session);
    _dso_trigger_dock->setWidget(_dso_trigger_widget);


    // Setup _view widget
    _view = new pv::view::View(_session, this);
    _vertical_layout->addWidget(_view);

	// Populate the device list and select the initially selected device
	update_device_list();

    connect(_device_bar, SIGNAL(device_selected()), this,
        SLOT(device_selected()));
//    connect(_device_bar, SIGNAL(device_selected()), this,
//            SLOT(init()));
    connect(_device_bar, SIGNAL(device_updated()), this,
        SLOT(update()));
    connect(_sampling_bar, SIGNAL(device_reload()), this,
        SLOT(init()));
    connect(_sampling_bar, SIGNAL(run_stop()), this,
        SLOT(run_stop()));
    addToolBar(_sampling_bar);
    addToolBar(_trig_bar);
    addToolBar(_device_bar);
    addToolBar(_file_bar);
    addToolBar(_logo_bar);

    // Setup the dockWidget
    // protocol dock
//    _protocol_dock=new QDockWidget(tr("Protocol"),this);
//    _protocol_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
//    _protocol_dock->setAllowedAreas(Qt::RightDockWidgetArea);
//    _protocol_dock->setVisible(false);
//    //dock::ProtocolDock *_protocol_widget = new dock::ProtocolDock(_protocol_dock, _session);
//    _protocol_widget = new dock::ProtocolDock(_protocol_dock, _session);
//    _protocol_dock->setWidget(_protocol_widget);
//    // trigger dock
//    _trigger_dock=new QDockWidget(tr("Trigger Setting..."),this);
//    _trigger_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
//    _trigger_dock->setAllowedAreas(Qt::RightDockWidgetArea);
//    _trigger_dock->setVisible(false);
//    dock::TriggerDock *_trigger_widget = new dock::TriggerDock(_trigger_dock, _session);
//    _trigger_dock->setWidget(_trigger_widget);
    // measure dock
    _measure_dock=new QDockWidget(tr("Measurement"),this);
    _measure_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    _measure_dock->setAllowedAreas(Qt::RightDockWidgetArea);
    _measure_dock->setVisible(false);
    dock::MeasureDock *_measure_widget = new dock::MeasureDock(_measure_dock, *_view, _session);
    _measure_dock->setWidget(_measure_widget);
    // search dock
    _search_dock=new QDockWidget(tr("Search..."), this);
    _search_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    _search_dock->setTitleBarWidget(new QWidget(_search_dock));
    _search_dock->setAllowedAreas(Qt::BottomDockWidgetArea);
    _search_dock->setVisible(false);
    //dock::SearchDock *_search_widget = new dock::SearchDock(_search_dock, *_view, _session);
    _search_widget = new dock::SearchDock(_search_dock, *_view, _session);
    _search_dock->setWidget(_search_widget);


    _protocol_dock->setObjectName(tr("protocolDock"));
    _trigger_dock->setObjectName(tr("triggerDock"));
    addDockWidget(Qt::RightDockWidgetArea,_protocol_dock);
    addDockWidget(Qt::RightDockWidgetArea,_trigger_dock);
    addDockWidget(Qt::RightDockWidgetArea,_dso_trigger_dock);
    addDockWidget(Qt::RightDockWidgetArea, _measure_dock);
    addDockWidget(Qt::BottomDockWidgetArea, _search_dock);

	// Set the title
    setWindowTitle(QApplication::translate("MainWindow", "DSLogic(Beta)", 0,
		QApplication::UnicodeUTF8));

	// Setup _session events
	connect(&_session, SIGNAL(capture_state_changed(int)), this,
		SLOT(capture_state_changed(int)));
    connect(&_session, SIGNAL(device_attach()), this,
            SLOT(device_attach()));
    connect(&_session, SIGNAL(device_detach()), this,
            SLOT(device_detach()));
    connect(&_session, SIGNAL(test_data_error()), this,
            SLOT(test_data_error()));
    connect(&_session, SIGNAL(dso_ch_changed(uint16_t)), this,
            SLOT(dso_ch_changed(uint16_t)));

    connect(_view, SIGNAL(cursor_update()), _measure_widget,
            SLOT(cursor_update()));
    connect(_view, SIGNAL(cursor_moved()), _measure_widget,
            SLOT(cursor_moved()));
    connect(_view, SIGNAL(mouse_moved()), _measure_widget,
            SLOT(mouse_moved()));
}

void MainWindow::init()
{
    _protocol_widget->del_all_protocol();
    _trigger_widget->device_change();
    if (_session.get_device())
        _session.init_signals(_session.get_device());
    if (_session.get_device()->mode == DSO) {
        _sampling_bar->set_record_length(DefaultDSODepth*2);
        _sampling_bar->set_sample_rate(DefaultDSORate*2);
        _sampling_bar->enable_toggle(false);
        _view->hDial_changed(0);
    } else if(_session.get_device()->mode == LOGIC) {
        _sampling_bar->enable_toggle(true);
    }
}

void MainWindow::update()
{
    if (_session.get_device())
        _session.update_signals(_session.get_device());
}

void MainWindow::session_error(
	const QString text, const QString info_text)
{
	QMetaObject::invokeMethod(this, "show_session_error",
		Qt::QueuedConnection, Q_ARG(QString, text),
		Q_ARG(QString, info_text));
}

void MainWindow::update_device_list(struct sr_dev_inst *selected_device)
{
    assert(_device_bar);

	const list<sr_dev_inst*> &devices = _device_manager.devices();
    _device_bar->set_device_list(devices);

	if (!selected_device && !devices.empty()) {
		// Fall back to the first device in the list.
		selected_device = devices.front();

		// Try and find the demo device and select that by default
		BOOST_FOREACH (struct sr_dev_inst *sdi, devices)
            if (strcmp(sdi->driver->name, "DSLogic") == 0) {
				selected_device = sdi;
			}
	}

	if (selected_device) {
        if (_session.set_device(selected_device) == SR_OK) {
            _device_bar->set_selected_device(selected_device, false);
            _sampling_bar->set_device(selected_device);
            _sampling_bar->update_sample_rate_selector();
            _logo_bar->dslogic_connected(strcmp(selected_device->driver->name, "DSLogic") == 0);
            init();
        } else {
            show_session_error("Open Device Failed",
                               "the selected device can't be opened!");
        }
    }

//    #ifdef HAVE_LA_DSLOGIC
    _session.start_hotplug_proc(boost::bind(&MainWindow::session_error, this,
                                             QString("Hotplug failed"), _1));
    _session.start_dso_ctrl_proc(boost::bind(&MainWindow::session_error, this,
                                             QString("Hotplug failed"), _1));
//    #endif
}

void MainWindow::device_change()
{
    assert(_device_bar);

    struct sr_dev_inst *selected_device;

    const list<sr_dev_inst*> &devices = _device_manager.devices();
    _device_bar->set_device_list(devices);

    // Fall back to the first device in the list.
    selected_device = devices.front();

    // Try and find the demo device and select that by default
    BOOST_FOREACH (struct sr_dev_inst *sdi, devices)
        if (strcmp(sdi->driver->name, "DSLogic") == 0) {
            selected_device = sdi;
        }

    if (_session.set_device(selected_device) == SR_OK) {;
        _device_bar->set_selected_device(selected_device, true);
        _sampling_bar->set_device(selected_device);
        _sampling_bar->update_sample_rate_selector();
        _logo_bar->dslogic_connected(strcmp(selected_device->driver->name, "DSLogic") == 0);
        init();
    } else {
//        show_session_error("Open Device Failed",
//                           "the selected device can't be opened!");
        device_detach();
    }

//    #ifdef HAVE_LA_DSLOGIC
    _session.stop_hotplug_proc();
    _session.start_hotplug_proc(boost::bind(&MainWindow::session_error, this,
                                             QString("Hotplug failed"), _1));
//    #endif


}

void MainWindow::load_file(QString file_name)
{
	const QString errorMessage(
		QString("Failed to load file %1").arg(file_name));
	const QString infoMessage;
	_session.load_file(file_name.toStdString(),
		boost::bind(&MainWindow::session_error, this,
			errorMessage, infoMessage));
}

void MainWindow::show_session_error(
	const QString text, const QString info_text)
{
	QMessageBox msg(this);
	msg.setText(text);
	msg.setInformativeText(info_text);
	msg.setStandardButtons(QMessageBox::Ok);
	msg.setIcon(QMessageBox::Warning);
	msg.exec();
}

void MainWindow::device_selected()
{   
    if (_session.set_device(_device_bar->get_selected_device()) == SR_OK) {;
        _sampling_bar->set_device(_device_bar->get_selected_device());
        _sampling_bar->update_sample_rate_selector();
        _view->show_trig_cursor(false);
        _trigger_dock->setVisible(false);
        _dso_trigger_dock->setVisible(false);
        _protocol_dock->setVisible(false);
        _measure_dock->setVisible(false);
        _search_dock->setVisible(false);
        init();
    } else {
        show_session_error("Open Device Failed",
                           "the selected device can't be opened!");
    }

    if (_device_bar->get_selected_device()->mode == DSO) {
       QMessageBox msg(this);
       msg.setText("Zero Adjustment");
       msg.setInformativeText("Please left both of channels unconnect for zero adjustment!");
       msg.setStandardButtons(QMessageBox::Ok);
       msg.setIcon(QMessageBox::Warning);
       msg.exec();

       int ret = sr_config_set(_device_bar->get_selected_device(), SR_CONF_ZERO, g_variant_new_boolean(TRUE));
       if (ret != SR_OK) {
            QMessageBox msg(this);
            msg.setText("Zero Adjustment Issue");
            msg.setInformativeText("Can't send out the command of zero adjustment!");
            msg.setStandardButtons(QMessageBox::Ok);
            msg.setIcon(QMessageBox::Warning);
            msg.exec();
        } else {
           run_stop();
           g_usleep(100000);
           run_stop();
       }
    }
}

void MainWindow::device_attach()
{
    _session.stop_hotplug_proc();

    if (_session.get_capture_state() == SigSession::Running)
        _session.stop_capture();

    struct sr_dev_driver **const drivers = sr_driver_list();
    struct sr_dev_driver **driver;
    for (driver = drivers; strcmp(((struct sr_dev_driver *)*driver)->name, "DSLogic") != 0 && *driver; driver++);

    if (*driver)
        _device_manager.driver_scan(*driver);

    device_change();
}

void MainWindow::device_detach()
{
    _session.stop_hotplug_proc();

    if (_session.get_capture_state() == SigSession::Running)
        _session.stop_capture();

    struct sr_dev_driver **const drivers = sr_driver_list();
    struct sr_dev_driver **driver;
    for (driver = drivers; strcmp(((struct sr_dev_driver *)*driver)->name, "DSLogic") != 0 && *driver; driver++);

    if (*driver)
        _device_manager.driver_scan(*driver);

    device_change();
}

void MainWindow::run_stop()
{
    _sampling_bar->enable_run_stop(false);
    switch(_session.get_capture_state()) {
    case SigSession::Init:
	case SigSession::Stopped:
        _view->show_trig_cursor(false);
        _session.set_total_sample_len(_sampling_bar->get_record_length());
		_session.start_capture(_sampling_bar->get_record_length(),
			boost::bind(&MainWindow::session_error, this,
				QString("Capture failed"), _1));
		break;

	case SigSession::Running:
		_session.stop_capture();
		break;
	}
    g_usleep(1000);
    _sampling_bar->enable_run_stop(true);
}

void MainWindow::dso_ch_changed(uint16_t num)
{
    if(num == 1) {
        _sampling_bar->set_record_length(DefaultDSODepth*2);
        _sampling_bar->set_sample_rate(DefaultDSORate*2);
        _session.set_total_sample_len(_sampling_bar->get_record_length());
        if (_session.get_capture_state() == SigSession::Running) {
            _session.stop_capture();
            _session.start_capture(_sampling_bar->get_record_length(),
                boost::bind(&MainWindow::session_error, this,
                    QString("Capture failed"), _1));
        }
    } else {
        _sampling_bar->set_record_length(DefaultDSODepth);
        _sampling_bar->set_sample_rate(DefaultDSORate);
        _session.set_total_sample_len(_sampling_bar->get_record_length());
        if (_session.get_capture_state() == SigSession::Running) {
            _session.stop_capture();
            _session.start_capture(_sampling_bar->get_record_length(),
                boost::bind(&MainWindow::session_error, this,
                    QString("Capture failed"), _1));
        }
    }
}

void MainWindow::test_data_error()
{
    _session.stop_capture();
    QMessageBox msg(this);
    msg.setText("Data Error");
    msg.setInformativeText("the receive data are not consist with pre-defined test data");
    msg.setStandardButtons(QMessageBox::Ok);
    msg.setIcon(QMessageBox::Warning);
    msg.exec();
}

void MainWindow::capture_state_changed(int state)
{
    if (_session.get_device()->mode != DSO) {
        _sampling_bar->enable_toggle(state != SigSession::Running);
        _trig_bar->enable_toggle(state != SigSession::Running);
        _measure_dock->widget()->setEnabled(state != SigSession::Running);
    }
    _device_bar->enable_toggle(state != SigSession::Running);
    _file_bar->enable_toggle(state != SigSession::Running);
    _sampling_bar->set_sampling(state == SigSession::Running);
    _view->on_state_changed(state != SigSession::Running);
}

void MainWindow::on_protocol(bool visible)
{
    _protocol_dock->setVisible(visible);
}

void MainWindow::on_trigger(bool visible)
{
    if (_session.get_device()->mode != DSO)
        _trigger_dock->setVisible(visible);
    else
        _dso_trigger_dock->setVisible(visible);
}

void MainWindow::on_measure(bool visible)
{
    _measure_dock->setVisible(visible);
}

void MainWindow::on_search(bool visible)
{
    _search_dock->setVisible(visible);
    _view->show_search_cursor(visible);
}

void MainWindow::on_screenShot()
{
    QPixmap pixmap;
    QDesktopWidget *desktop = QApplication::desktop();
    pixmap = QPixmap::grabWindow(desktop->winId(), pos().x(), pos().y(), frameGeometry().width(), frameGeometry().height());
    QString format = "png";
    QString initialPath = QDir::currentPath()+
                          tr("/untitled.") + format;

    QString fileName = QFileDialog::getSaveFileName(this,
                       tr("Save As"),initialPath,
                       tr("%1 Files (*.%2);;All Files (*)")
                       .arg(format.toUpper()).arg(format));
    if (!fileName.isEmpty())
        pixmap.save(fileName, format.toAscii());
}

} // namespace pv
