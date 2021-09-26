/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2013 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include <extdef.h>

#define __STDC_LIMIT_MACROS
#include <stdint.h>

#include <boost/test/unit_test.hpp>

#include "../../pv/data/analogsnapshot.h"

using namespace std;

using pv::data::AnalogSnapshot;

BOOST_AUTO_TEST_SUITE(AnalogSnapshotTest)

void push_analog(AnalogSnapshot &s, unsigned int num_samples,
	float value)
{
	sr_datafeed_analog analog;
	analog.num_samples = num_samples;

	float *data = new float[num_samples];
	analog.data = data;
	while(num_samples-- != 0)
		*data++ = value;

	s.append_payload(analog);
	delete[] (float*)analog.data;
}

BOOST_AUTO_TEST_CASE(Basic)
{
	// Create an empty AnalogSnapshot object
	sr_datafeed_analog analog;
	analog.num_samples = 0;
	analog.data = NULL;

	AnalogSnapshot s(analog);

	//----- Test AnalogSnapshot::push_analog -----//

	BOOST_CHECK(s.get_sample_count() == 0);
	for (unsigned int i = 0; i < AnalogSnapshot::ScaleStepCount; i++)
	{
		const AnalogSnapshot::Envelope &m = s._envelope_levels[i];
		BOOST_CHECK_EQUAL(m.length, 0);
		BOOST_CHECK_EQUAL(m.data_length, 0);
		BOOST_CHECK(m.samples == NULL);
	}

	// Push 8 samples of all zeros
	push_analog(s, 8, 0.0f);

	BOOST_CHECK(s.get_sample_count() == 8);

	// There should not be enough samples to have a single mip map sample
	for (unsigned int i = 0; i < AnalogSnapshot::ScaleStepCount; i++)
	{
		const AnalogSnapshot::Envelope &m = s._envelope_levels[i];
		BOOST_CHECK_EQUAL(m.length, 0);
		BOOST_CHECK_EQUAL(m.data_length, 0);
		BOOST_CHECK(m.samples == NULL);
	}

	// Push 8 samples of 1.0s to bring the total up to 16
	push_analog(s, 8, 1.0f);

	// There should now be enough data for exactly one sample
	// in mip map level 0, and that sample should be 0
	const AnalogSnapshot::Envelope &e0 = s._envelope_levels[0];
	BOOST_CHECK_EQUAL(e0.length, 1);
	BOOST_CHECK_EQUAL(e0.data_length, AnalogSnapshot::EnvelopeDataUnit);
	BOOST_REQUIRE(e0.samples != NULL);
	BOOST_CHECK_EQUAL(e0.samples[0].min, 0.0f);
	BOOST_CHECK_EQUAL(e0.samples[0].max, 1.0f);

	// The higher levels should still be empty
	for (unsigned int i = 1; i < AnalogSnapshot::ScaleStepCount; i++)
	{
		const AnalogSnapshot::Envelope &m = s._envelope_levels[i];
		BOOST_CHECK_EQUAL(m.length, 0);
		BOOST_CHECK_EQUAL(m.data_length, 0);
		BOOST_CHECK(m.samples == NULL);
	}

	// Push 240 samples of all zeros to bring the total up to 256
	push_analog(s, 240, -1.0f);

	BOOST_CHECK_EQUAL(e0.length, 16);
	BOOST_CHECK_EQUAL(e0.data_length, AnalogSnapshot::EnvelopeDataUnit);

	for (unsigned int i = 1; i < e0.length; i++) {
		BOOST_CHECK_EQUAL(e0.samples[i].min, -1.0f);
		BOOST_CHECK_EQUAL(e0.samples[i].max, -1.0f);
	}

	const AnalogSnapshot::Envelope &e1 = s._envelope_levels[1];
	BOOST_CHECK_EQUAL(e1.length, 1);
	BOOST_CHECK_EQUAL(e1.data_length, AnalogSnapshot::EnvelopeDataUnit);
	BOOST_REQUIRE(e1.samples != NULL);
	BOOST_CHECK_EQUAL(e1.samples[0].min, -1.0f);
	BOOST_CHECK_EQUAL(e1.samples[0].max, 1.0f);
}

BOOST_AUTO_TEST_SUITE_END()
