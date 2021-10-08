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

//test git

#pragma once

struct  zip_source;
struct  zip_error;

typedef struct zip_source zip_archive_source_t;
typedef struct zip_error zip_err_t;

class ZipMaker
{
public:
    ZipMaker();

    ~ZipMaker();

    //create new zip archive in the memory
    bool CreateNew();

    //free the source
    void Release();

    //save data to file
    bool SaveToFile(const char *fileName);

    //add a inner file from  buffer
    bool AddFromBuffer(const char *innerFile, const char *buffer, int buferSize);

    //add a inner file from local file
    bool AddFromFile(const char *localFile, const char *innerFile);

    //get the last error
    const char *GetError();

private:
    zip_archive_source_t    *m_docSource; //zip file handle
    zip_err_t               *m_errobj;
    char                     m_error[500];
};
