/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2023 DreamSourceLab <support@dreamsourcelab.com>
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

#include "langitemlist.h"
#include <assert.h>
#include "langresource.h"

LangItemList::LangItemList()
{

}

LangItemList::~LangItemList()
{
   Clear();
}

void LangItemList::Clear()
{
    for(auto it = m_items.begin(); it != m_items.end(); it++)
    {
        delete (*it).setter;
    }
    m_items.clear();
}

void LangItemList::AddItem(int page_id, const char *key_id, const char *defaultStr, ILangSetter *setter)
{
    assert(key_id);
    assert(defaultStr);
    assert(setter);

    LangItemInfo item = {page_id, key_id, defaultStr, setter};
    m_items.push_back(item);
}

void LangItemList::RemoveItemBySetterHandle(void *setterHandle)
{
    assert(setterHandle);

    for(auto it = m_items.begin(); it != m_items.end(); it++)
    {
        if ((*it).setter->GetControlHandle() == setterHandle){
            m_items.erase(it);
            break;
        }
    }
}

void LangItemList::LoadAllText()
{
    for(auto it = m_items.begin(); it != m_items.end(); it++)
    {
        int page_id = (*it).lang_page_id;
        const char *key_id = (*it).key_id;
        const char *def_str = (*it).default_str;
        ILangSetter *setter = (*it).setter;

        const char *str = LangResource::Instance()->get_lang_text(page_id, key_id, def_str);
        if (str == NULL){
            str = def_str;
        }

        setter->SetLangText(str);
    }
}