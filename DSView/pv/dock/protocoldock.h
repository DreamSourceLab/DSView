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


#ifndef DSVIEW_PV_PROTOCOLDOCK_H
#define DSVIEW_PV_PROTOCOLDOCK_H

#include <libsigrokdecode.h>

#include <QDockWidget>
#include <QPushButton>
#include <QLabel> 
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QSplitter>
#include <QTableView>
#include <QSortFilterProxyModel>
#include <QLineEdit>
#include <QToolButton>

#include <vector>
#include <mutex>
#include <list>

#include "../data/decodermodel.h"
#include "protocolitemlayer.h"
#include "keywordlineedit.h"
#include "searchcombobox.h"
#include "../interface/icallbacks.h"

struct DecoderInfoItem{
    void  *_data_handle; //srd_decoder* type
};

namespace pv {

class SigSession;
  
namespace data {
    class DecoderModel;
    namespace decode{
        class Decoder;
    }
}

namespace view {
class View;
}

namespace dock {
  
class ProtocolDock : public QScrollArea, 
public IProtocolItemLayerCallback, 
public IKeywordActive,
public ISearchItemClick,
public IDecoderPannel,
public IFontForm
{
    Q_OBJECT

public:
    static const uint64_t ProgressRows = 100000;

public:
    ProtocolDock(QWidget *parent, view::View &view, SigSession *session);
    ~ProtocolDock();

    void del_all_protocol(); 
    bool add_protocol_by_id(QString id, bool silent, std::list<pv::data::decode::Decoder*> &sub_decoders);

    void reset_view();
    void update_view_status();

private:
    void changeEvent(QEvent *event);
    void retranslateUi();
    void reStyle();
    int get_protocol_index_by_id(QString id);
    static QString parse_protocol_id(const char *id);
    int get_output_protocol_by_id(QString id);

    static int decoder_name_cmp(const void *a, const void *b);
    void resize_table_view(data::DecoderModel *decoder_model);
    static bool protocol_sort_callback(const DecoderInfoItem *o1, const DecoderInfoItem *o2);

    //IProtocolItemLayerCallback
    void OnProtocolSetting(void *handle) override;
    void OnProtocolDelete(void *handle) override;
    void OnProtocolFormatChanged(QString format, void *handle) override;

    //IKeywordActive
    void BeginEditKeyword() override; 

    //ISearchItemClick
    void OnItemClick(void *sender, void *data_handle) override;

    //IDecoderPannel
    void update_deocder_item_name(void *trace_handel, const char *name) override;

    //IFontForm
    void update_font() override;

signals:
    void protocol_updated();

public slots:
    void update_model();

private slots:
    void on_add_protocol(); 
    void on_del_all_protocol();
    void decoded_progress(int progress);
    void set_model();   
    void export_table_view();
    void nav_table_view();
    void item_clicked(const QModelIndex &index);
    void column_resize(int index, int old_size, int new_size);
    void search_pre();
    void search_nxt();
    void search_done();
    void search_changed();
    void search_update();
    void show_protocol_select();

private:
    SigSession *_session;
    view::View &_view;
    QSortFilterProxyModel _model_proxy;
    int _cur_search_index;
    QStringList _str_list;

    QWidget     *_top_panel; 
    QTableView  *_table_view;
    QPushButton *_pre_button;
    QPushButton *_nxt_button;
    QLineEdit *_ann_search_edit; 
    QLabel *_matchs_label;
    QLabel *_matchs_title_label;
    QLabel *_bot_title_label;

    QPushButton *_pro_add_button;
    QPushButton *_del_all_button; 
    QVBoxLayout *_top_layout;
    std::vector <ProtocolItemLayer*> _protocol_lay_items; //protocol item layers

    QPushButton *_bot_set_button;
    QPushButton *_bot_save_button;
    QPushButton *_dn_nav_button;
    QPushButton *_ann_search_button;
    std::vector<DecoderInfoItem*> _decoderInfoList;
    KeywordLineEdit *_pro_keyword_edit;
    QString     _selected_protocol_id;
    QToolButton *_pro_search_button; 

    mutable std::mutex _search_mutex;
    bool _search_edited; 
};

} // namespace dock
} // namespace pv

#endif // DSVIEW_PV_PROTOCOLDOCK_H
