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
 * Check whether srd_decoder_load_all() works.
 * If it returns != SRD_OK (or segfaults) this test will fail.
 */
START_TEST(test_load_all)
{
	int ret;

	srd_init(DECODERS_TESTDIR);
	ret = srd_decoder_load_all();
	fail_unless(ret == SRD_OK, "srd_decoder_load_all() failed: %d.", ret);
	srd_exit();
}
END_TEST

/*
 * Check whether srd_decoder_load_all() fails without prior srd_init().
 * If it returns != SRD_OK (or segfaults) this test will fail.
 * See also: http://sigrok.org/bugzilla/show_bug.cgi?id=178
 */
START_TEST(test_load_all_no_init)
{
	int ret;

	ret = srd_decoder_load_all();
	fail_unless(ret != SRD_OK, "srd_decoder_load_all() didn't fail properly.");
}
END_TEST

/*
 * Check whether srd_decoder_load() works.
 * If it returns != SRD_OK (or segfaults) this test will fail.
 */
START_TEST(test_load)
{
	int ret;

	srd_init(DECODERS_TESTDIR);
	ret = srd_decoder_load("uart");
	fail_unless(ret == SRD_OK, "srd_decoder_load(uart) failed: %d.", ret);
	ret = srd_decoder_load("spi");
	fail_unless(ret == SRD_OK, "srd_decoder_load(spi) failed: %d.", ret);
	ret = srd_decoder_load("usb_signalling");
	fail_unless(ret == SRD_OK, "srd_decoder_load(usb_signalling) failed: %d.", ret);
	srd_exit();
}
END_TEST

/*
 * Check whether srd_decoder_load() fails for non-existing or bogus PDs.
 * If it returns SRD_OK (or segfaults) this test will fail.
 */
START_TEST(test_load_bogus)
{
	srd_init(DECODERS_TESTDIR);
	/* http://sigrok.org/bugzilla/show_bug.cgi?id=176 */
	fail_unless(srd_decoder_load(NULL) != SRD_OK);
	fail_unless(srd_decoder_load("") != SRD_OK);
	fail_unless(srd_decoder_load(" ") != SRD_OK);
	fail_unless(srd_decoder_load("nonexisting") != SRD_OK);
	fail_unless(srd_decoder_load("UART") != SRD_OK);
	fail_unless(srd_decoder_load("UaRt") != SRD_OK);
	fail_unless(srd_decoder_load("u a r t") != SRD_OK);
	fail_unless(srd_decoder_load("uart ") != SRD_OK);
	fail_unless(srd_decoder_load(" uart") != SRD_OK);
	fail_unless(srd_decoder_load(" uart ") != SRD_OK);
	fail_unless(srd_decoder_load("uart spi") != SRD_OK);
	srd_exit();
}
END_TEST

/*
 * Check whether srd_decoder_load() works/fails for valid/bogus PDs.
 * If it returns incorrect values (or segfaults) this test will fail.
 */
START_TEST(test_load_valid_and_bogus)
{
	srd_init(DECODERS_TESTDIR);
	fail_unless(srd_decoder_load("") != SRD_OK);
	fail_unless(srd_decoder_load("uart") == SRD_OK);
	fail_unless(srd_decoder_load("") != SRD_OK);
	fail_unless(srd_decoder_load("spi") == SRD_OK);
	fail_unless(srd_decoder_load("") != SRD_OK);
	fail_unless(srd_decoder_load("can") == SRD_OK);
	fail_unless(srd_decoder_load("") != SRD_OK);
	srd_exit();
}
END_TEST

/*
 * Check whether srd_decoder_load() fails when run multiple times.
 * If it returns a value != SRD_OK (or segfaults) this test will fail.
 * See also: http://sigrok.org/bugzilla/show_bug.cgi?id=177
 */
