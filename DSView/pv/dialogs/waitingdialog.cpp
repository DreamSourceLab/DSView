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


#include "waitingdialog.h"

#include <boost/foreach.hpp>

#include <QMovie>
#include <QAbstractButton>
#include <QFuture>
#include <QProgressDialog>
#include <QtConcurrent/QtConcurrent>
#include <QVBoxLayout>

#include "libsigrok4DSL/libsigrok.h"
#include "../view/trace.h"

using namespace boost;
using namespace std;

namespace pv {
namespace dialogs {

const QString WaitingDialog::TIPS_WAIT = QT_TR_NOOP("Waiting");
const QString WaitingDialog::TIPS_FINISHED = QT_TR_NOOP("Finished!");

WaitingDialog::WaitingDialog(QWidget *parent, boost::shared_ptr<pv::device::DevInst> dev_inst) :
    DSDialog(parent),
    _dev_inst(dev_inst),
    _button_box(QDialogButtonBox::Abort,
        Qt::Horizontal, this)
{
    this->setFixedSize((GIF_WIDTH+TIP_WIDTH)*1.2, (GIF_HEIGHT+TIP_HEIGHT)*4);
    this->setWindowOpacity(0.7);

    label = new QLabel(this);
    movie = new QMovie(":/icons/wait.gif");
    label->setMovie(movie);
    label->setAlignment(Qt::AlignCenter);

    tips = new QLabel(this);
    tips->setText(TIPS_WAIT);
    QFont font;
    font.setPointSize(10);
    font.setBold(true);
    tips->setFont(font);
    tips->setAlignment(Qt::AlignCenter);

    index = 0;
    timer = new QTimer();
    connect(timer, SIGNAL(timeout()), this, SLOT(changeText()));
    connect(&_button_box, SIGNAL(accepted()), this, SLOT(accept()));
    connect(&_button_box, SIGNAL(rejected()), this, SLOT(reject()));
    connect(_dev_inst.get(), SIGNAL(device_updated()), this, SLOT(stop()));


    QVBoxLayout *mlayout = new QVBoxLayout();
    mlayout->addWidget(label, Qt::AlignHCenter);
    mlayout->addWidget(tips, Qt::AlignHCenter);
    mlayout->addWidget(&_button_box);

    layout()->addLayout(mlayout);
    setTitle(tr("Zero Adjustment"));
}

void WaitingDialog::accept()
{
	using namespace Qt;
    movie->stop();
    timer->stop();
    QDialog::accept();

    QFuture<void> future;
    future = QtConcurrent::run([&]{
        //QTime dieTime = QTime::currentTime().addSecs(1);
        _dev_inst->set_config(NULL, NULL, SR_CONF_ZERO_SET,
                              g_variant_new_boolean(true));
        //while( QTime::currentTime() < dieTime );
    });
    Qt::WindowFlags flags = Qt::CustomizeWindowHint;
    QProgressDialog dlg(tr("Save Auto Zero Result... It can take a while."),
                        tr("Cancel"),0,0,this,flags);
    dlg.setWindowModality(Qt::WindowModal);
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dlg.setCancelButton(NULL);

    QFutureWatcher<void> watcher;
    watcher.setFuture(future);
    connect(&watcher,SIGNAL(finished()),&dlg,SLOT(cancel()));

    dlg.exec();
}

void WaitingDialog::reject()
{
    using namespace Qt;

    movie->stop();
    timer->stop();
    QDialog::reject();

    QFuture<void> future;
    future = QtConcurrent::run([&]{
        //QTime dieTime = QTime::currentTime().addSecs(1);
        _dev_inst->set_config(NULL, NULL, SR_CONF_ZERO, g_variant_new_boolean(false));
        _dev_inst->set_config(NULL, NULL, SR_CONF_ZERO_LOAD,
                              g_variant_new_boolean(true));
        //while( QTime::currentTime() < dieTime );
    });
    Qt::WindowFlags flags = Qt::CustomizeWindowHint;
    QProgressDialog dlg(tr("Load Current Setting... It can take a while."),
                        tr("Cancel"),0,0,this,flags);
    dlg.setWindowModality(Qt::WindowModal);
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    dlg.setCancelButton(NULL);

    QFutureWatcher<void> watcher;
    connect(&watcher,SIGNAL(finished()),&dlg,SLOT(cancel()));
    watcher.setFuture(future);

    dlg.exec();
}

void WaitingDialog::stop()
{
    using namespace Qt;

    movie->stop();
    timer->stop();
    QDialog::reject();
}

int WaitingDialog::start()
{
    movie->start();
    timer->start(300);
    return this->exec();
}

void WaitingDialog::changeText()
{
    index++;
    if(index == WPOINTS_NUM + 1)
    {
        tips->setText(TIPS_WAIT);
        index = 0;

        GVariant* gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_ZERO);
        if (gvar != NULL) {
            bool zero = g_variant_get_boolean(gvar);
            g_variant_unref(gvar);
            if (!zero) {
                movie->stop();
                movie->jumpToFrame(0);
                timer->stop();
                tips->setAlignment(Qt::AlignHCenter);
                tips->setText(TIPS_FINISHED);
                _button_box.addButton(QDialogButtonBox::Save);
            }
        }
    } else {
        tips->setText(tips->text()+".");
    }
}

} // namespace dialogs
} // namespace pv
