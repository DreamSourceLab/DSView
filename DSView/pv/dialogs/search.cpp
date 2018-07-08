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

#include "search.h"
#include "../view/logicsignal.h"

#include <assert.h>
#include <QRegExpValidator>

#include <boost/foreach.hpp>

namespace pv {
namespace dialogs {

Search::Search(QWidget *parent, SigSession &session, std::map<uint16_t, QString> pattern) :
    DSDialog(parent),
    _session(session)
{

    QFont font("Monaco");
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    //this->setMinimumWidth(350);

    QRegExp value_rx("[10XRFCxrfc]+");
    QValidator *value_validator = new QRegExpValidator(value_rx, this);

    search_buttonBox.addButton(QDialogButtonBox::Ok);
    search_buttonBox.addButton(QDialogButtonBox::Cancel);

    QGridLayout *search_layout = new QGridLayout();
    search_layout->setVerticalSpacing(0);

    int index = 0;
    BOOST_FOREACH(const boost::shared_ptr<view::Signal> sig,
                  _session.get_signals()) {
        assert(sig);
        boost::shared_ptr<view::LogicSignal> logic_sig;
        if ((logic_sig = boost::dynamic_pointer_cast<view::LogicSignal>(sig))) {
            QLineEdit *search_lineEdit = new QLineEdit(this);
            if (pattern.find(logic_sig->get_index()) != pattern.end())
                search_lineEdit->setText(pattern[logic_sig->get_index()]);
            else
                search_lineEdit->setText("X");
            search_lineEdit->setValidator(value_validator);
            search_lineEdit->setMaxLength(1);
            search_lineEdit->setInputMask("X");
            search_lineEdit->setFont(font);
            _search_lineEdit_vec.push_back(search_lineEdit);

            search_layout->addWidget(new QLabel(logic_sig->get_name()+":"), index, 0, Qt::AlignRight);
            search_layout->addWidget(new QLabel(QString::number(logic_sig->get_index())), index, 1, Qt::AlignRight);
            search_layout->addWidget(search_lineEdit, index, 2);

            connect(search_lineEdit, SIGNAL(editingFinished()), this, SLOT(format()));

            index++;
        }
    }

    search_layout->addWidget(new QLabel(" "), index,0);
    search_layout->addWidget(new QLabel(tr("X: Don't care\n0: Low level\n1: High level\nR: Rising edge\nF: Falling edge\nC: Rising/Falling edge")), 0, 3, index, 1);
    search_layout->addWidget(&search_buttonBox, index+1, 3);
    search_layout->setColumnStretch(3, 100);

    layout()->addLayout(search_layout);
    setTitle(tr("Search Options"));

    connect(&search_buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(&search_buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    connect(_session.get_device().get(), SIGNAL(device_updated()), this, SLOT(reject()));
}

Search::~Search()
{
}

void Search::accept()
{
    using namespace Qt;

    QDialog::accept();
}

void Search::format()
{
    QLineEdit *sc = qobject_cast<QLineEdit *>(sender());
    sc->setText(sc->text().toUpper());
}

std::map<uint16_t, QString> Search::get_pattern()
{
    std::map<uint16_t, QString> pattern;

    int index = 0;
    BOOST_FOREACH(const boost::shared_ptr<view::Signal> sig,
                  _session.get_signals()) {
        assert(sig);
        boost::shared_ptr<view::LogicSignal> logic_sig;
        if ((logic_sig = boost::dynamic_pointer_cast<view::LogicSignal>(sig))) {
            pattern[logic_sig->get_index()] = _search_lineEdit_vec[index]->text();
            index++;
        }
    }

    return pattern;
}

} // namespace decoder
} // namespace pv
