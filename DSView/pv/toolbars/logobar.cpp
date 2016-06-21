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
    _session(session),
    _logo_button(this)
{
    setMovable(false);

    _about = new QAction(this);
    _about->setText(QApplication::translate(
        "File", "&About...", 0));
    _about->setIcon(QIcon::fromTheme("file",
        QIcon(":/icons/about.png")));
    _about->setObjectName(QString::fromUtf8("actionAbout"));
    _logo_button.addAction(_about);
    connect(_about, SIGNAL(triggered()), this, SLOT(on_actionAbout_triggered()));

    _wiki = new QAction(this);
    _wiki->setText(QApplication::translate(
        "File", "&Wiki", 0));
    _wiki->setIcon(QIcon::fromTheme("file",
        QIcon(":/icons/wiki.png")));
    _wiki->setObjectName(QString::fromUtf8("actionWiki"));
    _logo_button.addAction(_wiki);
    connect(_wiki, SIGNAL(triggered()), this, SLOT(on_actionWiki_triggered()));

    _logo_button.setPopupMode(QToolButton::InstantPopup);
    _logo_button.setIcon(QIcon(":/icons/logo_noColor.png"));

    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    addWidget(spacer);
    addWidget(&_logo_button);
    QWidget *margin = new QWidget(this);
    margin->setMinimumWidth(20);
    addWidget(margin);
}

void LogoBar::dsl_connected(bool conn)
{
    if (conn)
        _logo_button.setIcon(QIcon(":/icons/logo_color.png"));
    else
        _logo_button.setIcon(QIcon(":/icons/logo_noColor.png"));
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

void LogoBar::on_actionAbout_triggered()
{
    dialogs::About dlg(this);
    dlg.exec();
}

void LogoBar::on_actionWiki_triggered()
{
    QDesktopServices::openUrl(
                QUrl(QLatin1String("http://www.dreamsourcelab.com/wiki")));
}

void LogoBar::enable_toggle(bool enable)
{
    _logo_button.setDisabled(!enable);
}

} // namespace toolbars
} // namespace pv
