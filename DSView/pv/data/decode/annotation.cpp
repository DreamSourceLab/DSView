/*
 * This file is part of the PulseView project.
 * DSView is based on PulseView.
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

#include <libsigrokdecode.h>

#include <vector>
#include <assert.h>

#include "annotation.h"
#include "annotationrestable.h"
#include <cstring>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "../../config/appconfig.h"
#include "decoderstatus.h"
#include "../../dsvdef.h"
 

namespace pv {
namespace data {
namespace decode {
 
Annotation::Annotation(const srd_proto_data *const pdata, DecoderStatus *status)
{
	assert(pdata);
	const srd_proto_data_annotation *const pda =
		(const srd_proto_data_annotation*)pdata->data;
	assert(pda);
	assert(status);

	_start_sample =	pdata->start_sample;
	_end_sample	  =	pdata->end_sample;
	_format 	= pda->ann_class;
    _type 		= pda->ann_type;
	_resIndex 	= -1;
	_status 	= status;
 
	//make resource find key
	std::string key;

    char **annotations = pda->ann_text;
    while(annotations && *annotations) {
		if ((*annotations)[0] != '\n'){
			key.append(*annotations, strlen(*annotations));
		}		
		annotations++;  
	}
	
	if (pda->str_number_hex[0]){
		//append numeric string
		key.append(pda->str_number_hex, strlen(pda->str_number_hex));
	}
 
	AnnotationSourceItem *resItem = NULL;
    _resIndex = _status->m_resTable.MakeIndex(key, resItem);
     
     //is a new item
	if (resItem != NULL){ 
        char **annotations = pda->ann_text;
    	while(annotations && *annotations) {
			if ((*annotations)[0] != '\n'){
				resItem->src_lines.push_back(QString::fromUtf8(*annotations));
			}
			annotations++;  
		}
 
		//get numeric data
		if (pda->str_number_hex[0]){
			int str_len = strlen(pda->str_number_hex);

			if (str_len <= DECODER_MAX_DATA_BLOCK_LEN){
				resItem->str_number_hex = (char*)malloc(str_len + 1);
			
				if (resItem->str_number_hex != NULL){
					strcpy(resItem->str_number_hex, pda->str_number_hex);
					resItem->is_numeric = true;
				}
			}			
		}

		_status->m_bNumeric |= resItem->is_numeric;
	}
}

Annotation::Annotation()
{
    _start_sample = 0;
    _end_sample = 0;
	_resIndex = -1;
}
 
Annotation::~Annotation()
{     
}
  
const std::vector<QString>& Annotation::annotations() const
{  
	 AnnotationSourceItem *pobj = _status->m_resTable.GetItem(_resIndex);	 
	 assert(pobj);
	
     AnnotationSourceItem &resItem = *pobj;

	//get origin data, is not a numberic value
     if (!resItem.is_numeric){
        return resItem.src_lines;
     }

	//resItem.str_number_hex must be not null
	if (resItem.str_number_hex[0] == 0){
		assert(false);
	}
 
	 if (resItem.cur_display_format !=  _status->m_format){
		 resItem.cur_display_format = _status->m_format;
		 resItem.cvt_lines.clear();

		 if (resItem.src_lines.size() > 0)
		 { 
			 int 	text_format_buf_len = 0;
			 char  *text_format_buf = NULL;

			 //have custom string
			 for (QString &rd_src : resItem.src_lines)
			 {			
				 QString src = rd_src.replace("{$}", "%s");

				 const char *num_str = _status->m_resTable.format_numberic(resItem.str_number_hex, resItem.cur_display_format);
				 const char *src_str = src.toUtf8().data();
				
				 int textlen = strlen(src_str) + strlen(num_str);
				 assert(textlen > 0);

				 if (textlen >= text_format_buf_len)
				 {
					if (text_format_buf)
						free(text_format_buf);
					text_format_buf = (char*)malloc(textlen + 8);
					text_format_buf_len = textlen + 8;

					assert(text_format_buf);
				 }
				 
				 sprintf(text_format_buf, src_str, num_str);
				 resItem.cvt_lines.push_back(QString(text_format_buf));
			 }

			 if (text_format_buf)
				free(text_format_buf);
		 }
		 else{
			 //have only numberic value
			 const char *num_str = _status->m_resTable.format_numberic(resItem.str_number_hex, resItem.cur_display_format);
			 resItem.cvt_lines.push_back(QString(num_str));
		 }
	 }

	return resItem.cvt_lines;
}       

bool Annotation::is_numberic()
{
    AnnotationSourceItem *resItem = _status->m_resTable.GetItem(_resIndex);
	return resItem->is_numeric;
} 

} // namespace decode
} // namespace data
} // namespace pv
