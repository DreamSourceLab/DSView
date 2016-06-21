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

#include "storeprogress.h"
#include "dsmessagebox.h"

namespace pv {
namespace dialogs {

StoreProgress::StoreProgress(const QString &file_name,
    SigSession &session, QWidget *parent) :
	QProgressDialog(tr("Saving..."), tr("Cancel"), 0, 0, parent),
	_session(file_name.toStdString(), session)
{
	connect(&_session, SIGNAL(progress_updated()),
		this, SLOT(on_progress_updated()));
}

StoreProgress::~StoreProgress()
{
	_session.wait();
}

void StoreProgress::run()
{
	if (_session.start())
		show();
	else
		show_error();
}

void StoreProgress::show_error()
{
    dialogs::DSMessageBox msg(parentWidget());
    msg.mBox()->setText(tr("Failed to save session."));
    msg.mBox()->setInformativeText(_session.error());
    msg.mBox()->setStandardButtons(QMessageBox::Ok);
    msg.mBox()->setIcon(QMessageBox::Warning);
	msg.exec();
}

void StoreProgress::closeEvent(QCloseEvent*)
{
	_session.cancel();
}

void StoreProgress::on_progress_updated()
{
	const std::pair<uint64_t, uint64_t> p = _session.progress();
	assert(p.first <= p.second);

	setValue(p.first);
	setMaximum(p.second);

	const QString err = _session.error();
	if (!err.isEmpty()) {
		show_error();
		close();
	}

	if (p.first == p.second)
		close();
}

} // dialogs
} // pv
