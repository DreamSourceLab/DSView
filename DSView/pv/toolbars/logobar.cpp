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

#include <boost/bind.hpp>
#include <boost/foreach.hpp>

#include <QMetaObject>
#include <QFileDialog>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>

#include "logobar.h"
#include "../dialogs/about.h"
#include "../dialogs/dsmessagebox.h"

namespace pv {
namespace toolbars {

LogoBar::LogoBar(SigSession &session, QWidget *parent) :
    QToolBar("File Bar", parent),
    _enable(true),
    _connected(false),
    _session(session),
    _logo_button(this)
{
    setMovable(false);
    setContentsMargins(0,0,0,0);

    _action_en = new QAction(this);
    _action_en->setObjectName(QString::fromUtf8("actionEn"));
    connect(_action_en, SIGNAL(triggered()), this, SLOT(on_actionEn_triggered()));

    _action_cn = new QAction(this);
    _action_cn->setObjectName(QString::fromUtf8("actionCn"));
    connect(_action_cn, SIGNAL(triggered()), this, SLOT(on_actionCn_triggered()));

    _language = new QMenu(this);
    _language->setObjectName(QString::fromUtf8("menuLanguage"));
    _language->addAction(_action_cn);
    _language->addAction(_action_en);

    _action_en->setIcon(QIcon(":/icons/English.svg"));
    _action_cn->setIcon(QIcon(":/icons/Chinese.svg"));

    _about = new QAction(this);
    _about->setObjectName(QString::fromUtf8("actionAbout"));
    _logo_button.addAction(_about);
    connect(_about, SIGNAL(triggered()), this, SLOT(on_actionAbout_triggered()));

    _manual = new QAction(this);
    _manual->setObjectName(QString::fromUtf8("actionManual"));
    _logo_button.addAction(_manual);
    connect(_manual, SIGNAL(triggered()), this, SIGNAL(openDoc()));

    _issue = new QAction(this);
    _issue->setObjectName(QString::fromUtf8("actionManual"));
    _logo_button.addAction(_issue);
    connect(_issue, SIGNAL(triggered()), this, SLOT(on_actionIssue_triggered()));

    _menu = new QMenu(this);
    _menu->addMenu(_language);
    _menu->addAction(_about);
    _menu->addAction(_manual);
    _menu->addAction(_issue);
    _logo_button.setMenu(_menu);

    _logo_button.setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    _logo_button.setPopupMode(QToolButton::InstantPopup);

    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    addWidget(spacer);
    addWidget(&_logo_button);
    QWidget *margin = new QWidget(this);
    margin->setMinimumWidth(20);
    addWidget(margin);

    retranslateUi();
}

void LogoBar::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    else if (event->type() == QEvent::StyleChange)
        reStyle();
    QToolBar::changeEvent(event);
}

void LogoBar::retranslateUi()
{
    _logo_button.setText(tr("Help"));
    _action_en->setText("English");
    _action_cn->setText("中文");
    _language->setTitle(tr("&Language"));
    _about->setText(tr("&About..."));
    _manual->setText(tr("&Manual"));
    _issue->setText(tr("&Bug Report"));

    if (qApp->property("Language") == QLocale::Chinese)
        _language->setIcon(QIcon(":/icons/Chinese.svg"));
    else
        _language->setIcon(QIcon(":/icons/English.svg"));
}

void LogoBar::reStyle()
{
    QString iconPath = ":/icons/" + qApp->property("Style").toString();

    _about->setIcon(QIcon(iconPath+"/about.svg"));
    _manual->setIcon(QIcon(iconPath+"/manual.svg"));
    _issue->setIcon(QIcon(iconPath+"/bug.svg"));
    if (_connected)
        _logo_button.setIcon(QIcon(iconPath+"/logo_color.svg"));
    else
        _logo_button.setIcon(QIcon(iconPath+"/logo_noColor.svg"));
}

void LogoBar::dsl_connected(bool conn)
{
    _connected = conn;
    QString iconPath = ":/icons/" + qApp->property("Style").toString();
    if (_connected)
        _logo_button.setIcon(QIcon(iconPath+"/logo_color.svg"));
    else
        _logo_button.setIcon(QIcon(iconPath+"/logo_noColor.svg"));
}

void LogoBar::session_error(
    const QString text, const QString info_text)
{
    QMetaObject::invokeMethod(this, "show_session_error",
        Qt::QueuedConnection, Q_ARG(QString, text),
        Q_ARG(QString, info_text));
}

void LogoBar::show_session_error(
    const QString text, const QString info_text)
{
    dialogs::DSMessageBox msg(this);
    msg.mBox()->setText(text);
    msg.mBox()->setInformativeText(info_text);
    msg.mBox()->setStandardButtons(QMessageBox::Ok);
    msg.mBox()->setIcon(QMessageBox::Warning);
    msg.exec();
}

void LogoBar::on_actionEn_triggered()
{
    _language->setIcon(QIcon::fromTheme("file",
        QIcon(":/icons/English.svg")));
    setLanguage(QLocale::English);
}

void LogoBar::on_actionCn_triggered()
{
    _language->setIcon(QIcon::fromTheme("file",
        QIcon(":/icons/Chinese.svg")));
    setLanguage(QLocale::Chinese);
}

void LogoBar::on_actionAbout_triggered()
{
    dialogs::About dlg(this);
    dlg.exec();
}

void LogoBar::on_actionManual_triggered()
{
    #ifndef Q_OS_LINUX
    QDir dir(QCoreApplication::applicationDirPath());
    #else
    QDir dir(DS_RES_PATH);
    dir.cdUp();
    #endif
    QDesktopServices::openUrl(
                QUrl("file:///"+dir.absolutePath() + "/ug.pdf"));
}

void LogoBar::on_actionIssue_triggered()
{
    QDir dir(QCoreApplication::applicationDirPath());
    QDesktopServices::openUrl(
                QUrl(QLatin1String("https://github.com/DreamSourceLab/DSView/issues")));
}

void LogoBar::enable_toggle(bool enable)
{
    _logo_button.setDisabled(!enable);
}

} // namespace toolbars
} // namespace pv
