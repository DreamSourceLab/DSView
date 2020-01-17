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

#include "../view/trace.h"
#include "../view/dsosignal.h"

using namespace boost;
using namespace std;

namespace pv {
namespace dialogs {

const QString WaitingDialog::TIPS_WAIT = "Waiting";
const QString WaitingDialog::TIPS_FINISHED = "Finished!";

WaitingDialog::WaitingDialog(QWidget *parent, SigSession &session, int key) :
    DSDialog(parent),
    _key(key),
    _session(session),
    _button_box(QDialogButtonBox::Abort,
        Qt::Horizontal, this)
{
    _dev_inst = _session.get_device();
    this->setFixedSize((GIF_WIDTH+2*TIP_WIDTH)*1.2, (GIF_HEIGHT+2*TIP_HEIGHT)*4);
    this->setWindowOpacity(0.7);

    QFont font;
    font.setPointSize(10);
    font.setBold(true);

    QLabel *warning_tips = new QLabel(this);
    warning_tips->setText(tr("Don't connect any probes!"));
    warning_tips->setFont(font);
    warning_tips->setAlignment(Qt::AlignCenter);

    QString iconPath = ":/icons/" + qApp->property("Style").toString();
    label = new QLabel(this);
    movie = new QMovie(iconPath+"/wait.gif");
    label->setMovie(movie);
    label->setAlignment(Qt::AlignCenter);

    tips = new QLabel(this);
    tips->setText(tr("Waiting"));
    tips->setFont(font);
    tips->setAlignment(Qt::AlignCenter);

    index = 0;
    timer = new QTimer();
    connect(timer, SIGNAL(timeout()), this, SLOT(changeText()));
    connect(&_button_box, SIGNAL(accepted()), this, SLOT(accept()));
    connect(&_button_box, SIGNAL(rejected()), this, SLOT(reject()));
    connect(_dev_inst.get(), SIGNAL(device_updated()), this, SLOT(stop()));


    QVBoxLayout *mlayout = new QVBoxLayout();
    mlayout->addWidget(warning_tips, Qt::AlignHCenter);
    mlayout->addWidget(label, Qt::AlignHCenter);
    mlayout->addWidget(tips, Qt::AlignHCenter);
    mlayout->addWidget(&_button_box);

    layout()->addLayout(mlayout);
    setTitle(tr("Auto Calibration"));
}

void WaitingDialog::accept()
{
	using namespace Qt;
    movie->stop();
    timer->stop();
    QDialog::accept();

    QFuture<void> future;
    future = QtConcurrent::run([&]{
        _dev_inst->set_config(NULL, NULL, SR_CONF_ZERO_SET,
                              g_variant_new_boolean(true));
    });
    Qt::WindowFlags flags = Qt::CustomizeWindowHint;
    QProgressDialog dlg(tr("Save calibration Result... It can take a while."),
                        tr("Cancel"),0,0,this,flags);
    dlg.setWindowModality(Qt::WindowModal);
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint |
                       Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
    dlg.setCancelButton(NULL);

    QFutureWatcher<void> watcher;
    connect(&watcher,SIGNAL(finished()),&dlg,SLOT(cancel()));
    watcher.setFuture(future);

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
        _dev_inst->set_config(NULL, NULL, _key, g_variant_new_boolean(false));
        _dev_inst->set_config(NULL, NULL, SR_CONF_ZERO_LOAD,
                              g_variant_new_boolean(true));
    });
    Qt::WindowFlags flags = Qt::CustomizeWindowHint;
    QProgressDialog dlg(tr("Load current setting... It can take a while."),
                        tr("Cancel"),0,0,this,flags);
    dlg.setWindowModality(Qt::WindowModal);
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint |
                       Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
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
        tips->setText(tr("Waiting"));
        index = 0;
        GVariant* gvar;
        bool comb_comp_en = false;
        bool zero_fgain = false;

        gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_PROBE_COMB_COMP_EN);
        if (gvar != NULL) {
            comb_comp_en = g_variant_get_boolean(gvar);
            g_variant_unref(gvar);
            if (comb_comp_en) {
                gvar = _dev_inst->get_config(NULL, NULL, SR_CONF_ZERO_COMB_FGAIN);
                if (gvar != NULL) {
                    zero_fgain = g_variant_get_boolean(gvar);
                    g_variant_unref(gvar);
                    if (zero_fgain) {
                        boost::shared_ptr<view::DsoSignal> dsoSig;
                        BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _session.get_signals())
                        {
                            if ((dsoSig = dynamic_pointer_cast<view::DsoSignal>(s)))
                                dsoSig->set_enable(dsoSig->get_index() == 0);
                        }
                        boost::this_thread::sleep(boost::posix_time::millisec(100));
                        _dev_inst->set_config(NULL, NULL, SR_CONF_ZERO_COMB, g_variant_new_boolean(true));
                    }
                }
            }
        }

        gvar = _dev_inst->get_config(NULL, NULL, _key);
        if (gvar != NULL) {
            bool zero = g_variant_get_boolean(gvar);
            g_variant_unref(gvar);
            if (!zero) {
                movie->stop();
                movie->jumpToFrame(0);
                timer->stop();
                tips->setAlignment(Qt::AlignHCenter);
                tips->setText(tr("Finished!"));
                _button_box.addButton(QDialogButtonBox::Save);
            }
        }
    } else {
        tips->setText(tips->text()+".");
    }
}

} // namespace dialogs
} // namespace pv