START_TEST(test_load_multiple)
{
	int ret;

	srd_init(DECODERS_TESTDIR);
	ret = srd_decoder_load("uart");
	fail_unless(ret == SRD_OK, "Loading uart PD 1x failed: %d", ret);
	ret = srd_decoder_load("uart");
	fail_unless(ret == SRD_OK, "Loading uart PD 2x failed: %d", ret);
	ret = srd_decoder_load("uart");
	fail_unless(ret == SRD_OK, "Loading uart PD 3x failed: %d", ret);
	srd_exit();
}
END_TEST

/*
 * Check whether srd_decoder_load() fails if a non-existing PD dir is used.
 * If it returns SRD_OK (or segfaults) this test will fail.
 */
START_TEST(test_load_nonexisting_pd_dir)
{
#if 0
	/* TODO: Build libsigrokdecode with no default PD dir. */
	srd_init("/nonexisting_dir");
	fail_unless(srd_decoder_load("spi") != SRD_OK);
	fail_unless(g_slist_length((GSList *)srd_decoder_list()) == 0);
	srd_exit();
#endif
}
END_TEST

/*
 * Check whether srd_decoder_unload_all() works.
 * If it returns != SRD_OK (or segfaults) this test will fail.
 */
START_TEST(test_unload_all)
{
	int ret;

	srd_init(DECODERS_TESTDIR);
	ret = srd_decoder_load_all();
	fail_unless(ret == SRD_OK, "srd_decoder_load_all() failed: %d.", ret);
	ret = srd_decoder_unload_all();
	fail_unless(ret == SRD_OK, "srd_decoder_unload_all() failed: %d.", ret);
	srd_exit();
}
END_TEST

/*
 * Check whether srd_decoder_unload_all() works without prior srd_init().
 * If it returns != SRD_OK (or segfaults) this test will fail.
 */
START_TEST(test_unload_all_no_init)
{
	int ret;

	ret = srd_decoder_unload_all();
	fail_unless(ret == SRD_OK, "srd_decoder_unload_all() failed: %d.", ret);
}
END_TEST

/*
 * Check whether srd_decoder_unload_all() works multiple times.
 * If it returns != SRD_OK (or segfaults) this test will fail.
 */
START_TEST(test_unload_all_multiple)
{
	int ret, i;

	srd_init(DECODERS_TESTDIR);
	for (i = 0; i < 10; i++) {
		ret = srd_decoder_load_all();
		fail_unless(ret == SRD_OK, "srd_decoder_load_all() failed: %d.", ret);
		ret = srd_decoder_unload_all();
		fail_unless(ret == SRD_OK, "srd_decoder_unload_all() failed: %d.", ret);
	}
	srd_exit();
}
END_TEST

/*
 * Check whether srd_decoder_unload_all() works multiple times (no load).
 * If it returns != SRD_OK (or segfaults) this test will fail.
 */
START_TEST(test_unload_all_multiple_noload)
{
	int ret, i;

	srd_init(DECODERS_TESTDIR);
	for (i = 0; i < 10; i++) {
		ret = srd_decoder_unload_all();
		fail_unless(ret == SRD_OK, "srd_decoder_unload_all() failed: %d.", ret);
	}
	srd_exit();
}
END_TEST

/*
 * Check whether srd_decoder_unload() works.
 * If it returns != SRD_OK (or segfaults) this test will fail.
 */
START_TEST(test_unload)
{
	int ret;
	struct srd_decoder *dec;

	srd_init(DECODERS_TESTDIR);
	ret = srd_decoder_load("uart");
	fail_unless(ret == SRD_OK, "srd_decoder_load(uart) failed: %d.", ret);
	dec = srd_decoder_get_by_id("uart");
	fail_unless(dec != NULL);
	ret = srd_decoder_unload(dec);
	fail_unless(ret == SRD_OK, "srd_decoder_unload() failed: %d.", ret);
	srd_exit();
}
END_TEST

/*
 * Check whether srd_decoder_unload(NULL) fails.
 * If it returns SRD_OK (or segfaults) this test will fail.
 */
START_TEST(test_unload_null)
{
	srd_init(DECODERS_TESTDIR);
	fail_unless(srd_decoder_unload(NULL) != SRD_OK);
	srd_exit();
}
END_TEST

