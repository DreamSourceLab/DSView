/*
 * This file is part of the libsigrokdecode project.
 *
 * Copyright (C) 2012 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2016 DreamSourceLab <support@dreamsourcelab.com>
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

#include "config.h"
#include "libsigrokdecode-internal.h" /* First, so we avoid a _POSIX_C_SOURCE warning. */
#include "libsigrokdecode.h"
#include <inttypes.h>
#include <string.h>

static PyObject *srd_logic_iter(PyObject *self)
{
	return self;
}

static PyObject *srd_logic_iternext(PyObject *self)
{
	srd_logic *logic;
	PyObject *py_samplenum, *py_samples;
	int i;

	logic = (srd_logic *)self;
	uint64_t offset = floor(logic->itercnt);
	logic->di->cur_pos = logic->cur_pos;
	logic->di->logic_mask = logic->logic_mask;
	logic->di->exp_logic = logic->exp_logic;
	logic->di->edge_index = -1;
	if (logic->logic_mask != 0 && logic->edge_index != -1)
		logic->di->edge_index = logic->di->dec_channelmap[logic->edge_index];

	if (offset > logic->samplenum || logic->logic_mask != 0) {
		/* End iteration loop. */
		return NULL;
	}

	/*
	 * Convert the bit-packed sample to an array of bytes, with only 0x01
	 * and 0x00 values, so the PD doesn't need to do any bitshifting.
	 */
	for (i = 0; i < logic->di->dec_num_channels; i++) {
		/* A channelmap value of -1 means "unused optional channel". */
		if (logic->di->dec_channelmap[i] == -1) {
			/* Value of unused channel is 0xff, instead of 0 or 1. */
			logic->di->channel_samples[i] = 0xff;
		} else {
			if (*(logic->inbuf + i) == NULL) {
				logic->di->channel_samples[i] = *(logic->inbuf_const + i) ? 1 : 0;
			} else {
				uint64_t inbuf_offset = (offset + (logic->start_samplenum % 8));
				uint8_t *ptr = *(logic->inbuf + i) + (inbuf_offset / 8);
				logic->di->channel_samples[i] = *ptr & (1 << (inbuf_offset % 8)) ? 1 : 0;
			}
		}
	}

	/* Prepare the next samplenum/sample list in this iteration. */
	py_samplenum = PyLong_FromUnsignedLongLong(logic->start_samplenum + offset);
	PyList_SetItem(logic->sample, 0, py_samplenum);
	py_samples = PyBytes_FromStringAndSize((const char *)logic->di->channel_samples,
					       logic->di->dec_num_channels);
	PyList_SetItem(logic->sample, 1, py_samples);
	Py_INCREF(logic->sample);

	return logic->sample;
}

static PyMemberDef srd_logic_members[] = {
	{"itercnt", T_FLOAT, offsetof(srd_logic, itercnt), 0,
	 "next expacted samples offset"},
	{"logic_mask", T_ULONGLONG, offsetof(srd_logic, logic_mask), 0,
	 "next expacted logic value mask"},
	{"exp_logic", T_ULONGLONG, offsetof(srd_logic, exp_logic), 0,
	 "next expacted logic value"},
	{"edge_index", T_INT, offsetof(srd_logic, edge_index), 0,
	 "channel index of next expacted edge"},
	{"cur_pos", T_ULONGLONG, offsetof(srd_logic, cur_pos), 0,
	 "current sample position"},
	{NULL}  /* Sentinel */
};

//static PyMemberDef srd_logic_members[] = {
////    {"itercnt", T_FLOAT, offsetof(srd_logic, itercnt), 0,
////     "next expacted samples offset"},
//    {"logic_mask", T_ULONGLONG, offsetof(srd_logic, logic_mask), 0,
//     "next expacted logic value mask"},
////    {"exp_logic", T_ULONGLONG, offsetof(srd_logic, exp_logic), 0,
////     "next expacted logic value"},
////    {"edge_index", T_INT, offsetof(srd_logic, edge_index), 0,
////     "channel index of next expacted edge"},
////    {"cur_pos", T_ULONGLONG, offsetof(srd_logic, cur_pos), 0,
////     "current sample position"},
//    {NULL}  /* Sentinel */
//};


/** Create the srd_logic type.
 * @return The new type object.
 * @private
 */
SRD_PRIV PyObject *srd_logic_type_new(void)
{
	PyType_Spec spec;
	PyType_Slot slots[] = {
		{ Py_tp_doc, "sigrokdecode logic sample object" },
		{ Py_tp_iter, (void *)&srd_logic_iter },
		{ Py_tp_iternext, (void *)&srd_logic_iternext },
		{ Py_tp_new, (void *)&PyType_GenericNew },
		{ Py_tp_members, srd_logic_members },
		{ 0, NULL }
	};
	spec.name = "srd_logic";
	spec.basicsize = sizeof(srd_logic);
	spec.itemsize = 0;
	spec.flags = Py_TPFLAGS_DEFAULT;
	spec.slots = slots;

	return PyType_FromSpec(&spec);
}
