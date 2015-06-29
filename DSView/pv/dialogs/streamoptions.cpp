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


#include "streamoptions.h"

#include <boost/foreach.hpp>

#include <QFormLayout>
#include <QListWidget>
#include <QMessageBox>

#include <pv/prop/property.h>

using namespace boost;
using namespace std;

namespace pv {
namespace dialogs {

StreamOptions::StreamOptions(QWidget *parent, boost::shared_ptr<pv::device::DevInst> dev_inst,
                             uint64_t sample_count, bool stream) :
	QDialog(parent),
    _dev_inst(dev_inst),
    _sample_count(sample_count),
    _layout(this),
    _stream(stream),
    _button_box(QDialogButtonBox::Ok,
        Qt::Horizontal, this)
{
    setWindowTitle(tr("Stream Mode Options"));
	setLayout(&_layout);

    QLabel *_info = new QLabel(this);
    if (_stream)
        _info->setText(tr("Stream Mode Active!"));
    else
        _info->setText(tr("Buffer Mode Active!"));

    _layout.addWidget(_info);

    if (_stream) {
        _op0 = new QRadioButton(this);
        _op1 = new QRadioButton(this);
        _op0->setText(tr("16 Channels, Max 10MHz sample rate"));
        _op1->setText(tr(" 8 Channels, Max 25MHz sample rate"));
        _layout.addWidget(_op0);
        _layout.addWidget(_op1);

        if (_sample_count >= SR_GB(1)) {
            _op0->setDisabled(true);
            _op1->setChecked(true);
        }else{
            _op0->setChecked(true);
        }
    }

    _layout.addWidget(&_button_box);

    connect(&_button_box, SIGNAL(accepted()), this, SLOT(accept()));
    connect(&_button_box, SIGNAL(rejected()), this, SLOT(accept()));
}

void StreamOptions::accept()
{
	using namespace Qt;

    uint64_t sample_rate = _dev_inst->get_sample_rate();
    if (_stream) {
        if (_op0->isChecked())
            sample_rate = (sample_rate <= SR_MHZ(10)) ? sample_rate : SR_MHZ(10);
        else if (_op1->isChecked())
            sample_rate = (sample_rate > SR_MHZ(10) && sample_rate <= SR_MHZ(25)) ? sample_rate : SR_MHZ(25);
    }
    _dev_inst->set_config(NULL, NULL,
                          SR_CONF_SAMPLERATE,
                          g_variant_new_uint64(sample_rate));

	QDialog::accept();
}

void StreamOptions::reject()
{
    accept();
}

} // namespace dialogs
} // namespace pv