/*
 * Check whether srd_decoder_unload(NULL) fails without prior srd_init().
 * If it returns SRD_OK (or segfaults) this test will fail.
 */
START_TEST(test_unload_null_no_init)
{
	fail_unless(srd_decoder_unload(NULL) != SRD_OK);
}
END_TEST

/*
 * Check whether srd_decoder_list() returns a non-empty list.
 * If it returns an empty list (or segfaults) this test will fail.
 */
START_TEST(test_decoder_list)
{
	srd_init(DECODERS_TESTDIR);
	srd_decoder_load_all();
	fail_unless(srd_decoder_list() != NULL);
	srd_exit();
}
END_TEST

/*
 * Check whether srd_decoder_list() without prior srd_decoder_load_all()
 * returns an empty list (return value != NULL).
 * If it returns a non-empty list (or segfaults) this test will fail.
 */
START_TEST(test_decoder_list_no_load)
{
	srd_init(DECODERS_TESTDIR);
	fail_unless(srd_decoder_list() == NULL);
	srd_exit();
}
END_TEST

/*
 * Check whether srd_decoder_list() without prior srd_init()
 * returns an empty list.
 * If it returns a non-empty list (or segfaults) this test will fail.
 * See also: http://sigrok.org/bugzilla/show_bug.cgi?id=178
 */
START_TEST(test_decoder_list_no_init)
{
	srd_decoder_load_all();
	fail_unless(srd_decoder_list() == NULL);
}
END_TEST

/*
 * Check whether srd_decoder_list() without prior srd_init() and without
 * prior srd_decoder_load_all() returns an empty list.
 * If it returns a non-empty list (or segfaults) this test will fail.
 */
START_TEST(test_decoder_list_no_init_no_load)
{
	fail_unless(srd_decoder_list() == NULL);
}
END_TEST

/*
 * Check whether srd_decoder_list() returns the correct number of PDs.
 * If it returns a wrong number (or segfaults) this test will fail.
 */
START_TEST(test_decoder_list_correct_numbers)
{
	srd_init(DECODERS_TESTDIR);
	fail_unless(g_slist_length((GSList *)srd_decoder_list()) == 0);
	srd_decoder_load("spi");
	fail_unless(g_slist_length((GSList *)srd_decoder_list()) == 1);
	srd_decoder_load("uart");
	fail_unless(g_slist_length((GSList *)srd_decoder_list()) == 2);
	srd_decoder_load("can");
	fail_unless(g_slist_length((GSList *)srd_decoder_list()) == 3);
	srd_decoder_load("can"); /* Load same PD twice. */
	fail_unless(g_slist_length((GSList *)srd_decoder_list()) == 3);
	srd_exit();
}
END_TEST

/*
 * Check whether srd_decoder_get_by_id() works.
 * If it returns NULL for valid PDs (or segfaults) this test will fail.
 */
START_TEST(test_get_by_id)
{
	srd_init(DECODERS_TESTDIR);
	srd_decoder_load("uart");
	fail_unless(srd_decoder_get_by_id("uart") != NULL);
	fail_unless(srd_decoder_get_by_id("can") == NULL);
	srd_decoder_load("can");
	fail_unless(srd_decoder_get_by_id("uart") != NULL);
	fail_unless(srd_decoder_get_by_id("can") != NULL);
	srd_exit();
}
END_TEST

/*
 * Check whether srd_decoder_get_by_id() works multiple times in a row.
 * If it returns NULL for valid PDs (or segfaults) this test will fail.
 */
START_TEST(test_get_by_id_multiple)
{
	srd_init(DECODERS_TESTDIR);
	srd_decoder_load("uart");
	fail_unless(srd_decoder_get_by_id("uart") != NULL);
	fail_unless(srd_decoder_get_by_id("uart") != NULL);
	fail_unless(srd_decoder_get_by_id("uart") != NULL);
	fail_unless(srd_decoder_get_by_id("uart") != NULL);
	srd_exit();
}
END_TEST

/*
 * Check whether srd_decoder_get_by_id() fails for bogus PDs.
 * If it returns a value != NULL (or segfaults) this test will fail.
 */
