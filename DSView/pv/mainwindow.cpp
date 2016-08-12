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
#include <libsigrokdecode/libsigrokdecode.h>
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
#include <QKeyEvent>
#include <QEvent>
#include <QtGlobal>

#include "mainwindow.h"

#include "devicemanager.h"
#include "device/device.h"
#include "device/file.h"

#include "dialogs/about.h"
#include "dialogs/deviceoptions.h"
#include "dialogs/storeprogress.h"
#include "dialogs/waitingdialog.h"
#include "dialogs/dsmessagebox.h"

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

/* __STDC_FORMAT_MACROS is required for PRIu64 and friends (in C++). */
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdint.h>
#include <stdarg.h>
#include <glib.h>
#include <list>
#include <libsigrok4DSL/libsigrok.h>

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
    _session(device_manager)
{
	setup_ui();
	if (open_file_name) {
		const QString s(QString::fromUtf8(open_file_name));
		QMetaObject::invokeMethod(this, "load_file",
			Qt::QueuedConnection,
			Q_ARG(QString, s));
	}
    test_timer_linked = false;
    test_timer.stop();
    test_timer.setSingleShot(true);
}

void MainWindow::setup_ui()
{
	setObjectName(QString::fromUtf8("MainWindow"));
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
    _trig_bar = new toolbars::TrigBar(_session, this);
    _file_bar = new toolbars::FileBar(_session, this);
    _logo_bar = new toolbars::LogoBar(_session, this);

    connect(_trig_bar, SIGNAL(on_protocol(bool)), this,
            SLOT(on_protocol(bool)));
    connect(_trig_bar, SIGNAL(on_trigger(bool)), this,
            SLOT(on_trigger(bool)));
    connect(_trig_bar, SIGNAL(on_measure(bool)), this,
            SLOT(on_measure(bool)));
    connect(_trig_bar, SIGNAL(on_search(bool)), this,
            SLOT(on_search(bool)));
    connect(_file_bar, SIGNAL(load_file(QString)), this,
            SLOT(load_file(QString)));
    connect(_file_bar, SIGNAL(save()), this,
            SLOT(on_save()));
    connect(_file_bar, SIGNAL(on_screenShot()), this,
            SLOT(on_screenShot()), Qt::QueuedConnection);
    connect(_file_bar, SIGNAL(load_session(QString)), this,
            SLOT(load_session(QString)));
    connect(_file_bar, SIGNAL(store_session(QString)), this,
            SLOT(store_session(QString)));

#ifdef ENABLE_DECODE
    // protocol dock
    _protocol_dock=new QDockWidget(tr("Protocol"),this);
    _protocol_dock->setFeatures(QDockWidget::DockWidgetMovable);
    _protocol_dock->setAllowedAreas(Qt::RightDockWidgetArea);
    _protocol_dock->setVisible(false);
    //dock::ProtocolDock *_protocol_widget = new dock::ProtocolDock(_protocol_dock, _session);
    _protocol_widget = new dock::ProtocolDock(_protocol_dock, _session);
    _protocol_dock->setWidget(_protocol_widget);
    qDebug() << "Protocol decoder enabled!\n";
#endif
    // trigger dock
    _trigger_dock=new QDockWidget(tr("Trigger Setting..."),this);
    _trigger_dock->setFeatures(QDockWidget::DockWidgetMovable);
    _trigger_dock->setAllowedAreas(Qt::RightDockWidgetArea);
    _trigger_dock->setVisible(false);
    _trigger_widget = new dock::TriggerDock(_trigger_dock, _session);
    _trigger_dock->setWidget(_trigger_widget);

    _dso_trigger_dock=new QDockWidget(tr("Trigger Setting..."),this);
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
    connect(_sampling_bar, SIGNAL(sample_count_changed()), _trigger_widget,
        SLOT(device_change()));
    connect(_sampling_bar, SIGNAL(show_calibration()), _view,
        SLOT(show_calibration()));
    connect(_sampling_bar, SIGNAL(hide_calibration()), _view,
        SLOT(hide_calibration()));
    connect(_dso_trigger_widget, SIGNAL(set_trig_pos(int)), _view,
        SLOT(set_trig_pos(int)));
    connect(_protocol_widget, SIGNAL(protocol_updated()), _view, SLOT(signals_changed()));

    setIconSize(QSize(40,40));
    addToolBar(_sampling_bar);
    addToolBar(_trig_bar);
    addToolBar(_file_bar);
    addToolBar(_logo_bar);

    // Setup the dockWidget
    // measure dock
    _measure_dock=new QDockWidget(tr("Measurement"),this);
    _measure_dock->setFeatures(QDockWidget::DockWidgetMovable);
    _measure_dock->setAllowedAreas(Qt::RightDockWidgetArea);
    _measure_dock->setVisible(false);
    _measure_widget = new dock::MeasureDock(_measure_dock, *_view, _session);
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
    connect(&_session, SIGNAL(test_data_error()), this,
            SLOT(test_data_error()));
    connect(&_session, SIGNAL(malloc_error()), this,
            SLOT(malloc_error()));
    connect(&_session, SIGNAL(hardware_connect_failed()), this,
            SLOT(hardware_connect_failed()));
    connect(&_session, SIGNAL(on_mode_change()), this,
            SLOT(session_save()));

    connect(_view, SIGNAL(cursor_update()), _measure_widget,
            SLOT(cursor_update()));
    connect(_view, SIGNAL(cursor_moved()), _measure_widget,
            SLOT(cursor_moved()));

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
                                            QString("Set Default Device failed"), _1));
    update_device_list();
    _session.start_hotplug_proc(boost::bind(&MainWindow::session_error, this,
                                             QString("Hotplug failed"), _1));
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

    _session.stop_capture();
    _view->reload();
    _trigger_widget->device_change();
