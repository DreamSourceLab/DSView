/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
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

#include "../../pv/data/logicsnapshot.h"

using namespace std;

using pv::data::LogicSnapshot;

BOOST_AUTO_TEST_SUITE(LogicSnapshotTest)

void push_logic(LogicSnapshot &s, unsigned int length, uint8_t value)
{
	sr_datafeed_logic logic;
	logic.unitsize = 1;
	logic.length = length;
	logic.data = new uint8_t[length];
	memset(logic.data, value, length * logic.unitsize);
	s.append_payload(logic);
	delete[] (uint8_t*)logic.data;
}

BOOST_AUTO_TEST_CASE(Pow2)
{
	BOOST_CHECK_EQUAL(LogicSnapshot::pow2_ceil(0, 0), 0);
	BOOST_CHECK_EQUAL(LogicSnapshot::pow2_ceil(1, 0), 1);
	BOOST_CHECK_EQUAL(LogicSnapshot::pow2_ceil(2, 0), 2);

	BOOST_CHECK_EQUAL(
		LogicSnapshot::pow2_ceil(INT64_MIN, 0), INT64_MIN);
	BOOST_CHECK_EQUAL(
		LogicSnapshot::pow2_ceil(INT64_MAX, 0), INT64_MAX);

	BOOST_CHECK_EQUAL(LogicSnapshot::pow2_ceil(0, 1), 0);
	BOOST_CHECK_EQUAL(LogicSnapshot::pow2_ceil(1, 1), 2);
	BOOST_CHECK_EQUAL(LogicSnapshot::pow2_ceil(2, 1), 2);
	BOOST_CHECK_EQUAL(LogicSnapshot::pow2_ceil(3, 1), 4);
}

BOOST_AUTO_TEST_CASE(Basic)
{
	// Create an empty LogicSnapshot object
	sr_datafeed_logic logic;
	logic.length = 0;
	logic.unitsize = 1;
	logic.data = NULL;

	LogicSnapshot s(logic);

	//----- Test LogicSnapshot::push_logic -----//

	BOOST_CHECK(s.get_sample_count() == 0);
	for (unsigned int i = 0; i < LogicSnapshot::ScaleStepCount; i++)
	{
		const LogicSnapshot::MipMapLevel &m = s._mip_map[i];
		BOOST_CHECK_EQUAL(m.length, 0);
		BOOST_CHECK_EQUAL(m.data_length, 0);
		BOOST_CHECK(m.data == NULL);
	}

	// Push 8 samples of all zeros
	push_logic(s, 8, 0);

	BOOST_CHECK(s.get_sample_count() == 8);

	// There should not be enough samples to have a single mip map sample
	for (unsigned int i = 0; i < LogicSnapshot::ScaleStepCount; i++)
	{
		const LogicSnapshot::MipMapLevel &m = s._mip_map[i];
		BOOST_CHECK_EQUAL(m.length, 0);
		BOOST_CHECK_EQUAL(m.data_length, 0);
		BOOST_CHECK(m.data == NULL);
	}

	// Push 8 samples of 0x11s to bring the total up to 16
	push_logic(s, 8, 0x11);

	// There should now be enough data for exactly one sample
	// in mip map level 0, and that sample should be 0
	const LogicSnapshot::MipMapLevel &m0 = s._mip_map[0];
	BOOST_CHECK_EQUAL(m0.length, 1);
	BOOST_CHECK_EQUAL(m0.data_length, LogicSnapshot::MipMapDataUnit);
	BOOST_REQUIRE(m0.data != NULL);
	BOOST_CHECK_EQUAL(((uint8_t*)m0.data)[0], 0x11);

	// The higher levels should still be empty
	for (unsigned int i = 1; i < LogicSnapshot::ScaleStepCount; i++)
	{
		const LogicSnapshot::MipMapLevel &m = s._mip_map[i];
		BOOST_CHECK_EQUAL(m.length, 0);
		BOOST_CHECK_EQUAL(m.data_length, 0);
		BOOST_CHECK(m.data == NULL);
	}

	// Push 240 samples of all zeros to bring the total up to 256
	push_logic(s, 240, 0);

	BOOST_CHECK_EQUAL(m0.length, 16);
	BOOST_CHECK_EQUAL(m0.data_length, LogicSnapshot::MipMapDataUnit);

	BOOST_CHECK_EQUAL(((uint8_t*)m0.data)[1], 0x11);
	for (unsigned int i = 2; i < m0.length; i++)
		BOOST_CHECK_EQUAL(((uint8_t*)m0.data)[i], 0);

	const LogicSnapshot::MipMapLevel &m1 = s._mip_map[1];
	BOOST_CHECK_EQUAL(m1.length, 1);
	BOOST_CHECK_EQUAL(m1.data_length, LogicSnapshot::MipMapDataUnit);
	BOOST_REQUIRE(m1.data != NULL);
	BOOST_CHECK_EQUAL(((uint8_t*)m1.data)[0], 0x11);

	//----- Test LogicSnapshot::get_subsampled_edges -----//

	// Test a full view at full zoom.
	vector<LogicSnapshot::EdgePair> edges;
	s.get_subsampled_edges(edges, 0, 255, 1, 0);
	BOOST_REQUIRE_EQUAL(edges.size(), 4);

	BOOST_CHECK_EQUAL(edges[0].first, 0);
	BOOST_CHECK_EQUAL(edges[1].first, 8);
	BOOST_CHECK_EQUAL(edges[2].first, 16);
	BOOST_CHECK_EQUAL(edges[3].first, 255);

	// Test a subset at high zoom
	edges.clear();
	s.get_subsampled_edges(edges, 6, 17, 0.05f, 0);
	BOOST_REQUIRE_EQUAL(edges.size(), 4);

	BOOST_CHECK_EQUAL(edges[0].first, 6);
	BOOST_CHECK_EQUAL(edges[1].first, 8);
	BOOST_CHECK_EQUAL(edges[2].first, 16);
	BOOST_CHECK_EQUAL(edges[3].first, 17);
}

