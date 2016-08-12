/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#ifndef DSVIEW_PV_DIALOGS_SAVEPROGRESS_H
#define DSVIEW_PV_DIALOGS_SAVEPROGRESS_H

#include <set>

#include <boost/shared_ptr.hpp>

#include <QProgressDialog>

#include <pv/storesession.h>
#include "../toolbars/titlebar.h"

namespace pv {

class SigSession;

namespace dialogs {

class StoreProgress : public QProgressDialog
{
	Q_OBJECT

public:
    StoreProgress(const QString &file_name, SigSession &session,
        QWidget *parent = 0);

	virtual ~StoreProgress();

	void run();

private:
	void show_error();

	void closeEvent(QCloseEvent*);

private slots:
	void on_progress_updated();

private:
	pv::StoreSession _session;

    toolbars::TitleBar *_titlebar;
};

} // dialogs
} // pv

#endif // PULSEVIEW_PV_DIALOGS_SAVEPROGRESS_H
