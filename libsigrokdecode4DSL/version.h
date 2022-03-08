/* version.h.  Generated from version.h.in by configure.  */
/*
 * This file is part of the libsigrokdecode project.
 *
 * Copyright (C) 2010 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2012 Bert Vermeulen <bert@biot.com>
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

#ifndef LIBSIGROKDECODE_VERSION_H
#define LIBSIGROKDECODE_VERSION_H

/**
 * @file
 *
 * Version number definitions and macros.
 */

/**
 * @ingroup grp_versions
 *
 * @{
 */

/*
 * Package version macros (can be used for conditional compilation).
 */

/** The libsigrokdecode package 'major' version number. */
#define SRD_PACKAGE_VERSION_MAJOR 0

/** The libsigrokdecode package 'minor' version number. */
#define SRD_PACKAGE_VERSION_MINOR 6

/** The libsigrokdecode package 'micro' version number. */
#define SRD_PACKAGE_VERSION_MICRO 0

/** The libsigrokdecode package version ("major.minor.micro") as string. */
#define SRD_PACKAGE_VERSION_STRING "0.6.0-git-3914467"

/*
 * Library/libtool version macros (can be used for conditional compilation).
 */

/** The libsigrokdecode libtool 'current' version number. */
#define SRD_LIB_VERSION_CURRENT 4

/** The libsigrokdecode libtool 'revision' version number. */
#define SRD_LIB_VERSION_REVISION 0

/** The libsigrokdecode libtool 'age' version number. */
#define SRD_LIB_VERSION_AGE 0

/** The libsigrokdecode libtool version ("current:revision:age") as string. */
#define SRD_LIB_VERSION_STRING "4:0:0"

/** @} */

#endif
