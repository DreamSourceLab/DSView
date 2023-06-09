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
#include <QTimer>
#include <QTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include "../ui/msgbox.h"
#include "../config/appconfig.h"
#include "../interface/icallbacks.h"
#include "../log.h"

#include "../ui/langresource.h"

namespace pv {
namespace dialogs {

StoreProgress::StoreProgress(SigSession *session, QWidget *parent) :
    DSDialog(parent),
    _store_session(session)
{
    _fileLab = NULL;
    _ckOrigin = NULL;

    this->setMinimumSize(550, 220);
    this->setModal(true);
 
    _progress.setValue(0);
    _progress.setMaximum(100);

    _isExport = false;
    _done = false;
    _isBusy = false;

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

    grid->addWidget(&_progress, 0, 0, 1, 4);
    grid->addWidget(_fileLab, 1, 0, 1, 3);
    grid->addWidget(_openButton, 1, 3, 1, 1);    
    grid->addWidget(_space);

    QDialogButtonBox  *_button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, 
                                        Qt::Horizontal, this);
    grid->addWidget(_button_box, 2, 2, 1, 2, Qt::AlignRight | Qt::AlignBottom);

    layout()->addLayout(grid);

    connect(_button_box, SIGNAL(rejected()), this, SLOT(reject()));
    connect(_button_box, SIGNAL(accepted()), this, SLOT(accept()));

    connect(&_store_session, SIGNAL(progress_updated()),
        this, SLOT(on_progress_updated()), Qt::QueuedConnection);

    connect(_openButton, SIGNAL(clicked()),this, SLOT(on_change_file()));

    _progress.setVisible(false);
}

StoreProgress::~StoreProgress()
{
    _store_session.wait();
}

 void StoreProgress::on_change_file()
 {
    QString file  = "";
    if (_isExport)
        file = _store_session.MakeExportFile(true);
    else
        file = _store_session.MakeSaveFile(true);

    if (file != ""){
        _fileLab->setText(file); 

        if (_ckOrigin != NULL){
            bool bFlag = file.endsWith(".csv");
            _ckOrigin->setVisible(bFlag);
            _ckCompress->setVisible(bFlag);
        }
    }          
 }

void StoreProgress::reject()
{
    using namespace Qt;
    _store_session.cancel();
    _store_session.session()->set_saving(false);
    save_done(); 
    DSDialog::reject();
    _store_session.session()->broadcast_msg(DSV_MSG_SAVE_COMPLETE);
}

void StoreProgress::accept()
{
    if (_store_session.GetFileName() == ""){
        MsgBox::Show(NULL, L_S(STR_PAGE_MSG, S_ID(IDS_MSG_SEL_FILENAME), "You need to select a file name."));
        return;
    }

    if (_isBusy)
        return;
    
     _progress.setVisible(true);
     _fileLab->setVisible(false);     
     _fileLab->setVisible(false);
     _openButton->setVisible(false);

     if (_ckOrigin != NULL){
        _ckOrigin->setVisible(false);
        _ckCompress->setVisible(false);
     }
     _space->setVisible(true);


    if (_isExport && _store_session.IsLogicDataType()){
        bool ck  = _ckOrigin->isChecked();
        AppConfig &app = AppConfig::Instance();
        if (app.appOptions.originalData != ck){
            app.appOptions.originalData = ck;
            app.SaveApp();
        }
    }

    //start done 
    if (_isExport){
        if (_store_session.export_start()){
            _isBusy = true;
            _store_session.session()->set_saving(true);
            QTimer::singleShot(100, this, SLOT(timeout()));
            setTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_EXPORTING), "Exporting..."));    
        }
        else{
            save_done();
            close(); 
            show_error();
        }
    }
    else{
         if (_store_session.save_start()){
            _isBusy = true;
            _store_session.session()->set_saving(true);
            QTimer::singleShot(100, this, SLOT(timeout()));
            setTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SAVING), "Saving..."));
        }
        else{
            save_done();
            close(); 
            show_error();
        }
    }
    //do not to call base class method, otherwise the window will be closed;
}

void StoreProgress::timeout()
{
    if (_done) {
        _store_session.session()->set_saving(false);
        save_done();
        close(); 
        delete this;
        
    } else {
        QTimer::singleShot(100, this, SLOT(timeout()));
    }
}

void StoreProgress::save_run(ISessionDataGetter *getter)
{
    _isExport = false;
    setTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_SAVE), "Save"));
    QString file = _store_session.MakeSaveFile(false);
    _fileLab->setText(file); 
    _store_session._sessionDataGetter = getter;
    show();  
}

void StoreProgress::export_run()
{
    if (_store_session.IsLogicDataType())
    { 
        QGridLayout *lay = new QGridLayout();
        lay->setContentsMargins(5, 0, 0, 0); 
        bool isOrg = AppConfig::Instance().appOptions.originalData;

        _ckOrigin  = new QRadioButton();
        _ckOrigin->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_ORIGINAL_DATA), "Original data"));   
        _ckOrigin->setChecked(isOrg);       

        _ckCompress  = new QRadioButton();
        _ckCompress->setText(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_COMPRESSED_DATA), "Compressed data"));
        _ckCompress->setChecked(!isOrg);

        lay->addWidget(_ckOrigin);
        lay->addWidget(_ckCompress);
        _grid->addLayout(lay, 2, 0, 1, 2);

        connect(_ckOrigin, SIGNAL(clicked(bool)), this, SLOT(on_ck_origin(bool)));
        connect(_ckCompress, SIGNAL(clicked(bool)), this, SLOT(on_ck_compress(bool)));
    }

    _isExport = true;
    setTitle(L_S(STR_PAGE_DLG, S_ID(IDS_DLG_EXPORT), "Export"));
    QString file = _store_session.MakeExportFile(false);
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
    _done = true;
    if (!_store_session.error().isEmpty()) { 
        MsgBox::Show(NULL, _store_session.error().toStdString().c_str(), NULL);
    }
}

void StoreProgress::closeEvent(QCloseEvent* e)
{ 
    _store_session.cancel();
    _store_session.session()->set_saving(false);
    DSDialog::closeEvent(e);
    _store_session.session()->broadcast_msg(DSV_MSG_SAVE_COMPLETE);
}

void StoreProgress::on_progress_updated()
{
    const std::pair<uint64_t, uint64_t> p = _store_session.progress();
	assert(p.first <= p.second);
    int percent = p.first * 1.0 / p.second * 100;
    _progress.setValue(percent);

    const QString err = _store_session.error();
	if (!err.isEmpty()) {
		show_error();
	}

    if (p.first == p.second) {
        _done = true;
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
