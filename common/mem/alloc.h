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

#ifndef DS_MEM_ALLOC_H
#define DS_MEM_ALLOC_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _WIN32
#define XMEM_API __attribute__((visibility("default")))
#else
#define XMEM_API
#endif

XMEM_API void* x_malloc(int size);

XMEM_API void* x_malloc_zero(int size);

XMEM_API void x_free(void *pmem);

XMEM_API char* str_clone(const char *s);

XMEM_API void* x_realloc(void *pmem, int size);

XMEM_API void** x_malloc_ptr_array(int count);

XMEM_API void x_free_ptr_array(void **pArray);

#ifdef __cplusplus
}
#endif

#endif