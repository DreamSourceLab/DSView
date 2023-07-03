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

#include "../ui/langresource.h"

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

    connect(&_button_box, SIGNAL(accepted()), this, SLOT(accept()));
    connect(&_button_box, SIGNAL(rejected()), this, SLOT(reject()));
    connect(_session->device_event_object(), SIGNAL(device_updated()), this, SLOT(reject()));

}

void ProtocolExp::accept()
{ 
    QDialog::accept();

    if (_row_sel_list.empty()){
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
    default_name += _session->get_session_time().toString("-yyMMdd-hhmmss");

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

    QString title;
    int index = 0;
    for (std::list<QCheckBox *>::const_iterator i = _row_sel_list.begin();
         i != _row_sel_list.end(); i++)
    {
        if ((*i)->isChecked())
        {
            title = (*i)->property("title").toString();
            index = (*i)->property("index").toULongLong();
            break;
        }
    }

    out << QString("%1,%2,%3\n")
               .arg("Id")
               .arg("Time[ns]")
               .arg(title);

    pv::data::DecoderModel *decoder_model = _session->get_decoder_model();
    const auto decoder_stack = decoder_model->getDecoderStack();
    int row_index = 0;
    Row row;

    const std::map<const Row, bool> rows_lshow = decoder_stack->get_rows_lshow();
    for (std::map<const Row, bool>::const_iterator i = rows_lshow.begin();
         i != rows_lshow.end(); i++)
    {
        if ((*i).second)
        {
            if (index == row_index)
            { 
                row = Row((*i).first);
                break;
            }
            row_index++;
        }
    }

    uint64_t exported = 0;
    double ns_per_sample = SR_SEC(1) * 1.0 / decoder_stack->samplerate();
    std::vector<Annotation*> annotations;
    decoder_stack->get_annotation_subset(annotations, row,
                                         0, decoder_stack->sample_count() - 1);
    if (annotations.size() > 0 )
    { 
        for (Annotation *a : annotations)
        {
            out << QString("%1,%2,%3\n")
                       .arg(QString::number(exported))
                       .arg(QString::number(a->start_sample() * ns_per_sample, 'f', 20))
                       .arg(a->annotations().at(0));
            exported++;
            emit export_progress(exported * 100 / annotations.size());
            if (_export_cancel)
                break;
        }
    }

    file.close();
}

void ProtocolExp::reject()
{
    using namespace Qt;

    QDialog::reject();
}

void ProtocolExp::cancel_export()
{
    _export_cancel = true;
}

} // namespace dialogs
} // namespace pv