#ifdef ENABLE_DECODE
    _protocol_widget->del_all_protocol();
#endif
    _trig_bar->reload();
    _sampling_bar->reload();

    shared_ptr<pv::device::DevInst> selected_device = _session.get_device();
    _device_manager.add_device(selected_device);
    _sampling_bar->set_device_list(_device_manager.devices(), selected_device);
    _session.init_signals();

    if(dynamic_pointer_cast<pv::device::File>(selected_device)) {
        const QString errorMessage(
            QString("Failed to capture file data!"));
        const QString infoMessage;
        _session.start_capture(true, boost::bind(&MainWindow::session_error, this,
            errorMessage, infoMessage));
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
            QString ses_name = str +
                               selected_device->name() +
                               QString::number(selected_device->dev_inst()->mode) +
                               ".dsc";
            load_session(ses_name);
        }
    } else {
        _file_bar->set_settings_en(false);
        _logo_bar->dsl_connected(false);
    }
    _view->status_clear();
    _trigger_widget->init();
    _dso_trigger_widget->init();
}

void MainWindow::reload()
{
    _trigger_widget->device_change();
    _session.reload();
}

void MainWindow::mode_changed()
{
    update_device_list();
}

void MainWindow::load_file(QString file_name)
{
    try {
        //_session.set_file(file_name.toStdString());
        _session.set_file(file_name);
    } catch(QString e) {
        show_session_error(tr("Failed to load ") + file_name, e);
        _session.set_default_device(boost::bind(&MainWindow::session_error, this,
                                                QString("Set Default Device failed"), _1));
        update_device_list();
        return;
    }

    update_device_list();
}

void MainWindow::show_session_error(
	const QString text, const QString info_text)
{
    dialogs::DSMessageBox msg(this);
    msg.mBox()->setText(text);
    msg.mBox()->setInformativeText(info_text);
    msg.mBox()->setStandardButtons(QMessageBox::Ok);
    msg.mBox()->setIcon(QMessageBox::Warning);
	msg.exec();
}

void MainWindow::device_attach()
{
    _session.get_device()->device_updated();
    //_session.stop_hot_plug_proc();

    if (_session.get_capture_state() == SigSession::Running)
        _session.stop_capture();

    struct sr_dev_driver **const drivers = sr_driver_list();
    struct sr_dev_driver **driver;
    for (driver = drivers; *driver; driver++)
        if (*driver)
            _device_manager.driver_scan(*driver);

    _session.set_default_device(boost::bind(&MainWindow::session_error, this,
                                            QString("Set Default Device failed"), _1));
    update_device_list();
}

