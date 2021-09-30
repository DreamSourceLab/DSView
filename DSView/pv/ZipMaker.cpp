
/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2013 DreamSourceLab <support@dreamsourcelab.com>
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
#include <zip.h>
#include <stdio.h>
#include <assert.h>
#include <malloc.h>
#include <errno.h>
#include <string.h>

  
ZipMaker::ZipMaker() :
    m_docSource(NULL),
    m_errobj(NULL)
{
    m_error[0] = 0;   
}

ZipMaker::~ZipMaker()
{
    Release();
}

bool ZipMaker::CreateNew()
{
     Release();

     m_errobj = new zip_error_t();
     zip_error_init(m_errobj);

     if ((m_docSource = zip_source_buffer_create(NULL, 0, 0, m_errobj)) == NULL) {
        sprintf(m_error, "can't create source: %s", zip_error_strerror(m_errobj));
    }
 
    return m_docSource != NULL;
}

void ZipMaker::Release()
{
     if (m_errobj){
        zip_error_fini(m_errobj); //release the error obj
        delete m_errobj;
        m_errobj = NULL;
    }

    if (m_docSource){
       zip_source_free(m_docSource);
       m_docSource = NULL;
   }
}

bool ZipMaker::SaveToFile(const char *fileName){
    assert(fileName);
    assert(m_docSource); 

    zip_stat_t zst;
    if (zip_source_stat(m_docSource, &zst) < 0) {
        sprintf(m_error, "can't stat source: %s\n", zip_error_strerror(zip_source_error(m_docSource)));
        return 1;
    }

    long long size = zst.size;
    void *data = NULL;
    FILE *fp = NULL;
    bool  berr = false;

    if (zip_source_open(m_docSource) < 0) {
        sprintf(m_error, "can't open source: %s\n", zip_error_strerror(zip_source_error(m_docSource)));
        return false;
    }
    if (!berr && (data = malloc(size)) == NULL) {
        sprintf(m_error, "malloc failed: %s\n", strerror(errno));
        berr = true;
    }

    if ((zip_uint64_t)zip_source_read(m_docSource, data, size) < size) {
        sprintf(m_error, "can't read data from source: %s\n", zip_error_strerror(zip_source_error(m_docSource)));
        berr = true;
    }

    if (!berr && (fp = fopen(fileName, "wb")) == NULL) {
        sprintf(m_error, "can't open %s: %s\n", fileName, strerror(errno));
        berr = true;
    }
    if (!berr && fwrite(data, 1, size, fp) < size) {
        sprintf(m_error, "can't write %s: %s\n", fileName, strerror(errno));
        berr = true;
        fclose(fp);
    }
    if (!berr && fclose(fp) < 0) {
        sprintf(m_error, "can't write %s: %s\n", fileName, strerror(errno));
        berr = true;
    }

    zip_source_close(m_docSource);
    if (data){
        free(data);
    }        

    return !berr;
}

bool ZipMaker::AddFromBuffer(const char *innerFile, const char *buffer, int buferSize)
{
    assert(buffer);
    assert(innerFile);
    assert(m_docSource); 

    zip_t *zip_archive = zip_open_from_source(m_docSource, 0, m_errobj);
    if (zip_archive == NULL)
    {
         sprintf(m_error, "can't open zip from source: %s", zip_error_strerror(m_errobj));
         return false;
    }

   zip_source_keep(m_docSource);

   zip_source_t *src = zip_source_buffer(zip_archive, buffer, buferSize, 0);
   if (src == NULL){
       sprintf(m_error, "zip_source_buffer error: %s", zip_error_strerror(zip_source_error(m_docSource)));
       return false;
   }

    if (zip_file_add(zip_archive, innerFile, src, ZIP_FL_OVERWRITE) == -1)
    {
        sprintf(m_error, "zip_file_add error: %s", zip_error_strerror(zip_source_error(m_docSource)));
        return false;
    }

    if (zip_close(zip_archive) < 0) {
          sprintf(m_error, "zip_close'%s'", zip_strerror(zip_archive));
          return false;
    }

    return true;
}

bool ZipMaker::AddFromFile(const char *localFile, const char *innerFile)
{
    assert(localFile);
    assert(innerFile);
    assert(m_docSource); 

    zip_t *zip_archive = zip_open_from_source(m_docSource, 0, m_errobj);
    if (zip_archive == NULL)
    {
         sprintf(m_error, "can't open zip from source: %s", zip_error_strerror(m_errobj));
         return false;
    }
    
    zip_source_keep(m_docSource);
  
    zip_source_t  *src = zip_source_file(zip_archive, localFile, 0, -1);
    if (src == NULL){
        sprintf(m_error, "zip_source_buffer error: %s\n", zip_error_strerror(zip_source_error(m_docSource)));
        return false;
    }

    if (zip_add(zip_archive, innerFile, src) == -1)
    {
        sprintf(m_error, "zip_file_add error: %s\n", zip_error_strerror(zip_source_error(m_docSource)));
        return false;
    }

     if (zip_close(zip_archive) < 0) {
          sprintf(m_error, "zip_close'%s'", zip_strerror(zip_archive));
          return false;
    }

    return true;
}

const char *ZipMaker::GetError()
{
    if (m_error[0])
        return m_error;
    return NULL;
}
