/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2013 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2014 DreamSourceLab <support@dreamsourcelab.com>
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

#include <map>
#include <string>
#include <vector>
#include <QString>

struct AnnotationSourceItem
{
    bool    is_numeric;
    char    str_number_hex[18]; //numerical value hex format string
    long long numberic_value;
    std::vector<QString> src_lines; //the origin source string lines
    std::vector<QString> cvt_lines; //the converted to bin/hex/oct format string lines
    int     cur_display_format; //current format  as bin/ex/oct..., init with -1
};
 
class AnnotationResTable
{ 
    public:
       int MakeIndex(const std::string &key, AnnotationSourceItem* &newItem);
       AnnotationSourceItem* GetItem(int index);

       inline int GetCount(){
           return m_resourceTable.size();} 

       static const char* format_numberic(const char *hex_str, int fmt);

    private:
        std::map<std::string, int>          m_indexs;
        std::vector<AnnotationSourceItem*>  m_resourceTable;
};