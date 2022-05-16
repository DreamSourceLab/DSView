/*
 * This file is part of the PulseView project.
 * DSView is based on PulseView.
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
#include "libsigrokdecode.h"

#include "decodergroupbox.h"
#include "../data/decoderstack.h"
#include "../data/decode/decoder.h"
#include "../data/decode/row.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVariant>
//#include <QScrollBar>
//#include <QScreen>
#include <QApplication>
 
#include "../config/appconfig.h"

#include <assert.h>

namespace pv {
namespace widgets {

DecoderGroupBox::DecoderGroupBox(data::DecoderStack *decoder_stack,
                                 data::decode::Decoder *dec,
                                 QLayout *dec_layout,
                                 QWidget *parent) :
    QWidget(parent)
{
    _dec = dec;
    _decoder_stack = decoder_stack;
    _widget = new QWidget(this);
    _layout = new QGridLayout(_widget);
    _layout->setContentsMargins(0, 0, 0, 0);
    _layout->setVerticalSpacing(2);

  //setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  //setWidgetResizable(true);

    QString iconPath = GetIconPath();
    _layout->addWidget(new QLabel(QString("<h3 style='font-style:italic'>%1</h3>").arg(_dec->decoder()->name), _widget),
   //_layout->addWidget(new QLabel(QString("<span style='text-decoration:underline'>%1</span>").arg(_dec->decoder()->name), _widget),
        0, 0);
	_layout->setColumnStretch(0, 1);

    const srd_decoder *const d = _dec->decoder();
    assert(d);
    const bool have_probes = (d->channels || d->opt_channels) != 0;
    if (!have_probes) {
       // _del_button = new QPushButton(QIcon(iconPath+"/del.svg"), QString(), _widget);
       // _layout->addWidget(_del_button, 0, 1);
        //connect(_del_button, SIGNAL(clicked()), this, SLOT(on_del_stack()));
    }

    _index = 0;
    for(auto &dec : _decoder_stack->stack()) {
        if (dec == _dec)
            break;
        _index++;
    }
    _show_button = new QPushButton(QIcon(_dec->shown() ?
                                             iconPath+"/shown.svg" :
                                             iconPath+"/hidden.svg"), QString(), _widget);
    _show_button->setProperty("index", -1);

    connect(_show_button, SIGNAL(clicked()), this, SLOT(tog_icon()));

    _layout->addWidget(_show_button, 0, 2);


    // add row show/hide
    int index = 0;
    auto rows = _decoder_stack->get_rows_gshow();

    for (auto i = rows.begin();
        i != rows.end(); i++) {
        if ((*i).first.decoder() == _dec->decoder()) {
            QPushButton *show_button = new QPushButton(QIcon((*i).second ?
                                                                 iconPath+"/shown.svg" :
                                                                 iconPath+"/hidden.svg"), QString(), _widget);
            show_button->setProperty("index", index);
            connect(show_button, SIGNAL(clicked()), this, SLOT(tog_icon()));

            _row_show_button.push_back(show_button);
            _layout->addWidget(new QLabel((*i).first.title(), _widget), _row_show_button.size(), 0);
            _layout->addWidget(show_button, _row_show_button.size(), 2);
        }
        index++;
    }

    _layout->addLayout(dec_layout, _row_show_button.size()+1, 0, 1, 3);

    _widget->setLayout(_layout);
    parent->layout()->addWidget(_widget);
   // setWidget(_widget);
   // _widget->installEventFilter(this);
}

DecoderGroupBox::~DecoderGroupBox()
{
}

bool DecoderGroupBox::eventFilter(QObject *o, QEvent *e)
{
    /*
    if(o == _widget && e->type() == QEvent::Resize) {
        setMinimumWidth(_widget->minimumSizeHint().width() + verticalScrollBar()->width());
        QScreen *screen=QGuiApplication::primaryScreen ();
        QRect mm=screen->availableGeometry() ;
        if (_widget->minimumSizeHint().height() < mm.height()/2)
            setMinimumHeight(_widget->minimumSizeHint().height());
    }
    */

    return false;
}

void DecoderGroupBox::tog_icon()
{
    QString iconPath = GetIconPath();
    QPushButton *sc = dynamic_cast<QPushButton*>(sender());
    int index = sc->property("index").toInt();

    if (index == -1) {
        int i = _index;

       for(auto dec : _decoder_stack->stack()) {
            if (i-- == 0) {
                dec->show(!dec->shown());
                sc->setIcon(QIcon(dec->shown() ? iconPath+"/shown.svg" :
                                                 iconPath+"/hidden.svg"));
                break;
            }
        }
    } else {
        auto rows = _decoder_stack->get_rows_gshow();

        for (auto i = rows.begin(); i != rows.end(); i++) {
            if (index-- == 0) {
                _decoder_stack->set_rows_gshow((*i).first, !(*i).second);
                //rows[(*i).first] = !(*i).second;
                sc->setIcon(QIcon(rows[(*i).first] ? iconPath+"/hidden.svg" :
                                                    iconPath+"/shown.svg"));
                break;
            }
        }
    }
}

void DecoderGroupBox::on_del_stack()
{
    int i = _index;
    for(auto &dec : _decoder_stack->stack()) {
        if (i-- == 0) {
            del_stack(dec);
            break;
        }
    }
}

} // widgets
} // pv
