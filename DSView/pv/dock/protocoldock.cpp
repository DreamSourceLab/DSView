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

#include "protocoldock.h"
#include "../sigsession.h"
#include "../view/decodetrace.h"
#include "../device/devinst.h"
#include "../data/decodermodel.h"
#include "../data/decoderstack.h"
#include "../dialogs/protocollist.h"
#include "../dialogs/protocolexp.h" 
#include "../view/view.h"

#include <QObject>
#include <QHBoxLayout>
#include <QPainter>
#include <QFormLayout>
#include <QStandardItemModel>
#include <QTableView>
#include <QHeaderView>
#include <QScrollBar>
#include <QRegularExpression>
#include <QFuture>
#include <QProgressDialog>
#include <QtConcurrent/QtConcurrent>
#include <QSizePolicy>
#include <assert.h>
#include <map>
#include <string>
 
#include <algorithm>
#include "../ui/msgbox.h"
#include "../dsvdef.h"
#include "../config/appconfig.h"
#include "../data/decode/decoderstatus.h"
#include "../data/decode/decoder.h"

using namespace std;

 
namespace pv {
namespace dock {
   
ProtocolDock::ProtocolDock(QWidget *parent, view::View &view, SigSession *session) :
    QScrollArea(parent),
    _view(view)
{
    _session = session;
    _cur_search_index = -1;
    _search_edited = false;
    _searching = false;
    _add_silent = false;  
    _bSettingList = false;

    _up_widget = new QWidget(this);
    
    _add_button = new QPushButton(_up_widget);
    _add_button->setFlat(true);
    _del_all_button = new QPushButton(_up_widget);
    _del_all_button->setFlat(true);
    _del_all_button->setCheckable(true);

    _protocol_combobox = new DsComboBox(_up_widget);
    _protocol_combobox->setEditable(true);
    _protocol_combobox->setLineEdit(new KeywordLineEdit(_protocol_combobox));
    _protocol_combobox->setCompleter(NULL);

    //GSList *l = g_slist_sort(g_slist_copy((GSList*)srd_decoder_list()), decoder_name_cmp);

    GSList *l = const_cast<GSList*>(srd_decoder_list());

    std::map<std::string, int> pro_key_table;
    QString repeatNammes;

    for(; l; l = l->next)
    {
        const srd_decoder *const d = (srd_decoder*)l->data;
        assert(d);
        // const bool have_probes = (d->channels || d->opt_channels) != 0;
 
        if (true) {
            DecoderInfoItem *info = new DecoderInfoItem(); 
            srd_decoder *dec = (srd_decoder *)(l->data);
            info->ObjectHandle = dec;
            _decoderInfoList.push_back(info);  

            std::string prokey(dec->id);
            if (pro_key_table.find(prokey) != pro_key_table.end()){
                if (repeatNammes != "")
                        repeatNammes += ",";
                repeatNammes += QString(dec->id);
            }
            else{
                pro_key_table[prokey] = 1;
            }
        }
    }
    g_slist_free(l);
 
    //sort protocol list
    sort(_decoderInfoList.begin(), _decoderInfoList.end(), ProtocolDock::protocol_sort_callback);

    _bSettingList = true;
    int protocol_index = 0;
    for (auto info : _decoderInfoList){
         info->Index = protocol_index;
         protocol_index++;
         srd_decoder *dec = (srd_decoder *)(info->ObjectHandle);
         _protocol_combobox->addItem(QString::fromUtf8(dec->name), QVariant::fromValue(info->Index));
    }

    _protocol_combobox->setCurrentIndex(-1);
    _protocol_combobox->lineEdit()->setText(PROTOCOL_FIND_TITLE);
    _bSettingList = false;

    if (repeatNammes != ""){
        QString err = "Any protocol have repeated id or name: ";
        err += repeatNammes;
        MsgBox::Show("error", err.toUtf8().data());
    }

    _up_layout = new QVBoxLayout();

    QHBoxLayout *hori_layout = new QHBoxLayout();
    hori_layout->addWidget(_add_button);
    hori_layout->addWidget(_del_all_button);
    hori_layout->addWidget(_protocol_combobox);
    hori_layout->addStretch(1);
  
    _up_layout->addLayout(hori_layout);
    _up_layout->addStretch(1);

    _up_widget->setLayout(_up_layout);
    _up_widget->setMinimumHeight(150);


    _dn_widget = new QWidget(this);

    _dn_set_button = new QPushButton(_dn_widget);
    _dn_set_button->setFlat(true);

    _dn_save_button = new QPushButton(_dn_widget);
    _dn_save_button->setFlat(true);   

    _dn_nav_button = new QPushButton(_dn_widget);
    _dn_nav_button->setFlat(true);  

    QHBoxLayout *dn_title_layout = new QHBoxLayout();
    _dn_title_label = new QLabel(_dn_widget);
    dn_title_layout->addWidget(_dn_set_button, 0, Qt::AlignLeft);
    dn_title_layout->addWidget(_dn_save_button, 0, Qt::AlignLeft);
    dn_title_layout->addWidget(_dn_title_label, 1, Qt::AlignLeft);
    dn_title_layout->addWidget(_dn_nav_button, 0, Qt::AlignRight);
    //dn_title_layout->addStretch(1);

    _table_view = new QTableView(_dn_widget);
    _table_view->setModel(_session->get_decoder_model());
    _table_view->setAlternatingRowColors(true);
    _table_view->setShowGrid(false);
    _table_view->horizontalHeader()->setStretchLastSection(true);
    _table_view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    _table_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    _pre_button = new QPushButton(_dn_widget);
    _nxt_button = new QPushButton(_dn_widget);

    _search_button = new QPushButton(this);
    _search_button->setFixedWidth(_search_button->height());
    _search_button->setDisabled(true);
    _search_edit = new QLineEdit(_dn_widget);
    _search_edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    QHBoxLayout *search_layout = new QHBoxLayout();
    search_layout->addWidget(_search_button);
    search_layout->addStretch(1);
    search_layout->setContentsMargins(0, 0, 0, 0);
    _search_edit->setLayout(search_layout);
    _search_edit->setTextMargins(_search_button->width(), 0, 0, 0);

    _dn_search_layout = new QHBoxLayout();
    _dn_search_layout->addWidget(_pre_button, 0, Qt::AlignLeft);
    _dn_search_layout->addWidget(_search_edit, 1, Qt::AlignLeft);
    _dn_search_layout->addWidget(_nxt_button, 0, Qt::AlignRight);

    _matchs_label = new QLabel(_dn_widget);
    _matchs_title_label = new QLabel(_dn_widget);
    QHBoxLayout *dn_match_layout = new QHBoxLayout();
    dn_match_layout->addWidget(_matchs_title_label, 0, Qt::AlignLeft);
    dn_match_layout->addWidget(_matchs_label, 0, Qt::AlignLeft);
    dn_match_layout->addStretch(1);

    _dn_layout = new QVBoxLayout();
    _dn_layout->addLayout(dn_title_layout);
    _dn_layout->addLayout(_dn_search_layout);
    _dn_layout->addLayout(dn_match_layout);
    _dn_layout->addWidget(_table_view);

    _dn_widget->setLayout(_dn_layout);
    _dn_widget->setMinimumHeight(350);

    _split_widget = new QSplitter(this);
    _split_widget->insertWidget(0, _up_widget);
    _split_widget->insertWidget(1, _dn_widget);
    _split_widget->setOrientation(Qt::Vertical);
    _split_widget->setCollapsible(0, false);
    _split_widget->setCollapsible(1, false);

    this->setWidgetResizable(true);
    this->setWidget(_split_widget);
    _split_widget->setObjectName("protocolWidget");

    retranslateUi(); 

    _key_find_timer.SetCallback(std::bind(&ProtocolDock::show_protocol_list_panel, this));
     //when porotocol list panel was showPopup statu, receive key press event
     QWidget *popup1 = _protocol_combobox->findChild<QFrame*>();
     QWidget *wid1 = popup1->findChild<QWidget*>();
     wid1->installEventFilter(this);

    connect(_dn_nav_button, SIGNAL(clicked()),this, SLOT(nav_table_view()));
    connect(_dn_save_button, SIGNAL(clicked()),this, SLOT(export_table_view()));
    connect(_dn_set_button, SIGNAL(clicked()),this, SLOT(set_model()));
    connect(_pre_button, SIGNAL(clicked()),this, SLOT(search_pre()));
    connect(_nxt_button, SIGNAL(clicked()),this, SLOT(search_nxt()));
    connect(_add_button, SIGNAL(clicked()),this, SLOT(on_add_protocol()));
    connect(_del_all_button, SIGNAL(clicked()),this, SLOT(on_del_all_protocol())); 

    connect(this, SIGNAL(protocol_updated()), this, SLOT(update_model()));
    connect(_table_view, SIGNAL(clicked(QModelIndex)), this, SLOT(item_clicked(QModelIndex)));

    connect(_table_view->horizontalHeader(), SIGNAL(sectionResized(int,int,int)), 
                    this, SLOT(column_resize(int, int, int)));

    connect(_search_edit, SIGNAL(editingFinished()), this, SLOT(search_changed()));

    connect(_protocol_combobox->lineEdit(), SIGNAL(textEdited(const QString &)), 
                    this, SLOT(on_decoder_name_edited(const QString &)));

    connect(_protocol_combobox, SIGNAL(currentIndexChanged(int)), this, SLOT(on_new_decoder_selected(int)));
}

ProtocolDock::~ProtocolDock()
{
    //destroy protocol item layers
   for (auto it = _protocol_lay_items.begin(); it != _protocol_lay_items.end(); it++){
       DESTROY_QT_LATER(*it);
   }
   _protocol_lay_items.clear();

   //clear protocol infos list
   RELEASE_ARRAY(_decoderInfoList);
}

void ProtocolDock::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    else if (event->type() == QEvent::StyleChange)
        reStyle();
    QScrollArea::changeEvent(event);
}

void ProtocolDock::retranslateUi()
{
    _search_edit->setPlaceholderText(tr("search"));
    _matchs_title_label->setText(tr("Matching Items:"));
    _dn_title_label->setText(tr("Protocol List Viewer"));
}

void ProtocolDock::reStyle()
{
    QString iconPath = GetIconPath();

    _add_button->setIcon(QIcon(iconPath+"/add.svg"));
    _del_all_button->setIcon(QIcon(iconPath+"/del.svg"));
    _dn_set_button->setIcon(QIcon(iconPath+"/gear.svg"));
    _dn_save_button->setIcon(QIcon(iconPath+"/save.svg"));
    _dn_nav_button->setIcon(QIcon(iconPath+"/nav.svg"));
    _pre_button->setIcon(QIcon(iconPath+"/pre.svg"));
    _nxt_button->setIcon(QIcon(iconPath+"/next.svg"));
    _search_button->setIcon(QIcon(iconPath+"/search.svg"));

    for (auto it = _protocol_lay_items.begin(); it != _protocol_lay_items.end(); it++){
       (*it)->ResetStyle();
    } 
}

void ProtocolDock::paintEvent(QPaintEvent *)
{ 
}

void ProtocolDock::resizeEvent(QResizeEvent *event)
{
    int width = this->visibleRegion().boundingRect().width();

    int mg1 = 10;
    int mg2 = 10;

    width = width - mg1 * 2 -
            mg2 * 2 -
            _dn_search_layout->spacing() * 2 -
            _pre_button->width()-_nxt_button->width();
    width = std::max(width, 0);
    _search_edit->setMinimumWidth(width);
    QScrollArea::resizeEvent(event);
}

int ProtocolDock::decoder_name_cmp(const void *a, const void *b)
{
    return strcmp(((const srd_decoder*)a)->name,
        ((const srd_decoder*)b)->name);
}

int ProtocolDock::get_protocol_index_by_id(QString id)
{
    int dex = 0;
    for (auto info : _decoderInfoList){
        srd_decoder *dec = (srd_decoder *)(info->ObjectHandle);
        QString proid(dec->id);
        if (id == proid){
            return dex;
        }
        ++dex;
    }
    return -1;
}

void ProtocolDock::on_add_protocol()
{ 
    if (_decoderInfoList.size() == 0){
        MsgBox::Show(NULL, "Protocol list is empty!");
        return;
    }
    if (_protocol_combobox->currentIndex() == -1){
        MsgBox::Show(NULL, "Please select a protocol!");
        return;
    }

    int dex = _protocol_combobox->itemData(_protocol_combobox->currentIndex()).toInt();

    //check the base protocol
    srd_decoder *const dec = (srd_decoder *)(_decoderInfoList[dex]->ObjectHandle);
    QString pro_id(dec->id);
    std::list<data::decode::Decoder*> sub_decoders;
    
    assert(dec->inputs);

    QString input_id = parse_protocol_id((char *)dec->inputs->data);

    if (input_id != "logic")
    {
        pro_id = ""; //reset base protocol

        int base_dex = get_output_protocol_by_id(input_id);
        sub_decoders.push_front(new data::decode::Decoder(dec));

        while (base_dex != -1)
        {
            srd_decoder *base_dec = (srd_decoder *)(_decoderInfoList[base_dex]->ObjectHandle);
            pro_id = QString(base_dec->id); //change base protocol           

            assert(base_dec->inputs);

            input_id = parse_protocol_id((char *)base_dec->inputs->data);

            if (input_id == "logic")
            {
                break;
            }

            sub_decoders.push_front(new data::decode::Decoder(base_dec));
            pro_id = ""; //reset base protocol
            base_dex = get_output_protocol_by_id(input_id);
        }
    }

    if (pro_id == ""){
        MsgBox::Show("error", "find the base protocol error!");

        for(auto sub: sub_decoders){
            delete sub;
        }
        sub_decoders.clear();

        return;
    }

    add_protocol_by_id(pro_id, false, sub_decoders);
}

bool ProtocolDock::add_protocol_by_id(QString id, bool silent, std::list<pv::data::decode::Decoder*> &sub_decoders)
{    
    if (_session->get_device()->dev_inst()->mode != LOGIC) {         
        qDebug()<<"Protocol Analyzer\nProtocol Analyzer is only valid in Digital Mode!";
        return false;
    }

    int dex = this->get_protocol_index_by_id(id);
    if (dex == -1){
        qDebug()<<"Protocol not exists! id:"<< id;
        return false;
    }

    srd_decoder *const decoder = (srd_decoder *)(_decoderInfoList[dex]->ObjectHandle);
    DecoderStatus *dstatus = new DecoderStatus();
    dstatus->m_format = (int)DecoderDataFormat::hex;

    QString protocolName(decoder->name);
    QString protocolId(decoder->id);

    if (sub_decoders.size()){
        auto it = sub_decoders.end();
        it--;
        protocolName = QString((*it)->decoder()->name);
        protocolId = QString((*it)->decoder()->id); 
    }

    if (_session->add_decoder(decoder, silent, dstatus, sub_decoders) == false){
        return false;
    }

    // create item layer
    ProtocolItemLayer *layer = new ProtocolItemLayer(_up_widget, protocolName, this);
    _protocol_lay_items.push_back(layer);
    _up_layout->insertLayout(_protocol_lay_items.size(), layer);
    layer->m_decoderStatus = dstatus; 
    layer->m_protocolId = protocolId;

    // set current protocol format
    string fmt = AppConfig::Instance().GetProtocolFormat(protocolId.toStdString());
    if (fmt != "")
    {
        layer->SetProtocolFormat(fmt.c_str());
        dstatus->m_format = DecoderDataFormat::Parse(fmt.c_str());
    }

    // progress connection
    const auto &decode_sigs = _session->get_decode_signals();   
    protocol_updated();
    connect(decode_sigs.back(), SIGNAL(decoded_progress(int)), this, SLOT(decoded_progress(int)));

    return true;
}
 
