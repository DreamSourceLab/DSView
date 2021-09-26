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


#ifdef ENABLE_DECODE
#include <libsigrokdecode4DSL/libsigrokdecode.h>
#include "dock/protocoldock.h"
#endif

#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>

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
#include <QDesktopServices>
#include <QKeyEvent>
#include <QEvent>
#include <QtGlobal>
#include <QScreen>

#include "mainwindow.h"

#include "devicemanager.h"
#include "device/device.h"
#include "device/file.h"

#include "data/logicsnapshot.h"
#include "data/dsosnapshot.h"
#include "data/analogsnapshot.h"

#include "dialogs/about.h"
#include "dialogs/deviceoptions.h"
#include "dialogs/storeprogress.h"
#include "dialogs/waitingdialog.h"
#include "dialogs/regionoptions.h"

#include "toolbars/samplingbar.h"
#include "toolbars/trigbar.h"
#include "toolbars/filebar.h"
#include "toolbars/logobar.h"
#include "toolbars/titlebar.h"

#include "dock/triggerdock.h"
#include "dock/dsotriggerdock.h"
#include "dock/measuredock.h"
#include "dock/searchdock.h"

#include "view/view.h"
#include "view/trace.h"
#include "view/signal.h"
#include "view/dsosignal.h"
#include "view/logicsignal.h"
#include "view/analogsignal.h"

/* __STDC_FORMAT_MACROS is required for PRIu64 and friends (in C++). */
#include <inttypes.h>
#include <stdint.h>
#include <stdarg.h>
#include <glib.h>
#include <list>
#include <libusb.h>

using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using std::list;
using std::vector;