BOOST_AUTO_TEST_CASE(LargeData)
{
	uint8_t prev_sample;
	const unsigned int Length = 1000000;

	sr_datafeed_logic logic;
	logic.unitsize = 1;
	logic.length = Length;
	logic.data = new uint8_t[Length];
	uint8_t *data = (uint8_t*)logic.data;

	for (unsigned int i = 0; i < Length; i++)
		*data++ = (uint8_t)(i >> 8);

	LogicSnapshot s(logic);
	delete[] (uint8_t*)logic.data;

	BOOST_CHECK(s.get_sample_count() == Length);

	// Check mip map level 0
	BOOST_CHECK_EQUAL(s._mip_map[0].length, 62500);
	BOOST_CHECK_EQUAL(s._mip_map[0].data_length,
		LogicSnapshot::MipMapDataUnit);
	BOOST_REQUIRE(s._mip_map[0].data != NULL);

	prev_sample = 0;
	for (unsigned int i = 0; i < s._mip_map[0].length;)
	{
		BOOST_TEST_MESSAGE("Testing mip_map[0].data[" << i << "]");

		const uint8_t sample = (uint8_t)((i*16) >> 8);
		BOOST_CHECK_EQUAL(s.get_subsample(0, i++) & 0xFF,
			prev_sample ^ sample);
		prev_sample = sample;

		for (int j = 1; i < s._mip_map[0].length && j < 16; j++)
		{
			BOOST_TEST_MESSAGE("Testing mip_map[0].data[" << i << "]");
			BOOST_CHECK_EQUAL(s.get_subsample(0, i++) & 0xFF, 0);
		}
	}

	// Check mip map level 1
	BOOST_CHECK_EQUAL(s._mip_map[1].length, 3906);
	BOOST_CHECK_EQUAL(s._mip_map[1].data_length,
		LogicSnapshot::MipMapDataUnit);
	BOOST_REQUIRE(s._mip_map[1].data != NULL);

	prev_sample = 0;
	for (unsigned int i = 0; i < s._mip_map[1].length; i++)
	{
		BOOST_TEST_MESSAGE("Testing mip_map[1].data[" << i << "]");

		const uint8_t sample = i;
		const uint8_t expected = sample ^ prev_sample;
		prev_sample = i;

		BOOST_CHECK_EQUAL(s.get_subsample(1, i) & 0xFF, expected);
	}

	// Check mip map level 2
	BOOST_CHECK_EQUAL(s._mip_map[2].length, 244);
	BOOST_CHECK_EQUAL(s._mip_map[2].data_length,
		LogicSnapshot::MipMapDataUnit);
	BOOST_REQUIRE(s._mip_map[2].data != NULL);

	prev_sample = 0;
	for (unsigned int i = 0; i < s._mip_map[2].length; i++)
	{
		BOOST_TEST_MESSAGE("Testing mip_map[2].data[" << i << "]");

		const uint8_t sample = i << 4;
		const uint8_t expected = (sample ^ prev_sample) | 0x0F;
		prev_sample = sample;

		BOOST_CHECK_EQUAL(s.get_subsample(2, i) & 0xFF, expected);
	}

	// Check mip map level 3
	BOOST_CHECK_EQUAL(s._mip_map[3].length, 15);
	BOOST_CHECK_EQUAL(s._mip_map[3].data_length,
		LogicSnapshot::MipMapDataUnit);
	BOOST_REQUIRE(s._mip_map[3].data != NULL);

	for (unsigned int i = 0; i < s._mip_map[3].length; i++)
		BOOST_CHECK_EQUAL(*((uint8_t*)s._mip_map[3].data + i),
			0xFF);

	// Check the higher levels
	for (unsigned int i = 4; i < LogicSnapshot::ScaleStepCount; i++)
	{
		const LogicSnapshot::MipMapLevel &m = s._mip_map[i];
		BOOST_CHECK_EQUAL(m.length, 0);
		BOOST_CHECK_EQUAL(m.data_length, 0);
		BOOST_CHECK(m.data == NULL);
	}

	//----- Test LogicSnapshot::get_subsampled_edges -----//
	// Check in normal case
	vector<LogicSnapshot::EdgePair> edges;
	s.get_subsampled_edges(edges, 0, Length-1, 1, 7);

	BOOST_CHECK_EQUAL(edges.size(), 32);

	for (unsigned int i = 0; i < edges.size() - 1; i++)
	{
		BOOST_CHECK_EQUAL(edges[i].first, i * 32768);
		BOOST_CHECK_EQUAL(edges[i].second, i & 1);
	}

	BOOST_CHECK_EQUAL(edges[31].first, 999999);

	// Check in very low zoom case
	edges.clear();
	s.get_subsampled_edges(edges, 0, Length-1, 50e6f, 7);

	BOOST_CHECK_EQUAL(edges.size(), 2);
}

