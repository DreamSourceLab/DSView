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

#include "mathoptions.h"
#include "../device/devinst.h"
#include "../sigsession.h"
#include "../view/view.h"
#include "../view/mathtrace.h"
#include "../data/mathstack.h"

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

MathOptions::MathOptions(SigSession &session, QWidget *parent) :
    DSDialog(parent),
    _session(session),
    _button_box(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal, this)
{
    setMinimumSize(300, 300);

    _enable = new QCheckBox(this);

    QLabel *lisa_label = new QLabel(this);
    lisa_label->setPixmap(QPixmap(":/icons/math.svg"));

    _math_group = new QGroupBox(this);
    QHBoxLayout *type_layout = new QHBoxLayout();
    QRadioButton *add_radio = new QRadioButton(tr("Add"), _math_group);
    add_radio->setProperty("type", data::MathStack::MATH_ADD);
    type_layout->addWidget(add_radio);
    QRadioButton *sub_radio = new QRadioButton(tr("Substract"), _math_group);
    sub_radio->setProperty("type", data::MathStack::MATH_SUB);
    type_layout->addWidget(sub_radio);
    QRadioButton *mul_radio = new QRadioButton(tr("Multiply"), _math_group);
    mul_radio->setProperty("type", data::MathStack::MATH_MUL);
    type_layout->addWidget(mul_radio);
    QRadioButton *div_radio = new QRadioButton(tr("Divide"), _math_group);
    div_radio->setProperty("type", data::MathStack::MATH_DIV);
    type_layout->addWidget(div_radio);
    _math_radio.append(add_radio);
    _math_radio.append(sub_radio);
    _math_radio.append(mul_radio);
    _math_radio.append(div_radio);
    _math_group->setLayout(type_layout);

    _src1_group = new QGroupBox(this);
    _src2_group = new QGroupBox(this);
    QHBoxLayout *src1_layout = new QHBoxLayout();
    QHBoxLayout *src2_layout = new QHBoxLayout();
    BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _session.get_signals()) {
        boost::shared_ptr<view::DsoSignal> dsoSig;
        if ((dsoSig = dynamic_pointer_cast<view::DsoSignal>(s))) {
            QString index_str = QString::number(dsoSig->get_index());
            QRadioButton *xradio = new QRadioButton(index_str, _src1_group);
            xradio->setProperty("index", dsoSig->get_index());
            src1_layout->addWidget(xradio);
            QRadioButton *yradio = new QRadioButton(index_str, _src2_group);
            yradio->setProperty("index", dsoSig->get_index());
            src2_layout->addWidget(yradio);
            _src1_radio.append(xradio);
            _src2_radio.append(yradio);
        }
    }
    _src1_group->setLayout(src1_layout);
    _src2_group->setLayout(src2_layout);


    boost::shared_ptr<MathTrace> math = _session.get_math_trace();
    if (math) {
        _enable->setChecked(math->enabled());
        for (QVector<QRadioButton *>::const_iterator i = _src1_radio.begin();
            i != _src1_radio.end(); i++) {
            if ((*i)->property("index").toInt() == math->src1()) {
               (*i)->setChecked(true);
                break;
            }
        }
        for (QVector<QRadioButton *>::const_iterator i = _src2_radio.begin();
            i != _src2_radio.end(); i++) {
            if ((*i)->property("index").toInt() == math->src2()) {
               (*i)->setChecked(true);
                break;
            }
        }
        for (QVector<QRadioButton *>::const_iterator i = _math_radio.begin();
            i != _math_radio.end(); i++) {
            if ((*i)->property("type").toInt() == math->get_math_stack()->get_type()) {
                (*i)->setChecked(true);
                break;
            }
        }
    } else {
        _enable->setChecked(false);
        for (QVector<QRadioButton *>::const_iterator i = _src1_radio.begin();
            i != _src1_radio.end(); i++) {
           (*i)->setChecked(true);
            break;
        }
        for (QVector<QRadioButton *>::const_iterator i = _src2_radio.begin();
            i != _src2_radio.end(); i++) {
           (*i)->setChecked(true);
            break;
        }
        for (QVector<QRadioButton *>::const_iterator i = _math_radio.begin();
            i != _math_radio.end(); i++) {
            (*i)->setChecked(true);
            break;
        }
    }

    _layout = new QGridLayout();
    _layout->setMargin(0);
    _layout->setSpacing(0);
    _layout->addWidget(lisa_label, 0, 0, 1, 2, Qt::AlignCenter);
    _layout->addWidget(_enable, 1, 0, 1, 1);
    _layout->addWidget(_math_group, 2, 0, 1, 2);
    _layout->addWidget(_src1_group, 3, 0, 1, 1);
    _layout->addWidget(_src2_group, 3, 1, 1, 1);
    _layout->addWidget(new QLabel(this), 4, 1, 1, 1);
    _layout->addWidget(&_button_box, 5, 1, 1, 1, Qt::AlignHCenter | Qt::AlignBottom);

    layout()->addLayout(_layout);

    connect(&_button_box, SIGNAL(rejected()), this, SLOT(reject()));
    connect(&_button_box, SIGNAL(accepted()), this, SLOT(accept()));

    retranslateUi();
}

void MathOptions::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    DSDialog::changeEvent(event);
}

void MathOptions::retranslateUi()
{
    _enable->setText(tr("Enable"));
    _math_group->setTitle(tr("Math Type"));
    _src1_group->setTitle(tr("1st Source"));
    _src2_group->setTitle(tr("2nd Source"));
    setTitle(tr("Math Options"));
}

void MathOptions::accept()
{
	using namespace Qt;
    QDialog::accept();

    int src1 = -1;
    int src2 = -1;
    data::MathStack::MathType type = data::MathStack::MATH_ADD;
    for (QVector<QRadioButton *>::const_iterator i = _src1_radio.begin();
        i != _src1_radio.end(); i++) {
        if ((*i)->isChecked()) {
            src1 = (*i)->property("index").toInt();
            break;
        }
    }
    for (QVector<QRadioButton *>::const_iterator i = _src2_radio.begin();
        i != _src2_radio.end(); i++) {
        if ((*i)->isChecked()) {
            src2 = (*i)->property("index").toInt();
            break;
        }
    }
    for (QVector<QRadioButton *>::const_iterator i = _math_radio.begin();
        i != _math_radio.end(); i++) {
        if ((*i)->isChecked()) {
            type = (data::MathStack::MathType)(*i)->property("type").toInt();
            break;
        }
    }
    bool enable = (src1 != -1 && src2 != -1 && _enable->isChecked());
    boost::shared_ptr<view::DsoSignal> dsoSig1;
    boost::shared_ptr<view::DsoSignal> dsoSig2;
    BOOST_FOREACH(const boost::shared_ptr<view::Signal> s, _session.get_signals()) {
        boost::shared_ptr<view::DsoSignal> dsoSig;
        if ((dsoSig = dynamic_pointer_cast<view::DsoSignal>(s))) {
            if (dsoSig->get_index() == src1)
                dsoSig1 = dsoSig;
            if (dsoSig->get_index() == src2)
                dsoSig2 = dsoSig;
        }
    }
    _session.math_rebuild(enable, dsoSig1, dsoSig2, type);
}

void MathOptions::reject()
{
    using namespace Qt;
    QDialog::reject();
}

} // namespace dialogs
} // namespace pv
