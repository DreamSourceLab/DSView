/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 * 
 * Copyright (C) 2021 DreamSourceLab <support@dreamsourcelab.com>
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

#pragma once

#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QString>
#include "../ui/dscombobox.h"

class DecoderStatus;
 
namespace pv{
namespace dock{

class IProtocolItemLayerCallback
 {
    public:
        virtual void OnProtocolSetting(void *handle)=0;
        virtual void OnProtocolDelete(void *handle)=0;
        virtual void OnProtocolFormatChanged(QString format, void *handle)=0;
};

class ProtocolItemLayer: public QHBoxLayout
{
    Q_OBJECT

public:
    ProtocolItemLayer(QWidget *parent, QString protocolName, IProtocolItemLayerCallback *callback);
    ~ProtocolItemLayer();

    void SetProgress(int progress, QString text);
    void ResetStyle();
    void LoadFormatSelect(bool bSingle);
    inline QString &GetProtocolName(){return _protocolName;}
    void SetProtocolFormat(const char *format);
  
private slots: 
    void on_set_protocol(); 
    void on_del_protocol();
    void on_format_select_changed(int index);

public:
    DecoderStatus *m_decoderStatus; //DecoderStatus
  
private:
    QLabel *_protocol_label;
    QLabel *_progress_label;
    QPushButton *_set_button;
    QPushButton *_del_button;
    DsComboBox  *_format_combox;
    IProtocolItemLayerCallback *m_callback;
    QString _protocolName;
    bool    m_bSetting;
    bool    m_singleFlag;
};

 } //dock
} //pv
