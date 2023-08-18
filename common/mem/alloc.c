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

#include "alloc.h"
#include <glib.h>


XMEM_API void* x_malloc(int size)
{
    return (void*)g_try_malloc(size);
}

XMEM_API void* x_malloc_zero(int size)
{
    void *buf = x_malloc(size);
    char *wr = (char*)buf;
    char *end = wr + size;

    if (buf != NULL){
        while (wr < end)
        {
            *wr++ = 0;
        }        
    }

    return buf;
}

XMEM_API void x_free(void *pmem)
{
    if (pmem != NULL){
        g_free(pmem);
    }
}

XMEM_API char* str_clone(const char *s)
{
    if (s != NULL){
        return  g_strdup(s);
    }
    return NULL;
}

XMEM_API void* x_realloc(void *pmem, int size)
{
    return g_try_realloc(pmem, size);
}

XMEM_API void** x_malloc_ptr_array(int count)
{
    void **buf = (void**)x_malloc(sizeof(void*) * (count +1));    
    void **pArray = (void**)buf;

    for (int i=0; i<=count; i++){
        *pArray = NULL;
        pArray++;
    }

    return buf;
}

XMEM_API void x_free_ptr_array(void **pArray)
{
    if (pArray != NULL){
        while (*pArray)
        {
            x_free(*pArray);
            *pArray = NULL;
            pArray++;            
        }        
    }
}