/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2013 Joel Holdsworth <joel@airwebreathe.org.uk>
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
extern "C" {
#include <libsigrokdecode/libsigrokdecode.h>
}

#include "decodergroupbox.h"
#include "../data/decoderstack.h"
#include "../data/decode/decoder.h"
#include "../data/decode/row.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVariant>

#include <boost/foreach.hpp>

#include <assert.h>

namespace pv {
namespace widgets {

DecoderGroupBox::DecoderGroupBox(boost::shared_ptr<data::DecoderStack> &decoder_stack,
                                 boost::shared_ptr<data::decode::Decoder> &dec,
                                 QWidget *parent) :
    QWidget(parent),
    _decoder_stack(decoder_stack),
    _dec(dec),
    _layout(new QGridLayout(this))
{
    _layout->setContentsMargins(0, 0, 0, 0);
    _layout->setVerticalSpacing(5);
    setLayout(_layout);

    _layout->addWidget(new QLabel(QString("<h3>%1</h3>").arg(_dec->decoder()->name), this),
        0, 0);
	_layout->setColumnStretch(0, 1);

    const srd_decoder *const d = _dec->decoder();
    assert(d);
    const bool have_probes = (d->channels || d->opt_channels) != 0;
    if (!have_probes) {
        _del_button = new QPushButton(QIcon(":/icons/del.png"), QString(), this);
        _layout->addWidget(_del_button, 0, 1);
        connect(_del_button, SIGNAL(clicked()), this, SLOT(on_del_stack()));
    }

    _index = 0;
    BOOST_FOREACH(boost::shared_ptr<data::decode::Decoder> dec,
        _decoder_stack->stack()) {
        if (dec == _dec)
            break;
        _index++;
    }
    _show_button = new QPushButton(QIcon(_dec->shown() ?
                                             ":/icons/shown.png" :
                                             ":/icons/hidden.png"), QString(), this);
    _show_button->setProperty("index", -1);
    connect(_show_button, SIGNAL(clicked()),
        this, SLOT(tog_icon()));
    _layout->addWidget(_show_button, 0, 2);


    // add row show/hide
    int index = 0;
    const std::map<const pv::data::decode::Row, bool> rows = _decoder_stack->get_rows_gshow();
    for (std::map<const pv::data::decode::Row, bool>::const_iterator i = rows.begin();
        i != rows.end(); i++) {
        if ((*i).first.decoder() == _dec->decoder()) {
            QPushButton *show_button = new QPushButton(QIcon((*i).second ?
                                                                 ":/icons/shown.png" :
                                                                 ":/icons/hidden.png"), QString(), this);
            show_button->setProperty("index", index);
            connect(show_button, SIGNAL(clicked()), this, SLOT(tog_icon()));
            _row_show_button.push_back(show_button);
            _layout->addWidget(new QLabel((*i).first.title(), this), _row_show_button.size(), 0);
            _layout->addWidget(show_button, _row_show_button.size(), 2);
        }
        index++;
    }
}

DecoderGroupBox::~DecoderGroupBox()
{
}

void DecoderGroupBox::add_layout(QLayout *layout)
{
	assert(layout);
    _layout->addLayout(layout, _row_show_button.size()+1, 0, 1, 3);
}

void DecoderGroupBox::tog_icon()
{
    QPushButton *sc = dynamic_cast<QPushButton*>(sender());
    QVariant id = sc->property("index");
    int index = id.toInt();
    if (index == -1) {
        int i = _index;
        BOOST_FOREACH(boost::shared_ptr<data::decode::Decoder> dec,
            _decoder_stack->stack()) {
            if (i-- == 0) {
                dec->show(!dec->shown());
                sc->setIcon(QIcon(dec->shown() ? ":/icons/shown.png" :
                                                 ":/icons/hidden.png"));
                break;
            }
        }
    } else {
        std::map<const pv::data::decode::Row, bool> rows = _decoder_stack->get_rows_gshow();
        for (std::map<const pv::data::decode::Row, bool>::const_iterator i = rows.begin();
            i != rows.end(); i++) {
            if (index-- == 0) {
                _decoder_stack->set_rows_gshow((*i).first, !(*i).second);
                //rows[(*i).first] = !(*i).second;
                sc->setIcon(QIcon(rows[(*i).first] ? ":/icons/hidden.png" :
                                                    ":/icons/shown.png"));
                break;
            }
        }
    }
}

void DecoderGroupBox::on_del_stack()
{
    int i = _index;
    BOOST_FOREACH(boost::shared_ptr<data::decode::Decoder> dec,
        _decoder_stack->stack()) {
        if (i-- == 0) {
            del_stack(dec);
            break;
        }
    }
}

} // widgets
} // pv