void MainWindow::device_detach()
{
    _session.get_device()->device_updated();
    //_session.stop_hot_plug_proc();

    if (_session.get_capture_state() == SigSession::Running)
        _session.stop_capture();

    session_save();
    _view->hide_calibration();

    struct sr_dev_driver **const drivers = sr_driver_list();
    struct sr_dev_driver **driver;
    for (driver = drivers; *driver; driver++)
        if (*driver)
            _device_manager.driver_scan(*driver);

    _session.set_default_device(boost::bind(&MainWindow::session_error, this,
                                            QString("Set Default Device failed"), _1));
    update_device_list();
}

void MainWindow::run_stop()
{
#ifdef TEST_MODE
    if (!test_timer_linked) {
        connect(&test_timer, SIGNAL(timeout()),
                this, SLOT(run_stop()));
        test_timer_linked = true;
    }
#endif
    switch(_session.get_capture_state()) {
    case SigSession::Init:
	case SigSession::Stopped:
        if (_session.get_device()->dev_inst()->mode == DSO)
            _view->show_trig_cursor(true);
        else
            _view->show_trig_cursor(false);
        _view->update_sample(false);
        commit_trigger(false);
        _session.start_capture(false,
			boost::bind(&MainWindow::session_error, this,
				QString("Capture failed"), _1));
        break;

	case SigSession::Running:
		_session.stop_capture();
		break;
	}
}

void MainWindow::instant_stop()
{
#ifdef TEST_MODE
    if (!test_timer_linked) {
        connect(&test_timer, SIGNAL(timeout()),
                this, SLOT(instant_stop()));
        test_timer_linked = true;
    }
#endif
    switch(_session.get_capture_state()) {
    case SigSession::Init:
    case SigSession::Stopped:
        if (_session.get_device()->dev_inst()->mode == DSO)
            _view->show_trig_cursor(true);
        else
            _view->show_trig_cursor(false);
        _view->update_sample(true);
        commit_trigger(true);
        _session.start_capture(true,
            boost::bind(&MainWindow::session_error, this,
                QString("Capture failed"), _1));
        break;

    case SigSession::Running:
        _session.stop_capture();
        break;
    }

}

void MainWindow::test_data_error()
{

    _session.stop_capture();
    dialogs::DSMessageBox msg(this);
    msg.mBox()->setText(tr("Data Error"));
    msg.mBox()->setInformativeText(tr("the receive data are not consist with pre-defined test data"));
    msg.mBox()->setStandardButtons(QMessageBox::Ok);
    msg.mBox()->setIcon(QMessageBox::Warning);
    msg.exec();
}

void MainWindow::malloc_error()
{
    _session.stop_capture();
    dialogs::DSMessageBox msg(this);
    msg.mBox()->setText(tr("Malloc Error"));
    msg.mBox()->setInformativeText(tr("Memory is not enough for this sample!\nPlease reduce the sample depth!"));
    msg.mBox()->setStandardButtons(QMessageBox::Ok);
    msg.mBox()->setIcon(QMessageBox::Warning);
    msg.exec();
}

void MainWindow::hardware_connect_failed()
{
    _session.stop_capture();
    dialogs::DSMessageBox msg(this);
    msg.mBox()->setText(tr("Hardware Connect Failed"));
    msg.mBox()->setInformativeText(tr("Please check hardware connection!"));
    msg.mBox()->setStandardButtons(QMessageBox::Ok);
    msg.mBox()->setIcon(QMessageBox::Warning);
    msg.exec();
}

