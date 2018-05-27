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
 * Check whether srd_inst_new() works.
 * If it returns NULL (or segfaults) this test will fail.
 */
START_TEST(test_inst_new)
{
	struct srd_session *sess;
	struct srd_decoder_inst *inst;

	srd_init(DECODERS_TESTDIR);
	srd_decoder_load("uart");
	srd_session_new(&sess);
	inst = srd_inst_new(sess, "uart", NULL);
	fail_unless(inst != NULL, "srd_inst_new() failed.");
	srd_exit();
}
END_TEST

/*
 * Check whether multiple srd_inst_new() calls work.
 * If any of them returns NULL (or segfaults) this test will fail.
 */
START_TEST(test_inst_new_multiple)
{
	struct srd_session *sess;
	struct srd_decoder_inst *inst1, *inst2, *inst3;

	inst1 = inst2 = inst3 = NULL;

	srd_init(DECODERS_TESTDIR);
	srd_decoder_load_all();
	srd_session_new(&sess);

	/* Multiple srd_inst_new() calls must work. */
	inst1 = srd_inst_new(sess, "uart", NULL);
	fail_unless(inst1 != NULL, "srd_inst_new() 1 failed.");
	inst2 = srd_inst_new(sess, "spi", NULL);
	fail_unless(inst2 != NULL, "srd_inst_new() 2 failed.");
	inst3 = srd_inst_new(sess, "can", NULL);
	fail_unless(inst3 != NULL, "srd_inst_new() 3 failed.");

	/* The returned instance pointers must not be the same. */
	fail_unless(inst1 != inst2);
	fail_unless(inst1 != inst3);
	fail_unless(inst2 != inst3);

	/* Each instance must have another py_inst than any of the others. */
	fail_unless(inst1->py_inst != inst2->py_inst);
	fail_unless(inst1->py_inst != inst3->py_inst);
	fail_unless(inst2->py_inst != inst3->py_inst);

	srd_exit();
}
END_TEST

/*
 * Check whether srd_inst_option_set() works for an empty options hash.
 * If it returns != SRD_OK (or segfaults) this test will fail.
 */
START_TEST(test_inst_option_set_empty)
{
	int ret;
	struct srd_session *sess;
	struct srd_decoder_inst *inst;
	GHashTable *options;

	srd_init(DECODERS_TESTDIR);
	srd_decoder_load_all();
	srd_session_new(&sess);
	inst = srd_inst_new(sess, "uart", NULL);
	options = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
			(GDestroyNotify)g_variant_unref);
	ret = srd_inst_option_set(inst, options);
	fail_unless(ret == SRD_OK, "srd_inst_option_set() with empty options "
			"hash failed: %d.", ret);
	srd_exit();
}
END_TEST

/*
 * Check whether srd_inst_option_set() works for bogus options.
 * If it returns != SRD_OK (or segfaults) this test will fail.
 */
START_TEST(test_inst_option_set_bogus)
{
	int ret;
	struct srd_session *sess;
	struct srd_decoder_inst *inst;
	GHashTable *options;

	srd_init(DECODERS_TESTDIR);
	srd_decoder_load_all();
	srd_session_new(&sess);
	inst = srd_inst_new(sess, "uart", NULL);

	options = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
			(GDestroyNotify)g_variant_unref);

	/* NULL instance. */
	ret = srd_inst_option_set(NULL, options);
	fail_unless(ret != SRD_OK, "srd_inst_option_set() with NULL "
			"instance failed: %d.", ret);

	/* NULL 'options' GHashTable. */
	ret = srd_inst_option_set(inst, NULL);
	fail_unless(ret != SRD_OK, "srd_inst_option_set() with NULL "
			"options hash failed: %d.", ret);

	/* NULL instance and NULL 'options' GHashTable. */
	ret = srd_inst_option_set(NULL, NULL);
	fail_unless(ret != SRD_OK, "srd_inst_option_set() with NULL "
			"instance and NULL options hash failed: %d.", ret);

	srd_exit();
}
END_TEST

Suite *suite_inst(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("inst");

	tc = tcase_create("new");
	tcase_add_checked_fixture(tc, srdtest_setup, srdtest_teardown);
	tcase_add_test(tc, test_inst_new);
	tcase_add_test(tc, test_inst_new_multiple);
	suite_add_tcase(s, tc);

	tc = tcase_create("option");
	tcase_add_checked_fixture(tc, srdtest_setup, srdtest_teardown);
	tcase_add_test(tc, test_inst_option_set_empty);
	tcase_add_test(tc, test_inst_option_set_bogus);
	suite_add_tcase(s, tc);

	return s;
}