 void ProtocolDock::on_del_all_protocol(){
     if (_protocol_lay_items.size() == 0){
        MsgBox::Show(NULL, "No Protocol Analyzer to delete!", this);
        return;
     }

    if (MsgBox::Confirm("Are you sure to remove all protocol analyzer?", this)){
         del_all_protocol();
    }
 } 

void ProtocolDock::del_all_protocol()
{  
    if (_protocol_lay_items.size() > 0)
    {
        _session->clear_all_decoder();

        for (auto it = _protocol_lay_items.begin(); it != _protocol_lay_items.end(); it++)
        {
             DESTROY_QT_LATER((*it)); //destory control
        }

        _protocol_lay_items.clear();
        protocol_updated();
    }
}

void ProtocolDock::decoded_progress(int progress)
{
    (void) progress;

    int pg = 0;
    QString err="";
    const auto &decode_sigs = _session->get_decode_signals();
    int index = 0;

    for(auto &d : decode_sigs) {
        pg = d->get_progress();
        if (d->decoder()->out_of_memory())
            err = tr("(Out of Memory)");

        if (index < _protocol_lay_items.size())
        {
            ProtocolItemLayer &lay =  *(_protocol_lay_items.at(index));
            lay.SetProgress(pg, err);
        }

        index++;
    }

    if (pg == 0 || pg % 10 == 1){
         update_model();
    }  
}

void ProtocolDock::set_model()
{
    pv::dialogs::ProtocolList *protocollist_dlg = new pv::dialogs::ProtocolList(this, _session);
    protocollist_dlg->exec();
    resize_table_view(_session->get_decoder_model());
    _model_proxy.setSourceModel(_session->get_decoder_model());
    search_done();

    // clear mark_index of all DecoderStacks
    const auto &decode_sigs = _session->get_decode_signals();
        
    for(auto &d : decode_sigs) {
        d->decoder()->set_mark_index(-1);
    }
}

void ProtocolDock::update_model()
{
    pv::data::DecoderModel *decoder_model = _session->get_decoder_model();
    const auto &decode_sigs = _session->get_decode_signals();

    if (decode_sigs.size() == 0)
        decoder_model->setDecoderStack(NULL);
    else if (!decoder_model->getDecoderStack())
        decoder_model->setDecoderStack(decode_sigs.at(0)->decoder());
    else {
        unsigned int index = 0;
        for(auto &d : decode_sigs) {
            if (d->decoder() == decoder_model->getDecoderStack()) {
                decoder_model->setDecoderStack(d->decoder());
                break;
            }
            index++;
        }
        if (index >= decode_sigs.size())
            decoder_model->setDecoderStack(decode_sigs.at(0)->decoder());
    }
    _model_proxy.setSourceModel(decoder_model);
    search_done();
    resize_table_view(decoder_model);
}

void ProtocolDock::resize_table_view(data::DecoderModel* decoder_model)
{
    if (decoder_model->getDecoderStack()) {
        for (int i = 0; i < decoder_model->columnCount(QModelIndex()) - 1; i++) {
            _table_view->resizeColumnToContents(i);
            if (_table_view->columnWidth(i) > 200)
                _table_view->setColumnWidth(i, 200);
        }
        int top_row = _table_view->rowAt(0);
        int bom_row = _table_view->rowAt(_table_view->height());
        if (bom_row >= top_row && top_row >= 0) {
            for (int i = top_row; i <= bom_row; i++)
                _table_view->resizeRowToContents(i);
        }
    }
}

void ProtocolDock::item_clicked(const QModelIndex &index)
{
    pv::data::DecoderModel *decoder_model = _session->get_decoder_model();

    auto decoder_stack = decoder_model->getDecoderStack();
    if (decoder_stack) {
        pv::data::decode::Annotation ann;
        if (decoder_stack->list_annotation(ann, index.column(), index.row())) {
            const auto &decode_sigs = _session->get_decode_signals();

            for(auto &d : decode_sigs) {
                d->decoder()->set_mark_index(-1);
            }

            decoder_stack->set_mark_index((ann.start_sample()+ann.end_sample())/2);
            _session->show_region(ann.start_sample(), ann.end_sample(), false);

           // qDebug()<<ann.annotations().at(0)<<"type:"<<ann.type();
        }
    }
    _table_view->resizeRowToContents(index.row());
    if (index.column() != _model_proxy.filterKeyColumn()) {
        _model_proxy.setFilterKeyColumn(index.column());
        _model_proxy.setSourceModel(decoder_model);
        search_done();
    }
    QModelIndex filterIndex = _model_proxy.mapFromSource(index);
    if (filterIndex.isValid()) {
        _cur_search_index = filterIndex.row();
    } else {
        if (_model_proxy.rowCount() == 0) {
            _cur_search_index = -1;
        } else {
            uint64_t up = 0;
            uint64_t dn = _model_proxy.rowCount() - 1;
            do {
                uint64_t md = (up + dn)/2;
                QModelIndex curIndex = _model_proxy.mapToSource(_model_proxy.index(md,_model_proxy.filterKeyColumn()));
                if (index.row() == curIndex.row()) {
                    _cur_search_index = md;
                    break;
                } else if (md == up) {
                    if (curIndex.row() < index.row() && up < dn) {
                        QModelIndex nxtIndex = _model_proxy.mapToSource(_model_proxy.index(md+1,_model_proxy.filterKeyColumn()));
                        if (nxtIndex.row() < index.row())
                            md++;
                    }
                    _cur_search_index = md + ((curIndex.row() < index.row()) ? 0.5 : -0.5);
                    break;
                } else if (curIndex.row() < index.row()) {
                    up = md;
                } else if (curIndex.row() > index.row()) {
                    dn = md;
                }
            }while(1);
        }
    }
}

void ProtocolDock::column_resize(int index, int old_size, int new_size)
{
    (void)index;
    (void)old_size;
    (void)new_size;
    pv::data::DecoderModel *decoder_model = _session->get_decoder_model();
    if (decoder_model->getDecoderStack()) {
        int top_row = _table_view->rowAt(0);
        int bom_row = _table_view->rowAt(_table_view->height());
        if (bom_row >= top_row && top_row >= 0) {
            for (int i = top_row; i <= bom_row; i++)
                _table_view->resizeRowToContents(i);
        }
    }
}

void ProtocolDock::export_table_view()
{
    pv::dialogs::ProtocolExp *protocolexp_dlg = new pv::dialogs::ProtocolExp(this, _session);
    protocolexp_dlg->exec();
}

void ProtocolDock::nav_table_view()
{
    uint64_t row_index = 0;
    pv::data::DecoderModel *decoder_model = _session->get_decoder_model();

    auto decoder_stack = decoder_model->getDecoderStack();
    if (decoder_stack) {
        uint64_t offset = _view.offset() * (decoder_stack->samplerate() * _view.scale());
        std::map<const pv::data::decode::Row, bool> rows = decoder_stack->get_rows_lshow();
        int column = _model_proxy.filterKeyColumn();
        for (std::map<const pv::data::decode::Row, bool>::const_iterator i = rows.begin();
            i != rows.end(); i++) {
            if ((*i).second && column-- == 0) {
                row_index = decoder_stack->get_annotation_index((*i).first, offset);
                break;
            }
        }
        QModelIndex index = _model_proxy.mapToSource(_model_proxy.index(row_index, _model_proxy.filterKeyColumn()));
        if(index.isValid()){
            _table_view->scrollTo(index);
            _table_view->setCurrentIndex(index);

            pv::data::decode::Annotation ann;
            decoder_stack->list_annotation(ann, index.column(), index.row());
            const auto &decode_sigs = _session->get_decode_signals();

            for(auto &d : decode_sigs) {
                d->decoder()->set_mark_index(-1);
            }
            decoder_stack->set_mark_index((ann.start_sample()+ann.end_sample())/2);
            _view.set_all_update(true);
            _view.update();
        }
    }
}

void ProtocolDock::search_pre()
{
    search_update();
    // now the proxy only contains rows that match the name
    // let's take the pre one and map it to the original model
    if (_model_proxy.rowCount() == 0) {
        _table_view->scrollToTop();
        _table_view->clearSelection();
        _matchs_label->setText(QString::number(0));
        _cur_search_index = -1;
        return;
    }
    int i = 0;
    uint64_t rowCount = _model_proxy.rowCount();
    QModelIndex matchingIndex;
    pv::data::DecoderModel *decoder_model = _session->get_decoder_model();

    auto decoder_stack = decoder_model->getDecoderStack();
    do {
        _cur_search_index--;
        if (_cur_search_index <= -1 || _cur_search_index >= _model_proxy.rowCount())
            _cur_search_index = _model_proxy.rowCount() - 1;

        matchingIndex = _model_proxy.mapToSource(_model_proxy.index(ceil(_cur_search_index),_model_proxy.filterKeyColumn()));
        if (!decoder_stack || !matchingIndex.isValid())
            break;
        i = 1;
        uint64_t row = matchingIndex.row() + 1;
        uint64_t col = matchingIndex.column();
        pv::data::decode::Annotation ann;
        bool ann_valid;
        while(i < _str_list.size()) {
            QString nxt = _str_list.at(i);

            do {
                ann_valid = decoder_stack->list_annotation(ann, col, row);
                row++;
            }while(ann_valid && !ann.is_numberic());

            QString source = ann.annotations().at(0);
            if (ann_valid && source.contains(nxt))
                i++;
            else
                break;
        }
    }while(i < _str_list.size() && --rowCount);

    if(i >= _str_list.size() && matchingIndex.isValid()){
        _table_view->scrollTo(matchingIndex);
        _table_view->setCurrentIndex(matchingIndex);
        _table_view->clicked(matchingIndex);
    } else {
        _table_view->scrollToTop();
        _table_view->clearSelection();
        _matchs_label->setText(QString::number(0));
        _cur_search_index = -1;
    }
}

void ProtocolDock::search_nxt()
{
    search_update();
    // now the proxy only contains rows that match the name
    // let's take the pre one and map it to the original model
    if (_model_proxy.rowCount() == 0) {
        _table_view->scrollToTop();
        _table_view->clearSelection();
        _matchs_label->setText(QString::number(0));
        _cur_search_index = -1;
        return;
    }

    int i = 0;
    uint64_t rowCount = _model_proxy.rowCount();
    QModelIndex matchingIndex;
    pv::data::DecoderModel *decoder_model = _session->get_decoder_model();
    auto decoder_stack = decoder_model->getDecoderStack();

    if (decoder_stack == NULL){
        qDebug()<<"decoder_stack is null";
        return;
    }  

    do {
        _cur_search_index++;
        if (_cur_search_index < 0 || _cur_search_index >= _model_proxy.rowCount())
            _cur_search_index = 0;

        matchingIndex = _model_proxy.mapToSource(_model_proxy.index(floor(_cur_search_index),_model_proxy.filterKeyColumn()));
        
        if (!matchingIndex.isValid())
            break;

      //  qDebug()<<"row:"<<matchingIndex.row();

        i = 1;
        uint64_t row = matchingIndex.row() + 1;
        uint64_t col = matchingIndex.column();
        pv::data::decode::Annotation ann;
        bool ann_valid;

        while(i < _str_list.size()) {
            QString nxt = _str_list.at(i);

            do {
                ann_valid = decoder_stack->list_annotation(ann, col, row);
                row++;
            }while(ann_valid && !ann.is_numberic());

            auto strlist = ann.annotations();
            QString source = ann.annotations().at(0);
            if (ann_valid && source.contains(nxt))
                i++;
            else
                break;
        }
    }while(i < _str_list.size() && --rowCount);

    if(i >= _str_list.size() && matchingIndex.isValid()){
        _table_view->scrollTo(matchingIndex);
        _table_view->setCurrentIndex(matchingIndex);
        _table_view->clicked(matchingIndex);
    } else {
        _table_view->scrollToTop();
        _table_view->clearSelection();
        _matchs_label->setText(QString::number(0));
        _cur_search_index = -1;
    }
}

void ProtocolDock::search_done()
{
    QString str = _search_edit->text().trimmed();
    QRegularExpression rx("(-)");
    _str_list = str.split(rx);
    _model_proxy.setFilterFixedString(_str_list.first());
    if (_str_list.size() > 1)
        _matchs_label->setText("...");
    else
        _matchs_label->setText(QString::number(_model_proxy.rowCount()));
}

void ProtocolDock::search_changed()
{
    _search_edited = true;
    _matchs_label->setText("...");
}

void ProtocolDock::search_update()
{
    if (!_search_edited)
        return;

    pv::data::DecoderModel *decoder_model = _session->get_decoder_model();

    auto decoder_stack = decoder_model->getDecoderStack();
    if (!decoder_stack)
        return;

    if (decoder_stack->list_annotation_size(_model_proxy.filterKeyColumn()) > ProgressRows) {
        QFuture<void> future;
        future = QtConcurrent::run([&]{
            search_done();
        });
        Qt::WindowFlags flags = Qt::CustomizeWindowHint;
        QProgressDialog dlg(tr("Searching..."),
                            tr("Cancel"),0,0,this,flags);
        dlg.setWindowModality(Qt::WindowModal);
        dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint |
                           Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);
        dlg.setCancelButton(NULL);

        QFutureWatcher<void> watcher;
        connect(&watcher,SIGNAL(finished()),&dlg,SLOT(cancel()));
        watcher.setFuture(future);

        dlg.exec();
    } else {
        search_done();
    }
    _search_edited = false;
}

 //-------------------IProtocolItemLayerCallback
