/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2022 DreamSourceLab <support@dreamsourcelab.com>
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

#include "decoderoptionsdlg.h"
#include "libsigrokdecode.h"
#include <QScrollArea> 
#include <QDialogButtonBox>
#include <assert.h>
#include <QVBoxLayout>
#include <QLabel> 
#include <QGridLayout>
#include <QFormLayout>
#include <QScrollArea> 
#include <QVariant>
#include <QGuiApplication>
#include <QScreen>

#include "../data/decoderstack.h"
#include "../prop/binding/decoderoptions.h"
#include "../data/decode/decoder.h"
#include "../ui/dscombobox.h"
#include "../view/logicsignal.h"
#include "../appcontrol.h"
#include "../sigsession.h"
#include "../view/view.h"
#include "../view/cursor.h"
#include "../widgets/decodergroupbox.h"
#include "../view/decodetrace.h"
#include "../ui/msgbox.h"

#include <QDebug>

namespace pv {
namespace dialogs {


DecoderOptionsDlg::DecoderOptionsDlg(QWidget *parent)
:DSDialog(parent)
{
    _cursor1 = 0;
    _cursor2 = 0;
    _contentHeight = 0;
}

DecoderOptionsDlg::~DecoderOptionsDlg()
{
    for(auto p : _bindings){
        delete p;
    }
    _bindings.clear();
}

void DecoderOptionsDlg::load_options(view::DecodeTrace *trace, bool isNew)
{  
    assert(trace);

    _trace = trace;
    DSDialog *dlg = this;   

    QFormLayout *form = new QFormLayout();
    form->setContentsMargins(0, 0, 0, 0);
    form->setVerticalSpacing(5);
    form->setFormAlignment(Qt::AlignLeft);
    form->setLabelAlignment(Qt::AlignLeft);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow); 
    dlg->layout()->addLayout(form);

    dlg->setTitle(tr("Decoder Options"));   
    
    //scroll pannel
    QWidget *scroll_pannel  = new QWidget();
    QVBoxLayout *scroll_lay = new QVBoxLayout();
    scroll_lay->setContentsMargins(0, 0, 0, 0);
    scroll_lay->setAlignment(Qt::AlignLeft);
    scroll_pannel->setLayout(scroll_lay);
    form->addRow(scroll_pannel);

    // decoder options 
    QWidget *container_panel = new QWidget();      
    QVBoxLayout *decoder_lay = new QVBoxLayout();
    decoder_lay->setContentsMargins(0, 0, 0, 0);
    decoder_lay->setDirection(QBoxLayout::TopToBottom);
    container_panel->setLayout(decoder_lay);
   // form->addRow(container_panel); 
    scroll_lay->addWidget(container_panel);
  
    load_decoder_forms(container_panel);
 
    if (_trace->decoder()->stack().size() > 0){
       // form->addRow(new QLabel(tr("<i>* Required channels</i>"), dlg));
    } 
 
    //Add region combobox
    _start_comboBox = new DsComboBox(dlg);
    _end_comboBox = new DsComboBox(dlg);
    _start_comboBox->addItem("Start");
    _end_comboBox->addItem("End"); 
    _start_comboBox->setMinimumContentsLength(7);
    _end_comboBox->setMinimumContentsLength(7);

    // Add cursor list
    auto view = _trace->get_view();
    int dex1 = 0;
    int dex2 = 0;

    if (view)
    {  
        int num = 1;
        for (auto c : view->get_cursorList()){
            QString curCursor = tr("Cursor ") + QString::number(num);
            _start_comboBox->addItem(curCursor, QVariant((quint64)c->get_key()));
            _end_comboBox->addItem(curCursor, QVariant((quint64)c->get_key()));

            if (c->get_key() == _cursor1)
                dex1 = num;
            if (c->get_key() == _cursor2) 
                dex2 = num; 

            num++;
        }
    }

    if (dex1 == 0)
        _cursor1 = 0;
    if (dex2 == 0)
        _cursor2 = 0;

    _start_comboBox->setCurrentIndex(dex1);
    _end_comboBox->setCurrentIndex(dex2);
 
    update_decode_range(); // set default sample range
  
    form->addRow(_start_comboBox, new QLabel(
                     tr("The cursor for decode start time")));
    form->addRow(_end_comboBox, new QLabel(
                     tr("The cursor for decode end time")));

    //space 
    QWidget *space = new QWidget();
    space->setFixedHeight(5);
    form->addRow(space);
 