BOOST_AUTO_TEST_CASE(Pulses)
{
	const int Cycles = 3;
	const int Period = 64;
	const int Length = Cycles * Period;

	vector<LogicSnapshot::EdgePair> edges;

	//----- Create a LogicSnapshot -----//
	sr_datafeed_logic logic;
	logic.unitsize = 1;
	logic.length = Length;
	logic.data = (uint64_t*)new uint8_t[Length];
	uint8_t *p = (uint8_t*)logic.data;

	for (int i = 0; i < Cycles; i++) {
		*p++ = 0xFF;
		for (int j = 1; j < Period; j++)
			*p++ = 0x00;
	}

	LogicSnapshot s(logic);
	delete[] (uint8_t*)logic.data;

	//----- Check the mip-map -----//
	// Check mip map level 0
	BOOST_CHECK_EQUAL(s._mip_map[0].length, 12);
	BOOST_CHECK_EQUAL(s._mip_map[0].data_length,
		LogicSnapshot::MipMapDataUnit);
	BOOST_REQUIRE(s._mip_map[0].data != NULL);

	for (unsigned int i = 0; i < s._mip_map[0].length;) {
		BOOST_TEST_MESSAGE("Testing mip_map[0].data[" << i << "]");
		BOOST_CHECK_EQUAL(s.get_subsample(0, i++) & 0xFF, 0xFF);

		for (int j = 1;
			i < s._mip_map[0].length &&
			j < Period/LogicSnapshot::MipMapScaleFactor; j++) {
			BOOST_TEST_MESSAGE(
				"Testing mip_map[0].data[" << i << "]");
			BOOST_CHECK_EQUAL(s.get_subsample(0, i++) & 0xFF, 0x00);
		}
	}

	// Check the higher levels are all inactive
	for (unsigned int i = 1; i < LogicSnapshot::ScaleStepCount; i++) {
		const LogicSnapshot::MipMapLevel &m = s._mip_map[i];
		BOOST_CHECK_EQUAL(m.length, 0);
		BOOST_CHECK_EQUAL(m.data_length, 0);
		BOOST_CHECK(m.data == NULL);
	}

	//----- Test get_subsampled_edges at reduced scale -----//
	s.get_subsampled_edges(edges, 0, Length-1, 16.0f, 2);
	BOOST_REQUIRE_EQUAL(edges.size(), Cycles + 2);

	BOOST_CHECK_EQUAL(0, false);
	for (unsigned int i = 1; i < edges.size(); i++)
		BOOST_CHECK_EQUAL(edges[i].second, false);
}

