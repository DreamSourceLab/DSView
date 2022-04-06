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

#include "appconfig.h" 
#include <QApplication>
#include <QSettings>
#include <QLocale>
#include <QDir>
#include <QDebug>
#include <assert.h>
#include <QStandardPaths>
  
#define MAX_PROTOCOL_FORMAT_LIST 15

StringPair::StringPair(const std::string &key, const std::string &value)
{
    m_key = key;
    m_value = value;
}

//------------function
QString FormatArrayToString(std::vector<StringPair> &protocolFormats){
    QString str;

    for (StringPair &o : protocolFormats){
         if (!str.isEmpty()){
             str += ";";
         } 
         str += o.m_key.c_str();
         str += "=";
         str += o.m_value.c_str(); 
    }

    return str;
}

void StringToFormatArray(const QString &str, std::vector<StringPair> &protocolFormats){
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

//read write field

void getFiled(const char *key, QSettings &st, QString &f, const char *dv){
    f = st.value(key, dv).toString();
}
void getFiled(const char *key, QSettings &st, int &f, int dv){
    f = st.value(key, dv).toInt();
}
void getFiled(const char *key, QSettings &st, bool &f, bool dv){
    f = st.value(key, dv).toBool();
}
 
void setFiled(const char *key, QSettings &st, QString f){
    st.setValue(key, f);
}
void setFiled(const char *key, QSettings &st, int f){
    st.setValue(key, f);
}
void setFiled(const char *key, QSettings &st, bool f){
    st.setValue(key, f);
}


///------ app
void _loadApp(AppOptions &o, QSettings &st){
    st.beginGroup("Application"); 
    getFiled("quickScroll", st, o.quickScroll, true);
    getFiled("warnofMultiTrig", st, o.warnofMultiTrig, true);
    getFiled("originalData", st, o.originalData, false);

    QString fmt;
    getFiled("protocalFormats", st, fmt, "");
    if (fmt != ""){
         StringToFormatArray(fmt, o.m_protocolFormats);
    }
   
    st.endGroup();  
}

void _saveApp(AppOptions &o, QSettings &st){
    st.beginGroup("Application");
    setFiled("quickScroll", st, o.quickScroll);
    setFiled("warnofMultiTrig", st, o.warnofMultiTrig);
    setFiled("originalData", st, o.originalData);

    QString fmt =  FormatArrayToString(o.m_protocolFormats);
    setFiled("protocalFormats", st, fmt);
    st.endGroup();  
}

//-----frame

void _loadDockOptions(DockOptions &o, QSettings &st, const char *group){
    st.beginGroup(group);
    getFiled("decodeDoc", st, o.decodeDoc, false);
    getFiled("triggerDoc", st, o.triggerDoc, false);
    getFiled("measureDoc", st, o.measureDoc, false);
    getFiled("searchDoc", st, o.searchDoc, false);
    st.endGroup();
}

void _saveDockOptions(DockOptions &o, QSettings &st, const char *group){
    st.beginGroup(group);
    setFiled("decodeDoc", st, o.decodeDoc);
    setFiled("triggerDoc", st, o.triggerDoc);
    setFiled("measureDoc", st, o.measureDoc);
    setFiled("searchDoc", st, o.searchDoc);
    st.endGroup();
}

void _loadFrame(FrameOptions &o, QSettings &st){
    st.beginGroup("MainFrame"); 
    getFiled("style", st, o.style, THEME_STYLE_DARK);
    getFiled("language", st, o.language, -1);
    getFiled("isMax", st, o.isMax, false);  
    getFiled("left", st, o.left, 0);
    getFiled("top", st, o.top, 0);
    getFiled("right", st, o.right, 0);
    getFiled("bottom", st, o.bottom, 0);

    _loadDockOptions(o._logicDock, st, "LOGIC_DOCK");
    _loadDockOptions(o._analogDock, st, "ANALOG_DOCK");
    _loadDockOptions(o._dsoDock, st, "DSO_DOCK");

    o.windowState = st.value("windowState", QByteArray()).toByteArray();
    st.endGroup();

    if (o.language == -1 || (o.language != LAN_CN && o.language != LAN_EN)){
        //get local language
        QLocale locale;

        if (QLocale::languageToString(locale.language()) == "Chinese")
            o.language = LAN_CN;            
        else
            o.language = LAN_EN; 
    }
}

void _saveFrame(FrameOptions &o, QSettings &st){
    st.beginGroup("MainFrame");
    setFiled("style", st, o.style);
    setFiled("language", st, o.language);
    setFiled("isMax", st, o.isMax);  
    setFiled("left", st, o.left);
    setFiled("top", st, o.top);
    setFiled("right", st, o.right);
    setFiled("bottom", st, o.bottom);
    st.setValue("windowState", o.windowState); 

    _saveDockOptions(o._logicDock, st, "LOGIC_DOCK");
    _saveDockOptions(o._analogDock, st, "ANALOG_DOCK");
    _saveDockOptions(o._dsoDock, st, "DSO_DOCK");
    
    st.endGroup();
}

//------history
void _loadHistory(UserHistory &o, QSettings &st){
    st.beginGroup("UserHistory");
    getFiled("exportDir", st, o.exportDir, ""); 
    getFiled("saveDir", st, o.saveDir, ""); 
    getFiled("showDocuments", st, o.showDocuments, true);
    getFiled("screenShotPath", st, o.screenShotPath, ""); 
    getFiled("sessionDir", st, o.sessionDir, ""); 
    getFiled("openDir", st, o.openDir, ""); 
    getFiled("protocolExportPath", st, o.protocolExportPath, ""); 
    getFiled("exportFormat", st, o.exportFormat, ""); 
    st.endGroup();
}
 
void _saveHistory(UserHistory &o, QSettings &st){
    st.beginGroup("UserHistory");
    setFiled("exportDir", st, o.exportDir); 
    setFiled("saveDir", st, o.saveDir); 
    setFiled("showDocuments", st, o.showDocuments); 
    setFiled("screenShotPath", st, o.screenShotPath); 
    setFiled("sessionDir", st, o.sessionDir); 
    setFiled("openDir", st, o.openDir); 
    setFiled("protocolExportPath", st, o.protocolExportPath);
    setFiled("exportFormat", st, o.exportFormat); 
    st.endGroup();
}

//------------AppConfig

AppConfig::AppConfig()
{ 
}

AppConfig::AppConfig(AppConfig &o) 
{
    (void)o;
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

void AppConfig::LoadAll()
{   
    QSettings st(QApplication::organizationName(), QApplication::applicationName());
    _loadApp(_appOptions, st);
    _loadHistory(_userHistory, st);
    _loadFrame(_frameOptions, st);
}

  void AppConfig::SaveApp()
  {
      QSettings st(QApplication::organizationName(), QApplication::applicationName());
      _saveApp(_appOptions, st);
  }

  void AppConfig::SaveHistory()
  {
      QSettings st(QApplication::organizationName(), QApplication::applicationName());
       _saveHistory(_userHistory, st);
  }

  void AppConfig::SaveFrame()
  {
      QSettings st(QApplication::organizationName(), QApplication::applicationName());
      _saveFrame(_frameOptions, st);
  }
 

void AppConfig::SetProtocolFormat(const std::string &protocolName, const std::string &value)
{
    bool bChange = false;
    for (StringPair &o : _appOptions.m_protocolFormats){
        if (o.m_key == protocolName){
            o.m_value = value;
            bChange = true;
            break;
        }    
    }

    if (!bChange)
    {
        if (_appOptions.m_protocolFormats.size() > MAX_PROTOCOL_FORMAT_LIST)
        {
            while (_appOptions.m_protocolFormats.size() < MAX_PROTOCOL_FORMAT_LIST)
            {
                _appOptions.m_protocolFormats.erase(_appOptions.m_protocolFormats.begin());
            }
        }
        _appOptions.m_protocolFormats.push_back(StringPair(protocolName, value));
        bChange = true;
    }

    if (bChange){
        SaveApp();
    }
}

std::string AppConfig::GetProtocolFormat(const std::string &protocolName)
{
     for (StringPair &o : _appOptions.m_protocolFormats){
        if (o.m_key == protocolName){ 
            return o.m_value;
        }
    }
    return "";
}

//-------------api
QString GetDirectoryName(QString path)
{
    int lstdex = path.lastIndexOf('/');
    if (lstdex != -1)
    {
        return path.left(lstdex);
    }
    return path;
}

QString GetIconPath()
{   
    QString style = AppConfig::Instance()._frameOptions.style;
    if (style == ""){
        style = THEME_STYLE_DARK;
    }
    return ":/icons/" + style;
}

QString GetAppDataDir()
{
//applicationDirPath not end with '/'
#ifdef Q_OS_LINUX
    QDir dir(QCoreApplication::applicationDirPath());
    if (dir.cd("..") && dir.cd("share") && dir.cd("DSView"))
    {
         return dir.absolutePath();        
    }
    QDir dir1("/usr/local/share/DSView");
    if (dir1.exists()){
        return dir1.absolutePath();
    }
    qDebug() << "dir is not exists:" <<"../share/DSView";
    assert(false);   
#else
    return QCoreApplication::applicationDirPath();
#endif
}

QString GetResourceDir(){
    QDir dir =  GetAppDataDir() + "/res";
    if (dir.exists()){
        return dir.absolutePath();
    }
    qDebug() << "app data dir is not exists:" << dir.absolutePath();
    assert(false);
}

QString GetUserDataDir()
{
    #if QT_VERSION >= 0x050400
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    #else
    return QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    #endif
}

QString GetDecodeScriptDir()
{
    QString path = GetAppDataDir() + "/decoders";

    #ifdef Q_OS_LINUX
    QDir dir1;
    // ../share/DSView/decoders
    if (dir1.exists(path))
    {
         return path;     
    }  

    QDir dir(QCoreApplication::applicationDirPath());
    // ../share/libsigrokdecode4DSL/decoders
    if (dir.cd("..") && dir.cd("share") && dir.cd("libsigrokdecode4DSL") && dir.cd("decoders"))
    {
         return dir.absolutePath();        
    } 
#endif

 return path;
}
