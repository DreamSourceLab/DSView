/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2016 DreamSourceLab <support@dreamsourcelab.com>
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

#include <libsigrokdecode4DSL/libsigrokdecode.h>

#include <vector>
#include <assert.h>

#include "annotation.h"
#include "AnnotationResTable.h"
#include <cstring>
#include <assert.h>
#include <string.h>
#include "../../config/appconfig.h"
#include "decoderstatus.h"
#include "../../dsvdef.h"
 
 //a find talbe instance
AnnotationResTable annTable;
char sz_format_tmp_buf[50];

bool is_hex_number_str(const char *str)
{ 
	char c = *str;

	while (c)
	{
		if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')){
			c = *str;
			str++;
			continue;
		}
		return false;		
	}
	
	return true;
}

namespace pv {
namespace data {
namespace decode {
 
Annotation::Annotation(const srd_proto_data *const pdata, DecoderStatus *status)
{
	assert(pdata);
	const srd_proto_data_annotation *const pda =
		(const srd_proto_data_annotation*)pdata->data;
	assert(pda);

	_start_sample =	pdata->start_sample;
	_end_sample	  =	pdata->end_sample;
	_format 	= pda->ann_class;
    _type 		= pda->ann_type;
	_resIndex 	= 0;
	_status 	= status;
 
	//make resource find key
	std::string key;

    char **annotations = pda->ann_text;
    while(annotations && *annotations) {
		key.append(*annotations, strlen(*annotations));
		annotations++;  
	}
	
	if (pda->str_number_hex[0]){
		//append numeric string
		key.append(pda->str_number_hex, strlen(pda->str_number_hex));
	}
 
	AnnotationSourceItem *resItem = NULL;
    _resIndex = annTable.MakeIndex(key, resItem);
     
     //is a new item
	if (resItem != NULL){ 
        char **annotations = pda->ann_text;
    	while(annotations && *annotations) {
			resItem->src_lines.push_back(QString::fromUtf8(*annotations));
			annotations++;  
		}

		//get numerical data
		if (pda->str_number_hex[0]){
			strcpy(resItem->str_number_hex, pda->str_number_hex);
			resItem->is_numerical = true;
		}
		else if (resItem->src_lines.size() == 1 && _type >= 100 && _type < 200){
			if (is_hex_number_str(resItem->src_lines[0].toLatin1().data())){
              	resItem->is_numerical = true;
			}
		}

		_status->m_bNumerical |= resItem->is_numerical;
	}
}

Annotation::Annotation()
{
    _start_sample = 0;
    _end_sample = 0;
}

Annotation::~Annotation()
{
     
}
  
const std::vector<QString>& Annotation::annotations() const
{
	 assert(_status);
 
     AnnotationSourceItem &resItem = *annTable.GetItem(_resIndex);
 
	//get origin data, is not a numberical value
     if (!resItem.is_numerical){
        return resItem.src_lines;
     }
  
	 if (resItem.cur_display_format !=  _status->m_format){
		 resItem.cur_display_format = _status->m_format;
		 resItem.cvt_lines.clear();

		 if (resItem.src_lines.size() > 0)
		 {
			 for (QString &rd_src : resItem.src_lines)
			 {
				 if (resItem.str_number_hex[0] != 0)
				 {
					 QString src = rd_src.replace("{$}", "%s");
					 const char *num_str = AnnotationResTable::format_numberic(resItem.str_number_hex, resItem.cur_display_format);
					 sprintf(sz_format_tmp_buf, src.toLatin1().data(), num_str);
					 resItem.cvt_lines.push_back(QString(sz_format_tmp_buf));
				 }
				 else
				 {
					 const char *src_str = rd_src.toLatin1().data();
					 const char *num_str = AnnotationResTable::format_numberic(src_str, resItem.cur_display_format);
					 if (src_str != num_str)
						 resItem.cvt_lines.push_back(QString(num_str));
					 else
						 resItem.cvt_lines.push_back(QString(rd_src));
				 }
			 }
		 }
		 else{
			 const char *num_str = AnnotationResTable::format_numberic(resItem.str_number_hex, resItem.cur_display_format);
			 resItem.cvt_lines.push_back(QString(num_str));
		 }
	 }

	return resItem.cvt_lines;
}                   
 

} // namespace decode
} // namespace data
} // namespace pv