START_TEST(test_get_by_id_bogus)
{
	srd_init(DECODERS_TESTDIR);
	fail_unless(srd_decoder_get_by_id(NULL) == NULL);
	fail_unless(srd_decoder_get_by_id("") == NULL);
	fail_unless(srd_decoder_get_by_id(" ") == NULL);
	fail_unless(srd_decoder_get_by_id("nonexisting") == NULL);
	fail_unless(srd_decoder_get_by_id("sPi") == NULL);
	fail_unless(srd_decoder_get_by_id("SPI") == NULL);
	fail_unless(srd_decoder_get_by_id("s p i") == NULL);
	fail_unless(srd_decoder_get_by_id(" spi") == NULL);
	fail_unless(srd_decoder_get_by_id("spi ") == NULL);
	fail_unless(srd_decoder_get_by_id(" spi ") == NULL);
	fail_unless(srd_decoder_get_by_id("spi uart") == NULL);
	srd_exit();
}
END_TEST

/*
 * Check whether srd_decoder_doc_get() works.
 * If it returns NULL for valid PDs (or segfaults) this test will fail.
 */
START_TEST(test_doc_get)
{
	struct srd_decoder *dec;

	srd_init(DECODERS_TESTDIR);
	srd_decoder_load("uart");
	dec = srd_decoder_get_by_id("uart");
	fail_unless(srd_decoder_doc_get(dec) != NULL);
	srd_exit();
}
END_TEST

/*
 * Check whether srd_decoder_doc_get() fails with NULL as argument.
 * If it returns a value != NULL (or segfaults) this test will fail.
 * See also: http://sigrok.org/bugzilla/show_bug.cgi?id=179
 */
START_TEST(test_doc_get_null)
{
	srd_init(DECODERS_TESTDIR);
	fail_unless(srd_decoder_doc_get(NULL) == NULL);
	srd_exit();
}
END_TEST

Suite *suite_decoder(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("decoder");

	tc = tcase_create("load");
	tcase_set_timeout(tc, 0);
	tcase_add_checked_fixture(tc, srdtest_setup, srdtest_teardown);
	tcase_add_test(tc, test_load_all);
	tcase_add_test(tc, test_load_all_no_init);
	tcase_add_test(tc, test_load);
	tcase_add_test(tc, test_load_bogus);
	tcase_add_test(tc, test_load_valid_and_bogus);
	tcase_add_test(tc, test_load_multiple);
	tcase_add_test(tc, test_load_nonexisting_pd_dir);
	suite_add_tcase(s, tc);

	tc = tcase_create("unload");
	tcase_add_checked_fixture(tc, srdtest_setup, srdtest_teardown);
	tcase_add_test(tc, test_unload_all);
	tcase_add_test(tc, test_unload_all_no_init);
	tcase_add_test(tc, test_unload_all_multiple);
	tcase_add_test(tc, test_unload_all_multiple_noload);
	tcase_add_test(tc, test_unload);
	tcase_add_test(tc, test_unload_null);
	tcase_add_test(tc, test_unload_null_no_init);
	suite_add_tcase(s, tc);

	tc = tcase_create("list");
	tcase_add_checked_fixture(tc, srdtest_setup, srdtest_teardown);
	tcase_add_test(tc, test_decoder_list);
	tcase_add_test(tc, test_decoder_list_no_load);
	tcase_add_test(tc, test_decoder_list_no_init);
	tcase_add_test(tc, test_decoder_list_no_init_no_load);
	tcase_add_test(tc, test_decoder_list_correct_numbers);
	suite_add_tcase(s, tc);

	tc = tcase_create("get_by_id");
	tcase_add_test(tc, test_get_by_id);
	tcase_add_test(tc, test_get_by_id_multiple);
	tcase_add_test(tc, test_get_by_id_bogus);
	suite_add_tcase(s, tc);

	tc = tcase_create("doc_get");
	tcase_add_test(tc, test_doc_get);
	tcase_add_test(tc, test_doc_get_null);
	suite_add_tcase(s, tc);

	return s;
}
