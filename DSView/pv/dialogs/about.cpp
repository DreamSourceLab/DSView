/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
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


#include <QTextDocument>

#include "about.h"
#include <ui_about.h>

/* __STDC_FORMAT_MACROS is required for PRIu64 and friends (in C++). */
#define __STDC_FORMAT_MACROS
#include <glib.h>
#include <libsigrok4DSL/libsigrok.h>


namespace pv {
namespace dialogs {

About::About(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::About)
{
	ui->setupUi(this);

	/* Setup the version field */
    ui->versionInfo->setText(tr("%1 %2<br /><a href=\"%4\">%4</a>")
				 .arg(QApplication::applicationName())
				 .arg(QApplication::applicationVersion())
				 .arg(QApplication::organizationDomain()));
    ui->versionInfo->setOpenExternalLinks(true);

	connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
}

About::~About()
{
    delete ui;
}

void About::accept()
{
    using namespace Qt;
    QDialog::accept();
}

} // namespace dialogs
} // namespace pv
