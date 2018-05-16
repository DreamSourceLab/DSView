/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2016 DreamSourceLab <support@dreamsourcelab.com>
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

#include "QVBoxLayout"

namespace pv {
namespace dialogs {

StoreProgress::StoreProgress(SigSession &session, QWidget *parent) :
    DSDialog(parent),
    _store_session(session),
    _button_box(QDialogButtonBox::Cancel, Qt::Horizontal, this),
    _done(false)
{
    this->setModal(true);

    _info.setText("...");
    _progress.setValue(0);
    _progress.setMaximum(100);

    QVBoxLayout* add_layout = new QVBoxLayout();
    add_layout->addWidget(&_info, 0, Qt::AlignCenter);
    add_layout->addWidget(&_progress);
    add_layout->addWidget(&_button_box);
    layout()->addLayout(add_layout);

    connect(&_button_box, SIGNAL(rejected()), this, SLOT(reject()));
    connect(&_store_session, SIGNAL(progress_updated()),
        this, SLOT(on_progress_updated()), Qt::QueuedConnection);
}

StoreProgress::~StoreProgress()
{
    _store_session.wait();
}

void StoreProgress::reject()
{
    using namespace Qt;
    _store_session.cancel();
    QDialog::reject();
}

void StoreProgress::timeout()
{
    if (_done)
        close();
    else
        QTimer::singleShot(100, this, SLOT(timeout()));
}

void StoreProgress::save_run()
{
    _info.setText("Saving...");
    if (_store_session.save_start())
        show();
    else
		show_error();

    QTimer::singleShot(100, this, SLOT(timeout()));
}

void StoreProgress::export_run()
{
    _info.setText("Exporting...");
    if (_store_session.export_start())
        show();
    else
        show_error();

    QTimer::singleShot(100, this, SLOT(timeout()));
}

void StoreProgress::show_error()
{
    if (!_store_session.error().isEmpty()) {
        dialogs::DSMessageBox msg(parentWidget());
        msg.mBox()->setText(tr("Failed to save data."));
        msg.mBox()->setInformativeText(_store_session.error());
        msg.mBox()->setStandardButtons(QMessageBox::Ok);
        msg.mBox()->setIcon(QMessageBox::Warning);
        msg.exec();
    }
}

void StoreProgress::closeEvent(QCloseEvent* e)
{
    _store_session.cancel();
    DSDialog::closeEvent(e);
}

void StoreProgress::on_progress_updated()
{
    const std::pair<uint64_t, uint64_t> p = _store_session.progress();
	assert(p.first <= p.second);
    int percent = p.first * 1.0 / p.second * 100;
    _progress.setValue(percent);

    const QString err = _store_session.error();
	if (!err.isEmpty()) {
		show_error();
        //close();
        _done = true;
	}

    if (p.first == p.second) {
        //close();
        _done = true;
    }
}

} // dialogs
} // pv
