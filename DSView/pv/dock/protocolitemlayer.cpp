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

#include "protocolitemlayer.h"
#include "../dsvdef.h"
#include <assert.h> 
#include "../config/appconfig.h"
 
namespace pv {
namespace dock {
  
ProtocolItemLayer::ProtocolItemLayer(QWidget *parent, QString protocolName, IProtocolItemLayerCallback *callback){
        assert(parent);
        assert(callback);

        m_callback = callback;
        _protocolName = protocolName;
        m_bSetting = false;

        _protocol_label = new QLabel(parent);
        _progress_label = new QLabel(parent);
        _set_button = new QPushButton(parent);
        _del_button = new QPushButton(parent);
        _format_combox = new DsComboBox(parent);

        QString iconPath = GetIconPath();
        _del_button->setFlat(true);
        _del_button->setIcon(QIcon(iconPath + "/del.svg"));
        _set_button->setFlat(true);
        _set_button->setIcon(QIcon(iconPath + "/gear.svg"));
        _protocol_label->setText(protocolName);

        m_singleFlag = true;
        m_decoderStatus = NULL;

        LoadFormatSelect(false);
 
        QHBoxLayout *hori_layout = this;
        hori_layout->addWidget(_set_button);
        hori_layout->addWidget(_del_button);
        hori_layout->addWidget(_format_combox);
        hori_layout->addWidget(_protocol_label);
        hori_layout->addWidget(_progress_label);   

        hori_layout->addStretch(1);

        connect(_del_button, SIGNAL(clicked()),this, SLOT(on_del_protocol()));
        
        connect(_set_button, SIGNAL(clicked()),this, SLOT(on_set_protocol()));

        connect(_format_combox, SIGNAL(currentIndexChanged(int)),this, SLOT(on_format_select_changed(int)));
}

ProtocolItemLayer::~ProtocolItemLayer(){ 
     DESTROY_QT_OBJECT(_progress_label);
     DESTROY_QT_OBJECT(_protocol_label);
     DESTROY_QT_OBJECT(_set_button);
     DESTROY_QT_OBJECT(_del_button);
     DESTROY_QT_OBJECT(_format_combox);
}
 

//-------------control event
void ProtocolItemLayer::on_set_protocol()
{
    m_callback->OnProtocolSetting(this);
}

void ProtocolItemLayer::on_del_protocol(){
    m_callback->OnProtocolDelete(this);
}

void ProtocolItemLayer::on_format_select_changed(int index){
    if (index >= 0 && !m_bSetting){
        QString text = _format_combox->currentText();
        m_callback->OnProtocolFormatChanged(text, this);
    } 
}
//-----------------

 void ProtocolItemLayer::SetProgress(int progress, QString text){
      QString str = QString::number(progress) + "%" + text;

       if (progress == 100)
            _progress_label->setStyleSheet("color:green;");
        else
            _progress_label->setStyleSheet("color:red;");
        _progress_label->setText(str);
 }

void ProtocolItemLayer::ResetStyle(){
    QString iconPath = GetIconPath();
     _del_button->setIcon(QIcon(iconPath + "/del.svg"));
    _set_button->setIcon(QIcon(iconPath + "/gear.svg"));
}

void ProtocolItemLayer::LoadFormatSelect(bool bSingle)
{
    if (bSingle == m_singleFlag){
        return;
    }
    m_singleFlag = bSingle;

    m_bSetting = true;
    _format_combox->clear();
    _format_combox->addItem("ascii");
    int dex = 0;

    if (!bSingle)
    {
        _format_combox->addItem("dec");
        _format_combox->addItem("hex");
        _format_combox->addItem("oct");
        _format_combox->addItem("bin");
        dex = 2;
    }
    
    _format_combox->setCurrentIndex(dex);
    m_bSetting = false;
}

 void ProtocolItemLayer::SetProtocolFormat(const char *format)
 {
     assert(format);

     m_bSetting = true;
     int dex = DecoderDataFormat::Parse(format);
     if (dex < (int)_format_combox->count()){
         _format_combox->setCurrentIndex(dex);
     }
     m_bSetting = false;
 }

} //dock
} //pv
