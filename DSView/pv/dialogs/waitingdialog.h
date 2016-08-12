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


#ifndef DSVIEW_PV_WAITINGDIALOG_H
#define DSVIEW_PV_WAITINGDIALOG_H

#include <QDialogButtonBox>
#include <QTimer>
#include <QLabel>
#include <QMovie>

#include <boost/shared_ptr.hpp>

#include <pv/device/devinst.h>
#include "../toolbars/titlebar.h"
#include "dsdialog.h"

namespace pv {
namespace dialogs {

class WaitingDialog : public DSDialog
{
	Q_OBJECT

private:
    static const int GIF_SIZE = 80;
    static const int GIF_WIDTH = 220;
    static const int GIF_HEIGHT = 20;
    static const int TIP_WIDTH = 100;
    static const int TIP_HEIGHT = 40;
    static const int WPOINTS_NUM = 6;
    static const QString TIPS_WAIT;
    static const QString TIPS_FINISHED;

public:
    WaitingDialog(QWidget *parent, boost::shared_ptr<pv::device::DevInst> dev_inst);
    int start();

protected:
	void accept();
    void reject();

private slots:
    void changeText();
    void stop();

private:
    boost::shared_ptr<pv::device::DevInst>  _dev_inst;
    toolbars::TitleBar *_titlebar;
    QDialogButtonBox _button_box;

    int index;
    QLabel *label;
    QMovie *movie;
    QTimer *timer;
    QLabel *tips;
};

} // namespace dialogs
} // namespace pv

#endif // DSVIEW_PV_WAITINGDIALOG_H
