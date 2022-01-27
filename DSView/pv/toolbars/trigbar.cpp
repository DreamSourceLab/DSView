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

#include "trigbar.h"

#include <QBitmap>
#include <QPainter>
#include <QEvent>

#include "../sigsession.h"
#include "../device/devinst.h"
#include "../dialogs/fftoptions.h"
#include "../dialogs/lissajousoptions.h"
#include "../dialogs/mathoptions.h"
#include "../view/trace.h"
#include "../dialogs/applicationpardlg.h"
#include "../config/appconfig.h"

namespace pv {
namespace toolbars {

const QString TrigBar::DARK_STYLE = "dark";
const QString TrigBar::LIGHT_STYLE = "light";

TrigBar::TrigBar(SigSession *session, QWidget *parent) :
    QToolBar("Trig Bar", parent),
    _session(session),
 
    _trig_button(this),
    _protocol_button(this),
    _measure_button(this),
    _search_button(this),
    _function_button(this),
    _setting_button(this)
{
    _enable = true;

    setMovable(false);
    setContentsMargins(0,0,0,0);
 
    _trig_button.setCheckable(true);
 
    _protocol_button.setCheckable(true);
 
    _measure_button.setCheckable(true);
    _search_button.setCheckable(true);

    _action_fft = new QAction(this);
    _action_fft->setObjectName(QString::fromUtf8("actionFft"));
   
    _action_math = new QAction(this);
    _action_math->setObjectName(QString::fromUtf8("actionMath"));
     
    _function_menu = new QMenu(this);
    _function_menu->setContentsMargins(0,0,0,0);
    _function_menu->addAction(_action_fft);
    _function_menu->addAction(_action_math);
    _function_button.setPopupMode(QToolButton::InstantPopup);
    _function_button.setMenu(_function_menu);

    _action_lissajous = new QAction(this);
    _action_lissajous->setObjectName(QString::fromUtf8("actionLissajous"));
   
    _dark_style = new QAction(this);
    _dark_style->setObjectName(QString::fromUtf8("actionDark"));
    
    _light_style = new QAction(this);
    _light_style->setObjectName(QString::fromUtf8("actionLight"));
     
    _themes = new QMenu(this);
    _themes->setObjectName(QString::fromUtf8("menuThemes"));
    _themes->addAction(_light_style);
    _themes->addAction(_dark_style);

     _appParam_action = new QAction(this);

    _display_menu = new QMenu(this);
    _display_menu->setContentsMargins(0,0,0,0);
    
    _display_menu->addAction(_appParam_action);
    _display_menu->addAction(_action_lissajous);    
    _display_menu->addMenu(_themes);

    _setting_button.setPopupMode(QToolButton::InstantPopup);
    _setting_button.setMenu(_display_menu);

    _trig_button.setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    _protocol_button.setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    _measure_button.setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    _search_button.setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    _function_button.setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    _setting_button.setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    _protocol_button.setContentsMargins(0,0,0,0);

    _trig_action = addWidget(&_trig_button);
    _protocol_action = addWidget(&_protocol_button);
    _measure_action = addWidget(&_measure_button);
    _search_action = addWidget(&_search_button);
    _function_action = addWidget(&_function_button); 
    _display_action = addWidget(&_setting_button); //must be created
 
    retranslateUi();

    connect(&_trig_button, SIGNAL(clicked()),this, SLOT(trigger_clicked()));
    connect(&_protocol_button, SIGNAL(clicked()),this, SLOT(protocol_clicked()));
    connect(&_measure_button, SIGNAL(clicked()),this, SLOT(measure_clicked()));
    connect(&_search_button, SIGNAL(clicked()), this, SLOT(search_clicked()));

    connect(_action_fft, SIGNAL(triggered()), this, SLOT(on_actionFft_triggered()));
    connect(_action_math, SIGNAL(triggered()), this, SLOT(on_actionMath_triggered()));
    connect(_action_lissajous, SIGNAL(triggered()), this, SLOT(on_actionLissajous_triggered()));
    connect(_dark_style, SIGNAL(triggered()), this, SLOT(on_actionDark_triggered()));
    connect(_light_style, SIGNAL(triggered()), this, SLOT(on_actionLight_triggered()));
    connect(_appParam_action, SIGNAL(triggered()), this, SLOT(on_application_param()));
}

void TrigBar::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    else if (event->type() == QEvent::StyleChange)
        reStyle();
    QToolBar::changeEvent(event);
}

void TrigBar::retranslateUi()
{
    _trig_button.setText(tr("Trigger"));
    _protocol_button.setText(tr("Decode"));
    _measure_button.setText(tr("Measure"));
    _search_button.setText(tr("Search"));
    _function_button.setText(tr("Function"));
    _setting_button.setText(tr("Setting"));
 
    _action_lissajous->setText(tr("&Lissajous"));

    _themes->setTitle(tr("Themes"));
    _dark_style->setText(tr("Dark"));
    _light_style->setText(tr("Light"));

    _action_fft->setText(tr("FFT"));
    _action_math->setText(tr("Math"));

    _appParam_action->setText(tr("Application"));
}

void TrigBar::reStyle()
{
    QString iconPath = GetIconPath();

    _trig_button.setIcon(QIcon(iconPath+"/trigger.svg"));
    _protocol_button.setIcon(QIcon(iconPath+"/protocol.svg"));
    _measure_button.setIcon(QIcon(iconPath+"/measure.svg"));
    _search_button.setIcon(QIcon(iconPath+"/search-bar.svg"));
    _function_button.setIcon(QIcon(iconPath+"/function.svg"));
    _setting_button.setIcon(QIcon(iconPath+"/display.svg"));

    _action_fft->setIcon(QIcon(iconPath+"/fft.svg"));
    _action_math->setIcon(QIcon(iconPath+"/math.svg"));
    _action_lissajous->setIcon(QIcon(iconPath+"/lissajous.svg"));
    _dark_style->setIcon(QIcon(iconPath+"/dark.svg"));
    _light_style->setIcon(QIcon(iconPath+"/light.svg"));

     _appParam_action->setIcon(QIcon(iconPath+"/params.svg"));

     AppConfig &app = AppConfig::Instance();    

     QString icon_fname = iconPath +"/"+ app._frameOptions.style +".svg";  
    _themes->setIcon(QIcon(icon_fname));
}

void TrigBar::protocol_clicked()
{  
    sig_protocol(_protocol_button.isChecked());
 
    DockOptions *opt = getDockOptions();
    opt->decodeDoc = _protocol_button.isChecked();
    AppConfig::Instance().SaveFrame();
}

void TrigBar::trigger_clicked()
{
    sig_trigger(_trig_button.isChecked());
 
    DockOptions *opt = getDockOptions();
    opt->triggerDoc = _trig_button.isChecked();
    AppConfig::Instance().SaveFrame();
}

void TrigBar::update_trig_btn(bool checked)
{
    _trig_button.setChecked(checked);
}

void TrigBar::update_protocol_btn(bool checked)
{
    _protocol_button.setChecked(checked);
}

void TrigBar::update_measure_btn(bool checked)
{
    _measure_button.setChecked(checked);
}

void TrigBar::update_search_btn(bool checked)
{
    _search_button.setChecked(checked);
}

void TrigBar::measure_clicked()
{
    sig_measure(_measure_button.isChecked());
    
    DockOptions *opt = getDockOptions();
    opt->measureDoc = _measure_button.isChecked();
    AppConfig::Instance().SaveFrame();
}

void TrigBar::search_clicked()
{
    sig_search(_search_button.isChecked());
    
    DockOptions *opt = getDockOptions();
    opt->searchDoc = _search_button.isChecked();
    AppConfig::Instance().SaveFrame();
}

void TrigBar::enable_toggle(bool enable)
{
    _trig_button.setDisabled(!enable);
    _protocol_button.setDisabled(!enable);
    _measure_button.setDisabled(!enable);
    _search_button.setDisabled(!enable);
    _function_button.setDisabled(!enable);
    _setting_button.setDisabled(!enable);
}

void TrigBar::enable_protocol(bool enable)
{
    _protocol_button.setDisabled(!enable);
}

void TrigBar::close_all()
{
    if (_trig_button.isChecked()) {
        _trig_button.setChecked(false);
        sig_trigger(false);
    }
    if (_protocol_button.isChecked()) {
        _protocol_button.setChecked(false);
        sig_protocol(false);
    }
    if (_measure_button.isChecked()) {
        _measure_button.setChecked(false);
        sig_measure(false);
    }
    if(_search_button.isChecked()) {
        _search_button.setChecked(false);
        sig_search(false);
    }
}

void TrigBar::reload()
{
    close_all();

    if (_session->get_device()->dev_inst()->mode == LOGIC) {
        _trig_action->setVisible(true);
        _protocol_action->setVisible(true);
        _measure_action->setVisible(true);
        _search_action->setVisible(true);
        _function_action->setVisible(false);
        _action_lissajous->setVisible(false);

    } else if (_session->get_device()->dev_inst()->mode == ANALOG) {
        _trig_action->setVisible(false);
        _protocol_action->setVisible(false);
        _measure_action->setVisible(true);
        _search_action->setVisible(false);
        _function_action->setVisible(false);
        _action_lissajous->setVisible(false);

    } else if (_session->get_device()->dev_inst()->mode == DSO) {
        _trig_action->setVisible(true);
        _protocol_action->setVisible(false);
        _measure_action->setVisible(true);
        _search_action->setVisible(false);
        _function_action->setVisible(true);
        _action_lissajous->setVisible(true);
    }

    enable_toggle(true);
    update();
}

void TrigBar::on_actionFft_triggered()
{
    pv::dialogs::FftOptions fft_dlg(this, _session);
    fft_dlg.exec();
}

void TrigBar::on_actionMath_triggered()
{
    pv::dialogs::MathOptions math_dlg(_session, this);
    math_dlg.exec();
}

void TrigBar::on_actionDark_triggered()
{
    sig_setTheme(DARK_STYLE);
    QString icon = GetIconPath() + "/" + DARK_STYLE + ".svg";
    _themes->setIcon(QIcon(icon));
}

void TrigBar::on_actionLight_triggered()
{
    sig_setTheme(LIGHT_STYLE);
    QString icon = GetIconPath() + "/" + LIGHT_STYLE +".svg";
    _themes->setIcon(QIcon(icon));
}

void TrigBar::on_actionLissajous_triggered()
{
    pv::dialogs::LissajousOptions lissajous_dlg(_session, this);
    lissajous_dlg.exec();
}

 void TrigBar::on_application_param(){
   //  pv::dialogs::MathOptions math_dlg(_session, this);  math_dlg.exec();   return;
    
     pv::dialogs::ApplicationParamDlg dlg;
     dlg.ShowDlg(this);
 }

 
void TrigBar::restore_status()
{ 
    DockOptions *opt = getDockOptions();
    int mode = _session->get_device()->dev_inst()->mode;

    if (opt->decodeDoc){
         _protocol_button.setChecked(true);
         sig_protocol(true);
    }

    if (opt->triggerDoc){
         _trig_button.setChecked(true);
         sig_trigger(true);
    }

    if (opt->measureDoc){
        _measure_button.setChecked(true);
        sig_measure(true);
    }

    if (opt->searchDoc){
        _search_button.setChecked(true);
        sig_search(true);
    }
}

 DockOptions* TrigBar::getDockOptions()
 {
     AppConfig &app = AppConfig::Instance(); 
     int mode = _session->get_device()->dev_inst()->mode;

     if (mode == LOGIC)
        return &app._frameOptions._logicDock;
     else if (mode == DSO)
        return &app._frameOptions._dsoDock;
    else
        return &app._frameOptions._analogDock;
 }

} // namespace toolbars
} // namespace pv
