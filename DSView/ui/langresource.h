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

#ifndef LANG_RESOURCE_H
#define LANG_RESOURCE_H

#include <map>
#include <vector>
#include <QString>
#include <string>
#include "string_ids.h"
#include <mutex>

struct lang_key_item
{
    int id;
    const char *name;
};

class Lang_resource_page
{
public:
    Lang_resource_page();
    void Clear();

public:
    int     _id;
    const char   *_source;
    bool    _loaded;
    bool    _released;
    bool    _is_dynamic;
    std::map<std::string, std::string> _res;
    std::map<std::string, std::string> _res_history;
};

struct lang_page_item
{
    int id;
    const char *source;
    bool is_dynamic;
};

static const struct lang_key_item lang_id_keys[] = 
{
    {25, "cn"},
    {31, "en"}
};

static const struct lang_page_item lange_page_keys[] = 
{
    {STR_PAGE_TOOLBAR, "toolbar.json", false},
    {STR_PAGE_MSG, "msg.json", false},
    {STR_PAGE_DLG, "dlg.json", false},
    {STR_PAGE_DSL, "dsl_list.json, dsl_label.json, dsl_channel.json", false},
    {STR_PAGE_DECODER, "dec/0.json,dec/a.json,dec/f.json,dec/k.json,dec/p.json,dec/u.json", true},
};

class LangResource
{
private:
    LangResource();

public:
    static LangResource* Instance();
    bool Load(int lang);
    void Release();
  
    const char* get_lang_text(int page_id, const char *str_id, const char *default_str);
    bool is_new_decoder(const char *decoder_id);
    void reload_dynamic();
    void release_dynamic();

    inline bool is_lang_en(){
        return _cur_lang == 31;
    }
   
private:
    void release_self();
    const char *get_lang_key(int lang);

    void load_page(Lang_resource_page &p);

    void load_page(Lang_resource_page &p, QString file);
 
private:
    std::vector<Lang_resource_page*> _pages;
    Lang_resource_page    *_current_page;
    int     _cur_lang;
    std::map<std::string, int> _query_decoders;
    mutable std::mutex  _mutex;  
};

#define S_ID(id) #id
#define L_S(pid,sid,dv) (LangResource::Instance()->get_lang_text((pid), (sid), (dv)))

#endif