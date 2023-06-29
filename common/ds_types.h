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

/*
*	example:
*  	xlog_context *ctx = xlog_new();
*	xlog_writer *wr = xlog_create_writer(ctx, "module name");
*	xlog_err(wr, "count:%d", 100);
*	xlog_free(ctx); //free the context, all writer will can't to use
*/

#ifndef _DS_TYPES_H
#define _DS_TYPES_H

typedef unsigned long long u64_t;

#endif