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

#include "ZipMaker.h" 
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
  
ZipMaker::ZipMaker() :
    m_zDoc(NULL)
{
    m_error[0] = 0; 
    m_opt_compress_level = Z_BEST_SPEED;
    m_zi = NULL;
}

ZipMaker::~ZipMaker()
{
    Release();
}

bool ZipMaker::CreateNew(const char *fileName, bool bAppend)
{
     assert(fileName);

     Release();
 
     m_zDoc = zipOpen64(fileName, bAppend); 
     if (m_zDoc == NULL) {
        strcpy(m_error, "zipOpen64 error");
    } 

//make zip inner file time 
    m_zi = new zip_fileinfo();

    time_t rawtime;
    time (&rawtime);
    struct tm *tinf= localtime(&rawtime);

    struct tm &ti = *tinf;
    zip_fileinfo &zi= *(zip_fileinfo*)m_zi;

    zi.tmz_date.tm_year = ti.tm_year;
    zi.tmz_date.tm_mon  = ti.tm_mon;
    zi.tmz_date.tm_mday = ti.tm_mday;
    zi.tmz_date.tm_hour = ti.tm_hour;
    zi.tmz_date.tm_min  = ti.tm_min;
    zi.tmz_date.tm_sec  = ti.tm_sec;
    zi.dosDate = 0;
      
    return m_zDoc != NULL;
}

void ZipMaker::Release()
{  
    if (m_zDoc){
       zipClose((zipFile)m_zDoc, NULL);
       m_zDoc = NULL;       
   }
   if (m_zi){
       delete ((zip_fileinfo*)m_zi);
       m_zi = NULL;
   }
}

bool ZipMaker::Close(){
    if (m_zDoc){
       zipClose((zipFile)m_zDoc, NULL);
       m_zDoc = NULL;
       return true;
   }
   return false;     
}

bool ZipMaker::AddFromBuffer(const char *innerFile, const char *buffer, unsigned int buferSize)
{
    assert(buffer);
    assert(innerFile);
    assert(m_zDoc);   
    int level = m_opt_compress_level;

    if (level < Z_DEFAULT_COMPRESSION  || level > Z_BEST_COMPRESSION){
        level = Z_DEFAULT_COMPRESSION;
    }

    zipOpenNewFileInZip((zipFile)m_zDoc,innerFile,(zip_fileinfo*)m_zi,
                                NULL,0,NULL,0,NULL ,
                                Z_DEFLATED,
                                level);

    zipWriteInFileInZip((zipFile)m_zDoc, buffer, (unsigned int)buferSize);

    zipCloseFileInZip((zipFile)m_zDoc);

    return true;
}

bool ZipMaker::AddFromFile(const char *localFile, const char *innerFile)
{
    assert(localFile);

    struct stat st;
    FILE *fp;
    char *data = NULL;
    long long size = 0;

    if ((fp = fopen(localFile, "rb")) == NULL) {
        strcpy(m_error, "fopen error");        
        return false;
    }

    if (fstat(fileno(fp), &st) < 0) {
        strcpy(m_error, "fstat error");    
        fclose(fp);
        return -1;
    } 

    if ((data = (char*)malloc((size_t)st.st_size)) == NULL) {
        strcpy(m_error, "can't malloc buffer");
        fclose(fp);
        return false;
    }

    if (fread(data, 1, (size_t)st.st_size, fp) < (size_t)st.st_size) {
        strcpy(m_error, "fread error");
        free(data);
        fclose(fp);
        return false;
    }

    fclose(fp);
    size = (size_t)st.st_size;

    bool ret = AddFromBuffer(innerFile, data, size);
    return ret;
}

const char *ZipMaker::GetError()
{
    if (m_error[0])
        return m_error;
    return NULL;
}

//-----------------ZipReader

ZipInnerFileData::ZipInnerFileData(char *data, int size)
{
    _data = data;
    _size = size;
}

ZipInnerFileData::~ZipInnerFileData()
{
    if (_data != NULL){
        free(_data);
        _data = NULL;
    }
}

ZipReader::ZipReader(const char *filePath)
{
    m_archive = NULL;
    m_archive = unzOpen64(filePath);
}

ZipReader::~ZipReader()
{
    Close();
}

void ZipReader::Close()
{
    if (m_archive != NULL){
        unzClose(m_archive);
        m_archive = NULL;
    }
}

ZipInnerFileData* ZipReader::GetInnterFileData(const char *innerFile)
{
    char *metafile = NULL;
    char szFilePath[15];
    unz_file_info64 fileInfo;
   
    if (m_archive == NULL){
        return NULL;
    }
  
    // inner file not exists
    if (unzLocateFile(m_archive, innerFile, 0) != UNZ_OK){
        return NULL;
    }

    if (unzGetCurrentFileInfo64(m_archive, &fileInfo, szFilePath,
                                sizeof(szFilePath), NULL, 0, NULL, 0) != UNZ_OK)
    {  
        return NULL;
    }

    if (unzOpenCurrentFile(m_archive) != UNZ_OK)
    { 
        return NULL;
    }

    if (fileInfo.uncompressed_size > 0 && (metafile = (char *)malloc(fileInfo.uncompressed_size)))
    {
        unzReadCurrentFile(m_archive, metafile, fileInfo.uncompressed_size);
        unzCloseCurrentFile(m_archive);

         ZipInnerFileData *data = new ZipInnerFileData(metafile, fileInfo.uncompressed_size);
         return data;
    } 
 
    return NULL;
}

void ZipReader::ReleaseInnerFileData(ZipInnerFileData *data)
{
    if (data){
        delete data;
    }
}