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
 * Input file/data format handling.
 */

/**
 * @defgroup grp_input Input formats
 *
 * Input file/data format handling.
 *
 * libsigrok can process acquisition data in several different ways.
 * Aside from acquiring data from a hardware device, it can also take it from
 * a file in various formats (binary, CSV, VCD, and so on).
 *
 * Like everything in libsigrok that handles data, processing is done in a
 * streaming manner -- input should be supplied to libsigrok a chunk at a time.
 * This way anything that processes data can do so in real time, without the
 * user having to wait for the whole thing to be finished.
 *
 * Every input module is "pluggable", meaning it's handled as being separate
 * from the main libsigrok, but linked in to it statically. To keep things
 * modular and separate like this, functions within an input module should be
 * declared static, with only the respective 'struct sr_input_format' being
 * exported for use into the wider libsigrok namespace.
 *
 * @{
 */

/** @cond PRIVATE */

extern SR_PRIV struct sr_input_format input_binary;
extern SR_PRIV struct sr_input_format input_vcd;
extern SR_PRIV struct sr_input_format input_wav;
/* @endcond */

static struct sr_input_format *input_module_list[] = {
	&input_vcd,
	&input_wav,
	/* This one has to be last, because it will take any input. */
	&input_binary,
	NULL,
};

SR_API struct sr_input_format **sr_input_list(void)
{
	return input_module_list;
}

/** @} */
