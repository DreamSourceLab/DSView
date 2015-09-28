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


#include "search.h"
#include <assert.h>
#include <QRegExpValidator>

namespace pv {
namespace dialogs {

Search::Search(QWidget *parent, struct sr_dev_inst *sdi, QString pattern) :
    QDialog(parent),
    _sdi(sdi)
{
    assert(_sdi);

    QFont font("Monaco");
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);

    QRegExp value_rx("[10XRFCxrfc ]+");
    QValidator *value_validator = new QRegExpValidator(value_rx, this);


    search_lineEdit.setText(pattern);
    search_lineEdit.setValidator(value_validator);
    search_lineEdit.setMaxLength(16 * 2 - 1);
    search_lineEdit.setInputMask("X X X X X X X X X X X X X X X X");
    search_lineEdit.setFont(font);

    QLabel *search_label = new QLabel("1 1 1 1 1 1\n5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0");
    search_label->setFont(font);

    search_buttonBox.addButton(QDialogButtonBox::Ok);
    search_buttonBox.addButton(QDialogButtonBox::Cancel);

    QGridLayout *search_layout = new QGridLayout();
    search_layout->addWidget(search_label, 0, 1);
    search_layout->addWidget(new QLabel(tr("Search Value: ")), 1,0, Qt::AlignRight);
    search_layout->addWidget(&search_lineEdit, 1, 1);
    search_layout->addWidget(new QLabel(" "), 2,0);
    search_layout->addWidget(new QLabel(tr("X: Don't care\n0: Low level\n1: High level\nR: Rising edge\nF: Falling edge\nC: Rising/Falling edge")), 3, 0);
    search_layout->addWidget(&search_buttonBox, 4, 2);
    //search_layout->addStretch(1);

    setLayout(search_layout);

    connect(&search_buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(&search_buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
}

Search::~Search()
{
}

void Search::accept()
{
    using namespace Qt;

    QDialog::accept();
}

QString Search::get_pattern()
{
    QString pattern = search_lineEdit.text();
    //pattern.remove(QChar('/r'), Qt::CaseInsensitive);
    return pattern;
}

} // namespace decoder
} // namespace pv
