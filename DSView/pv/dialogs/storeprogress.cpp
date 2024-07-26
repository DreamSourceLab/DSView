/*
 * This file is part of the PulseView project.
 * DSView is based on PulseView.
 * 
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include "storeprogress.h" 
#include "../sigsession.h"
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QComboBox>
#include <QFormLayout>
#include "../ui/msgbox.h"
#include "../config/appconfig.h"
#include "../interface/icallbacks.h"
#include "../log.h"
#include "../view/view.h"
#include "../view/cursor.h"
#include "../ui/langresource.h"
#include "../ui/dscombobox.h"
#include "../ui/xprogressbar.h"

namespace pv {
namespace dialogs {

StoreProgress::StoreProgress(SigSession *session, QWidget *parent) :
    DSDialog(parent)
{
    _fileLab = NULL;
    _ckOrigin = NULL;

    _store_session = new StoreSession(session);
    _progress = new XProgressBar(this);

    this->setMinimumSize(550, 220);
    this->setModal(true);
 
    _progress->setValue(0);
    _progress->setMaximum(100);

    _isExport = false;
    _is_done = false;
    _start_cursor = NULL;
    _end_cursor = NULL;
    _view = NULL;  

    QGridLayout *grid = new QGridLayout(); 
    _grid = grid;
    grid->setContentsMargins(10, 20, 10, 10);
    grid->setVerticalSpacing(25);

    grid->setColumnStretch(0, 2);
    grid->setColumnStretch(1, 2);
    grid->setColumnStretch(2, 1);
    grid->setColumnStretch(3, 1);

    _fileLab = new QTextEdit();
    _fileLab->setReadOnly(true);   
    _fileLab->setObjectName("PathLine");
    _fileLab->setMaximumHeight(50); 

    _openButton = new QPushButton(this);
    _openButton->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_PATH_NAME), "path"));

    _space = new QWidget(this);
    _space->setMinimumHeight(80);
    _space->setVisible(false);

    grid->addWidget(_progress, 0, 0, 1, 4);
    grid->addWidget(_fileLab, 1, 0, 1, 3);
    grid->addWidget(_openButton, 1, 3, 1, 1);    
    grid->addWidget(_space);

    QDialogButtonBox  *_button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, 
                                        Qt::Horizontal, this);
    grid->addWidget(_button_box, 2, 2, 1, 2, Qt::AlignRight | Qt::AlignBottom);

    layout()->addLayout(grid);

    connect(_button_box, SIGNAL(rejected()), this, SLOT(reject()));
    connect(_button_box, SIGNAL(accepted()), this, SLOT(accept()));

    connect(_store_session, SIGNAL(progress_updated()),
        this, SLOT(on_progress_updated()), Qt::QueuedConnection);

    connect(_openButton, SIGNAL(clicked()),this, SLOT(on_change_file()));

    _progress->setVisible(false);

    connect(&m_timer, &QTimer::timeout, this, &StoreProgress::on_timeout);
    m_timer.setInterval(100);
}

StoreProgress::~StoreProgress()
{
    _store_session->wait();
}

void StoreProgress::closeEvent(QCloseEvent* event)
{ 
    //Wait the thread ends.
    if (_store_session->is_busy()){
        _store_session->cancel();
        event->ignore();
        return;
    }
   
    _store_session->session()->set_saving(false);
    _store_session->session()->broadcast_msg(DSV_MSG_SAVE_COMPLETE);

    delete this;
}

void StoreProgress::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        close();
    }
    else { 
        QWidget::keyPressEvent(event);
    }
}

void StoreProgress::reject()
{  
    close();
}

void StoreProgress::on_timeout()
{   
    //The task is end, to close the window.
    if (_store_session->is_busy() == false) {
        close();      
    }
}

void StoreProgress::accept()
{
    if (_store_session->GetFileName() == ""){
        MsgBox::Show(NULL, L_S(STR_PAGE_MSG, S_ID(IDS_MSG_SEL_FILENAME), "You need to select a file name."));
        return;
    }

    if (_is_done){
        return;
    }

    if (_isExport && _store_session->IsLogicDataType()){
        bool ck  = _ckOrigin->isChecked();
        AppConfig &app = AppConfig::Instance();
        if (app.appOptions.originalData != ck){
            app.appOptions.originalData = ck;
            app.SaveApp();
        }
    }

    // Get data range
    if (_store_session->IsLogicDataType() && _view != NULL)
    {
        uint64_t start_index = 0;
        uint64_t end_index = 0;

        auto &cursor_list = _view->get_cursorList();

        int dex1 = _start_cursor->currentIndex();
        int dex2 = _end_cursor->currentIndex();

        if (dex1 > 0)
        {   
            auto c = _view->get_cursor_by_index(dex1-1);
            assert(c);
            start_index = c->get_index();
        }

        if (dex2 > 0){
            auto c = _view->get_cursor_by_index(dex2-1);
            assert(c);
            end_index = c->get_index();
        }

        if (dex1 > 0 && dex2 > 0)
        {
            if (dex1 == dex2){
                QString mStr = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_DATA_RANGE_ERROR), "Data range error");
                MsgBox::Show(mStr);
                return;
            }

            uint64_t total_count = _view->session().get_ring_sample_count();

            if (start_index > total_count && end_index > total_count)
            {
                QString mStr = L_S(STR_PAGE_MSG, S_ID(IDS_MSG_DATA_RANGE_HAVE_NO_DATA), "No data in the data range");
                MsgBox::Show(mStr);
                return;
            }
            
            if (start_index > end_index){
                uint64_t tmp = start_index;
                start_index = end_index;
                end_index = tmp;
            }
        }

        _store_session->SetDataRange(start_index, end_index);
    }

    _progress->setVisible(true);
    _fileLab->setVisible(false);     
    _fileLab->setVisible(false);
    _openButton->setVisible(false);

    if (_ckOrigin != NULL){
        _ckOrigin->setVisible(false);
        _ckCompress->setVisible(false);
    }
    _space->setVisible(true);

    //start done 
    if (_isExport){
        if (_store_session->export_start()){
            _is_done = true;
            _store_session->session()->set_saving(true);             
            setTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_EXPORTING), "Exporting...")); 
            m_timer.start();
        }
    }
    else{        
        if (_store_session->save_start()){
            _is_done = true;
            _store_session->session()->set_saving(true); 
            setTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SAVING), "Saving..."));
            m_timer.start();
        }
    }
   
   if (!_is_done){
        show_error();
        close();
   }
}

void StoreProgress::save_run(ISessionDataGetter *getter)
{
    _isExport = false;
    setTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SAVE), "Save"));
    QString file = _store_session->MakeSaveFile(false);
    _fileLab->setText(file); 
    _store_session->_sessionDataGetter = getter;

    if (_store_session->IsLogicDataType() && _view != NULL)
    {
        QFormLayout *lay = new QFormLayout();
        lay->setContentsMargins(5, 0, 0, 0); 
        _start_cursor = new DsComboBox();
        _end_cursor = new DsComboBox();
   
        _start_cursor->addItem("-");
        _end_cursor->addItem("-");
        
        auto &cursor_list = _view->get_cursorList();

        for (int i=0; i<cursor_list.size(); i++){
            //tr
            QString cursor_name = L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CURSOR), "Cursor") + 
                                QString::number(i+1);
            _start_cursor->addItem(cursor_name);
            _end_cursor->addItem(cursor_name);
        }

        lay->addRow(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_START_CURSOR), "Start") , _start_cursor);
        lay->addRow(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_END_CURSOR), "End"), _end_cursor);
        _grid->addLayout(lay, 2, 0, 1, 2);
    }

    show();  
}

void StoreProgress::export_run()
{
    if (_store_session->IsLogicDataType())
    { 
        QFormLayout *lay = new QFormLayout();
        lay->setContentsMargins(5, 0, 0, 0); 
        bool isOrg = AppConfig::Instance().appOptions.originalData;

        _ckOrigin  = new QRadioButton();
        _ckOrigin->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_ORIGINAL_DATA), "Original data"));   
        _ckOrigin->setChecked(isOrg);       

        _ckCompress  = new QRadioButton();
        _ckCompress->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_COMPRESSED_DATA), "Compressed data"));
        _ckCompress->setChecked(!isOrg);

        _start_cursor = new DsComboBox();
        _end_cursor = new DsComboBox();
   
        _start_cursor->addItem("-");
        _end_cursor->addItem("-");
        
        auto &cursor_list = _view->get_cursorList();
        
        for (int i=0; i<cursor_list.size(); i++){
            //tr
            QString cursor_name = L_S(STR_PAGE_DLG, S_ID(IDS_DLG_CURSOR), "Cursor") + 
                                QString::number(i+1);
            _start_cursor->addItem(cursor_name);
            _end_cursor->addItem(cursor_name);
        }

        lay->addRow(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_START_CURSOR), "Start") , _start_cursor);
        lay->addRow(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_END_CURSOR), "End"), _end_cursor);

        QWidget *space = new QWidget();
        space->setFixedHeight(5);
        lay->addRow(space);

        lay->addRow("",_ckOrigin);
        lay->addRow("", _ckCompress);
        _grid->addLayout(lay, 2, 0, 1, 2);

        connect(_ckOrigin, SIGNAL(clicked(bool)), this, SLOT(on_ck_origin(bool)));
        connect(_ckCompress, SIGNAL(clicked(bool)), this, SLOT(on_ck_compress(bool)));
    }

    _isExport = true;
    setTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_EXPORT), "Export"));
    QString file = _store_session->MakeExportFile(false);
    _fileLab->setText(file); 

    if (_ckOrigin != NULL){
        bool bFlag = file.endsWith(".csv");
        _ckOrigin->setVisible(bFlag);
        _ckCompress->setVisible(bFlag);
    }

    show();
}

void StoreProgress::show_error()
{
    if (!_store_session->error().isEmpty()) { 
        MsgBox::Show(NULL, _store_session->error().toStdString().c_str(), NULL);
    }
}

void StoreProgress::on_progress_updated()
{
    uint64_t writed = 0;
    uint64_t total = 0;

    _store_session->get_progress(&writed, &total);

    if (writed < total){
        int percent = writed * 1.0 / total * 100.0;
        _progress->setValue(percent);
    }
    else{
        _progress->setValue(100);
    }

    const QString err = _store_session->error();
	if (!err.isEmpty()) {
		show_error();
	}
}

void StoreProgress::on_change_file()
{
    QString file  = "";
    if (_isExport)
        file = _store_session->MakeExportFile(true);
    else
        file = _store_session->MakeSaveFile(true);

    if (file != "")
    {
        _fileLab->setText(file); 

#ifdef _WIN32
        _file_path = file;

        QTimer::singleShot(300, this, [this](){
            _fileLab->setText(_file_path); 
        }); 
#endif

        if (_ckOrigin != NULL){
            bool bFlag = file.endsWith(".csv");
            _ckOrigin->setVisible(bFlag);
            _ckCompress->setVisible(bFlag);
        }
    }          
}

void StoreProgress::on_ck_origin(bool ck)
{
    if (ck){
        _ckCompress->setChecked(false);
    }
}

void StoreProgress::on_ck_compress(bool ck)
{
    if (ck){
        _ckOrigin->setChecked(false);
    }
}

} // dialogs
} // pv
