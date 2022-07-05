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

//test git 2

#pragma once 

#include <minizip/zip.h>
#include <minizip/unzip.h>
 

class ZipMaker
{
public:
    ZipMaker();

    ~ZipMaker();

    //create new zip archive in the memory
    bool CreateNew(const char *fileName, bool bAppend);

    //free the source
    void Release();

    //save data to file
    bool Close();

    //add a inner file from  buffer
    bool AddFromBuffer(const char *innerFile, const char *buffer, unsigned int buferSize);

    //add a inner file from local file
    bool AddFromFile(const char *localFile, const char *innerFile);

    //get the last error
    const char *GetError();

public:
    int m_opt_compress_level;

private:
    zipFile         m_zDoc; //zip file handle
    zip_fileinfo    *m_zi; //life must as m_zDoc; 
    char     m_error[500];
};


//------------------ZipReader
class ZipInnerFileData
{
public:
    ZipInnerFileData(char *data, int size);
    ~ZipInnerFileData();

    inline char *data(){
        return _data;
    }
    inline int size(){
        return _size;
    }

private:
    char *_data;
    int  _size;
};

class ZipReader{
public:
    ZipReader(const char *filePath);
    ~ZipReader();
    void Close();

    inline bool HaveArchive(){
        return m_archive != NULL;
    }

    ZipInnerFileData* GetInnterFileData(const char *innerFile);

    void ReleaseInnerFileData(ZipInnerFileData *data);


private:
    unzFile  m_archive;
};
 