namespace pv {

MainWindow::MainWindow(DeviceManager &device_manager,
	const char *open_file_name,
	QWidget *parent) :
    QMainWindow(parent),
    _device_manager(device_manager),
    _session(device_manager),
    _hot_detach(false),
    _msg(NULL)
{
	setup_ui();
	if (open_file_name) {
        qDebug("Open file: %s", open_file_name);
        const QString s(QString::fromUtf8(open_file_name));
		QMetaObject::invokeMethod(this, "load_file",
			Qt::QueuedConnection,
			Q_ARG(QString, s));
	}
}

void MainWindow::setup_ui()
{
	setObjectName(QString::fromUtf8("MainWindow"));
    setContentsMargins(0,0,0,0);
    layout()->setMargin(0);
    layout()->setSpacing(0);

	// Setup the central widget
	_central_widget = new QWidget(this);
	_vertical_layout = new QVBoxLayout(_central_widget);
    _vertical_layout->setSpacing(0);
	_vertical_layout->setContentsMargins(0, 0, 0, 0);
	setCentralWidget(_central_widget);

	// Setup the sampling bar
    _sampling_bar = new toolbars::SamplingBar(_session, this);
    _sampling_bar->setObjectName("sampling_bar");
    _trig_bar = new toolbars::TrigBar(_session, this);
    _trig_bar->setObjectName("trig_bar");
    _file_bar = new toolbars::FileBar(_session, this);
    _file_bar->setObjectName("file_bar");
    _logo_bar = new toolbars::LogoBar(_session, this);
    _logo_bar->setObjectName("logo_bar");

    connect(_trig_bar, SIGNAL(on_protocol(bool)), this,
            SLOT(on_protocol(bool)));
    connect(_trig_bar, SIGNAL(on_trigger(bool)), this,
            SLOT(on_trigger(bool)));
    connect(_trig_bar, SIGNAL(on_measure(bool)), this,
            SLOT(on_measure(bool)));
    connect(_trig_bar, SIGNAL(on_search(bool)), this,
            SLOT(on_search(bool)));
    connect(_trig_bar, SIGNAL(setTheme(QString)), this,
            SLOT(switchTheme(QString)));
    connect(_file_bar, SIGNAL(load_file(QString)), this,
            SLOT(load_file(QString)));
    connect(_file_bar, SIGNAL(on_save()), this,
            SLOT(on_save()));
    connect(_file_bar, SIGNAL(on_export()), this,
            SLOT(on_export()));
    connect(_file_bar, SIGNAL(on_screenShot()), this,
            SLOT(on_screenShot()), Qt::QueuedConnection);
    connect(_file_bar, SIGNAL(load_session(QString)), this,
            SLOT(load_session(QString)));
    connect(_file_bar, SIGNAL(store_session(QString)), this,
            SLOT(store_session(QString)));
    connect(_logo_bar, SIGNAL(setLanguage(int)), this,
            SLOT(switchLanguage(int)));
    connect(_logo_bar, SIGNAL(openDoc()), this,
            SLOT(openDoc()));

    // trigger dock
    _trigger_dock=new QDockWidget(tr("Trigger Setting..."),this);
    _trigger_dock->setObjectName("trigger_dock");
    _trigger_dock->setFeatures(QDockWidget::DockWidgetMovable);
    _trigger_dock->setAllowedAreas(Qt::RightDockWidgetArea);
    _trigger_dock->setVisible(false);
    _trigger_widget = new dock::TriggerDock(_trigger_dock, _session);
    _trigger_dock->setWidget(_trigger_widget);

    _dso_trigger_dock=new QDockWidget(tr("Trigger Setting..."),this);
    _dso_trigger_dock->setObjectName("dso_trigger_dock");
    _dso_trigger_dock->setFeatures(QDockWidget::DockWidgetMovable);
    _dso_trigger_dock->setAllowedAreas(Qt::RightDockWidgetArea);
    _dso_trigger_dock->setVisible(false);
    _dso_trigger_widget = new dock::DsoTriggerDock(_dso_trigger_dock, _session);
    _dso_trigger_dock->setWidget(_dso_trigger_widget);


    // Setup _view widget
    _view = new pv::view::View(_session, _sampling_bar, this);
    _vertical_layout->addWidget(_view);

    connect(_sampling_bar, SIGNAL(device_selected()), this,
            SLOT(update_device_list()));
    connect(_sampling_bar, SIGNAL(device_updated()), this,
        SLOT(reload()));
    connect(_sampling_bar, SIGNAL(run_stop()), this,
        SLOT(run_stop()));
    connect(_sampling_bar, SIGNAL(instant_stop()), this,
        SLOT(instant_stop()));
    connect(_sampling_bar, SIGNAL(duration_changed()), _trigger_widget,
        SLOT(device_updated()));
    connect(_sampling_bar, SIGNAL(duration_changed()), _view,
        SLOT(timebase_changed()));
    connect(_sampling_bar, SIGNAL(show_calibration()), _view,
        SLOT(show_calibration()));
    connect(_trig_bar, SIGNAL(show_lissajous(bool)), _view,
        SLOT(show_lissajous(bool)));
    connect(_dso_trigger_widget, SIGNAL(set_trig_pos(int)), _view,
        SLOT(set_trig_pos(int)));
    connect(_view, SIGNAL(auto_trig(int)), _dso_trigger_widget,
        SLOT(auto_trig(int)));

    setIconSize(QSize(40,40));
    addToolBar(_sampling_bar);
    addToolBar(_trig_bar);
    addToolBar(_file_bar);
    addToolBar(_logo_bar);

    // Setup the dockWidget
#ifdef ENABLE_DECODE
    // protocol dock
    _protocol_dock=new QDockWidget(tr("Protocol"),this);
    _protocol_dock->setObjectName("protocol_dock");
    _protocol_dock->setFeatures(QDockWidget::DockWidgetMovable);
    _protocol_dock->setAllowedAreas(Qt::RightDockWidgetArea);
    _protocol_dock->setVisible(false);
    //dock::ProtocolDock *_protocol_widget = new dock::ProtocolDock(_protocol_dock, _session);
    _protocol_widget = new dock::ProtocolDock(_protocol_dock, *_view, _session);
    _protocol_dock->setWidget(_protocol_widget);
    qDebug() << "Protocol decoder enabled!\n";

    connect(_protocol_widget, SIGNAL(protocol_updated()), _view, SLOT(signals_changed()));
#endif
    // measure dock
    _measure_dock=new QDockWidget(tr("Measurement"),this);
    _measure_dock->setObjectName("measure_dock");
    _measure_dock->setFeatures(QDockWidget::DockWidgetMovable);
    _measure_dock->setAllowedAreas(Qt::RightDockWidgetArea);
    _measure_dock->setVisible(false);
    _measure_widget = new dock::MeasureDock(_measure_dock, *_view, _session);
    _measure_dock->setWidget(_measure_widget);
    // search dock
    _search_dock=new QDockWidget(tr("Search..."), this);
    _search_dock->setObjectName("search_dock");
    _search_dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    _search_dock->setTitleBarWidget(new QWidget(_search_dock));
    _search_dock->setAllowedAreas(Qt::BottomDockWidgetArea);
    _search_dock->setVisible(false);
    //dock::SearchDock *_search_widget = new dock::SearchDock(_search_dock, *_view, _session);
    _search_widget = new dock::SearchDock(_search_dock, *_view, _session);
    _search_dock->setWidget(_search_widget);

#ifdef ENABLE_DECODE
    addDockWidget(Qt::RightDockWidgetArea,_protocol_dock);
#endif
    addDockWidget(Qt::RightDockWidgetArea,_trigger_dock);
    addDockWidget(Qt::RightDockWidgetArea,_dso_trigger_dock);
    addDockWidget(Qt::RightDockWidgetArea, _measure_dock);
    addDockWidget(Qt::BottomDockWidgetArea, _search_dock);

	// Set the title
    QString title = QApplication::applicationName()+" v"+QApplication::applicationVersion();
    std::string std_title = title.toStdString();
    setWindowTitle(QApplication::translate("MainWindow", std_title.c_str(), 0));

	// Setup _session events
	connect(&_session, SIGNAL(capture_state_changed(int)), this,
		SLOT(capture_state_changed(int)));
    connect(&_session, SIGNAL(device_attach()), this,
            SLOT(device_attach()), Qt::QueuedConnection);
    connect(&_session, SIGNAL(device_detach()), this,
            SLOT(device_detach()), Qt::QueuedConnection);
    connect(&_session, SIGNAL(session_error()), this,
            SLOT(show_error()), Qt::QueuedConnection);
    connect(&_session, SIGNAL(session_save()), this,
            SLOT(session_save()));
    connect(&_session, SIGNAL(data_updated()), _measure_widget,
            SLOT(reCalc()));
    connect(&_session, SIGNAL(repeat_resume()), this,
            SLOT(repeat_resume()));
    connect(&_session, SIGNAL(update_capture()), _view,
            SLOT(update_hori_res()), Qt::DirectConnection);

    connect(&_session, SIGNAL(cur_snap_samplerate_changed()), _measure_widget,
            SLOT(cursor_update()));
    connect(_view, SIGNAL(cursor_update()), _measure_widget,
            SLOT(cursor_update()));
    connect(_view, SIGNAL(cursor_moving()), _measure_widget,
            SLOT(cursor_moving()));
    connect(_view, SIGNAL(cursor_moved()), _measure_widget,
            SLOT(reCalc()));
    connect(_view, SIGNAL(prgRate(int)), this, SIGNAL(prgRate(int)));
    connect(_view, SIGNAL(device_changed(bool)),
            this, SLOT(device_changed(bool)), Qt::DirectConnection);

    // event filter
    _view->installEventFilter(this);
    _sampling_bar->installEventFilter(this);
    _trig_bar->installEventFilter(this);
    _file_bar->installEventFilter(this);
    _logo_bar->installEventFilter(this);
    _dso_trigger_dock->installEventFilter(this);
    _trigger_dock->installEventFilter(this);
#ifdef ENABLE_DECODE
    _protocol_dock->installEventFilter(this);
#endif
    _measure_dock->installEventFilter(this);
    _search_dock->installEventFilter(this);

    // Populate the device list and select the initially selected device
    _session.set_default_device(boost::bind(&MainWindow::session_error, this,
                                            QString(tr("Set Default Device failed")), _1));

    // defaut language
    QSettings settings(QApplication::organizationName(), QApplication::applicationName());
    QLocale locale;
    settings.beginGroup("MainFrame");
    switchLanguage(settings.value("language", locale.language()).toInt());
    switchTheme(settings.value("style", "dark").toString());
    settings.endGroup();

    // UI initial
    _measure_widget->add_dist_measure();

    // update device
    update_device_list();
    _session.start_hotplug_proc(boost::bind(&MainWindow::session_error, this,
                                             QString(tr("Hotplug failed")), _1));

    retranslateUi();
}


void MainWindow::retranslateUi()
{
    _trigger_dock->setWindowTitle(tr("Trigger Setting..."));
    _dso_trigger_dock->setWindowTitle(tr("Trigger Setting..."));
    _protocol_dock->setWindowTitle(tr("Protocol"));
    _measure_dock->setWindowTitle(tr("Measurement"));
    _search_dock->setWindowTitle(tr("Search..."));
}

void MainWindow::session_error(
	const QString text, const QString info_text)
{
	QMetaObject::invokeMethod(this, "show_session_error",
		Qt::QueuedConnection, Q_ARG(QString, text),
		Q_ARG(QString, info_text));
}

void MainWindow::update_device_list()
{
    assert(_sampling_bar);

    if (_msg)
        _msg->close();

    switchLanguage(_language);
    _session.stop_capture();
    _view->reload();
    _trigger_widget->device_updated();
#ifdef ENABLE_DECODE
    _protocol_widget->del_all_protocol();
#endif
    _trig_bar->reload();

    shared_ptr<pv::device::DevInst> selected_device = _session.get_device();
    _device_manager.add_device(selected_device);
    _session.init_signals();
    _sampling_bar->set_device_list(_device_manager.devices(), selected_device);

    shared_ptr<pv::device::File> file_dev;
    if((file_dev = dynamic_pointer_cast<pv::device::File>(selected_device))) {
        // check version
        if (selected_device->dev_inst()->mode == LOGIC) {
            GVariant* gvar = selected_device->get_config(NULL, NULL, SR_CONF_FILE_VERSION);
            if (gvar != NULL) {
                int16_t version = g_variant_get_int16(gvar);
                g_variant_unref(gvar);
                if (version == 1) {
                    show_session_error(tr("Attension"),
                                       tr("Current loading file has an old format. "
                                          "This will lead to a slow loading speed. "
                                          "Please resave it after loaded."));
                }
            }
        }

        #ifdef ENABLE_DECODE
        // load decoders
        StoreSession ss(_session);
        ss.load_decoders(_protocol_widget, file_dev->get_decoders());
        #endif

        // load session
        load_session_json(file_dev->get_session(), true);

        // load data
        const QString errorMessage(
            QString(tr("Failed to capture file data!")));
        _session.start_capture(true, boost::bind(&MainWindow::session_error, this,
            errorMessage, _1));
    }

    if (!selected_device->name().contains("virtual")) {
        _file_bar->set_settings_en(true);
        _logo_bar->dsl_connected(true);
        #if QT_VERSION >= 0x050400
        QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
        #else
        QDir dir(QStandardPaths::writableLocation(QStandardPaths::DataLocation));
        #endif
        if (dir.exists()) {
            QString str = dir.absolutePath() + "/";
            QString lang_name = ".ses" + QString::number(_language);
            QString ses_name = str +
                               selected_device->name() +
                               QString::number(selected_device->dev_inst()->mode) +
                               lang_name + ".dsc";
            load_session(ses_name);
        }
    } else {
        _file_bar->set_settings_en(false);
        _logo_bar->dsl_connected(false);
        #ifdef Q_OS_LINUX
            QDir dir(DS_RES_PATH);
        #else
            QDir dir(QCoreApplication::applicationDirPath());
            assert(dir.cd("res"));
        #endif
        if (dir.exists()) {
            QString str = dir.absolutePath() + "/";
            QString ses_name = str +
                               selected_device->name() +
                               QString::number(selected_device->dev_inst()->mode) +
                               ".dsc";
            if (QFileInfo(ses_name).exists())
                load_session(ses_name);
        }
    }
    _sampling_bar->reload();
    _view->status_clear();
    _trigger_widget->init();
    _dso_trigger_widget->init();
	_measure_widget->reload();

    // USB device speed check
    if (!selected_device->name().contains("virtual")) {
        int usb_speed = LIBUSB_SPEED_HIGH;
        GVariant *gvar = selected_device->get_config(NULL, NULL, SR_CONF_USB_SPEED);
        if (gvar != NULL) {
            usb_speed = g_variant_get_int32(gvar);
            g_variant_unref(gvar);
        }

        bool usb30_support = false;
        gvar = selected_device->get_config(NULL, NULL, SR_CONF_USB30_SUPPORT);
        if (gvar != NULL) {
            usb30_support = g_variant_get_boolean(gvar);
            g_variant_unref(gvar);

            if (usb30_support && usb_speed == LIBUSB_SPEED_HIGH)
                show_session_error(tr("Speed limited"), tr("This is a super-speed usb device(USB 3.0). "
                                                           "Plug it into a USB 2.0 port will seriously affect its performance."
                                                           "Please replug it into a USB 3.0 port."));
        }
    }
}


void MainWindow::reload()
{
    _trigger_widget->device_updated();
    _session.reload();
    _measure_widget->reload();
}

void MainWindow::load_file(QString file_name)
{
    try {
        if (strncmp(_session.get_device()->name().toUtf8(), "virtual", 7))
            session_save();
        _session.set_file(file_name);
    } catch(QString e) {
        show_session_error(tr("Failed to load ") + file_name, e);
        _session.set_default_device(boost::bind(&MainWindow::session_error, this,
                                                QString(tr("Set Default Device failed")), _1));
        update_device_list();
        return;
    }

    update_device_list();
}

void MainWindow::show_session_error(
	const QString text, const QString info_text)
{
    dialogs::DSMessageBox msg(this);
    _msg = &msg;
    msg.mBox()->setText(text);
    msg.mBox()->setInformativeText(info_text);
    msg.mBox()->setStandardButtons(QMessageBox::Ok);
    msg.mBox()->setIcon(QMessageBox::Warning);
	msg.exec();
    _msg = NULL;
}

void MainWindow::device_attach()
{
    _session.get_device()->device_updated();
    //_session.stop_hot_plug_proc();

    _session.set_repeating(false);
    _session.stop_capture();
    _sampling_bar->set_sampling(false);
    _session.capture_state_changed(SigSession::Stopped);

    struct sr_dev_driver **const drivers = sr_driver_list();
    struct sr_dev_driver **driver;
    for (driver = drivers; *driver; driver++)
        if (*driver)
            _device_manager.driver_scan(*driver);

    _session.set_default_device(boost::bind(&MainWindow::session_error, this,
                                            QString(tr("Set Default Device failed")), _1));
    update_device_list();

}

void MainWindow::device_detach()
{
    _session.get_device()->device_updated();
    //_session.stop_hot_plug_proc();

    _session.set_repeating(false);
    _session.stop_capture();
    _sampling_bar->set_sampling(false);
    _session.capture_state_changed(SigSession::Stopped);

    session_save();
    _view->hide_calibration();
    if (_session.get_device()->dev_inst()->mode != DSO &&
        strncmp(_session.get_device()->name().toUtf8(), "virtual", 7)) {
        const boost::shared_ptr<data::Snapshot> logic_snapshot(_session.get_snapshot(SR_CHANNEL_LOGIC));
        assert(logic_snapshot);
        const boost::shared_ptr<data::Snapshot> analog_snapshot(_session.get_snapshot(SR_CHANNEL_ANALOG));
        assert(analog_snapshot);

        if (!logic_snapshot->empty() || !analog_snapshot->empty()) {
            dialogs::DSMessageBox msg(this);
            _msg = &msg;
            msg.mBox()->setText(tr("Hardware Detached"));
            msg.mBox()->setInformativeText(tr("Save captured data?"));
            msg.mBox()->addButton(tr("Ok"), QMessageBox::AcceptRole);
            msg.mBox()->addButton(tr("Cancel"), QMessageBox::RejectRole);
            msg.mBox()->setIcon(QMessageBox::Warning);
            if (msg.exec())
                on_save();
            _msg = NULL;
        }
    }

    _hot_detach = true;
    if (!_session.get_saving())
        device_detach_post();
}

void MainWindow::device_detach_post()
{
    if (!_hot_detach)
        return;

    _hot_detach = false;
    struct sr_dev_driver **const drivers = sr_driver_list();
    struct sr_dev_driver **driver;
    for (driver = drivers; *driver; driver++)
        if (*driver)
            _device_manager.driver_scan(*driver);

    _session.set_default_device(boost::bind(&MainWindow::session_error, this,
                                            QString(tr("Set Default Device failed")), _1));
    update_device_list();
}

void MainWindow::device_changed(bool close)
{
    if (close) {
        _sampling_bar->set_sampling(false);
        _session.set_default_device(boost::bind(&MainWindow::session_error, this,
                                                QString(tr("Set Default Device failed")), _1));
    }
    update_device_list();
}

void MainWindow::run_stop()
{
    switch(_session.get_capture_state()) {
    case SigSession::Init:
    case SigSession::Stopped:
        commit_trigger(false);
        _session.start_capture(false,
            boost::bind(&MainWindow::session_error, this,
                QString(tr("Capture failed")), _1));
        _view->capture_init();
        break;

    case SigSession::Running:
        _session.stop_capture();
        break;
    }
}

void MainWindow::instant_stop()
{
    switch(_session.get_capture_state()) {
    case SigSession::Init:
    case SigSession::Stopped:
        commit_trigger(true);
        _session.start_capture(true,
            boost::bind(&MainWindow::session_error, this,
                QString(tr("Capture failed")), _1));
        _view->capture_init();
        break;

    case SigSession::Running:
        _session.stop_capture();
        break;
    }
}

void MainWindow::repeat_resume()
{
    while(_view->session().get_capture_state() == SigSession::Running)
        QCoreApplication::processEvents();
    run_stop();
}

void MainWindow::show_error()
{
    QString title;
    QString details;
    QString ch_status = "";
    uint64_t error_pattern;

    switch(_session.get_error()) {
    case SigSession::Hw_err:
        _session.set_repeating(false);
        _session.stop_capture();
        title = tr("Hardware Operation Failed");
        details = tr("Please replug device to refresh hardware configuration!");
        break;
    case SigSession::Malloc_err:
        _session.set_repeating(false);
        _session.stop_capture();
        title = tr("Malloc Error");
        details = tr("Memory is not enough for this sample!\nPlease reduce the sample depth!");
        break;
    case SigSession::Test_data_err:
        _session.set_repeating(false);
        _session.stop_capture();
        _sampling_bar->set_sampling(false);
        _session.capture_state_changed(SigSession::Stopped);
        title = tr("Data Error");
        error_pattern = _session.get_error_pattern();
        for(int i = 0; i < 16; i++) {
            if (error_pattern & 0x01)
                ch_status += "X ";
            else
                ch_status += "  ";
            ch_status += (i > 9 ? " " : "");
            error_pattern >>= 1;
        }
        details = tr("the received data are not consist with pre-defined test data!") + "\n" +
                  tr("0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15")+ "\n" + ch_status;
        break;
    case SigSession::Pkt_data_err:
        title = tr("Packet Error");
        details = tr("the content of received packet are not expected!");
        _session.refresh(0);
        break;
    case SigSession::Data_overflow:
        _session.set_repeating(false);
        _session.stop_capture();
        title = tr("Data Overflow");
        details = tr("USB bandwidth can not support current sample rate! \nPlease reduce the sample rate!");
        break;
    default:
        title = tr("Undefined Error");
        details = tr("Not expected error!");
        break;
    }

    dialogs::DSMessageBox msg(this);
    connect(_session.get_device().get(), SIGNAL(device_updated()), &msg, SLOT(accept()));
    QFont font("Monaco");
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    msg.mBox()->setFont(font);

    msg.mBox()->setText(title);
    msg.mBox()->setInformativeText(details);
    msg.mBox()->setStandardButtons(QMessageBox::Ok);
    msg.mBox()->setIcon(QMessageBox::Warning);
    msg.exec();

    _session.clear_error();
}

void MainWindow::capture_state_changed(int state)
{
    if (!_session.repeat_check()) {
        _file_bar->enable_toggle(state != SigSession::Running);
        _sampling_bar->set_sampling(state == SigSession::Running);
        _view->on_state_changed(state != SigSession::Running);

        if (_session.get_device()->dev_inst()->mode != DSO ||
            _session.get_instant()) {
            _sampling_bar->enable_toggle(state != SigSession::Running);
            _trig_bar->enable_toggle(state != SigSession::Running);
            //_measure_dock->widget()->setEnabled(state != SigSession::Running);
            _measure_widget->refresh();
        }
    }

    if (state == SigSession::Stopped) {
        prgRate(0);
        _view->repeat_unshow();
    }
}

void MainWindow::session_save()
{
    QDir dir;
    #if QT_VERSION >= 0x050400
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    #else
    QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    #endif
    if(dir.mkpath(path)) {
        dir.cd(path);
        QString driver_name = _session.get_device()->name();
        QString mode_name = QString::number(_session.get_device()->dev_inst()->mode);
        QString lang_name = ".ses" + QString::number(_language);
        QString file_name = dir.absolutePath() + "/" +
                            driver_name + mode_name +
                            lang_name + ".dsc";
        if (strncmp(driver_name.toUtf8(), "virtual", 7) &&
            !file_name.isEmpty()) {
            store_session(file_name);
        }
    }

    QSettings settings(QApplication::organizationName(), QApplication::applicationName());
    settings.setValue("windowState", saveState());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // not used, refer to closeEvent of mainFrame
    session_save();
    event->accept();
}

void MainWindow::on_protocol(bool visible)
{
#ifdef ENABLE_DECODE
    _protocol_dock->setVisible(visible);
#endif
}

void MainWindow::on_trigger(bool visible)
{
    if (_session.get_device()->dev_inst()->mode != DSO) {
        _trigger_widget->init();
        _trigger_dock->setVisible(visible);
        _dso_trigger_dock->setVisible(false);
    } else {
        _dso_trigger_widget->init();
        _trigger_dock->setVisible(false);
        _dso_trigger_dock->setVisible(visible);
    }
    _trig_bar->update_trig_btn(visible);
}

void MainWindow::commit_trigger(bool instant)
{
    int i = 0;
    const QString TRIG_KEY("WarnofMultiTrig");
    QSettings settings(QApplication::organizationName(), QApplication::applicationName());

    ds_trigger_init();

    if (_session.get_device()->dev_inst()->mode != LOGIC ||
        instant)
        return;

    if (!_trigger_widget->commit_trigger()) {
        /* simple trigger check trigger_enable */
        BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _session.get_signals())
        {
            assert(s);
            boost::shared_ptr<view::LogicSignal> logicSig;
            if ((logicSig = dynamic_pointer_cast<view::LogicSignal>(s))) {
                if (logicSig->commit_trig())
                    i++;
            }
        }
        if (!settings.contains(TRIG_KEY) &&
            i > 1) {
            dialogs::DSMessageBox msg(this);
            msg.mBox()->setText(tr("Trigger"));
            msg.mBox()->setInformativeText(tr("Trigger setted on multiple channels! "
                                              "Capture will Only triggered when all setted channels fullfill at one sample"));
            msg.mBox()->setIcon(QMessageBox::Information);

            QPushButton *noMoreButton = msg.mBox()->addButton(tr("Not Show Again"), QMessageBox::ActionRole);
            QPushButton *cancelButton = msg.mBox()->addButton(tr("Clear Trig"), QMessageBox::ActionRole);
            msg.mBox()->addButton(tr("Continue"), QMessageBox::ActionRole);

            msg.exec();
            if (msg.mBox()->clickedButton() == cancelButton) {
                BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _session.get_signals())
                {
                    assert(s);
                    boost::shared_ptr<view::LogicSignal> logicSig;
                    if ((logicSig = dynamic_pointer_cast<view::LogicSignal>(s))) {
                        logicSig->set_trig(view::LogicSignal::NONTRIG);
                        logicSig->commit_trig();
                    }
                }
            }
            if (msg.mBox()->clickedButton() == noMoreButton)
                  settings.setValue(TRIG_KEY, false);
        }
    }
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
    const QString DIR_KEY("ScreenShotPath");
    QSettings settings(QApplication::organizationName(), QApplication::applicationName());
    QString default_name = settings.value(DIR_KEY).toString() + "/DSView" + QDateTime::currentDateTime().toString("-yyMMdd-hhmmss");
    QPixmap pixmap;
    QScreen *screen = QGuiApplication::primaryScreen();
    QDesktopWidget *desktop = QApplication::desktop();
    pixmap = screen->grabWindow(desktop->winId(), parentWidget()->pos().x(), parentWidget()->pos().y(),
                                                 parentWidget()->frameGeometry().width(), parentWidget()->frameGeometry().height());
    QString format = "png";

    QString fileName = QFileDialog::getSaveFileName(this,
                       tr("Save As"), default_name,
                       tr("%1 Files (*.%2);;All Files (*)")
                       .arg(format.toUpper()).arg(format));
    if (!fileName.isEmpty()) {
        QDir CurrentDir;
        settings.setValue(DIR_KEY, CurrentDir.filePath(fileName));
        pixmap.save(fileName, format.toLatin1());
    }
}

