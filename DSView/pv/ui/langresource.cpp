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

void Lang_resource_page::Clear()
{
    _res.clear();
}

//---------------LangResource

LangResource::LangResource()
{
    _current_page = NULL;
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
    char *lan_name = NULL;

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
    int num = sizeof(lang_id_keys) / sizeof(lang_key_item);
    const char *lan_name = get_lang_key(lang);

    if (lan_name == NULL)
    {
        dsv_err("Can't find language key,lang:%d", lang);
        return false;
    }

    Release();

    num = sizeof(lange_page_keys) / sizeof(lang_page_item);

    for (int i = 0; i < num; i++)
    { 
        Lang_resource_page *p = new Lang_resource_page();
        p->_id = lange_page_keys[i].id;
        p->_source = lange_page_keys[i].source;
        _pages.push_back(p);

        QString file = GetAppDataDir() + "/lang/" + QString(lan_name) + "/" + p->_source;
        load_page(*p, file);
    }

    return false;
}

void LangResource::Release()
{
    for (Lang_resource_page *p : _pages)
    {
        p->Clear();
        delete p;
    }
    _pages.clear();
}

void LangResource::load_page(Lang_resource_page &p, QString file)
{ 
    QFile f(file);
    if (f.exists() == false){
        dsv_warn("Warning:Language source file is not exists: %s", file.toLocal8Bit().data());
        return;
    }
    f.open(QFile::ReadOnly | QFile::Text);
    QByteArray raw_bytes = f.readAll();
    f.close();

    if (raw_bytes.length() == 0)
        return;

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
            QString text = obj["text"].toString().trimmed();
            p._res[id.toStdString()] = text.toStdString();
        }
    }
}

const char* LangResource::get_lang_text(int page_id, const char *str_id, const char *default_str)
{
    assert(str_id);

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
        dsv_warn("Warning:Cant find language source page:%d", page_id);
        return default_str;
    }

    auto it = _current_page->_res.find(std::string(str_id));
    if (it != _current_page->_res.end()){
        return (*it).second.c_str();
    }
    else{
        dsv_warn("Warning:Cant't get language text:%s", str_id);
    }

    return default_str;
}