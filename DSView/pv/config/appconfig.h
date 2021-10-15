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

using namespace std;

struct app_status{


};


class StringPair
{
public:
   StringPair(const string &key, const string &value);
   string m_key;
   string m_value;
};

class config_data
{
  public:

    
};

class AppConfig
{
private:
  AppConfig();

  ~AppConfig();

public:
  static AppConfig &Instance();
  bool Load(const char *file);
  bool Save();
  string ToJsonString();
  void FromJson(string &json);

  void SetProtocolFormat(const string &protocolName, const string &value);
  string GetProtocolFormat(const string &protocolName);  

public:
  config_data m_data;
  app_status  m_status;

private:
  string m_fileName;
  vector<StringPair> m_protocolFormats;
};