BOOST_AUTO_TEST_CASE(LongPulses)
{
	const int Cycles = 3;
	const int Period = 64;
	const int PulseWidth = 16;
	const int Length = Cycles * Period;

	int j;
	vector<LogicSnapshot::EdgePair> edges;

	//----- Create a LogicSnapshot -----//
	sr_datafeed_logic logic;
	logic.unitsize = 8;
	logic.length = Length * 8;
	logic.data = (uint64_t*)new uint64_t[Length];
	uint64_t *p = (uint64_t*)logic.data;

	for (int i = 0; i < Cycles; i++) {
		for (j = 0; j < PulseWidth; j++)
			*p++ = ~0;
		for (; j < Period; j++)
			*p++ = 0;
	}

	LogicSnapshot s(logic);
	delete[] (uint64_t*)logic.data;

	//----- Check the mip-map -----//
	// Check mip map level 0
	BOOST_CHECK_EQUAL(s._mip_map[0].length, 12);
	BOOST_CHECK_EQUAL(s._mip_map[0].data_length,
		LogicSnapshot::MipMapDataUnit);
	BOOST_REQUIRE(s._mip_map[0].data != NULL);

	for (unsigned int i = 0; i < s._mip_map[0].length;) {
		for (j = 0; i < s._mip_map[0].length && j < 2; j++) {
			BOOST_TEST_MESSAGE(
				"Testing mip_map[0].data[" << i << "]");
			BOOST_CHECK_EQUAL(s.get_subsample(0, i++), ~0);
		}

		for (; i < s._mip_map[0].length &&
			j < Period/LogicSnapshot::MipMapScaleFactor; j++) {
			BOOST_TEST_MESSAGE(
				"Testing mip_map[0].data[" << i << "]");
			BOOST_CHECK_EQUAL(s.get_subsample(0, i++), 0);
		}
	}

	// Check the higher levels are all inactive
	for (unsigned int i = 1; i < LogicSnapshot::ScaleStepCount; i++) {
		const LogicSnapshot::MipMapLevel &m = s._mip_map[i];
		BOOST_CHECK_EQUAL(m.length, 0);
		BOOST_CHECK_EQUAL(m.data_length, 0);
		BOOST_CHECK(m.data == NULL);
	}

	//----- Test get_subsampled_edges at a full scale -----//
	s.get_subsampled_edges(edges, 0, Length-1, 16.0f, 2);
	BOOST_REQUIRE_EQUAL(edges.size(), Cycles * 2 + 1);

	for (int i = 0; i < Cycles; i++) {
		BOOST_CHECK_EQUAL(edges[i*2].first, i * Period);
		BOOST_CHECK_EQUAL(edges[i*2].second, true);
		BOOST_CHECK_EQUAL(edges[i*2+1].first, i * Period + PulseWidth);
		BOOST_CHECK_EQUAL(edges[i*2+1].second, false);
	}

	BOOST_CHECK_EQUAL(edges.back().first, Length-1);
	BOOST_CHECK_EQUAL(edges.back().second, false);

	//----- Test get_subsampled_edges at a simplified scale -----//
	edges.clear();
	s.get_subsampled_edges(edges, 0, Length-1, 17.0f, 2);

	BOOST_CHECK_EQUAL(edges[0].first, 0);
	BOOST_CHECK_EQUAL(edges[0].second, true);
	BOOST_CHECK_EQUAL(edges[1].first, 16);
	BOOST_CHECK_EQUAL(edges[1].second, false);
	
	for (int i = 1; i < Cycles; i++) {
		BOOST_CHECK_EQUAL(edges[i+1].first, i * Period);
		BOOST_CHECK_EQUAL(edges[i+1].second, false);
	}

	BOOST_CHECK_EQUAL(edges.back().first, Length-1);
	BOOST_CHECK_EQUAL(edges.back().second, false);
}

