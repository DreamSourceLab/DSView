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

#ifndef LIBDSLOGIC_HARDWARE_DSLOGIC_H
#define LIBDSLOGIC_HARDWARE_DSLOGIC_H

#include <glib.h>

/* Message logging helpers with subsystem-specific prefix string. */
#define LOG_PREFIX "DSLogic Hardware: "
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

#define DSLOGIC_REQUIRED_VERSION_MAJOR	1

#define MAX_8BIT_SAMPLE_RATE	DS_MHZ(24)
#define MAX_16BIT_SAMPLE_RATE	DS_MHZ(12)

/* 6 delay states of up to 256 clock ticks */
#define MAX_SAMPLE_DELAY	(6 * 256)

/* Software trigger implementation: positive values indicate trigger stage. */
#define TRIGGER_FIRED          -1

#define DEV_CAPS_16BIT_POS	0

#define DEV_CAPS_16BIT		(1 << DEV_CAPS_16BIT_POS)

#define XC3S250E_BYTE_CNT   169216
//#define XC6SLX9_BYTE_CNT   341160
#define XC6SLX9_BYTE_CNT   340884
//#define XC6SLX9_BYTE_CNT   340604

#define MAX_ANALOG_PROBES_NUM 9
#define MAX_DSO_PROBES_NUM 2

struct DSLogic_profile {
    uint16_t vid;
    uint16_t pid;

    const char *vendor;
    const char *model;
    const char *model_version;

    const char *firmware;

    const char *fpga_bit;

    uint32_t dev_caps;
};

static const struct DSLogic_profile supported_fx2[3] = {
    /*
     * DSLogic
     */
    {0x2A0E, 0x0001, NULL, "DSLogic", NULL,
     "DSLogic.fw",
     "DSLogic.bin",
     DEV_CAPS_16BIT},

    { 0, 0, 0, 0, 0, 0, 0 }
};

enum {
    DSLOGIC_ERROR = -1,
    DSLOGIC_INIT = 0,
    DSLOGIC_START = 1,
    DSLOGIC_TRIGGERED = 2,
    DSLOGIC_DATA = 3,
    DSLOGIC_STOP = 4,
};

struct dev_context {
    const struct DSLogic_profile *profile;
    /*
     * Since we can't keep track of an DSLogic device after upgrading
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
    uint16_t op_mode;
	uint16_t trigger_mask[NUM_TRIGGER_STAGES];
	uint16_t trigger_value[NUM_TRIGGER_STAGES];
	int trigger_stage;
	uint16_t trigger_buffer[NUM_TRIGGER_STAGES];
    uint64_t vdiv0;
    uint64_t vdiv1;
    uint64_t timebase;
    gboolean coupling0;
    gboolean coupling1;
    gboolean en_ch0;
    gboolean en_ch1;
    uint8_t trigger_slope;
    uint8_t trigger_source;
    uint16_t trigger_vpos;
    uint32_t trigger_hpos;
    gboolean zero;

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
};

struct DSLogic_setting {
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
