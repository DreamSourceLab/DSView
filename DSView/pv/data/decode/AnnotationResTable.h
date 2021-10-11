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

typedef std::vector<QString> AnnotationStringList;
 
class AnnotationResTable{

    public:
       AnnotationResTable();

       ~AnnotationResTable();

       int MakeIndex(const std::string &key, AnnotationStringList *&ls);

       const AnnotationStringList& GetString(int index);

       inline int GetCount(){return m_resourceTable.size();}
 

    private:
        std::map<std::string, int> m_indexs;
        std::vector<AnnotationStringList> m_resourceTable;
};