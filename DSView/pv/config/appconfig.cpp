/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include "appconfig.h"
#include <assert.h>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QStringList>

#define MAX_PROTOCOL_FORMAT_LIST 15

StringPair::StringPair(const string &key, const string &value)
{
    m_key = key;
    m_value = value;
}

//------------function
QString FormatArrayToString(vector<StringPair> &protocolFormats){
    QString str;

    for (StringPair &o : protocolFormats){
         if (!str.isEmpty()){
             str += ";";
         }
         std::string line;
         line += o.m_key;
         line += "=";
         line += o.m_value;
         str += line.c_str();
    }

    return str;
}

void StringToFormatArray(const QString &str, vector<StringPair> &protocolFormats){
    QStringList arr = str.split(";");
    for (int i=0; i<arr.size(); i++){
        QString line = arr[i];
        if (!line.isEmpty()){
            QStringList vs = line.split("=");
            if (vs.size() == 2){
                protocolFormats.push_back(StringPair(vs[0].toStdString(), vs[1].toStdString()));
            }
        }
    }
}

//------------AppConfig

AppConfig::AppConfig()
{
    
}

AppConfig::~AppConfig()
{
}

 AppConfig& AppConfig::Instance()
 {
     static AppConfig *ins = NULL;
     if (ins == NULL){
         ins = new AppConfig();
     }
     return *ins;
 }

bool AppConfig::Load(const char *file)
{
    assert(file);
    m_fileName = file;

    QFile qf(file);
    if (qf.open(QIODevice::ReadOnly))
    {
        QByteArray bytes = qf.readAll();
        qf.close();

        string json;
        json.append(bytes.data(), bytes.size());
        FromJson(json);
        return true;
    }

    return false;
}

bool AppConfig::Save()
{
    if (m_fileName != "")
    { 
        QFile qf(m_fileName.c_str());
        if (qf.open(QIODevice::WriteOnly | QIODevice::Text))
        { 
            string json = ToJsonString();
            QByteArray bytes(json.c_str(), json.size());

            QTextStream _stream(&qf);
            _stream.setCodec("UTF-8");
            _stream << QString::fromUtf8(bytes);
            qf.close();

            return true;
        }
    }
    return false;
}

  string AppConfig::ToJsonString()
  {
      QJsonObject jobj; 
      QString  profomats = FormatArrayToString(m_protocolFormats);
      jobj["ProtocolFormats"] = QJsonValue::fromVariant(profomats);

      QJsonDocument jdoc(jobj);
      QByteArray bytes = jdoc.toJson();
      string json;
      json.append(bytes.data(), bytes.size());
      return json;  
  }

  void AppConfig::FromJson(string &json)
  {
      if (!json.size())
        return;

      QByteArray bytes(json.c_str(), json.size());
      QJsonDocument jdoc = QJsonDocument::fromJson(bytes);
      QJsonObject jobj = jdoc.object();

      if (jobj.contains("ProtocolFormats")){
          m_protocolFormats.clear();
          StringToFormatArray(jobj["ProtocolFormats"].toString(), m_protocolFormats);
      } 
  }

void AppConfig::SetProtocolFormat(const string &protocolName, const string &value)
{
    for (StringPair &o : m_protocolFormats){
        if (o.m_key == protocolName){
            o.m_value = value;
            return;
        }    
    }
    if (m_protocolFormats.size() > MAX_PROTOCOL_FORMAT_LIST){
        while (m_protocolFormats.size() < MAX_PROTOCOL_FORMAT_LIST)
        {
           m_protocolFormats.erase(m_protocolFormats.begin());
        }
    }

    m_protocolFormats.push_back(StringPair(protocolName,value));    
}

string AppConfig::GetProtocolFormat(const string &protocolName)
{
     for (StringPair &o : m_protocolFormats){
        if (o.m_key == protocolName){ 
            return o.m_value;
        }
    }
    return "";
}
