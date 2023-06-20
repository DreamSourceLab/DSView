/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2022 DreamSourceLab <support@dreamsourcelab.com>
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

#include "langresource.h"
#include <stddef.h>
#include "../log.h"
#include "../config/appconfig.h"
#include <QFile>
#include <QByteArray>
#include <QJsonParseError>
#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>
#include <assert.h>

//---------------Lang_resource_page
Lang_resource_page::Lang_resource_page()
{
    _id = -1;
    _source = NULL;
    _loaded = false;
    _released = false;
    _is_dynamic = false;
}

void Lang_resource_page::Clear()
{
    _res.clear();
    _res_history.clear();
}

//---------------LangResource

LangResource::LangResource()
{
    _current_page = NULL;
    _cur_lang = -1;
}

LangResource *LangResource::Instance()
{
    static LangResource *ins = NULL;

    if (ins == NULL)
    {
        ins = new LangResource();
    }

    return ins;
}

const char *LangResource::get_lang_key(int lang)
{
    int num = sizeof(lang_id_keys) / sizeof(lang_key_item);
    const char *lan_name = NULL;

    for (int i = 0; i < num; i++)
    {
        if (lang_id_keys[i].id == lang)
        {
            lan_name = lang_id_keys[i].name;
            break;
        }
    }

    return lan_name;
}

bool LangResource::Load(int lang)
{
    std::lock_guard<std::mutex> lock(_mutex);

    int num = sizeof(lang_id_keys) / sizeof(lang_key_item);
    const char *lan_name = get_lang_key(lang);

    if (lan_name == NULL)
    {
        dsv_err("Can't find language key,lang:%d", lang);
        return false;
    }

    _cur_lang = lang;

    _query_decoders.clear();

    release_self();

    num = sizeof(lange_page_keys) / sizeof(lang_page_item);

    for (int i = 0; i < num; i++)
    { 
        Lang_resource_page *p = new Lang_resource_page();
        p->_id = lange_page_keys[i].id;
        p->_source = lange_page_keys[i].source;
        p->_is_dynamic = lange_page_keys[i].is_dynamic;
        _pages.push_back(p);
    }

    return true;
}

void LangResource::Release()
{
    std::lock_guard<std::mutex> lock(_mutex);
    release_self();
}

void LangResource::release_self()
{ 
    for (Lang_resource_page *p : _pages)
    {
        p->Clear();
        delete p;
    }
    _pages.clear();
}

void LangResource::load_page(Lang_resource_page &p)
{
    if (p._loaded)
        return;
    p._loaded = true;

    const char *lan_name = get_lang_key(_cur_lang);
    if (lan_name == NULL){
        dsv_err("Can't find language key,lang:%d", _cur_lang);
        return;
    }

    QString fileNmae(p._source);
    QStringList files = fileNmae.split(",");
        
    for (int x=0; x<files.count(); x++){
        QString file = GetAppDataDir() + "/lang/" + QString(lan_name) + "/" + files[x].trimmed();
        load_page(p, file);
    }
}

void LangResource::load_page(Lang_resource_page &p, QString file)
{ 
    QFile f(file);
    if (f.exists() == false){
        if (_cur_lang != LAN_EN && p._is_dynamic == false)
            dsv_warn("Warning:Language source file is not exists: %s", file.toLocal8Bit().data());
        return;
    }
    f.open(QFile::ReadOnly | QFile::Text);
    QByteArray raw_bytes = f.readAll();
    f.close();

    if (raw_bytes.length() == 0)
        return;

    //dsv_info("Load lang resouce file: %s", file.toLocal8Bit().data());

    QJsonParseError error;
    QString jsonStr(raw_bytes.data());
    QByteArray qbs = jsonStr.toUtf8();
    QJsonDocument doc = QJsonDocument::fromJson(qbs, &error);

    if (error.error != QJsonParseError::NoError)
    {
        QString estr = error.errorString();
        dsv_err("LangResource::load_page(), parse json error:\"%s\"!", estr.toUtf8().data());
        return;
    }

    QJsonArray jarray = doc.array();

    for (const QJsonValue &dec_value : jarray)
    {
        QJsonObject obj = dec_value.toObject();

        if (obj.contains("id") && obj.contains("text")){
            QString id = obj["id"].toString().trimmed();
            QString text = obj["text"].toString();
            p._res[id.toStdString()] = text.toStdString();
        }
    }
}

const char* LangResource::get_lang_text(int page_id, const char *str_id, const char *default_str)
{
    assert(str_id);
    assert(default_str); 

    std::lock_guard<std::mutex> lock(_mutex);

    if (*str_id == '\0' || *default_str == '\0'){
        dsv_err("LangResource::get_lang_text(), param is empty.");
        assert(false);
    }

    if (_current_page == NULL || _current_page->_id != page_id){
        _current_page = NULL; 
        for (Lang_resource_page *p : _pages){
            if (p->_id == page_id){
                _current_page = p;
                break;
            }
        }
    }

    if (_current_page == NULL){
        if (_cur_lang != LAN_EN)
            dsv_warn("Warning:Can't find language source page:%d", page_id);
        return default_str;
    }

    if (_current_page->_loaded == false)
        load_page(*_current_page);

    std::string key(str_id);

    if (_current_page->_released){
        auto it = _current_page->_res_history.find(key);
        if (it != _current_page->_res_history.end()){
            return (*it).second.c_str();
        }
    }
    else{
        auto it = _current_page->_res.find(key);
        if (it != _current_page->_res.end()){
            if (_current_page->_is_dynamic){
                _current_page->_res_history[key] = (*it).second; //Save to history list.
            }
            return (*it).second.c_str();
        }
    }   
   
    if(_cur_lang != LAN_EN){
        dsv_warn("Warning:Can't get language text:%s", str_id);
    }

    return default_str;
}

bool LangResource::is_new_decoder(const char *decoder_id)
{
    std::lock_guard<std::mutex> lock(_mutex);

    std::string key(decoder_id);
    if (_query_decoders.find(key) == _query_decoders.end()){
        _query_decoders[key] = 1;
        return true;
    }

    return false;
}

void LangResource::reload_dynamic()
{
    std::lock_guard<std::mutex> lock(_mutex);

    for (Lang_resource_page *p : _pages)
    {
        if (p->_is_dynamic){
            p->_released = false;
            p->_loaded = false;
            load_page(*p);
        }
    }
}

void LangResource::release_dynamic()
{
    std::lock_guard<std::mutex> lock(_mutex);

    for (Lang_resource_page *p : _pages)
    {
        if (p->_is_dynamic){
            p->_res.clear();
            p->_released = true;
        }
    }
}
