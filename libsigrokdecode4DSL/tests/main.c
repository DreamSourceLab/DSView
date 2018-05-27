/*
 * This file is part of the libsigrokdecode project.
 *
 * Copyright (C) 2013 Uwe Hermann <uwe@hermann-uwe.de>
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

#include <config.h>
#include <libsigrokdecode.h> /* First, to avoid compiler warning. */
#include <stdlib.h>
#include <check.h>
#include "lib.h"

void srdtest_setup(void)
{
	/* Silence libsigrokdecode while the unit tests run. */
	srd_log_loglevel_set(SRD_LOG_NONE);
}

void srdtest_teardown(void)
{
}

int main(void)
{
	int ret;
	Suite *s;
	SRunner *srunner;

	s = suite_create("mastersuite");
	srunner = srunner_create(s);

	/* Add all testsuites to the master suite. */
	srunner_add_suite(srunner, suite_core());
	srunner_add_suite(srunner, suite_decoder());
	srunner_add_suite(srunner, suite_inst());
	srunner_add_suite(srunner, suite_session());

	srunner_run_all(srunner, CK_VERBOSE);
	ret = srunner_ntests_failed(srunner);
	srunner_free(srunner);

	return (ret == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