void MainWindow::on_save()
{
    using pv::dialogs::StoreProgress;

//    dialogs::RegionOptions *regionDlg = new dialogs::RegionOptions(_view, _session, this);
//    regionDlg->exec();

    _session.set_saving(true);
    QString session_file;
    QDir dir;
    #if QT_VERSION >= 0x050400
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    #else
    QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    #endif
    if(dir.mkpath(path)) {
        dir.cd(path);

        session_file = dir.absolutePath() + "/DSView-session-XXXXXX";
        store_session(session_file);
    }

    StoreProgress *dlg = new StoreProgress(_session, this);
    connect(dlg, SIGNAL(save_done()), this, SLOT(device_detach_post()));
    dlg->save_run(session_file);
}

void MainWindow::on_export()
{
    using pv::dialogs::StoreProgress;

//    dialogs::RegionOptions *regionDlg = new dialogs::RegionOptions(_view, _session, this);
//    regionDlg->exec();

    StoreProgress *dlg = new StoreProgress(_session, this);
    dlg->export_run();
}

bool MainWindow::load_session(QString name)
{
    QFile sessionFile(name);
    if (!sessionFile.open(QIODevice::ReadOnly)) {
        qDebug("Warning: Couldn't open session file!");
        return false;
    }

    QString sessionData = QString::fromUtf8(sessionFile.readAll());
    QJsonDocument sessionDoc = QJsonDocument::fromJson(sessionData.toUtf8());

    return load_session_json(sessionDoc, false);
}