    // Add ButtonBox (OK/Cancel)
    QDialogButtonBox *button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                Qt::Horizontal, dlg);

    QHBoxLayout *confirm_button_box = new QHBoxLayout;
    confirm_button_box->addWidget(button_box, 0, Qt::AlignRight);
    form->addRow(confirm_button_box);

     // scroll     
    QSize tsize = dlg->sizeHint();
    int w = tsize.width(); 
    int other_height = 190; 
    _contentHeight += 5;

#ifdef Q_OS_DARWIN
        other_height += 40;
#endif

    int dlgHeight = _contentHeight + other_height; 
     
    float sk = QGuiApplication::primaryScreen()->logicalDotsPerInch() / 96;
    int srcHeight = 600;
    container_panel->setFixedHeight(_contentHeight);

    if (dlgHeight * sk > srcHeight)
    { 
        QScrollArea *scroll = new QScrollArea(scroll_pannel);
        scroll->setWidget(container_panel);
        scroll->setStyleSheet("QScrollArea{border:none;}");
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        dlg->setFixedSize(w + 20, srcHeight);
        scroll_pannel->setFixedSize(w, srcHeight - other_height);
        int sclw = w - 18;
#ifdef Q_OS_DARWIN
        sclw -= 20;
#endif
        scroll->setFixedSize(sclw, srcHeight - other_height);
    }
    else{
        dlg->setFixedSize(w + 20,dlgHeight);
    }
 
    connect(_start_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(on_region_set(int)));
    connect(_end_comboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(on_region_set(int))); 
    connect(button_box, SIGNAL(accepted()), dlg, SLOT(on_accept()));
    connect(button_box, SIGNAL(rejected()), dlg, SLOT(reject()));
}

void DecoderOptionsDlg::load_decoder_forms(QWidget *container)
{
	using pv::data::decode::Decoder; 
	assert(container); 

    int dex = 0;
 
    for(auto dec : _trace->decoder()->stack()) 
    { 
        ++dex;
        QWidget *panel = new QWidget(container);
        QFormLayout *form = new QFormLayout();
        form->setContentsMargins(0,0,0,0);
        panel->setLayout(form);
        container->layout()->addWidget(panel);
       
        create_decoder_form(dec, panel, form); 

        _contentHeight += panel->sizeHint().height();
	} 
}
 

DsComboBox* DecoderOptionsDlg::create_probe_selector(
    QWidget *parent, const data::decode::Decoder *dec,
	const srd_channel *const pdch)
{
	assert(dec);
    
    const auto &sigs = AppControl::Instance()->GetSession()->get_signals();

    data::decode::Decoder *_dec = const_cast<data::decode::Decoder*>(dec);
    auto probe_iter = _dec->channels().find(pdch);
	DsComboBox *selector = new DsComboBox(parent);
    selector->addItem("-", QVariant::fromValue(-1));

	if (probe_iter == _dec->channels().end())
		selector->setCurrentIndex(0);

	for(size_t i = 0; i < sigs.size(); i++) {
        const auto s = sigs[i];
		assert(s);

        if (dynamic_cast<view::LogicSignal*>(s) && s->enabled())
		{
			selector->addItem(s->get_name(),
                QVariant::fromValue(s->get_index()));
            if (probe_iter != _dec->channels().end()) {
                if ((*probe_iter).second == s->get_index())
                    selector->setCurrentIndex(i + 1);
            }
		}
	}

	return selector;
}

void DecoderOptionsDlg::on_region_set(int index)
{
    (void)index;
    update_decode_range();
}

