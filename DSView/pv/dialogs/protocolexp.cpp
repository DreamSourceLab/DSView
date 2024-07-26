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

#include "protocolexp.h"
  
#include <QFormLayout>
#include <QListWidget>
#include <QFile>
#include <QFileDialog>
#include <QTextStream>
#include <QProgressDialog>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>

#include "../sigsession.h"
#include "../data/decoderstack.h"
#include "../data/decode/row.h"
#include "../data/decode/annotation.h"
#include "../view/decodetrace.h"
#include "../data/decodermodel.h"
#include "../config/appconfig.h"
#include "../dsvdef.h"
#include "../utility/encoding.h"
#include "../utility/path.h"
#include "../log.h"
#include "../ui/langresource.h"
#include "../ui/msgbox.h"
 #include "../utility/formatting.h"

#define EXPORT_DEC_ROW_COUNT_MAX 20

using namespace pv::data::decode;

namespace pv {
namespace dialogs {

ProtocolExp::ProtocolExp(QWidget *parent, SigSession *session) :
    DSDialog(parent),
    _session(session),
    _button_box(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        Qt::Horizontal, this),
    _export_cancel(false)
{
    _bAbleClose = false;

    _format_combobox = new DsComboBox(this);
    //tr
    _format_combobox->addItem("Comma-Separated Values (*.csv)");
    _format_combobox->addItem("Text files (*.txt)");

    _flayout = new QFormLayout();
    _flayout->setVerticalSpacing(5);
    _flayout->setFormAlignment(Qt::AlignLeft);
    _flayout->setLabelAlignment(Qt::AlignLeft);
    _flayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    _flayout->addRow(new QLabel(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_EXPORT_FORMAT), "Export Format: "), this), _format_combobox);

    pv::data::DecoderModel* decoder_model = _session->get_decoder_model();

    const auto decoder_stack = decoder_model->getDecoderStack();
    if (decoder_stack) {
        int row_index = 0;
        auto rows = decoder_stack->get_rows_lshow();

        for (auto i = rows.begin();i != rows.end(); i++) {
            if ((*i).second) {
                QLabel *row_label = new QLabel((*i).first.title(), this);
                QCheckBox *row_sel = new QCheckBox(this);
                if (row_index == 0) {
                    row_sel->setChecked(true);
                }
                _row_label_list.push_back(row_label);
                _row_sel_list.push_back(row_sel);
                _flayout->addRow(row_label, row_sel);
                row_sel->setProperty("index", row_index);
                row_sel->setProperty("title", (*i).first.title());
                row_index++;
            }
        }
    }

    _layout = new QVBoxLayout();
    _layout->addLayout(_flayout);
    _layout->addWidget(&_button_box);

    layout()->addLayout(_layout);
    setTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_PROTOCOL_EXPORT), "Protocol Export"));

    connect(&_button_box, SIGNAL(accepted()), this, SLOT(onAccept()));
    connect(&_button_box, SIGNAL(rejected()), this, SLOT(onReject()));
    connect(_session->device_event_object(), SIGNAL(device_updated()), this, SLOT(onReject()));

    connect(&m_timer, &QTimer::timeout, this, &ProtocolExp::on_timeout);
    m_timer.setInterval(200);

}

void ProtocolExp::on_timeout()
{
    if (_bAbleClose){
        closeSelf();
    }
}

void ProtocolExp::onReject()
{
    using namespace Qt;

    QDialog::reject();
    this->deleteLater();
}

void ProtocolExp::cancel_export()
{
    _export_cancel = true;
}

void ProtocolExp::closeSelf()
{
    close();
    this->deleteLater();
}

void ProtocolExp::onAccept()
{
    if (_session->have_decoded_result() == false 
            || _row_sel_list.empty())
    {
        QString errMsg = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_NO_DECODED_RESULT), "No data to export");
        MsgBox::Show(errMsg);
        return;
    }
 
    QList<QString> supportedFormats;
    for (int i = _format_combobox->count() - 1; i >= 0; i--)
    {
        supportedFormats.push_back(_format_combobox->itemText(i));
    }

    QString filter;
    for (int i = 0; i < supportedFormats.count(); i++)
    {
        filter.append(supportedFormats[i]);
        if (i < supportedFormats.count() - 1)
            filter.append(";;");
    }

    AppConfig &app = AppConfig::Instance();
    QString default_filter = _format_combobox->currentText();
    QString default_name = app.userHistory.protocolExportPath + "/" + "decoder-";

    QString dateTimeString = Formatting::DateTimeToString(_session->get_session_time(), TimeStrigFormatType::TIME_STR_FORMAT_SHORT2);
    default_name += "-" + dateTimeString;

    QString file_name = QFileDialog::getSaveFileName(
        this,
        L_S(STR_PAGE_DLG, S_ID(IDS_DLG_EXPORT_DATA), "Export Data"),
        default_name, filter,
        &default_filter);

    if (file_name == ""){
        return;
    }

    QFileInfo f(file_name);
    QStringList list = default_filter.split('.').last().split(')');
    QString ext = list.first();
    if (f.suffix().compare(ext))
        //tr
        file_name += "." + ext;

    QString fname = path::GetDirectoryName(file_name);
    if (fname != app.userHistory.openDir)
    {
        app.userHistory.protocolExportPath = fname;
        app.SaveHistory();
    }
    _fileName = file_name;
 
    QFuture<void> future;
    future = QtConcurrent::run([&]{
                    save_proc();
                    _bAbleClose = true;
               });

    Qt::WindowFlags flags = Qt::CustomizeWindowHint;
    QProgressDialog dlg(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_EXPORT_PROTOCOL_LIST_RESULT), 
                        "Export Protocol List Result... It can take a while."),
                        L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CANCEL), "Cancel"), 0, 100, this, flags);
    dlg.setWindowModality(Qt::WindowModal);
    dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint |
                       Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint);

    QFutureWatcher<void> watcher;

    connect(&watcher, SIGNAL(finished()), &dlg, SLOT(cancel()));
    connect(this, SIGNAL(export_progress(int)), &dlg, SLOT(setValue(int)));
    connect(&dlg, SIGNAL(canceled()), this, SLOT(cancel_export()));

    m_timer.start();
    this->hide();
    watcher.setFuture(future);
    dlg.exec();

    future.waitForFinished();   
}

