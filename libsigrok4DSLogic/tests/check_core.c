/*
 * This file is part of the libsigrok project.
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

#include <stdlib.h>
#include <check.h>
#include "../libsigrok.h"

/*
 * Check various basic init related things.
 *
 *  - Check whether an sr_init() call with a proper sr_ctx works.
 *    If it returns != SR_OK (or segfaults) this test will fail.
 *    The sr_init() call (among other things) also runs sanity checks on
 *    all libsigrok hardware drivers and errors out upon issues.
 *
 *  - Check whether a subsequent sr_exit() with that sr_ctx works.
 *    If it returns != SR_OK (or segfaults) this test will fail.
 */
START_TEST(test_init_exit)
{
	int ret;
	struct sr_context *sr_ctx;

	ret = sr_init(&sr_ctx);
	fail_unless(ret == SR_OK, "sr_init() failed: %d.", ret);
	ret = sr_exit(sr_ctx);
	fail_unless(ret == SR_OK, "sr_exit() failed: %d.", ret);
}
END_TEST

/*
 * Check whether two nested sr_init() and sr_exit() calls work.
 * The two functions have two different contexts.
 * If any function returns != SR_OK (or segfaults) this test will fail.
 */
START_TEST(test_init_exit_2)
{
	int ret;
	struct sr_context *sr_ctx1, *sr_ctx2;

	ret = sr_init(&sr_ctx1);
	fail_unless(ret == SR_OK, "sr_init() 1 failed: %d.", ret);
	ret = sr_init(&sr_ctx2);
	fail_unless(ret == SR_OK, "sr_init() 2 failed: %d.", ret);
	ret = sr_exit(sr_ctx2);
	fail_unless(ret == SR_OK, "sr_exit() 2 failed: %d.", ret);
	ret = sr_exit(sr_ctx1);
	fail_unless(ret == SR_OK, "sr_exit() 1 failed: %d.", ret);
}
END_TEST

/*
 * Same as above, but sr_exit() in the "wrong" order.
 * This should work fine, it's not a bug to do this.
 */
START_TEST(test_init_exit_2_reverse)
{
	int ret;
	struct sr_context *sr_ctx1, *sr_ctx2;

	ret = sr_init(&sr_ctx1);
	fail_unless(ret == SR_OK, "sr_init() 1 failed: %d.", ret);
	ret = sr_init(&sr_ctx2);
	fail_unless(ret == SR_OK, "sr_init() 2 failed: %d.", ret);
	ret = sr_exit(sr_ctx1);
	fail_unless(ret == SR_OK, "sr_exit() 1 failed: %d.", ret);
	ret = sr_exit(sr_ctx2);
	fail_unless(ret == SR_OK, "sr_exit() 2 failed: %d.", ret);
}
END_TEST

/*
 * Check whether three nested sr_init() and sr_exit() calls work.
 * The three functions have three different contexts.
 * If any function returns != SR_OK (or segfaults) this test will fail.
 */
START_TEST(test_init_exit_3)
{
	int ret;
	struct sr_context *sr_ctx1, *sr_ctx2, *sr_ctx3;

	ret = sr_init(&sr_ctx1);
	fail_unless(ret == SR_OK, "sr_init() 1 failed: %d.", ret);
	ret = sr_init(&sr_ctx2);
	fail_unless(ret == SR_OK, "sr_init() 2 failed: %d.", ret);
	ret = sr_init(&sr_ctx3);
	fail_unless(ret == SR_OK, "sr_init() 3 failed: %d.", ret);
	ret = sr_exit(sr_ctx3);
	fail_unless(ret == SR_OK, "sr_exit() 3 failed: %d.", ret);
	ret = sr_exit(sr_ctx2);
	fail_unless(ret == SR_OK, "sr_exit() 2 failed: %d.", ret);
	ret = sr_exit(sr_ctx1);
	fail_unless(ret == SR_OK, "sr_exit() 1 failed: %d.", ret);
}
END_TEST

/*
 * Same as above, but sr_exit() in the "wrong" order.
 * This should work fine, it's not a bug to do this.
 */
START_TEST(test_init_exit_3_reverse)
{
	int ret;
	struct sr_context *sr_ctx1, *sr_ctx2, *sr_ctx3;

	ret = sr_init(&sr_ctx1);
	fail_unless(ret == SR_OK, "sr_init() 1 failed: %d.", ret);
	ret = sr_init(&sr_ctx2);
	fail_unless(ret == SR_OK, "sr_init() 2 failed: %d.", ret);
	ret = sr_init(&sr_ctx3);
	fail_unless(ret == SR_OK, "sr_init() 3 failed: %d.", ret);
	ret = sr_exit(sr_ctx1);
	fail_unless(ret == SR_OK, "sr_exit() 1 failed: %d.", ret);
	ret = sr_exit(sr_ctx2);
	fail_unless(ret == SR_OK, "sr_exit() 2 failed: %d.", ret);
	ret = sr_exit(sr_ctx3);
	fail_unless(ret == SR_OK, "sr_exit() 3 failed: %d.", ret);
}
END_TEST

/* Check whether sr_init(NULL) fails as it should. */
START_TEST(test_init_null)
{
	int ret;

        ret = sr_log_loglevel_set(SR_LOG_NONE);
        fail_unless(ret == SR_OK, "sr_log_loglevel_set() failed: %d.", ret);

	ret = sr_init(NULL);
	fail_unless(ret != SR_OK, "sr_init(NULL) should have failed.");
}
END_TEST

/* Check whether sr_exit(NULL) fails as it should. */
START_TEST(test_exit_null)
{
	int ret;

        ret = sr_log_loglevel_set(SR_LOG_NONE);
        fail_unless(ret == SR_OK, "sr_log_loglevel_set() failed: %d.", ret);

	ret = sr_exit(NULL);
	fail_unless(ret != SR_OK, "sr_exit(NULL) should have failed.");
}
END_TEST

Suite *suite_core(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("core");

	tc = tcase_create("init_exit");
	tcase_add_test(tc, test_init_exit);
	tcase_add_test(tc, test_init_exit_2);
	tcase_add_test(tc, test_init_exit_2_reverse);
	tcase_add_test(tc, test_init_exit_3);
	tcase_add_test(tc, test_init_exit_3_reverse);
	tcase_add_test(tc, test_init_null);
	tcase_add_test(tc, test_exit_null);
	suite_add_tcase(s, tc);

	return s;
}