void MainWindow::capture_state_changed(int state)
{
    _file_bar->enable_toggle(state != SigSession::Running);
    _sampling_bar->set_sampling(state == SigSession::Running);
    _view->on_state_changed(state != SigSession::Running);

    if (_session.get_device()->dev_inst()->mode != DSO) {
        _sampling_bar->enable_toggle(state != SigSession::Running);
        _trig_bar->enable_toggle(state != SigSession::Running);
        _measure_dock->widget()->setEnabled(state != SigSession::Running);
    }

#ifdef TEST_MODE
    if (state == SigSession::Stopped) {
        test_timer.start(qrand()%1000);
    }
#endif
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
        QString file_name = dir.absolutePath() + "/" + driver_name + mode_name + ".dsc";
        if (strncmp(driver_name.toLocal8Bit(), "virtual", 7) &&
            !file_name.isEmpty())
            store_session(file_name);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
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
            if (logicSig = dynamic_pointer_cast<view::LogicSignal>(s))
                logicSig->commit_trig();
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
    QSettings settings;
    QPixmap pixmap;
    QDesktopWidget *desktop = QApplication::desktop();
    pixmap = QPixmap::grabWindow(desktop->winId(), parentWidget()->pos().x(), parentWidget()->pos().y(),
                                                   parentWidget()->frameGeometry().width(), parentWidget()->frameGeometry().height());
    QString format = "png";

    QString fileName = QFileDialog::getSaveFileName(this,
                       tr("Save As"),settings.value(DIR_KEY).toString(),
                       tr("%1 Files (*.%2);;All Files (*)")
                       .arg(format.toUpper()).arg(format));
    if (!fileName.isEmpty()) {
        QDir CurrentDir;
        settings.setValue(DIR_KEY, CurrentDir.absoluteFilePath(fileName));
        pixmap.save(fileName, format.toLatin1());
    }
}

void MainWindow::on_save()
{
    using pv::dialogs::StoreProgress;

    const QString DIR_KEY("SavePath");
    QSettings settings;

    // Stop any currently running capture session
    _session.stop_capture();

    // Show the dialog
    const QString file_name = QFileDialog::getSaveFileName(
        this, tr("Save File"), settings.value(DIR_KEY).toString(), tr("DSView Data (*.dsl)"));

    if (file_name.isEmpty())
        return;
    QDir CurrentDir;
    settings.setValue(DIR_KEY, CurrentDir.absoluteFilePath(file_name));
    StoreProgress *dlg = new StoreProgress(file_name, _session, this);
    dlg->run();
}