void ProtocolDock::OnProtocolSetting(void *handle){
  
    for (auto it = _protocol_lay_items.begin(); it != _protocol_lay_items.end(); it++){
       if ((*it) == handle){ 
             void *key_handel = (*it)->get_protocol_key_handel();
            _session->rst_decoder_by_key_handel(key_handel);
            protocol_updated();
           break;
       } 
   }
}

void ProtocolDock::OnProtocolDelete(void *handle){
    if (!MsgBox::Confirm("Are you sure to remove this protocol analyzer?", this)){
         return;
    } 

     for (auto it = _protocol_lay_items.begin(); it != _protocol_lay_items.end(); it++)
     {
         if ((*it) == handle)
         {
             auto lay = (*it); 
             void *key_handel = lay->get_protocol_key_handel();
            _protocol_lay_items.erase(it);
             DESTROY_QT_LATER(lay);
             _session->remove_decoder_by_key_handel(key_handel);     
             protocol_updated();
             break;
         } 
     } 

  
}

void ProtocolDock::OnProtocolFormatChanged(QString format, void *handle){
    for (auto it = _protocol_lay_items.begin(); it != _protocol_lay_items.end(); it++){
       if ((*it) == handle){
           auto lay = (*it); 
           AppConfig::Instance().SetProtocolFormat(lay->m_protocolId.toStdString(), format.toStdString());

           if (lay->m_decoderStatus != NULL)
           {
                  lay->m_decoderStatus->m_format = DecoderDataFormat::Parse(format.toStdString().c_str());
                  protocol_updated();
           }
        
           break;
       }
   }
} 

