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

#include <check.h>
#include "../libsigrok.h"

struct sr_context *sr_ctx;

static void setup(void)
{
	int ret;

	ret = sr_init(&sr_ctx);
	fail_unless(ret == SR_OK, "sr_init() failed: %d.", ret);
}

static void teardown(void)
{
	int ret;

	ret = sr_exit(sr_ctx);
	fail_unless(ret == SR_OK, "sr_exit() failed: %d.", ret);
}

static void test_samplerate(uint64_t samplerate, const char *expected)
{
	char *s;

	s = sr_samplerate_string(samplerate);
	fail_unless(s != NULL);
	fail_unless(!strcmp(s, expected),
		    "Invalid result for '%s': %s.", expected, s);
	g_free(s);
}

/*
 * Check various inputs for sr_samplerate_string():
 *
 *  - One, two, or three digit results (e.g. 5/55/555 MHz).
 *  - Results which contain commas (e.g. 1.234 / 12.34 / 123.4 kHz).
 *  - Results with zeroes right after the comma (e.g. 1.034 Hz).
 *    See also: http://sigrok.org/bugzilla/show_bug.cgi?id=73
 *  - Results with trailing zeroes (e.g. 1.230 kHz).
 *    (This is currently allowed, but might be changed later)
 *  - Results with zeroes in the middle (e.g. 1.204 kHz).
 *  - All of the above, but using SR_MHZ() and friends.
 *    See also: http://sigrok.org/bugzilla/show_bug.cgi?id=72
 *
 * All of the above tests are done for the Hz/kHz/MHz/GHz ranges.
 */

START_TEST(test_hz)
{
	test_samplerate(0, "0 Hz");
	test_samplerate(1, "1 Hz");
	test_samplerate(23, "23 Hz");
	test_samplerate(644, "644 Hz");
	test_samplerate(604, "604 Hz");
	test_samplerate(550, "550 Hz");

	/* Again, but now using SR_HZ(). */
	test_samplerate(SR_HZ(0), "0 Hz");
	test_samplerate(SR_HZ(1), "1 Hz");
	test_samplerate(SR_HZ(23), "23 Hz");
	test_samplerate(SR_HZ(644), "644 Hz");
	test_samplerate(SR_HZ(604), "604 Hz");
	test_samplerate(SR_HZ(550), "550 Hz");
}
END_TEST

START_TEST(test_khz)
{
	test_samplerate(1000, "1 kHz");
	test_samplerate(99000, "99 kHz");
	test_samplerate(225000, "225 kHz");
	test_samplerate(1234, "1.234 kHz");
	test_samplerate(12345, "12.345 kHz");
	test_samplerate(123456, "123.456 kHz");
	test_samplerate(1034, "1.034 kHz");
	test_samplerate(1004, "1.004 kHz");
	test_samplerate(1230, "1.230 kHz");

	/* Again, but now using SR_KHZ(). */
	test_samplerate(SR_KHZ(1), "1 kHz");
	test_samplerate(SR_KHZ(99), "99 kHz");
	test_samplerate(SR_KHZ(225), "225 kHz");
	test_samplerate(SR_KHZ(1.234), "1.234 kHz");
	test_samplerate(SR_KHZ(12.345), "12.345 kHz");
	test_samplerate(SR_KHZ(123.456), "123.456 kHz");
	test_samplerate(SR_KHZ(1.204), "1.204 kHz");
	test_samplerate(SR_KHZ(1.034), "1.034 kHz");
	test_samplerate(SR_KHZ(1.004), "1.004 kHz");
	test_samplerate(SR_KHZ(1.230), "1.230 kHz");
}
END_TEST

START_TEST(test_mhz)
{
	test_samplerate(1000000, "1 MHz");
	test_samplerate(28000000, "28 MHz");
	test_samplerate(775000000, "775 MHz");
	test_samplerate(1234567, "1.234567 MHz");
	test_samplerate(12345678, "12.345678 MHz");
	test_samplerate(123456789, "123.456789 MHz");
	test_samplerate(1230007, "1.230007 MHz");
	test_samplerate(1034567, "1.034567 MHz");
	test_samplerate(1000007, "1.000007 MHz");
	test_samplerate(1234000, "1.234000 MHz");

	/* Again, but now using SR_MHZ(). */
	test_samplerate(SR_MHZ(1), "1 MHz");
	test_samplerate(SR_MHZ(28), "28 MHz");
	test_samplerate(SR_MHZ(775), "775 MHz");
	test_samplerate(SR_MHZ(1.234567), "1.234567 MHz");
	test_samplerate(SR_MHZ(12.345678), "12.345678 MHz");
	test_samplerate(SR_MHZ(123.456789), "123.456789 MHz");
	test_samplerate(SR_MHZ(1.230007), "1.230007 MHz");
	test_samplerate(SR_MHZ(1.034567), "1.034567 MHz");
	test_samplerate(SR_MHZ(1.000007), "1.000007 MHz");
	test_samplerate(SR_MHZ(1.234000), "1.234000 MHz");
}
END_TEST

START_TEST(test_ghz)
{
	/* Note: Numbers > 2^32 need a ULL suffix. */

	test_samplerate(1000000000, "1 GHz");
	test_samplerate(5000000000ULL, "5 GHz");
	test_samplerate(72000000000ULL, "72 GHz");
	test_samplerate(388000000000ULL, "388 GHz");
	test_samplerate(4417594444ULL, "4.417594444 GHz");
	test_samplerate(44175944444ULL, "44.175944444 GHz");
	test_samplerate(441759444441ULL, "441.759444441 GHz");
	test_samplerate(441759000001ULL, "441.759000001 GHz");
	test_samplerate(441050000000ULL, "441.05 GHz");
	test_samplerate(441000000005ULL, "441.000000005 GHz");
	test_samplerate(441500000000ULL, "441.500000000 GHz");

	/* Again, but now using SR_GHZ(). */
	test_samplerate(SR_GHZ(1), "1 GHz");
	test_samplerate(SR_GHZ(5), "5 GHz");
	test_samplerate(SR_GHZ(72), "72 GHz");
	test_samplerate(SR_GHZ(388), "388 GHz");
	test_samplerate(SR_GHZ(4.417594444), "4.417594444 GHz");
	test_samplerate(SR_GHZ(44.175944444), "44.175944444 GHz");
	test_samplerate(SR_GHZ(441.759444441), "441.759444441 GHz");
	test_samplerate(SR_GHZ(441.759000001), "441.759000001 GHz");
	test_samplerate(SR_GHZ(441.050000000), "441.05 GHz");
	test_samplerate(SR_GHZ(441.000000005), "441.000000005 GHz");
	test_samplerate(SR_GHZ(441.500000000), "441.500000000 GHz");

	/* Now check the biggest-possible samplerate (2^64 Hz). */
	test_samplerate(18446744073709551615ULL, "18446744073.709551615 GHz");
	test_samplerate(SR_GHZ(18446744073ULL), "18446744073 GHz");
}
END_TEST

Suite *suite_strutil(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("strutil");

	tc = tcase_create("sr_samplerate_string");
	tcase_add_checked_fixture(tc, setup, teardown);
	tcase_add_test(tc, test_hz);
	tcase_add_test(tc, test_khz);
	tcase_add_test(tc, test_mhz);
	tcase_add_test(tc, test_ghz);
	suite_add_tcase(s, tc);

	return s;
}
