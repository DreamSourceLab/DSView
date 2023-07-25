/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2023 DreamSourceLab <support@dreamsourcelab.com>
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

#ifndef UICORE_UI_TYPES_H
#define UICORE_UI_TYPES_H

class ILangSetter
{
public:
    virtual void SetLangText(const char *szText)=0;
    virtual void* GetControlHandle()=0;
};

class ILangForm
{
public:
    virtual void LoadLangText()=0;
};

class IFontForm
{
public:
    virtual void UpdateFont()=0;
};

class IMainForm
{
public:
    virtual void SwitchLanguage(int language)=0;
    virtual void AddLangForm(ILangForm *form)=0;
    virtual void RemoveLangForm(ILangForm *form)=0;    
    virtual void AddFontForm(IFontForm *form)=0;
    virtual void RemoveFontForm(IFontForm *form)=0;
    virtual void UpdateAllFontForm()=0;
};

#endif