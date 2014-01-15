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

/**
 * Get the major libsigrok package version number.
 *
 * @return The major package version number.
 *
 * @since 0.1.0
 */
SR_API int sr_package_version_major_get(void)
{
	return SR_PACKAGE_VERSION_MAJOR;
}

/**
 * Get the minor libsigrok package version number.
 *
 * @return The minor package version number.
 *
 * @since 0.1.0
 */
SR_API int sr_package_version_minor_get(void)
{
	return SR_PACKAGE_VERSION_MINOR;
}

/**
 * Get the micro libsigrok package version number.
 *
 * @return The micro package version number.
 *
 * @since 0.1.0
 */
SR_API int sr_package_version_micro_get(void)
{
	return SR_PACKAGE_VERSION_MICRO;
}

/**
 * Get the libsigrok package version number as a string.
 *
 * @return The package version number string. The returned string is
 *         static and thus should NOT be free'd by the caller.
 *
 * @since 0.1.0
 */
SR_API const char *sr_package_version_string_get(void)
{
	return SR_PACKAGE_VERSION_STRING;
}

/**
 * Get the "current" part of the libsigrok library version number.
 *
 * @return The "current" library version number.
 *
 * @since 0.1.0
 */
SR_API int sr_lib_version_current_get(void)
{
	return SR_LIB_VERSION_CURRENT;
}

/**
 * Get the "revision" part of the libsigrok library version number.
 *
 * @return The "revision" library version number.
 *
 * @since 0.1.0
 */
SR_API int sr_lib_version_revision_get(void)
{
	return SR_LIB_VERSION_REVISION;
}

/**
 * Get the "age" part of the libsigrok library version number.
 *
 * @return The "age" library version number.
 *
 * @since 0.1.0
 */
SR_API int sr_lib_version_age_get(void)
{
	return SR_LIB_VERSION_AGE;
}

/**
 * Get the libsigrok library version number as a string.
 *
 * @return The library version number string. The returned string is
 *         static and thus should NOT be free'd by the caller.
 *
 * @since 0.1.0
 */
SR_API const char *sr_lib_version_string_get(void)
{
	return SR_LIB_VERSION_STRING;
}

/** @} */
