/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2015 DreamSourceLab <support@dreamsourcelab.com>
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

#include "lissajousoptions.h"
#include "../device/devinst.h"
#include "../sigsession.h"
#include "../view/view.h"
#include "../view/lissajoustrace.h"

#include <QCheckBox>
#include <QVariant>
#include <QLabel>
#include <QTabBar>
#include <QBitmap>

#include <boost/foreach.hpp>

using namespace boost;
using namespace std;
using namespace pv::view;

namespace pv {
namespace dialogs {

LissajousOptions::LissajousOptions(SigSession &session, QWidget *parent) :
    DSDialog(parent),
    _session(session),
    _button_box(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal, this)
{
    setMinimumSize(300, 300);

    _enable = new QCheckBox(this);

    QLabel *lisa_label = new QLabel(this);
    lisa_label->setPixmap(QPixmap(":/icons/lissajous.svg"));

    _percent = new QSlider(Qt::Horizontal, this);
    _percent->setRange(100, 100);
    _percent->setEnabled(false);
    if (_session.cur_samplelimits() > WellLen) {
        int min = ceil(WellLen*100.0/_session.cur_samplelimits());
        _percent->setEnabled(true);
        _percent->setRange(min, 100);
        _percent->setValue(min);
    }

    _x_group = new QGroupBox(this);
    _y_group = new QGroupBox(this);
    QHBoxLayout *xlayout = new QHBoxLayout();
    QHBoxLayout *ylayout = new QHBoxLayout();
    BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _session.get_signals()) {
        boost::shared_ptr<view::DsoSignal> dsoSig;
        if ((dsoSig = dynamic_pointer_cast<view::DsoSignal>(s))) {
            QString index_str = QString::number(dsoSig->get_index());
            QRadioButton *xradio = new QRadioButton(index_str, _x_group);
            xradio->setProperty("index", dsoSig->get_index());
            xlayout->addWidget(xradio);
            QRadioButton *yradio = new QRadioButton(index_str, _y_group);
            yradio->setProperty("index", dsoSig->get_index());
            ylayout->addWidget(yradio);
            _x_radio.append(xradio);
            _y_radio.append(yradio);
        }
    }
    _x_group->setLayout(xlayout);
    _y_group->setLayout(ylayout);


    boost::shared_ptr<LissajousTrace> lissajous = _session.get_lissajous_trace();
    if (lissajous) {
        _enable->setChecked(lissajous->enabled());
        _percent->setValue(lissajous->percent());
        for (QVector<QRadioButton *>::const_iterator i = _x_radio.begin();
            i != _x_radio.end(); i++) {
            if ((*i)->property("index").toInt() == lissajous->xIndex()) {
               (*i)->setChecked(true);
                break;
            }
        }
        for (QVector<QRadioButton *>::const_iterator i = _y_radio.begin();
            i != _y_radio.end(); i++) {
            if ((*i)->property("index").toInt() == lissajous->yIndex()) {
               (*i)->setChecked(true);
                break;
            }
        }
    } else {
        _enable->setChecked(false);
        for (QVector<QRadioButton *>::const_iterator i = _x_radio.begin();
            i != _x_radio.end(); i++) {
           (*i)->setChecked(true);
            break;
        }
        for (QVector<QRadioButton *>::const_iterator i = _y_radio.begin();
            i != _y_radio.end(); i++) {
           (*i)->setChecked(true);
            break;
        }
    }

    _layout = new QGridLayout();
    _layout->setMargin(0);
    _layout->setSpacing(0);
    _layout->addWidget(lisa_label, 0, 0, 1, 2, Qt::AlignCenter);
    _layout->addWidget(_enable, 1, 0, 1, 1);
    _layout->addWidget(_percent, 2, 0, 1, 2);
    _layout->addWidget(_x_group, 3, 0, 1, 1);
    _layout->addWidget(_y_group, 3, 1, 1, 1);
    _layout->addWidget(new QLabel(this), 4, 1, 1, 1);
    _layout->addWidget(&_button_box, 5, 1, 1, 1, Qt::AlignHCenter | Qt::AlignBottom);

    layout()->addLayout(_layout);

    connect(&_button_box, SIGNAL(rejected()), this, SLOT(reject()));
    connect(&_button_box, SIGNAL(accepted()), this, SLOT(accept()));

    retranslateUi();
}

void LissajousOptions::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    DSDialog::changeEvent(event);
}

void LissajousOptions::retranslateUi()
{
    _enable->setText(tr("Enable"));
    _x_group->setTitle(tr("X-axis"));
    _y_group->setTitle(tr("Y-axis"));
    setTitle(tr("Lissajous Options"));
}

void LissajousOptions::accept()
{
	using namespace Qt;
    QDialog::accept();

    int xindex = -1;
    int yindex = -1;
    for (QVector<QRadioButton *>::const_iterator i = _x_radio.begin();
        i != _x_radio.end(); i++) {
        if ((*i)->isChecked()) {
            xindex = (*i)->property("index").toInt();
            break;
        }
    }
    for (QVector<QRadioButton *>::const_iterator i = _y_radio.begin();
        i != _y_radio.end(); i++) {
        if ((*i)->isChecked()) {
            yindex = (*i)->property("index").toInt();
            break;
        }
    }
    bool enable = (xindex != -1 && yindex != -1 && _enable->isChecked());
    _session.lissajous_rebuild(enable, xindex, yindex, _percent->value());

    BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _session.get_signals()) {
        boost::shared_ptr<view::DsoSignal> dsoSig;
        if ((dsoSig = dynamic_pointer_cast<view::DsoSignal>(s))) {
            dsoSig->set_show(!enable);
        }
    }
    boost::shared_ptr<view::MathTrace> mathTrace = _session.get_math_trace();
    if (mathTrace && mathTrace->enabled()) {
        mathTrace->set_show(!enable);
    }
}

void LissajousOptions::reject()
{
    using namespace Qt;
    QDialog::reject();
}

} // namespace dialogs
} // namespace pv
