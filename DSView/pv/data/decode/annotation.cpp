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

#define DECODER_MAX_DATA_BLOCK_LEN 300
#define FORMAT_TMP_BUFFER_SIZE 1200

const char g_bin_cvt_table[] = "0000000100100011010001010110011110001001101010111100110111101111";
char g_bin_format_tmp_buffer[FORMAT_TMP_BUFFER_SIZE + 3];
char g_oct_format_tmp_buffer[FORMAT_TMP_BUFFER_SIZE + 6];
std::vector<QString> g_format_ret_vector;

char* bin2oct_string(char *buf, int size, const char *bin, int len){
	char *wr = buf + size - 1;
	*wr = 0; //end flag

	char *rd = (char*)bin + len - 1; //move to last byte

	char tmp[3]; 

	while (rd >= bin && wr > buf)
	{  
		wr--;

		int num = 0;

		while (rd >= bin && num < 3)
		{
			tmp[2-num] = *rd;			 
			rd--;
			num++;
		}

		//fill
		while (num < 3)
		{
			tmp[2-num] = '0';
			++num;
		}
		

	    if (strncmp(tmp, "000", 3) == 0)
			*wr = '0';
		else if (strncmp(tmp, "001", 3) == 0)
			*wr = '1';
		else if (strncmp(tmp, "010", 3) == 0)
			*wr = '2';
		else if (strncmp(tmp, "011", 3) == 0)
			*wr = '3';
		else if (strncmp(tmp, "100", 3) == 0)
			*wr = '4';
		else if (strncmp(tmp, "101", 3) == 0)
			*wr = '5';
		else if (strncmp(tmp, "110", 3) == 0)
			*wr = '6';
		else if (strncmp(tmp, "111", 3) == 0)
			*wr = '7';
	} 

	return wr;
}

long long bin2long_string(const char *bin, int len)
{
	char *rd = (char *)bin + len - 1; //move to last byte
	int dex = 0;
	long long value = 0;
	long long bv = 0;

    while (rd >= bin)
	{
		if (*rd == '1')
		{
			bv = 1 << dex;
			value += bv;
		}
		rd--;
		++dex;
	}

	return value;
}

namespace pv {
namespace data {
namespace decode {

//a find talbe instance
AnnotationResTable *Annotation::m_resTable = new AnnotationResTable();

Annotation::Annotation(const srd_proto_data *const pdata, DecoderStatus *status) :
	_start_sample(pdata->start_sample),
	_end_sample(pdata->end_sample)
{
	assert(pdata);
	const srd_proto_data_annotation *const pda =
		(const srd_proto_data_annotation*)pdata->data;
	assert(pda);

    _format = pda->ann_class;
    _type = pda->ann_type;
	_status = status;

	//have numerical
	if (_type >= 100 && _type < 200){
		 status->m_bNumerical = true;
	}

	_strIndex = 0;

	std::string key; 
	//make resource find key
    const char *const *annotations = (char**)pda->ann_text;
    while(*annotations) { 
        const char *ptr = *annotations;
		key.append(ptr, strlen(ptr));
		annotations++;  
	}

	AnnotationStringList *annotationArray = NULL;
    _strIndex = Annotation::m_resTable->MakeIndex(key, annotationArray);
     
     //save new string lines
	if (annotationArray){
        annotations = (char **)pda->ann_text;
		while (*annotations)
		{
			annotationArray->push_back(QString::fromUtf8(*annotations));
			annotations++;
		}
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

     int fmt = _status->m_format;
     const std::vector<QString>& vct = Annotation::m_resTable->GetString(_strIndex);

     if (!(_type >= 100 && _type < 200) || fmt == DecoderDataFormat::ascii || fmt == DecoderDataFormat::hex){
        return vct;
     }
	 if (vct.size() != 1){
		 return vct;
	 }

	 //flow, convert to oct\dec\bin format
	 const char *data = vct[0].toStdString().c_str();
	 if (*data == 0 || *data == '['){
		 return vct;
	 }
	
	 //convert to bin format
	 char *buf = g_bin_format_tmp_buffer + FORMAT_TMP_BUFFER_SIZE;
	 *(buf + 1) = 0; //set the end flag
	 *buf = 0;

	 int len = strlen(data);
	  //buffer is not enough
	 if (len > DECODER_MAX_DATA_BLOCK_LEN){
		 return vct;
	 }

     char *rd = (char*)data + len - 1; //move to last byte
	 char c = 0;
	 int dex = 0;

	 while (rd >= data)
	 {
		 c = *rd;
		 dex = (int)(c <= '9' ? (c - '0') : (c - 'A' + 10));
         char *ptable = (char*)g_bin_cvt_table + dex * 4;

		 buf -= 4; //move to left for 4 bytes
		 buf[0] = ptable[0];
		 buf[1] = ptable[1];
		 buf[2] = ptable[2];
		 buf[3] = ptable[3];
	
		 rd--;
	 }
	 
	 std::vector<QString> &vct2 = g_format_ret_vector;
	 if (vct2.size() == 0){
		 vct2.push_back(QString());
	 }

	 //get bin format 
	 if (fmt == DecoderDataFormat::bin){
		 vct2[0].clear();
		 vct2[0].append(buf);
		 return vct2;
	 }

	 //get oct format
	 if (fmt == DecoderDataFormat::oct){
		 char *oct_buf = bin2oct_string(g_oct_format_tmp_buffer, 
		                  sizeof(g_oct_format_tmp_buffer), buf,  len * 4);
		 vct2[0].clear();
		 vct2[0].append(oct_buf);
		 return vct2;
	 }

	//64 bit integer
	 if (fmt == DecoderDataFormat::dec && len * 4 <= 64){
         long long lv = bin2long_string(buf, len *4);
		 char vbuf[30] = {0};
    	 sprintf(vbuf, "%lld", lv);

		 vct2[0].clear();
		 vct2[0].append(vbuf);
		 return vct2;
	 }

	 return vct2;
} 
 

} // namespace decode
} // namespace data
} // namespace pv
