/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2013 DreamSourceLab <dreamsourcelab@dreamsourcelab.com>
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

#ifndef LIBDSL_HARDWARE_DSL_H
#define LIBDSL_HARDWARE_DSL_H

#include <glib.h>
#include "libsigrok.h"
#include "libsigrok-internal.h"

/* Message logging helpers with subsystem-specific prefix string. */
#define LOG_PREFIX "DSL Hardware: "
#define ds_log(l, s, args...) ds_log(l, LOG_PREFIX s, ## args)
#define ds_spew(s, args...) ds_spew(LOG_PREFIX s, ## args)
#define ds_dbg(s, args...) ds_dbg(LOG_PREFIX s, ## args)
#define ds_info(s, args...) ds_info(LOG_PREFIX s, ## args)
#define ds_warn(s, args...) ds_warn(LOG_PREFIX s, ## args)
#define ds_err(s, args...) ds_err(LOG_PREFIX s, ## args)

#define USB_INTERFACE		0
#define USB_CONFIGURATION	1
#define NUM_TRIGGER_STAGES	16
#define TRIGGER_TYPE 		"01"

#define MAX_RENUM_DELAY_MS	3000
#define NUM_SIMUL_TRANSFERS	64
#define MAX_EMPTY_TRANSFERS	(NUM_SIMUL_TRANSFERS * 2)

#define DSL_REQUIRED_VERSION_MAJOR	1

#define MAX_8BIT_SAMPLE_RATE	DS_MHZ(24)
#define MAX_16BIT_SAMPLE_RATE	DS_MHZ(12)

/* 6 delay states of up to 256 clock ticks */
#define MAX_SAMPLE_DELAY	(6 * 256)

/* Software trigger implementation: positive values indicate trigger stage. */
#define TRIGGER_FIRED          -1

#define DEV_CAPS_16BIT_POS	0

#define DEV_CAPS_16BIT		(1 << DEV_CAPS_16BIT_POS)

#define MAX_ANALOG_PROBES_NUM 9
#define MAX_DSO_PROBES_NUM 2

#define DEFAULT_SAMPLERATE SR_MHZ(100)
#define DEFAULT_SAMPLELIMIT SR_MB(16)

#define VPOS_MINISTEP 0.083
#define VPOS_STEP 26.0

#define DSLOGIC_MAX_DSO_DEPTH SR_MB(2)
//#define DSLOGIC_MAX_DSO_DEPTH SR_KB(2)
#define DSLOGIC_MAX_DSO_SAMPLERATE SR_MHZ(200)
#define DSLOGIC_INSTANT_DEPTH SR_MB(32)
#define DSLOGIC_MAX_LOGIC_DEPTH SR_MB(16)
#define DSLOGIC_MAX_LOGIC_SAMPLERATE SR_MHZ(100)
#define DSCOPE_MAX_DEPTH SR_MB(2)
//#define DSCOPE_MAX_DEPTH SR_KB(512)
#define DSCOPE_MAX_SAMPLERATE SR_MHZ(200)
#define DSCOPE_INSTANT_DEPTH SR_MB(32)

struct DSL_profile {
    uint16_t vid;
    uint16_t pid;

    const char *vendor;
    const char *model;
    const char *model_version;

    const char *firmware;

    const char *fpga_bit33;
    const char *fpga_bit50;

    uint32_t dev_caps;
};

