/*
 * This file is part of the DSLogic-gui project.
 * DSLogic-gui is based on PulseView.
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
#include "ui_search.h"
#include <assert.h>

namespace pv {
namespace dialogs {

Search::Search(QWidget *parent, struct sr_dev_inst *sdi, QString pattern) :
    QDialog(parent),
    ui(new Ui::Search),
    _sdi(sdi)
{
    assert(_sdi);
    ui->setupUi(this);

    QRegExp value_rx("[10XRFCxrfc ]+");
    QValidator *value_validator = new QRegExpValidator(value_rx, this);

    //ui->_value_lineEdit->setText("X X X X X X X X X X X X X X X X");
    ui->_value_lineEdit->setText(pattern);
    ui->_value_lineEdit->setValidator(value_validator);
    ui->_value_lineEdit->setMaxLength(16 * 2 - 1);
    ui->_value_lineEdit->setInputMask("X X X X X X X X X X X X X X X X");
}

Search::~Search()
{
    delete ui;
}

void Search::accept()
{
    using namespace Qt;

    QDialog::accept();
}

QString Search::get_pattern()
{
    QString pattern = ui->_value_lineEdit->text();
    //pattern.remove(QChar('/r'), Qt::CaseInsensitive);
    return pattern;
}

} // namespace decoder
} // namespace pv
