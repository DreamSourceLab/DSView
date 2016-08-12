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


#include "protocollist.h"

#include <boost/foreach.hpp>

#include <QFormLayout>
#include <QListWidget>

#include "../sigsession.h"
#include "../data/decoderstack.h"
#include "../data/decode/row.h"
#include "../view/decodetrace.h"
#include "../data/decodermodel.h"

using namespace boost;
using namespace std;

namespace pv {
namespace dialogs {

ProtocolList::ProtocolList(QWidget *parent, SigSession &session) :
    DSDialog(parent),
    _session(session),
    _button_box(QDialogButtonBox::Ok,
        Qt::Horizontal, this)
{
    pv::data::DecoderModel* decoder_model = _session.get_decoder_model();


    _protocol_combobox = new QComboBox(this);
    const std::vector< boost::shared_ptr<pv::view::DecodeTrace> > decode_sigs(
        _session.get_decode_signals());
    int index = 0;
    BOOST_FOREACH(boost::shared_ptr<pv::view::DecodeTrace> d, decode_sigs) {
        _protocol_combobox->addItem(d->get_name());
        if (decoder_model->getDecoderStack() == d->decoder())
            _protocol_combobox->setCurrentIndex(index);
        index++;
    }
    _protocol_combobox->addItem("", qVariantFromValue(NULL));
    if (decoder_model->getDecoderStack() == NULL)
        _protocol_combobox->setCurrentIndex(index);

    _flayout = new QFormLayout();
    _flayout->setVerticalSpacing(5);
    _flayout->setFormAlignment(Qt::AlignLeft);
    _flayout->setLabelAlignment(Qt::AlignLeft);
    _flayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    _flayout->addRow(new QLabel(tr("Decoded Protocols: "), this), _protocol_combobox);

    _layout = new QVBoxLayout();
    _layout->addLayout(_flayout);
    _layout->addWidget(&_button_box);

    setMinimumWidth(300);
    layout()->addLayout(_layout);
    setTitle(tr("Protocol List Viewer"));

    connect(&_button_box, SIGNAL(accepted()), this, SLOT(accept()));
    connect(_protocol_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(set_protocol(int)));
    set_protocol(_protocol_combobox->currentIndex());
    connect(_session.get_device().get(), SIGNAL(device_updated()), this, SLOT(reject()));

}

void ProtocolList::accept()
{
    using namespace Qt;

    QDialog::accept();
}

void ProtocolList::reject()
{
    using namespace Qt;

    QDialog::accept();
}

void ProtocolList::set_protocol(int index)
{
    (void)index;

    for(std::list<QCheckBox *>::const_iterator i = _show_checkbox_list.begin();
        i != _show_checkbox_list.end(); i++) {
        (*i)->setParent(NULL);
        _flayout->removeWidget((*i));
        delete (*i);
    }
    _show_checkbox_list.clear();
    for(std::list<QLabel *>::const_iterator i = _show_label_list.begin();
        i != _show_label_list.end(); i++) {
        (*i)->setParent(NULL);
        _flayout->removeWidget((*i));
        delete (*i);
    }
    _show_label_list.clear();

    boost::shared_ptr<pv::data::DecoderStack> decoder_stack;
    const std::vector< boost::shared_ptr<pv::view::DecodeTrace> > decode_sigs(
        _session.get_decode_signals());
    int cur_index = 0;
    BOOST_FOREACH(boost::shared_ptr<pv::view::DecodeTrace> d, decode_sigs) {
        if (index == cur_index) {
            decoder_stack = d->decoder();
            break;
        }
        cur_index++;
    }

    if (!decoder_stack){
        _session.get_decoder_model()->setDecoderStack(NULL);
        return;
    }

    _session.get_decoder_model()->setDecoderStack(decoder_stack);
    int row_index = 0;
    const std::map<const pv::data::decode::Row, bool> rows = decoder_stack->get_rows_lshow();
    for (std::map<const pv::data::decode::Row, bool>::const_iterator i = rows.begin();
        i != rows.end(); i++) {
        QLabel *row_label = new QLabel((*i).first.title(), this);
        QCheckBox *row_checkbox = new QCheckBox(this);
        //row_checkbox->setChecked(false);
        _show_label_list.push_back(row_label);
        _show_checkbox_list.push_back(row_checkbox);
        _flayout->addRow(row_label, row_checkbox);

        row_checkbox->setChecked((*i).second);
        connect(row_checkbox, SIGNAL(clicked(bool)), this, SLOT(on_row_check(bool)));
        row_checkbox->setProperty("index", row_index);
        row_index++;
    }
}

void ProtocolList::on_row_check(bool show)
{
    QCheckBox *sc = dynamic_cast<QCheckBox*>(sender());
    QVariant id = sc->property("index");
    int index = id.toInt();

    boost::shared_ptr<pv::data::DecoderStack> decoder_stack;
    const std::vector< boost::shared_ptr<pv::view::DecodeTrace> > decode_sigs(
        _session.get_decode_signals());
    int cur_index = 0;
    BOOST_FOREACH(boost::shared_ptr<pv::view::DecodeTrace> d, decode_sigs) {
        if (cur_index == _protocol_combobox->currentIndex()) {
            decoder_stack = d->decoder();
            break;
        }
        cur_index++;
    }

    if (!decoder_stack)
        return;

    std::map<const pv::data::decode::Row, bool> rows = decoder_stack->get_rows_lshow();
    for (std::map<const pv::data::decode::Row, bool>::const_iterator i = rows.begin();
        i != rows.end(); i++) {
        if (index-- == 0) {
            decoder_stack->set_rows_lshow((*i).first, show);
            break;
        }
    }

    _session.get_decoder_model()->setDecoderStack(decoder_stack);
}

} // namespace dialogs
} // namespace pv
