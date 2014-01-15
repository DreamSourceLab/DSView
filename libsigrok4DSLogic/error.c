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
 * Error handling in libsigrok.
 */

/**
 * @defgroup grp_error Error handling
 *
 * Error handling in libsigrok.
 *
 * libsigrok functions usually return @ref SR_OK upon success, or a negative
 * error code on failure.
 *
 * @{
 */

/**
 * Return a human-readable error string for the given libsigrok error code.
 *
 * @param error_code A libsigrok error code number, such as SR_ERR_MALLOC.
 *
 * @return A const string containing a short, human-readable (English)
 *         description of the error, such as "memory allocation error".
 *         The string must NOT be free'd by the caller!
 *
 * @see sr_strerror_name
 *
 * @since 0.2.0
 */
SR_API const char *sr_strerror(int error_code)
{
	const char *str;

	/*
	 * Note: All defined SR_* error macros from libsigrok.h must have
	 * an entry in this function, as well as in sr_strerror_name().
	 */

	switch (error_code) {
	case SR_OK:
		str = "no error";
		break;
	case SR_ERR:
		str = "generic/unspecified error";
		break;
	case SR_ERR_MALLOC:
		str = "memory allocation error";
		break;
	case SR_ERR_ARG:
		str = "invalid argument";
		break;
	case SR_ERR_BUG:
		str = "internal error";
		break;
	case SR_ERR_SAMPLERATE:
		str = "invalid samplerate";
		break;
	case SR_ERR_NA:
		str = "not applicable";
		break;
	case SR_ERR_DEV_CLOSED:
		str = "device closed but should be open";
		break;
	default:
		str = "unknown error";
		break;
	}

	return str;
}

/**
 * Return the "name" string of the given libsigrok error code.
 *
 * For example, the "name" of the SR_ERR_MALLOC error code is "SR_ERR_MALLOC",
 * the name of the SR_OK code is "SR_OK", and so on.
 *
 * This function can be used for various purposes where the "name" string of
 * a libsigrok error code is useful.
 *
 * @param error_code A libsigrok error code number, such as SR_ERR_MALLOC.
 *
 * @return A const string containing the "name" of the error code as string.
 *         The string must NOT be free'd by the caller!
 *
 * @see sr_strerror
 *
 * @since 0.2.0
 */
SR_API const char *sr_strerror_name(int error_code)
{
	const char *str;

	/*
	 * Note: All defined SR_* error macros from libsigrok.h must have
	 * an entry in this function, as well as in sr_strerror().
	 */

	switch (error_code) {
	case SR_OK:
		str = "SR_OK";
		break;
	case SR_ERR:
		str = "SR_ERR";
		break;
	case SR_ERR_MALLOC:
		str = "SR_ERR_MALLOC";
		break;
	case SR_ERR_ARG:
		str = "SR_ERR_ARG";
		break;
	case SR_ERR_BUG:
		str = "SR_ERR_BUG";
		break;
	case SR_ERR_SAMPLERATE:
		str = "SR_ERR_SAMPLERATE";
		break;
	case SR_ERR_NA:
		str = "SR_ERR_NA";
		break;
	case SR_ERR_DEV_CLOSED:
		str = "SR_ERR_DEV_CLOSED";
		break;
	default:
		str = "unknown error code";
		break;
	}

	return str;
}

/** @} */