void ProtocolDock::on_decoder_name_edited(const QString &value)
{
    _bSettingList = true;

    while (_protocol_combobox->count())
    {
        _protocol_combobox->removeItem(0);
    }

    for (auto info: _decoderInfoList){
        srd_decoder *dec = (srd_decoder *)(info->ObjectHandle);
        QString name(dec->name);
        QString id(dec->id);

        if (value == "" 
        || name.indexOf(value, 0, Qt::CaseInsensitive) != -1 
        || id.indexOf(value, 0, Qt::CaseInsensitive) != -1 ){
            _protocol_combobox->addItem(QString::fromUtf8(dec->name), QVariant::fromValue(info->Index));
        }
    }    
  
    _protocol_combobox->setCurrentIndex(-1);
    _protocol_combobox->lineEdit()->setText(value);
    _bSettingList = false;

    if (_key_find_timer.IsActived() == false){
        //check input keep time
        _key_find_timer.Start(100);
    }
    else{
        _key_find_timer.ResetActiveTime();
    }
}

 void ProtocolDock::show_protocol_list_panel()
 {  
     //press key end, to popup list panel
     if (_key_find_timer.GetActiveTimeLong() >= 1000){
         _key_find_timer.Stop();
         _protocol_combobox->showPopup();
     }    
 }

