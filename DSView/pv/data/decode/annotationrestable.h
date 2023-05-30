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

#include <map>
#include <string>
#include <vector>
#include <QString>

#define DECODER_MAX_DATA_BLOCK_LEN 256
#define CONVERT_STR_MAX_LEN 150

struct AnnotationSourceItem
{
    bool    is_numeric;
    char    *str_number_hex; //numerical value hex format string

    std::vector<QString> src_lines; //the origin source string lines
    std::vector<QString> cvt_lines; //the converted to bin/hex/oct format string lines
    int     cur_display_format; //current format  as bin/ex/oct..., init with -1
};
 
class AnnotationResTable
{ 
    public:
    AnnotationResTable();
    ~AnnotationResTable();

    public:
       int MakeIndex(const std::string &key, AnnotationSourceItem* &newItem);
       AnnotationSourceItem* GetItem(int index);

       inline int GetCount(){
           return m_resourceTable.size();} 

       const char* format_numberic(const char *hex_str, int fmt);

       void reset();

       static int hexToDecimal(char * hex);
       static void decimalToBinString(unsigned long long num, int bitSize, char *buffer, int buffer_size);

    private:
        const char* format_to_string(const char *hex_str, int fmt);

    private:
        std::map<std::string, int>          m_indexs;
        std::vector<AnnotationSourceItem*>  m_resourceTable;
        char g_bin_format_tmp_buffer[DECODER_MAX_DATA_BLOCK_LEN * 4 + 2];
        char g_oct_format_tmp_buffer[DECODER_MAX_DATA_BLOCK_LEN * 3 + 2];
        char g_number_tmp_64[30];
        char g_all_buf[CONVERT_STR_MAX_LEN + 1];
};
