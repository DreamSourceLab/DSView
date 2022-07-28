/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2012 Uwe Hermann <uwe@hermann-uwe.de>
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

#include "libsigrok.h"
#include "version.h"

/**
 * @file
 *
 * Version number querying functions, definitions, and macros.
 */

/**
 * @defgroup grp_versions Versions
 *
 * Version number querying functions, definitions, and macros.
 *
 * This set of API calls returns two different version numbers related
 * to libsigrok. The "package version" is the release version number of the
 * libsigrok tarball in the usual "major.minor.micro" format, e.g. "0.1.0".
 *
 * The "library version" is independent of that; it is the libtool version
 * number in the "current:revision:age" format, e.g. "2:0:0".
 * See http://www.gnu.org/software/libtool/manual/libtool.html#Libtool-versioning for details.
 *
 * Both version numbers (and/or individual components of them) can be
 * retrieved via the API calls at runtime, and/or they can be checked at
 * compile/preprocessor time using the respective macros.
 *
 * @{
 */

SR_API const char *sr_get_lib_version_string()
{
	return SR_LIB_VERSION_STRING;
}

/** @} */