bool ProtocolDock::protocol_sort_callback(const DecoderInfoItem *o1, const DecoderInfoItem *o2)
{
    srd_decoder *dec1 = (srd_decoder *)(o1->ObjectHandle);
    srd_decoder *dec2 = (srd_decoder *)(o2->ObjectHandle);
    const char *s1 = dec1->name;
    const char *s2 = dec2->name;
    char c1 = 0;
    char c2 = 0;

    while (*s1 && *s2)
    {
        c1 = *s1;
        c2 = *s2;

        if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
        if (c2 >= 'a' && c2 <= 'z') c2 -= 32;                               

        if (c1 > c2)
            return false;
        else if (c1 < c2)
            return true;
        
        s1++;
        s2++;
    }

    if (*s1)
        return false;
    else if (*s2)
        return true;

    return true;
}

bool ProtocolDock::eventFilter(QObject *object, QEvent *event)
{
    if ( event->type() == QEvent::KeyPress )
    {  
        if (_protocol_combobox->IsPopup()){
            _protocol_combobox->hidePopup();
        }
    }
    return false;
}

 void ProtocolDock::on_new_decoder_selected(int index)
 {
     if (index >= 0 && _bSettingList == false){
         on_add_protocol();
     }
 }

 QString ProtocolDock::parse_protocol_id(const char *id)
 {
     if (id == NULL || *id == 0){
         assert(false);
     }
     char buf[25];
     strncpy(buf, id, sizeof(buf));
     char *rd = buf;
     char *start = NULL;
     int len = 0;

     while (*rd && len - 1 < sizeof(buf))
     { 
         if (*rd == '['){
             start = rd++;
         }
         else if (*rd == ']'){
             *rd = 0;
             break;
         }
         ++rd;
         len++;
     }
     if (start == NULL){
         start = const_cast<char*>(id);
     }

     return QString(start);
 }

  int ProtocolDock::get_output_protocol_by_id(QString id)
  {
      int dex = 0;

      for (auto info : _decoderInfoList)
      {
          srd_decoder *dec = (srd_decoder *)(info->ObjectHandle);
          if (dec->outputs)
          {
              QString output_id = parse_protocol_id((char*)dec->outputs->data);
              if (output_id == id)
              {
                  QString proid(dec->id);
                  if (!proid.startsWith("0:") || output_id == proid){
                      return dex;
                  } 
              }
          }
         
          ++dex;
      }

      return -1;
  }
 

//-------------------------


} // namespace dock
} // namespace pv
