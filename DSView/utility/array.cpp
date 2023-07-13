/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2022 DreamSourceLab <support@dreamsourcelab.com>
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
#include "array.h"
#include <assert.h>

namespace pv
{
    namespace array
    {
        uint64_t find_min_uint64(uint64_t *arr, int size)
        { 
            assert(arr);
            assert(size);

            uint64_t *p = arr;            
            uint64_t v = *p;            

            for (int i=1; i<size; i++){
                p++;
                if (*p < v)
                    v = *p;
            }
            return v;
        }

        uint64_t find_max_uint64(uint64_t *arr, int size)
        { 
            assert(arr);
            assert(size);

            uint64_t *p = arr;            
            uint64_t v = *p;            

            for (int i=1; i<size; i++){
                p++;
                if (*p > v)
                    v = *p;
            }
            return v;
        }
    }    
}