bool MainWindow::load_session_json(QJsonDocument json, bool file_dev)
{
    QJsonObject sessionObj = json.object();

    // check session file version
    if (!sessionObj.contains("Version") ||
        sessionObj["Version"].toInt() != Session_Version)
        return false;

    // check device and mode
    const sr_dev_inst *const sdi = _session.get_device()->dev_inst();
    if ((!file_dev && strcmp(sdi->driver->name, sessionObj["Device"].toString().toUtf8()) != 0) ||
        sdi->mode != sessionObj["DeviceMode"].toDouble()) {
        dialogs::DSMessageBox msg(this);
        msg.mBox()->setText(tr("Session Error"));
        msg.mBox()->setInformativeText(tr("Session File is not compatible with current device or mode!"));
        msg.mBox()->setStandardButtons(QMessageBox::Ok);
        msg.mBox()->setIcon(QMessageBox::Warning);
        msg.exec();
        return false;
    }

    // check language
    if (sessionObj.contains("Language")) {
        switchLanguage(sessionObj["Language"].toInt());
    } else if (sessionObj.contains("Operation Mode")) {
        bool language_matched = _session.get_device()->set_config(NULL, NULL, SR_CONF_OPERATION_MODE,
                                          g_variant_new_string(sessionObj["Operation Mode"].toString().toUtf8()));
        if (!language_matched) {
            if (_language != QLocale::Chinese)
                switchLanguage(QLocale::Chinese);
            else
                switchLanguage(QLocale::English);
        }
    }

    // clear decoders
    #ifdef ENABLE_DECODE
    if (sdi->mode == LOGIC) {
        _protocol_widget->del_all_protocol();
    }
    #endif

    // load device settings
    GVariant *gvar_opts;
    gsize num_opts;
    if ((sr_config_list(sdi->driver, sdi, NULL, SR_CONF_DEVICE_SESSIONS, &gvar_opts) == SR_OK)) {
        const int *const options = (const int32_t *)g_variant_get_fixed_array(
            gvar_opts, &num_opts, sizeof(int32_t));
        for (unsigned int i = 0; i < num_opts; i++) {
            const struct sr_config_info *const info =
                sr_config_info_get(options[i]);
            if (!sessionObj.contains(info->name))
                continue;
            if (info->datatype == SR_T_BOOL)
                _session.get_device()->set_config(NULL, NULL, info->key, g_variant_new_boolean(sessionObj[info->name].toDouble()));
            else if (info->datatype == SR_T_UINT64)
                _session.get_device()->set_config(NULL, NULL, info->key, g_variant_new_uint64(sessionObj[info->name].toString().toULongLong()));
            else if (info->datatype == SR_T_UINT8)
                _session.get_device()->set_config(NULL, NULL, info->key, g_variant_new_byte(sessionObj[info->name].toString().toUInt()));
            else if (info->datatype == SR_T_FLOAT)
                _session.get_device()->set_config(NULL, NULL, info->key, g_variant_new_double(sessionObj[info->name].toDouble()));
            else if (info->datatype == SR_T_CHAR)
                _session.get_device()->set_config(NULL, NULL, info->key, g_variant_new_string(sessionObj[info->name].toString().toUtf8()));
        }
    }

    // load channel settings
    if (file_dev && (sdi->mode == DSO)) {
        for (const GSList *l = _session.get_device()->dev_inst()->channels; l; l = l->next) {
            sr_channel *const probe = (sr_channel*)l->data;
            assert(probe);
            foreach (const QJsonValue &value, sessionObj["channel"].toArray()) {
                QJsonObject obj = value.toObject();
                if ((strcmp(probe->name, g_strdup(obj["name"].toString().toStdString().c_str())) == 0) &&
                    (probe->type == obj["type"].toDouble())) {
                    probe->vdiv = obj["vdiv"].toDouble();
                    probe->coupling = obj["coupling"].toDouble();
                    probe->vfactor = obj["vfactor"].toDouble();
                    probe->trig_value = obj["trigValue"].toDouble();
                    probe->map_unit = g_strdup(obj["mapUnit"].toString().toStdString().c_str());
                    probe->map_min = obj["mapMin"].toDouble();
                    probe->map_max = obj["mapMax"].toDouble();
                    break;
                }
            }
        }
    } else {
        for (const GSList *l = _session.get_device()->dev_inst()->channels; l; l = l->next) {
            sr_channel *const probe = (sr_channel*)l->data;
            assert(probe);
            bool isEnabled = false;
            foreach (const QJsonValue &value, sessionObj["channel"].toArray()) {
                QJsonObject obj = value.toObject();
                if ((probe->index == obj["index"].toDouble()) &&
                    (probe->type == obj["type"].toDouble())) {
                    isEnabled = true;
                    probe->enabled = obj["enabled"].toBool();
                    probe->name = g_strdup(obj["name"].toString().toStdString().c_str());
                    probe->vdiv = obj["vdiv"].toDouble();
                    probe->coupling = obj["coupling"].toDouble();
                    probe->vfactor = obj["vfactor"].toDouble();
                    probe->trig_value = obj["trigValue"].toDouble();
                    probe->map_unit = g_strdup(obj["mapUnit"].toString().toStdString().c_str());
                    probe->map_min = obj["mapMin"].toDouble();
                    probe->map_max = obj["mapMax"].toDouble();
                    break;
                }
            }
            if (!isEnabled)
                probe->enabled = false;
        }
    }

    //_session.init_signals();
    _session.reload();

    // load signal setting
    if (file_dev && (sdi->mode == DSO)) {
        BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _session.get_signals()) {
            foreach (const QJsonValue &value, sessionObj["channel"].toArray()) {
                QJsonObject obj = value.toObject();
                if ((strcmp(s->get_name().toStdString().c_str(), g_strdup(obj["name"].toString().toStdString().c_str())) == 0) &&
                    (s->get_type() == obj["type"].toDouble())) {
                    s->set_colour(QColor(obj["colour"].toString()));

                    boost::shared_ptr<view::DsoSignal> dsoSig;
                    if ((dsoSig = dynamic_pointer_cast<view::DsoSignal>(s))) {
                        dsoSig->load_settings();
                        dsoSig->set_zero_ratio(obj["zeroPos"].toDouble());
                        dsoSig->set_trig_ratio(obj["trigValue"].toDouble());
                        dsoSig->commit_settings();
                    }
                    break;
                }
            }
        }
    } else {
        BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _session.get_signals()) {
            foreach (const QJsonValue &value, sessionObj["channel"].toArray()) {
                QJsonObject obj = value.toObject();
                if ((s->get_index() == obj["index"].toDouble()) &&
                    (s->get_type() == obj["type"].toDouble())) {
                    s->set_colour(QColor(obj["colour"].toString()));
                    s->set_name(g_strdup(obj["name"].toString().toUtf8().data()));

                    boost::shared_ptr<view::LogicSignal> logicSig;
                    if ((logicSig = dynamic_pointer_cast<view::LogicSignal>(s))) {
                        logicSig->set_trig(obj["strigger"].toDouble());
                    }

                    boost::shared_ptr<view::DsoSignal> dsoSig;
                    if ((dsoSig = dynamic_pointer_cast<view::DsoSignal>(s))) {
                        dsoSig->load_settings();
                        dsoSig->set_zero_ratio(obj["zeroPos"].toDouble());
                        dsoSig->set_trig_ratio(obj["trigValue"].toDouble());
                        dsoSig->commit_settings();
                    }

                    boost::shared_ptr<view::AnalogSignal> analogSig;
                    if ((analogSig = dynamic_pointer_cast<view::AnalogSignal>(s))) {
                        analogSig->set_zero_ratio(obj["zeroPos"].toDouble());
                        analogSig->commit_settings();
                    }

                    break;
                }
            }
        }
    }

    // update UI settings
    _sampling_bar->update_sample_rate_selector();
    _trigger_widget->device_updated();
    _view->header_updated();

    // load trigger settings
    if (sessionObj.contains("trigger")) {
        _trigger_widget->set_session(sessionObj["trigger"].toObject());
    }
    on_trigger(false);

    #ifdef ENABLE_DECODE
    // load decoders
    if (sessionObj.contains("decoder")) {
        StoreSession ss(_session);
        ss.load_decoders(_protocol_widget, sessionObj["decoder"].toArray());
    }
    #endif

    // load measure
    if (sessionObj.contains("measure")) {
        _view->get_viewstatus()->load_session(sessionObj["measure"].toArray());
    }

    return true;
}


