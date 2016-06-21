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


#include <QPixmap>
#include <QApplication>

#include "about.h"

namespace pv {
namespace dialogs {

About::About(QWidget *parent) :
    DSDialog(parent, true)
{
    QPixmap pix(":/icons/dsl_logo.png");
    _logo = new QLabel(this);
    _logo->setPixmap(pix);
    _logo->setAlignment(Qt::AlignCenter);

    _info = new QLabel(this);
    _info->setText(tr("%1 %2<br /><a href=\"%4\">%4</a>")
                     .arg(QApplication::applicationName())
                     .arg(QApplication::applicationVersion())
                     .arg(QApplication::organizationDomain()));
    _info->setOpenExternalLinks(true);
    _info->setAlignment(Qt::AlignCenter);

    QVBoxLayout *xlayout = new QVBoxLayout();
    xlayout->addWidget(_logo);
    xlayout->addWidget(_info);

    layout()->addLayout(xlayout);
    setTitle(tr("About"));
    setFixedWidth(500);
}

About::~About()
{
}

} // namespace dialogs
} // namespace pv