bool MainWindow::load_session(QString name)
{
    QFile sessionFile(name);
    if (!sessionFile.open(QIODevice::ReadOnly)) {
//        dialogs::DSMessageBox msg(this);
//        msg.mBox()->setText(tr("File Error"));
//        msg.mBox()->setInformativeText(tr("Couldn't open session file!"));
//        msg.mBox()->setStandardButtons(QMessageBox::Ok);
//        msg.mBox()->setIcon(QMessageBox::Warning);
//        msg.exec();
        qDebug("Warning: Couldn't open session file!");
        return false;
    }

    QString sessionData = QString::fromUtf8(sessionFile.readAll());
    QJsonDocument sessionDoc = QJsonDocument::fromJson(sessionData.toUtf8());
    QJsonObject sessionObj = sessionDoc.object();

    // check device and mode
    const sr_dev_inst *const sdi = _session.get_device()->dev_inst();
    if (strcmp(sdi->driver->name, sessionObj["Device"].toString().toLocal8Bit()) != 0 ||
        sdi->mode != sessionObj["DeviceMode"].toDouble()) {
        dialogs::DSMessageBox msg(this);
        msg.mBox()->setText(tr("Session Error"));
        msg.mBox()->setInformativeText(tr("Session File is not compatible with current device or mode!"));
        msg.mBox()->setStandardButtons(QMessageBox::Ok);
        msg.mBox()->setIcon(QMessageBox::Warning);
        msg.exec();
        return false;
    }

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
    _sampling_bar->update_record_length();
    _sampling_bar->update_sample_rate();

    // load channel settings
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
                //probe->colour = obj["colour"].toString();
                probe->name = g_strdup(obj["name"].toString().toStdString().c_str());
                probe->vdiv = obj["vdiv"].toDouble();
                probe->coupling = obj["coupling"].toDouble();
                probe->vfactor = obj["vfactor"].toDouble();
                probe->trig_value = obj["trigValue"].toDouble();
                //probe->zeroPos = obj["zeroPos"].toDouble();
                break;
            }
        }
        if (!isEnabled)
            probe->enabled = false;
    }
    //_session.init_signals();
    _session.reload();

    // load signal setting
    BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _session.get_signals()) {
        foreach (const QJsonValue &value, sessionObj["channel"].toArray()) {
            QJsonObject obj = value.toObject();
            if ((s->get_index() == obj["index"].toDouble()) &&
                (s->get_type() == obj["type"].toDouble())) {
                s->set_colour(QColor(obj["colour"].toString()));
                s->set_name(g_strdup(obj["name"].toString().toStdString().c_str()));

                boost::shared_ptr<view::LogicSignal> logicSig;
                if (logicSig = dynamic_pointer_cast<view::LogicSignal>(s)) {
                    logicSig->set_trig(obj["strigger"].toDouble());
                }

                boost::shared_ptr<view::DsoSignal> dsoSig;
                if (dsoSig = dynamic_pointer_cast<view::DsoSignal>(s)) {
                    dsoSig->load_settings();
                    dsoSig->set_zero_vrate(obj["zeroPos"].toDouble());
                    dsoSig->set_trig_vrate(obj["trigValue"].toDouble());
                    dsoSig->commit_settings();
                }
                break;
            }
        }
    }

    // load trigger settings
    if (sessionObj.contains("trigger")) {
        _trigger_widget->set_session(sessionObj["trigger"].toObject());
    }
    on_trigger(false);
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
    outStream.setGenerateByteOrderMark(true);

    GVariant *gvar_opts;
    GVariant *gvar;
    gsize num_opts;
    const sr_dev_inst *const sdi = _session.get_device()->dev_inst();
    QJsonObject sessionVar;
    QJsonObject triggerVar;
    QJsonArray channelVar;
    sessionVar["Device"] = QJsonValue::fromVariant(sdi->driver->name);
    sessionVar["DeviceMode"] = QJsonValue::fromVariant(sdi->mode);

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
        s_obj["colour"] = QJsonValue::fromVariant(s->get_colour());

        boost::shared_ptr<view::LogicSignal> logicSig;
        if (logicSig = dynamic_pointer_cast<view::LogicSignal>(s)) {
            s_obj["strigger"] = logicSig->get_trig();
        }

        boost::shared_ptr<view::DsoSignal> dsoSig;
        if (dsoSig = dynamic_pointer_cast<view::DsoSignal>(s)) {
            s_obj["vdiv"] = QJsonValue::fromVariant(static_cast<qulonglong>(dsoSig->get_vDialValue()));
            s_obj["vfactor"] = QJsonValue::fromVariant(static_cast<qulonglong>(dsoSig->get_factor()));
            s_obj["coupling"] = dsoSig->get_acCoupling();
            s_obj["trigValue"] = dsoSig->get_trig_vrate();
            s_obj["zeroPos"] = dsoSig->get_zero_vrate();
        }
        channelVar.append(s_obj);
    }
    sessionVar["channel"] = channelVar;

    if (_session.get_device()->dev_inst()->mode == LOGIC) {
        sessionVar["trigger"] = _trigger_widget->get_session();
    }


    QJsonDocument sessionDoc(sessionVar);
    //sessionFile.write(QString::fromUtf8(sessionDoc.toJson()));
    outStream << QString::fromUtf8(sessionDoc.toJson());
    sessionFile.close();
    return true;
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
                                    _view->offset() - _view->scale()*_view->get_view_width());
            break;
        case Qt::Key_PageDown:
            _view->set_scale_offset(_view->scale(),
                                    _view->offset() + _view->scale()*_view->get_view_width());

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
                if (dsoSig = dynamic_pointer_cast<view::DsoSignal>(s)) {
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
                if (dsoSig = dynamic_pointer_cast<view::DsoSignal>(s)) {
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
                if (dsoSig = dynamic_pointer_cast<view::DsoSignal>(s)) {
                    if (dsoSig->get_vDialActive()) {
                        dsoSig->go_vDialNext();
                        update();
                        break;
                    }
                }
            }
            break;
        case Qt::Key_Down:
            BOOST_FOREACH(const shared_ptr<view::Signal> s, sigs) {
                shared_ptr<view::DsoSignal> dsoSig;
                if (dsoSig = dynamic_pointer_cast<view::DsoSignal>(s)) {
                    if (dsoSig->get_vDialActive()) {
                        dsoSig->go_vDialPre();
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

} // namespace pv
