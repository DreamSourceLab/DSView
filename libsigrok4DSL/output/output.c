/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2010-2012 Bert Vermeulen <bert@biot.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "libsigrok.h"
#include "libsigrok-internal.h"

/**
 * @file
 *
 * Output file/data format handling.
 */

/**
 * @defgroup grp_output Output formats
 *
 * Output file/data format handling.
 *
 * libsigrok supports several output (file) formats, e.g. binary, VCD,
 * gnuplot, and so on. It provides an output API that frontends can use.
 * New output formats can be added/implemented in libsigrok without having
 * to change the frontends at all.
 *
 * All output modules are fed data in a stream. Devices that can stream data
 * into libsigrok live, instead of storing and then transferring the whole
 * buffer, can thus generate output live.
 *
 * Output modules are responsible for allocating enough memory to store
 * their own output, and passing a pointer to that memory (and length) of
 * the allocated memory back to the caller. The caller is then expected to
 * free this memory when finished with it.
 *
 * @{
 */

/** @cond PRIVATE */
extern SR_PRIV struct sr_output_format output_text_bits;
extern SR_PRIV struct sr_output_format output_text_hex;
extern SR_PRIV struct sr_output_format output_text_ascii;
extern SR_PRIV struct sr_output_format output_binary;
extern SR_PRIV struct sr_output_format output_vcd;

extern SR_PRIV struct sr_output_format output_csv;
extern SR_PRIV struct sr_output_format output_analog;
/* extern SR_PRIV struct sr_output_format output_analog_gnuplot; */
/* @endcond */

static struct sr_output_format *output_module_list[] = {
	&output_text_bits,
	&output_text_hex,
	&output_text_ascii,
	&output_binary,
	&output_vcd,
	&output_csv,
	&output_analog,
	/* &output_analog_gnuplot, */
	NULL,
};

SR_API struct sr_output_format **sr_output_list(void)
{
	return output_module_list;
}

/** @} */