static const struct DSL_profile supported_DSLogic[3] = {
    /*
     * DSLogic
     */
    {0x2A0E, 0x0001, NULL, "DSLogic", NULL,
     "DSLogic.fw",
     "DSLogic33.bin",
     "DSLogic50.bin",
     DEV_CAPS_16BIT},

    {0x2A0E, 0x0003, NULL, "DSLogic Pro", NULL,
     "DSLogicPro.fw",
     "DSLogicPro.bin",
     "DSLogicPro.bin",
     DEV_CAPS_16BIT},

    { 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

static const struct DSL_profile supported_DSCope[2] = {
    /*
     * DSCope
     */
    {0x2A0E, 0x0002, NULL, "DSCope", NULL,
     "DSCope.fw",
     "DSCope.bin",
     "DSCope.bin",
     DEV_CAPS_16BIT},

    { 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};


enum {
    DSL_ERROR = -1,
    DSL_INIT = 0,
    DSL_START = 1,
    DSL_READY = 2,
    DSL_TRIGGERED = 3,
    DSL_DATA = 4,
    DSL_STOP = 5,
};

struct DSL_context {
    const struct DSL_profile *profile;
	/*
     * Since we can't keep track of an DSL device after upgrading
	 * the firmware (it renumerates into a different device address
	 * after the upgrade) this is like a global lock. No device will open
	 * until a proper delay after the last device was upgraded.
	 */
	int64_t fw_updated;

	/* Device/capture settings */
	uint64_t cur_samplerate;
	uint64_t limit_samples;

	/* Operational settings */
	gboolean sample_wide;
    gboolean clock_type;
    gboolean clock_edge;
    gboolean instant;
    uint16_t op_mode;
    uint16_t th_level;
    double vth;
    uint16_t filter;
	uint16_t trigger_mask[NUM_TRIGGER_STAGES];
	uint16_t trigger_value[NUM_TRIGGER_STAGES];
	int trigger_stage;
	uint16_t trigger_buffer[NUM_TRIGGER_STAGES];
    uint64_t timebase;
    uint8_t trigger_slope;
    uint8_t trigger_source;
    uint32_t trigger_hpos;
    gboolean zero;
    gboolean stream;
    gboolean lock;

	int num_samples;
	int submitted_transfers;
	int empty_transfer_count;

	void *cb_data;
	unsigned int num_transfers;
	struct libusb_transfer **transfers;
	int *usbfd;

    int pipe_fds[2];
    GIOChannel *channel;

    int status;
    gboolean mstatus_valid;
};

struct DSL_setting {
    uint32_t sync;
    uint16_t mode_header;                   // 0
    uint16_t mode;
    uint32_t divider_header;                // 1-2
    uint32_t divider;
    uint32_t count_header;                  // 3-4
    uint32_t count;
    uint32_t trig_pos_header;               // 5-6
    uint32_t trig_pos;
    uint16_t trig_glb_header;               // 7
    uint16_t trig_glb;
    uint32_t trig_adp_header;               // 10-11
    uint32_t trig_adp;
    uint32_t trig_sda_header;               // 12-13
    uint32_t trig_sda;
    uint32_t trig_mask0_header;              // 16
    uint16_t trig_mask0[NUM_TRIGGER_STAGES];
    uint32_t trig_mask1_header;              // 17
    uint16_t trig_mask1[NUM_TRIGGER_STAGES];
    //uint32_t trig_mask2_header;              // 18
    //uint16_t trig_mask2[NUM_TRIGGER_STAGES];
    //uint32_t trig_mask3_header;              // 19
    //uint16_t trig_mask3[NUM_TRIGGER_STAGES];
    uint32_t trig_value0_header;             // 20
    uint16_t trig_value0[NUM_TRIGGER_STAGES];
    uint32_t trig_value1_header;             // 21
    uint16_t trig_value1[NUM_TRIGGER_STAGES];
    //uint32_t trig_value2_header;             // 22
    //uint16_t trig_value2[NUM_TRIGGER_STAGES];
    //uint32_t trig_value3_header;             // 23
    //uint16_t trig_value3[NUM_TRIGGER_STAGES];
    uint32_t trig_edge0_header;              // 24
    uint16_t trig_edge0[NUM_TRIGGER_STAGES];
    uint32_t trig_edge1_header;              // 25
    uint16_t trig_edge1[NUM_TRIGGER_STAGES];
    //uint32_t trig_edge2_header;              // 26
    //uint16_t trig_edge2[NUM_TRIGGER_STAGES];
    //uint32_t trig_edge3_header;              // 27
    //uint16_t trig_edge3[NUM_TRIGGER_STAGES];
    uint32_t trig_count0_header;             // 28
    uint16_t trig_count0[NUM_TRIGGER_STAGES];
    uint32_t trig_count1_header;             // 29
    uint16_t trig_count1[NUM_TRIGGER_STAGES];
    //uint32_t trig_count2_header;             // 30
    //uint16_t trig_count2[NUM_TRIGGER_STAGES];
    //uint32_t trig_count3_header;             // 31
    //uint16_t trig_count3[NUM_TRIGGER_STAGES];
    uint32_t trig_logic0_header;             // 32
    uint16_t trig_logic0[NUM_TRIGGER_STAGES];
    uint32_t trig_logic1_header;             // 33
    uint16_t trig_logic1[NUM_TRIGGER_STAGES];
    //uint32_t trig_logic2_header;             // 34
    //uint16_t trig_logic2[NUM_TRIGGER_STAGES];
    //uint32_t trig_logic3_header;             // 35
    //uint16_t trig_logic3[NUM_TRIGGER_STAGES];
    uint32_t end_sync;
};

#endif
