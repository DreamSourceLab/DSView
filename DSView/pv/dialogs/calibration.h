/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
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


#ifndef DSVIEW_PV_CALIBRATION_H
#define DSVIEW_PV_CALIBRATION_H

#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QFormLayout>
#include <QSlider>

#include <list>
#include <boost/shared_ptr.hpp>

#include <pv/device/devinst.h>
#include "../toolbars/titlebar.h"
#include "dsdialog.h"

namespace pv {
namespace dialogs {

class Calibration : public DSDialog
{
	Q_OBJECT

private:
    static const QString VGAIN;
    static const QString VOFF;

public:
    Calibration(QWidget *parent);

    void set_device(boost::shared_ptr<pv::device::DevInst> dev_inst);
protected:
	void accept();
    void reject();

private slots:
    void set_value(int value);
    void on_save();
    void on_reset();
    void reload_value();

private:
    boost::shared_ptr<pv::device::DevInst>  _dev_inst;

    toolbars::TitleBar *_titlebar;
    QPushButton *_save_btn;
    QPushButton *_reset_btn;
    QPushButton *_exit_btn;
    QFormLayout *_flayout;
    std::list <QSlider *> _slider_list;
    std::list<QLabel *> _label_list;
};

} // namespace dialogs
} // namespace pv

#endif // DSVIEW_PV_CALIBRATION_H
