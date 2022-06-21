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

#include "dsvdef.h" 
#include <string.h>

#ifdef DS_DEBUG_TRACE
#include <stdio.h>
    void ds_print(const char *s){
        printf(s);
    }     
#endif

namespace DecoderDataFormat
{
      int Parse(const char *name){
        if (strcmp(name, "dec") == 0){
            return (int)dec;
        }
        if (strcmp(name, "hex") == 0){
            return (int)hex;
        }
        if (strcmp(name, "oct") == 0){
            return (int)oct;
        }
        if (strcmp(name, "bin") == 0){
            return (int)bin;
        }
        if (strcmp(name, "ascii") == 0){
            return (int)ascii;
        }
        return (int)hex;
    }
}
