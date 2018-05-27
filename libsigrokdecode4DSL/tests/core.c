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

/*
 * Check various basic init related things.
 *
 *  - Check whether an srd_init() call with path == NULL works.
 *    If it returns != SRD_OK (or segfaults) this test will fail.
 *
 *  - Check whether a subsequent srd_exit() works.
 *    If it returns != SRD_OK (or segfaults) this test will fail.
 */
START_TEST(test_init_exit)
{
	int ret;

	ret = srd_init(NULL);
	fail_unless(ret == SRD_OK, "srd_init() failed: %d.", ret);
	ret = srd_exit();
	fail_unless(ret == SRD_OK, "srd_exit() failed: %d.", ret);
}
END_TEST

/*
 * Check whether nested srd_init()/srd_exit() calls work/fail correctly.
 * Two consecutive srd_init() calls without any srd_exit() inbetween are
 * not allowed and should fail. However, two consecutive srd_exit() calls
 * are currently allowed, the second one will just be a NOP basically.
 */
START_TEST(test_init_exit_2)
{
	int ret;

	ret = srd_init(NULL);
	fail_unless(ret == SRD_OK, "srd_init() 1 failed: %d.", ret);
	ret = srd_init(NULL);
	fail_unless(ret != SRD_OK, "srd_init() 2 didn't fail: %d.", ret);
	ret = srd_exit();
	fail_unless(ret == SRD_OK, "srd_exit() 2 failed: %d.", ret);
	ret = srd_exit();
	fail_unless(ret == SRD_OK, "srd_exit() 1 failed: %d.", ret);
}
END_TEST

/*
 * Check whether three nested srd_init()/srd_exit() calls work/fail correctly.
 */
START_TEST(test_init_exit_3)
{
	int ret;

	ret = srd_init(NULL);
	fail_unless(ret == SRD_OK, "srd_init() 1 failed: %d.", ret);
	ret = srd_init(NULL);
	fail_unless(ret != SRD_OK, "srd_init() 2 didn't fail: %d.", ret);
	ret = srd_init(NULL);
	fail_unless(ret != SRD_OK, "srd_init() 3 didn't fail: %d.", ret);
	ret = srd_exit();
	fail_unless(ret == SRD_OK, "srd_exit() 3 failed: %d.", ret);
	ret = srd_exit();
	fail_unless(ret == SRD_OK, "srd_exit() 2 failed: %d.", ret);
	ret = srd_exit();
	fail_unless(ret == SRD_OK, "srd_exit() 1 failed: %d.", ret);
}
END_TEST

Suite *suite_core(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("core");

	tc = tcase_create("init_exit");
	tcase_add_checked_fixture(tc, srdtest_setup, srdtest_teardown);
	tcase_add_test(tc, test_init_exit);
	tcase_add_test(tc, test_init_exit_2);
	tcase_add_test(tc, test_init_exit_3);
	suite_add_tcase(s, tc);

	return s;
}