bool MainWindow::store_session(QString name)
{
    QFile sessionFile(name);
    if (!sessionFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
//        dialogs::DSMessageBox msg(this);
//        msg.mBox()->setText(tr("File Error"));
//        msg.mBox()->setInformativeText(tr("Couldn't open session file to write!"));
//        msg.mBox()->setStandardButtons(QMessageBox::Ok);
//        msg.mBox()->setIcon(QMessageBox::Warning);
//        msg.exec();
        qDebug("Warning: Couldn't open session file to write!");
        return false;
    }
    QTextStream outStream(&sessionFile);
    outStream.setCodec("UTF-8");
    //outStream.setGenerateByteOrderMark(true); // UTF-8 without BOM

    GVariant *gvar_opts;
    GVariant *gvar;
    gsize num_opts;
    const sr_dev_inst *const sdi = _session.get_device()->dev_inst();
    QJsonObject sessionVar;
    QJsonArray channelVar;
    sessionVar["Version"]= QJsonValue::fromVariant(Session_Version);
    sessionVar["Device"] = QJsonValue::fromVariant(sdi->driver->name);
    sessionVar["DeviceMode"] = QJsonValue::fromVariant(sdi->mode);
    sessionVar["Language"] = QJsonValue::fromVariant(_language);

    if ((sr_config_list(sdi->driver, sdi, NULL, SR_CONF_DEVICE_SESSIONS, &gvar_opts) != SR_OK))
        return false;   /* Driver supports no device instance sessions. */
    const int *const options = (const int32_t *)g_variant_get_fixed_array(
        gvar_opts, &num_opts, sizeof(int32_t));
    for (unsigned int i = 0; i < num_opts; i++) {
        const struct sr_config_info *const info =
            sr_config_info_get(options[i]);
        gvar = _session.get_device()->get_config(NULL, NULL, info->key);
        if (gvar != NULL) {
            if (info->datatype == SR_T_BOOL)
                sessionVar[info->name] = QJsonValue::fromVariant(g_variant_get_boolean(gvar));
            else if (info->datatype == SR_T_UINT64)
                sessionVar[info->name] = QJsonValue::fromVariant(QString::number(g_variant_get_uint64(gvar)));
            else if (info->datatype == SR_T_UINT8)
                sessionVar[info->name] = QJsonValue::fromVariant(QString::number(g_variant_get_byte(gvar)));
            else if (info->datatype == SR_T_FLOAT)
                sessionVar[info->name] = QJsonValue::fromVariant(g_variant_get_double(gvar));
            else if (info->datatype == SR_T_CHAR)
                sessionVar[info->name] = QJsonValue::fromVariant(g_variant_get_string(gvar, NULL));
            g_variant_unref(gvar);
        }
    }

    BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _session.get_signals()) {
        QJsonObject s_obj;
        s_obj["index"] = s->get_index();
        s_obj["type"] = s->get_type();
        s_obj["enabled"] = s->enabled();
        s_obj["name"] = s->get_name();
        if (s->get_colour().isValid())
            s_obj["colour"] = QJsonValue::fromVariant(s->get_colour());
        else
            s_obj["colour"] = QJsonValue::fromVariant("default");

        boost::shared_ptr<view::LogicSignal> logicSig;
        if ((logicSig = dynamic_pointer_cast<view::LogicSignal>(s))) {
            s_obj["strigger"] = logicSig->get_trig();
        }

        boost::shared_ptr<view::DsoSignal> dsoSig;
        if ((dsoSig = dynamic_pointer_cast<view::DsoSignal>(s))) {
            s_obj["vdiv"] = QJsonValue::fromVariant(static_cast<qulonglong>(dsoSig->get_vDialValue()));
            s_obj["vfactor"] = QJsonValue::fromVariant(static_cast<qulonglong>(dsoSig->get_factor()));
            s_obj["coupling"] = dsoSig->get_acCoupling();
            s_obj["trigValue"] = dsoSig->get_trig_vrate();
            s_obj["zeroPos"] = dsoSig->get_zero_ratio();
        }

        boost::shared_ptr<view::AnalogSignal> analogSig;
        if ((analogSig = dynamic_pointer_cast<view::AnalogSignal>(s))) {
            s_obj["vdiv"] = QJsonValue::fromVariant(static_cast<qulonglong>(analogSig->get_vdiv()));
            s_obj["vfactor"] = QJsonValue::fromVariant(static_cast<qulonglong>(analogSig->get_factor()));
            s_obj["coupling"] = analogSig->get_acCoupling();
            s_obj["zeroPos"] = analogSig->get_zero_ratio();
            s_obj["mapUnit"] = analogSig->get_mapUnit();
            s_obj["mapMin"] = analogSig->get_mapMin();
            s_obj["mapMax"] = analogSig->get_mapMax();
        }
        channelVar.append(s_obj);
    }
    sessionVar["channel"] = channelVar;

    if (_session.get_device()->dev_inst()->mode == LOGIC) {
        sessionVar["trigger"] = _trigger_widget->get_session();
    }

    #ifdef ENABLE_DECODE
    StoreSession ss(_session);
    sessionVar["decoder"] = ss.json_decoders();
    #endif

    if (_session.get_device()->dev_inst()->mode == DSO) {
        sessionVar["measure"] = _view->get_viewstatus()->get_session();
    }

    QJsonDocument sessionDoc(sessionVar);
    //sessionFile.write(QString::fromUtf8(sessionDoc.toJson()));
    outStream << QString::fromUtf8(sessionDoc.toJson());
    sessionFile.close();
    return true;
}