void ProtocolExp::save_proc()
{
    _export_cancel = false;

    QFile file(_fileName);
    file.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream out(&file);
    encoding::set_utf8(out);
    // out.setGenerateByteOrderMark(true); // UTF-8 without BOM
    int row_num = 0;
    ExportRowInfo row_inf_arr[EXPORT_DEC_ROW_COUNT_MAX];
    std::vector<Annotation*> annotations_arr[EXPORT_DEC_ROW_COUNT_MAX];

    for (std::list<QCheckBox *>::const_iterator i = _row_sel_list.begin();
         i != _row_sel_list.end(); i++)
    {
        if ((*i)->isChecked())
        {
            row_inf_arr[row_num].title = (*i)->property("title").toString();
            row_inf_arr[row_num].row_index = (*i)->property("index").toULongLong();
            row_num++;

            if (row_num == EXPORT_DEC_ROW_COUNT_MAX)
                break;
        }
    }

    if (row_num == 0){
        dsv_info("ERROR: There have no decode data row to export.");
        return;
    }

    pv::data::DecoderModel *decoder_model = _session->get_decoder_model();
    const auto decoder_stack = decoder_model->getDecoderStack();
    
    int fd_row_dex = 0;
    const std::map<const Row, bool> rows_lshow = decoder_stack->get_rows_lshow();
    for (auto it = rows_lshow.begin();it != rows_lshow.end(); it++)
    {
        if ((*it).second)
        {
            for (int i=0; i<row_num; i++) {
                if (row_inf_arr[i].row_index == fd_row_dex){
                    row_inf_arr[i].row = &(*it).first;
                    break;
                }
            }      
            fd_row_dex++;
        }
    }

    //get annotation list
    uint64_t total_ann_count = 0;

    for (int i=0; i<row_num; i++)
    {
        decoder_stack->get_annotation_subset(annotations_arr[i], *row_inf_arr[i].row,
                                         0, decoder_stack->sample_count() - 1);
        total_ann_count += (uint64_t)annotations_arr[i].size();
        sort(annotations_arr[i].begin(), annotations_arr[i].end(), compare_ann_index);  
        row_inf_arr[i].read_index = 0;
    }

    //title
    QString title_str;

    for (int i=0; i<row_num; i++) {
        if (i > 0 && i < row_num){
            title_str.append(",");
        }
        title_str.append(row_inf_arr[i].title);
    }

    out << QString("%1,%2,%3\n")
            .arg("Id")
            .arg("Time[ns]")
            .arg(title_str);

    uint64_t write_row_dex = 0;
    uint64_t write_ann_num = 0;
    double ns_per_sample = SR_SEC(1) * 1.0 / decoder_stack->samplerate();
    uint64_t sample_index = 0;
    uint64_t sample_index1 = 0;

    while (write_ann_num < total_ann_count && !_export_cancel)
    {    
        bool bFirtColumn = true;

        for (int i=0; i<row_num; i++)
        {   
            if (row_inf_arr[i].read_index >= annotations_arr[i].size())
                continue;
            
            Annotation *ann = annotations_arr[i].at(row_inf_arr[i].read_index);
            sample_index1 = ann->start_sample();

            if (bFirtColumn || sample_index1 < sample_index){
                sample_index = sample_index1;
                bFirtColumn = false;
            }
        }

        QString ann_row_str;

        for (int i=0; i<row_num; i++)
        {   
            if (i > 0 && i < row_num){
                ann_row_str.append(",");
            }

            if (row_inf_arr[i].read_index >= annotations_arr[i].size())
                continue;
            
            Annotation *ann = annotations_arr[i].at(row_inf_arr[i].read_index);           

            if (ann->start_sample() == sample_index){
                ann_row_str.append(ann->annotations().at(0));
                row_inf_arr[i].read_index++;
                write_ann_num++;
            }
        }

        write_row_dex++;

        out << QString("%1,%2,%3\n")
                       .arg(QString::number(write_row_dex))
                       .arg(QString::number(sample_index * ns_per_sample, 'f', 2))
                       .arg(ann_row_str);

        emit export_progress(write_ann_num * 100 / total_ann_count);
    }

    file.close();
}

bool ProtocolExp::compare_ann_index(const data::decode::Annotation *a, 
                    const data::decode::Annotation *b)
{   
    assert(a);
    assert(b);
    return a->start_sample() < b->start_sample();
}

} // namespace dialogs
} // namespace pv