BOOST_AUTO_TEST_CASE(LisaMUsbHid)
{
	/* This test was created from the beginning of the USB_DM signal in
	 * sigrok-dumps-usb/lisa_m_usbhid/lisa_m_usbhid.sr
	 */

	const int Edges[] = {
		7028, 7033, 7036, 7041, 7044, 7049, 7053, 7066, 7073, 7079,
		7086, 7095, 7103, 7108, 7111, 7116, 7119, 7124, 7136, 7141,
		7148, 7162, 7500
	};
	const int Length = Edges[countof(Edges) - 1];

	bool state = false;
	int lastEdgePos = 0;

	//----- Create a LogicSnapshot -----//
	sr_datafeed_logic logic;
	logic.unitsize = 1;
	logic.length = Length;
	logic.data = new uint8_t[Length];
	uint8_t *data = (uint8_t*)logic.data;

	for (unsigned int i = 0; i < countof(Edges); i++) {
		const int edgePos = Edges[i];
		memset(&data[lastEdgePos], state ? 0x02 : 0,
			edgePos - lastEdgePos - 1);

		lastEdgePos = edgePos;
		state = !state;
	}

	LogicSnapshot s(logic);
	delete[] (uint64_t*)logic.data;

	vector<LogicSnapshot::EdgePair> edges;


	/* The trailing edge of the pulse train is falling in the source data.
	 * Check this is always true at different scales
	 */

	edges.clear();
	s.get_subsampled_edges(edges, 0, Length-1, 33.333332f, 1);
	BOOST_CHECK_EQUAL(edges[edges.size() - 2].second, false);
}

/*
 * This test checks the rendering of wide data (more than 8 probes)
 * Probe signals are either all-high, or all-low, but are interleaved such that
 * they would toggle during every sample if treated like 8 probes.
 * The packet contains a large number of samples, so the mipmap generation kicks
 * in.
 *
 * The signals should not toggle (have exactly two edges: the start and end)
 */
BOOST_AUTO_TEST_CASE(WideData)
{
	const int Length = 512<<10;
	uint16_t *data = new uint16_t[Length];

	sr_datafeed_logic logic;
	logic.unitsize = sizeof(data[0]);
	logic.length = Length * sizeof(data[0]);
	logic.data = data;

	for (int i = 0; i < Length; i++)
		data[i] = 0x0FF0;

	LogicSnapshot s(logic);

	vector<LogicSnapshot::EdgePair> edges;

	edges.clear();
	s.get_subsampled_edges(edges, 0, Length-1, 1, 0);
	BOOST_CHECK_EQUAL(edges.size(), 2);

	edges.clear();
	s.get_subsampled_edges(edges, 0, Length-1, 1, 8);
	BOOST_CHECK_EQUAL(edges.size(), 2);

	// Cleanup
	delete [] data;
}

/*
 * This test is a replica of sixteen.sr attached to Bug #33.
 */
BOOST_AUTO_TEST_CASE(Sixteen)
{
	const int Length = 8;
	uint16_t data[Length];

	sr_datafeed_logic logic;
	logic.unitsize = sizeof(data[0]);
	logic.length = Length * sizeof(data[0]);
	logic.data = data;

	for (int i = 0; i < Length; i++)
		data[i] = 0xFFFE;

	LogicSnapshot s(logic);

	vector<LogicSnapshot::EdgePair> edges;
	s.get_subsampled_edges(edges, 0, 2, 0.0004, 1);

	BOOST_CHECK_EQUAL(edges.size(), 2);
}

BOOST_AUTO_TEST_SUITE_END()