void MainWindow::restore_dock()
{
    // default dockwidget size
    QSettings settings(QApplication::organizationName(), QApplication::applicationName());
    restoreState(settings.value("windowState").toByteArray());
    if (_session.get_device()->dev_inst()->mode != DSO) {
        _dso_trigger_dock->setVisible(false);
        _trig_bar->update_trig_btn(_trigger_dock->isVisible());
    } else {
        _trigger_dock->setVisible(false);
        _trig_bar->update_trig_btn(_dso_trigger_dock->isVisible());
    }
    if (_session.get_device()->dev_inst()->mode != LOGIC) {
#ifdef ENABLE_DECODE
        on_protocol(false);
#endif
    }
    _trig_bar->update_protocol_btn(_protocol_dock->isVisible());
    _trig_bar->update_measure_btn(_measure_dock->isVisible());
    _trig_bar->update_search_btn(_search_dock->isVisible());
}

bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
    (void) object;

    if ( event->type() == QEvent::KeyPress ) {
        const vector< shared_ptr<view::Signal> > sigs(_session.get_signals());
        QKeyEvent *ke = (QKeyEvent *) event;
        switch(ke->key()) {
        case Qt::Key_S:
            run_stop();
            break;
        case Qt::Key_I:
            instant_stop();
            break;
        case Qt::Key_T:
            if (_session.get_device()->dev_inst()->mode == DSO)
                on_trigger(!_dso_trigger_dock->isVisible());
            else
                on_trigger(!_trigger_dock->isVisible());
            break;
#ifdef ENABLE_DECODE
        case Qt::Key_D:
            on_protocol(!_protocol_dock->isVisible());
            break;
#endif
        case Qt::Key_M:
            on_measure(!_measure_dock->isVisible());
            break;
        case Qt::Key_R:
            on_search(!_search_dock->isVisible());
            break;
        case Qt::Key_O:
            _sampling_bar->on_configure();
            break;
        case Qt::Key_PageUp:
            _view->set_scale_offset(_view->scale(),
                                    _view->offset() - _view->get_view_width());
            break;
        case Qt::Key_PageDown:
            _view->set_scale_offset(_view->scale(),
                                    _view->offset() + _view->get_view_width());

            break;
        case Qt::Key_Left:
            _view->zoom(1);
            break;
        case Qt::Key_Right:
            _view->zoom(-1);
            break;
        case Qt::Key_0:
            BOOST_FOREACH(const shared_ptr<view::Signal> s, sigs) {
                shared_ptr<view::DsoSignal> dsoSig;
                if ((dsoSig = dynamic_pointer_cast<view::DsoSignal>(s))) {
                    if (dsoSig->get_index() == 0)
                        dsoSig->set_vDialActive(!dsoSig->get_vDialActive());
                    else
                        dsoSig->set_vDialActive(false);
                }
            }
            _view->setFocus();
            update();
            break;
        case Qt::Key_1:
            BOOST_FOREACH(const shared_ptr<view::Signal> s, sigs) {
                shared_ptr<view::DsoSignal> dsoSig;
                if ((dsoSig = dynamic_pointer_cast<view::DsoSignal>(s))) {
                    if (dsoSig->get_index() == 1)
                        dsoSig->set_vDialActive(!dsoSig->get_vDialActive());
                    else
                        dsoSig->set_vDialActive(false);
                }
            }
            _view->setFocus();
            update();
            break;
        case Qt::Key_Up:
            BOOST_FOREACH(const shared_ptr<view::Signal> s, sigs) {
                shared_ptr<view::DsoSignal> dsoSig;
                if ((dsoSig = dynamic_pointer_cast<view::DsoSignal>(s))) {
                    if (dsoSig->get_vDialActive()) {
                        dsoSig->go_vDialNext(true);
                        update();
                        break;
                    }
                }
            }
            break;
        case Qt::Key_Down:
            BOOST_FOREACH(const shared_ptr<view::Signal> s, sigs) {
                shared_ptr<view::DsoSignal> dsoSig;
                if ((dsoSig = dynamic_pointer_cast<view::DsoSignal>(s))) {
                    if (dsoSig->get_vDialActive()) {
                        dsoSig->go_vDialPre(true);
                        update();
                        break;
                    }
                }
            }
            break;
        default:
            QWidget::keyPressEvent((QKeyEvent *)event);
        }
        return true;
    }
    return false;
}

