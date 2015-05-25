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


#include "waitingdialog.h"

#include <boost/foreach.hpp>

#include <QMovie>
#include <QAbstractButton>

#include "libsigrok4DSL/libsigrok.h"
#include "../view/trace.h"

using namespace boost;
using namespace std;

namespace pv {
namespace dialogs {

const QString WaitingDialog::TIPS_INFO = tr("Waiting");

WaitingDialog::WaitingDialog(QWidget *parent, boost::shared_ptr<pv::device::DevInst> dev_inst) :
	QDialog(parent),
    _dev_inst(dev_inst),
    _button_box(QDialogButtonBox::Save | QDialogButtonBox::Abort,
        Qt::Horizontal, this)
{
    this->setFixedSize((GIF_SIZE+TIP_WIDTH)*2, (GIF_SIZE+TIP_HEIGHT)*2);
    int midx = this->width() / 2;
    int midy = this->height() / 2;
    this->setWindowOpacity(0.7);
    this->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);

    label = new QLabel(this);
    label->setStyleSheet("background-color: transparent;");
    label->setGeometry(midx-GIF_SIZE/2, midy-GIF_SIZE/2, GIF_SIZE, GIF_SIZE);
    movie = new QMovie(":/icons/wait.gif");
    label->setMovie(movie);

    tips = new QLabel(this);
    tips->setText(TIPS_INFO);
    QFont font;
    font.setPointSize(10);
    font.setBold(true);
    tips->setFont(font);
    tips->setGeometry(midx-TIP_WIDTH/2, midy+GIF_SIZE/2, TIP_WIDTH, TIP_HEIGHT);

    index = 0;
    timer = new QTimer();
    connect(timer, SIGNAL(timeout()), this, SLOT(changeText()));

    _button_box.setGeometry(width()-_button_box.width()-30, height()-_button_box.height()-15,
                            _button_box.width(), _button_box.height());
    connect(&_button_box, SIGNAL(accepted()), this, SLOT(accept()));
    connect(&_button_box, SIGNAL(rejected()), this, SLOT(reject()));
    _button_box.buttons().front()->setVisible(false);
}

void WaitingDialog::accept()
{
	using namespace Qt;

    movie->stop();
    timer->stop();

    _dev_inst->set_config(NULL, NULL, SR_CONF_ZERO_SET, g_variant_new_boolean(true));
	QDialog::accept();
}

void WaitingDialog::reject()
{
    using namespace Qt;

    movie->stop();
    timer->stop();
    QDialog::reject();
}

void WaitingDialog::start()
{
    movie->start();
    timer->start(300);
    this->exec();
}

void WaitingDialog::changeText()
{
    sr_status status;
    index++;
    if(index == WPOINTS_NUM + 1)
    {
        tips->setText(TIPS_INFO);
        index = 0;
        sr_status_get(_dev_inst->dev_inst(), &status, 0, 0);
        if (!status.zeroing) {
            movie->stop();
            movie->jumpToFrame(0);
            timer->stop();
            tips->setText("");
            _button_box.buttons().front()->setVisible(true);
        }
    } else {
        tips->setText(tips->text()+".");
    }
}

} // namespace dialogs
} // namespace pv
