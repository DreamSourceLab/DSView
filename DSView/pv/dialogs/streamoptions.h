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


#ifndef DSVIEW_PV_STREAMOPTIONS_H
#define DSVIEW_PV_STREAMOPTIONS_H

#include <QDialog>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QVector>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QRadioButton>

#include <boost/shared_ptr.hpp>

#include <pv/device/devinst.h>
#include <pv/prop/binding/deviceoptions.h>

namespace pv {
namespace dialogs {

class StreamOptions : public QDialog
{
	Q_OBJECT

public:
    StreamOptions(QWidget *parent, boost::shared_ptr<pv::device::DevInst> dev_inst,
                  uint64_t sample_count, bool stream);

protected:
	void accept();
    void reject();

private:
    boost::shared_ptr<pv::device::DevInst>  _dev_inst;
    uint64_t _sample_count;
	QVBoxLayout _layout;

    QRadioButton * _op0;
    QRadioButton * _op1;

    bool _stream;

    QDialogButtonBox _button_box;
};

} // namespace dialogs
} // namespace pv

#endif // DSVIEW_PV_STREAMOPTIONS_H
