##
## This file is part of the libsigrok project.
##
## Copyright (C) 2010-2012 Bert Vermeulen <bert@biot.com>
## Copyright (C) 2012 Alexandru Gagniuc <mr.nuke.me@gmail.com>
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.
##

# We require at least autoconf 2.63 (AC_INIT format changed there).
AC_PREREQ([2.63])

# libsigrok package version number (NOT the same as shared lib version!).
m4_define([sr_package_version_major], [0])
m4_define([sr_package_version_minor], [2])
m4_define([sr_package_version_micro], [0])
m4_define([sr_package_version], [sr_package_version_major.sr_package_version_minor.sr_package_version_micro])

AC_INIT([libsigrok4DSLogic], [sr_package_version], [support@dreamsourcelab.com],
	[libsigrok4DSLogic], [http://www.dreamsourcelab.com])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([autostuff])
AC_CONFIG_AUX_DIR([autostuff])

# We require at least automake 1.11 (needed for 'silent rules').
AM_INIT_AUTOMAKE([1.11 -Wall -Werror check-news color-tests])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

AH_TOP([#ifndef SR_CONFIG_H
#define SR_CONFIG_H    /* To stop multiple inclusions. */])
AH_BOTTOM([#endif /* SR_CONFIG_H */])

# Enable more compiler warnings via -Wall and -Wextra. Add -fvisibility=hidden
# and enforce use of SR_API to explicitly mark all public API functions.
CFLAGS="$CFLAGS -Wall -Wextra -fvisibility=hidden"

# Checks for programs.
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S

# Required for per-target flags or subdir-objects with C sources.
AM_PROG_CC_C_O

# Initialize libtool.
LT_INIT

# Initialize pkg-config.
# We require at least 0.22, as "Requires.private" behaviour changed there.
PKG_PROG_PKG_CONFIG([0.22])

# Library version for libsigrok (NOT the same as the package version).
# Carefully read the libtool docs before updating these numbers!
# The algorithm for determining which number to change (and how) is nontrivial!
# http://www.gnu.org/software/libtool/manual/libtool.html#Updating-version-info
SR_LIB_VERSION_CURRENT=1
SR_LIB_VERSION_REVISION=2
SR_LIB_VERSION_AGE=0
SR_LIB_VERSION="$SR_LIB_VERSION_CURRENT:$SR_LIB_VERSION_REVISION:$SR_LIB_VERSION_AGE"
SR_LIB_LDFLAGS="-version-info $SR_LIB_VERSION"
AC_SUBST(SR_LIB_VERSION_CURRENT)
AC_SUBST(SR_LIB_VERSION_REVISION)
AC_SUBST(SR_LIB_VERSION_AGE)
AC_SUBST(SR_LIB_VERSION)
AC_SUBST(SR_LIB_LDFLAGS)

AC_ARG_ENABLE(all-drivers, AC_HELP_STRING([--enable-all-drivers],
        [enable all drivers by default [default=yes]]),
        [HW_ENABLED_DEFAULT="$enableval"],
        [HW_ENABLED_DEFAULT="yes"])

AC_ARG_ENABLE(demo, AC_HELP_STRING([--enable-demo],
	[enable demo driver support [default=yes]]),
	[HW_DEMO="$enableval"],
	[HW_DEMO=$HW_ENABLED_DEFAULT])
# Checks for libraries.

case "$host" in
*mingw*)
	# We need to link against the Winsock2 library for SCPI over TCP.
	LIBS="$LIBS -lws2_32";;
esac

# This variable collects the pkg-config names of all detected libs.
# It is then used to construct the "Requires.private:" field in the
# libsigrok4DSLogic.pc file.
SR_PKGLIBS=""

# libm (the standard math library) is always needed.
AC_SEARCH_LIBS([pow], [m])

# libglib-2.0 is always needed. Abort if it's not found.
# Note: glib-2.0 is part of the libsigrok API (hard pkg-config requirement).
# We require at least 2.32.0 due to e.g. g_variant_new_fixed_array().
AM_PATH_GLIB_2_0([2.32.0],
	[CFLAGS="$CFLAGS $GLIB_CFLAGS"; LIBS="$LIBS $GLIB_LIBS"])

# libzip is always needed. Abort if it's not found.
PKG_CHECK_MODULES([libzip], [libzip >= 0.10],
	[CFLAGS="$CFLAGS $libzip_CFLAGS"; LIBS="$LIBS $libzip_LIBS";
	SR_PKGLIBS="$SR_PKGLIBS libzip"])

# libserialport is only needed for some hardware drivers. Disable the
# respective drivers if it is not found.
PKG_CHECK_MODULES([libserialport], [libserialport >= 0.1.0],
	[have_libserialport="yes"; CFLAGS="$CFLAGS $libserialport_CFLAGS";
	LIBS="$LIBS $libserialport_LIBS";
	SR_PKGLIBS="$SR_PKGLIBS libserialport"],
	[have_libserialport="no"])

# Define HAVE_LIBSERIALPORT in config.h if we found libserialport.
if test "x$have_libserialport" != "xno"; then
	AC_DEFINE_UNQUOTED(HAVE_LIBSERIALPORT, [1],
		[Specifies whether we have libserialport.])
fi

# Serial port helper code is only compiled in if libserialport was found.
AM_CONDITIONAL(NEED_SERIAL, test "x$have_libserialport" != xno)

# libusb-1.0 is only needed for some hardware drivers. Disable the respective
# drivers if it is not found.
case "$host" in
*freebsd*)
	# FreeBSD comes with an "integrated" libusb-1.0-style USB API.
	# This means libusb-1.0 is always available, no need to check for it,
	# and no need to (potentially) disable any drivers if it's not found.
	AC_DEFINE_UNQUOTED(HAVE_LIBUSB_1_0, [1],
		[Specifies whether we have a libusb.h header.])
	;;
*)
	PKG_CHECK_MODULES([libusb], [libusb-1.0 >= 1.0.9],
		[have_libusb1_0="yes"; CFLAGS="$CFLAGS $libusb_CFLAGS";
		LIBS="$LIBS $libusb_LIBS";
		SR_PKGLIBS="$SR_PKGLIBS libusb-1.0"],
		[have_libusb1_0="no"])

	# Define HAVE_LIBUSB_1_0 in config.h if we found libusb-1.0.
	if test "x$have_libusb1_0" != "xno"; then
		AC_DEFINE_UNQUOTED(HAVE_LIBUSB_1_0, [1],
			[Specifies whether we have a libusb.h header.])
	fi
	;;
esac



# The Check unit testing framework is optional. Disable if not found.
PKG_CHECK_MODULES([check], [check >= 0.9.4],
	[have_check="yes"; CFLAGS="$CFLAGS $check_CFLAGS";
	LIBS="$LIBS $check_LIBS"], [have_check="no"])
AM_CONDITIONAL(HAVE_CHECK, test x"$have_check" = "xyes")

# The OLS driver uses serial port file descriptors directly, and therefore
# will not currently work on Windows.


AC_SUBST(SR_PKGLIBS)

# Now set AM_CONDITIONALs and AC_DEFINEs for the enabled/disabled drivers.



AM_CONDITIONAL(HW_DEMO, test x$HW_DEMO = xyes)
if test "x$HW_DEMO" = "xyes"; then
	AC_DEFINE(HAVE_LA_DEMO, 1, [Demo driver support])
fi



# Checks for header files.
# These are already checked: inttypes.h stdint.h stdlib.h string.h unistd.h.
AC_CHECK_HEADERS([fcntl.h sys/time.h termios.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_BIGENDIAN
AC_C_INLINE
AC_TYPE_INT8_T
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([gettimeofday memset strchr strcspn strdup strerror strncasecmp strstr strtol strtoul strtoull])

AC_SUBST(FIRMWARE_DIR, "$datadir/sigrok-firmware")
AC_SUBST(MAKEFLAGS, '--no-print-directory')
AC_SUBST(AM_LIBTOOLFLAGS, '--silent')

SR_PACKAGE_VERSION_MAJOR=sr_package_version_major
SR_PACKAGE_VERSION_MINOR=sr_package_version_minor
SR_PACKAGE_VERSION_MICRO=sr_package_version_micro
SR_PACKAGE_VERSION=sr_package_version

AC_SUBST(SR_PACKAGE_VERSION_MAJOR)
AC_SUBST(SR_PACKAGE_VERSION_MINOR)
AC_SUBST(SR_PACKAGE_VERSION_MICRO)
AC_SUBST(SR_PACKAGE_VERSION)

AC_CONFIG_FILES([Makefile version.h hardware/Makefile
		 hardware/demo/Makefile
		 input/Makefile
		 output/Makefile
		 output/text/Makefile
		 libsigrok4DSLogic.pc
		 tests/Makefile
		])

AC_OUTPUT

echo
echo "libsigrok configuration summary:"
echo
echo "  - Package version (major.minor.micro):    $SR_PACKAGE_VERSION"
echo "  - Library version (current:revision:age): $SR_LIB_VERSION"
echo "  - Prefix: $prefix"
echo "  - Building on: $build"
echo "  - Building for: $host"
echo
echo "Detected libraries:"
echo

# Note: This only works for libs with pkg-config integration.
for lib in "glib-2.0 >= 2.32.0" "libzip >= 0.10" "libserialport >= 0.1.0" "libusb-1.0 >= 1.0.9" "libftdi >= 0.16" "libudev >= 151" "alsa >= 1.0" "check >= 0.9.4"; do
	if `$PKG_CONFIG --exists $lib`; then
		ver=`$PKG_CONFIG --modversion $lib`
		answer="yes ($ver)"
	else
		answer="no"
	fi
	echo "  - $lib: $answer"
done

echo -e "\nEnabled hardware drivers:\n"
echo "  - demo............................ $HW_DEMO"
echo