int MainWindow::language() const
{
    return _language;
}

void MainWindow::switchLanguage(int language)
{
    shared_ptr<pv::device::DevInst> dev = _session.get_device();
    dev->set_config(NULL, NULL, SR_CONF_LANGUAGE, g_variant_new_int16(language));

    if (_language != language) {
        _language = language;
        if (_language != QLocale::English) {
            _qtTrans.load(":/qt_"+QString::number(_language));
            qApp->installTranslator(&_qtTrans);
            _myTrans.load(":/my_"+QString::number(_language));
            qApp->installTranslator(&_myTrans);
            retranslateUi();
        } else {
            qApp->removeTranslator(&_qtTrans);
            qApp->removeTranslator(&_myTrans);
            retranslateUi();
        }
        qApp->setProperty("Language", _language);
    }
}

void MainWindow::switchTheme(QString style)
{
    if (_style != style) {
        _style = style;
        qApp->setProperty("Style", _style);
        QString qssRes = ":/"+_style+".qss";
        QFile qss(qssRes);
        qss.open(QFile::ReadOnly | QFile::Text);
        qApp->setStyleSheet(qss.readAll());
        qss.close();
        _session.data_updated();
    }
}

void MainWindow::openDoc()
{
    #ifndef Q_OS_LINUX
    QDir dir(QCoreApplication::applicationDirPath());
    #else
    QDir dir(DS_RES_PATH);
    dir.cdUp();
    #endif
    QDesktopServices::openUrl(
                QUrl("file:///"+dir.absolutePath() + "/ug"+QString::number(_language)+".pdf"));
}

} // namespace pv
