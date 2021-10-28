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

#pragma once

#include <string>
#include <vector>
#include <QString>
#include <QByteArray>

using namespace std;

//--------------------api

QString GetDirectoryName(QString path);

QString GetIconPath();

//------------------class
  
class StringPair
{
public:
   StringPair(const string &key, const string &value);
   string m_key;
   string m_value;
};

struct AppOptions
{   
    bool  quickScroll;
    bool  warnofMultiTrig;
    bool  originalData;

    vector<StringPair> m_protocolFormats;
};
 
struct FrameOptions
{ 
  QString     style;
  int         language;
  QByteArray  geometry;
  bool        isMax;
  QByteArray  windowState;
};

struct UserHistory
{ 
  QString   exportDir;
  QString   saveDir;
  bool      showDocuments;
  QString   screenShotPath;
  QString   sessionDir;
  QString   openDir;
  QString   protocolExportPath;
  QString   exportFormat;
};

class AppConfig
{
private:
  AppConfig();
  ~AppConfig();

public:
  static AppConfig &Instance();

  void LoadAll();
  void SaveApp();  
  void SaveHistory();
  void SaveFrame();
  
  void SetProtocolFormat(const string &protocolName, const string &value);
  string GetProtocolFormat(const string &protocolName); 

public:
  AppOptions    _appOptions;
  UserHistory   _userHistory;
  FrameOptions  _frameOptions; 
};