void DecoderOptionsDlg::update_decode_range()
{ 
    const uint64_t last_samples = AppControl::Instance()->GetSession()->cur_samplelimits() - 1;
    const int index1 = _start_comboBox->currentIndex();
    const int index2 = _end_comboBox->currentIndex();
    uint64_t decode_start, decode_end;

    auto view = _trace->get_view();

    if (index1 == 0) {
        decode_start = 0;
        _cursor1 = 0;

    } else {
        _cursor1 = _start_comboBox->itemData(index1).toULongLong();
        int cusrsor_index = view->get_cursor_index_by_key(_cursor1);
        if (cusrsor_index != -1){
            decode_start = view->get_cursor_samples(cusrsor_index);
        }
        else{
            decode_start = 0;
            _cursor1 = 0;
        }        
    }

    if (index2 == 0) {
        decode_end = last_samples;
        _cursor2 = 0;

    } else {
        _cursor2 = _end_comboBox->itemData(index2).toULongLong();
        int cusrsor_index = view->get_cursor_index_by_key(_cursor2);
        if (cusrsor_index != -1){
            decode_end = view->get_cursor_samples(cusrsor_index);
        }
        else{
            decode_end = last_samples;
            _cursor2 = 0;
        }       
    }

    if (decode_start > last_samples)
        decode_start = 0;
    if (decode_end > last_samples)
        decode_end = last_samples;

    if (decode_start > decode_end) {
        uint64_t tmp = decode_start;
        decode_start = decode_end;
        decode_end = tmp;
    }
  
    for(auto &dec : _trace->decoder()->stack()) {
        dec->set_decode_region(decode_start, decode_end);
    }
}
 

void DecoderOptionsDlg::create_decoder_form(
    data::decode::Decoder *dec, QWidget *parent,
    QFormLayout *form)
{
	const GSList *l;

    assert(dec);     
	const srd_decoder *const decoder = dec->decoder();
	assert(decoder);

    QFormLayout *const decoder_form = new QFormLayout();
    decoder_form->setContentsMargins(0,0,0,0);
    decoder_form->setVerticalSpacing(4);
    decoder_form->setFormAlignment(Qt::AlignLeft);
    decoder_form->setLabelAlignment(Qt::AlignLeft);
    decoder_form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
 
	// Add the mandatory channels
	for(l = decoder->channels; l; l = l->next) {
		const struct srd_channel *const pdch =
			(struct srd_channel *)l->data;
		DsComboBox *const combo = create_probe_selector(parent, dec, pdch);

		decoder_form->addRow(tr("<b>%1</b> (%2) *")
			.arg(QString::fromUtf8(pdch->name))
			.arg(QString::fromUtf8(pdch->desc)), combo);

        const ProbeSelector s = {combo, dec, pdch};
    	_probe_selectors.push_back(s);

         connect(combo, SIGNAL(currentIndexChanged(int)), this, SLOT(on_probe_selected(int)));
	} 

	// Add the optional channels
	for(l = decoder->opt_channels; l; l = l->next) {
		const struct srd_channel *const pdch =
			(struct srd_channel *)l->data;
		DsComboBox *const combo = create_probe_selector(parent, dec, pdch);
		
		decoder_form->addRow(tr("<b>%1</b> (%2)")
			.arg(QString::fromUtf8(pdch->name))
			.arg(QString::fromUtf8(pdch->desc)), combo);

        const ProbeSelector s = {combo, dec, pdch};
        _probe_selectors.push_back(s);

        connect(combo, SIGNAL(currentIndexChanged(int)), this, SLOT(on_probe_selected(int)));
	}

	// Add the options
    auto binding = new prop::binding::DecoderOptions(_trace->decoder(), dec);
    binding->add_properties_to_form(decoder_form, true);
	_bindings.push_back(binding);
  
    auto group = new pv::widgets::DecoderGroupBox(_trace->decoder(), 
                            dec, 
                            decoder_form, 
                            parent); 
	form->addRow(group); 
}

void DecoderOptionsDlg::commit_probes()
{ 
    for(auto &dec : _trace->decoder()->stack()){
		commit_decoder_probes(dec);
    }
}

void DecoderOptionsDlg::on_probe_selected(int)
{
	commit_probes();
}

void DecoderOptionsDlg::commit_decoder_probes(data::decode::Decoder *dec)
{
	assert(dec); 

    std::map<const srd_channel*, int> probe_map;
    const auto &sigs = AppControl::Instance()->GetSession()->get_signals();

    auto index_list = _trace->get_sig_index_list();
    index_list->clear();

	for(auto &s : _probe_selectors)
	{
		if(s._decoder != dec)
			break;

        const int selection = s._combo->itemData(
                s._combo->currentIndex()).value<int>();

        for(auto &sig : sigs)
            if(sig->get_index() == selection) {
                probe_map[s._pdch] = selection;
                index_list->push_back(selection);
				break;
			}
	}

	dec->set_probes(probe_map);
}
 
void DecoderOptionsDlg::on_accept()
{ 
    if (_cursor1 > 0 && _cursor1 == _cursor2){
        MsgBox::Show("error", "Invalid cursor index for sample range!");
        return;
    }

    this->accept();
} 

}//dialogs
}//pv
