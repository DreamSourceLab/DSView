/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2013 DreamSourceLab <support@dreamsourcelab.com>
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

#define DEFAULT_SAMPLERATE SR_MHZ(1)
#define DEFAULT_SAMPLELIMIT SR_MB(1)

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

/*
 * for DSCope device
 * trans: x << 8 + y
 * x = vpos(coarse), each step(1024 total) indicate x(mv) at 1/20 attenuation, and x/10(mv) at 1/2 attenuation
 * y = voff(fine), each step(1024 total) indicate y/100(mv) at 1/20 attenuation, adn y/1000(mv) at 1/2 attenuation
 * voff: x << 10 + y
 * x = vpos(coarse) default bias
 * y = voff(fine) default bias
 * the final offset: x+DSCOPE_CONSTANT_BIAS->vpos(coarse); y->voff(fine)
 */
#define DSCOPE_DEFAULT_TRANS (129<<8)+167
#define DSCOPE_DEFAULT_VOFF  (32<<10)+558
#define DSCOPE_CONSTANT_BIAS 160
#define DSCOPE_TRANS_CMULTI   10
#define DSCOPE_TRANS_FMULTI   100.0
#define DSCOPE_DEFAULT_VGAIN0 0x162400
#define DSCOPE_DEFAULT_VGAIN1 0x14C000
#define DSCOPE_DEFAULT_VGAIN2 0x12E800
#define DSCOPE_DEFAULT_VGAIN3 0x118000
#define DSCOPE_DEFAULT_VGAIN4 0x102400
#define DSCOPE_DEFAULT_VGAIN5 0x2E800
#define DSCOPE_DEFAULT_VGAIN6 0x18000
#define DSCOPE_DEFAULT_VGAIN7 0x02400

/*
 * for DSCope20 device
 * trans: the whole windows offset map to the offset pwm(1024 total)
 * voff: offset pwm constant bias to balance circuit offset
 */
#define DSCOPE20_DEFAULT_TRANS 920
#define DSCOPE20_DEFAULT_VOFF 45
#define DSCOPE20_DEFAULT_VGAIN0 0x1DA800
#define DSCOPE20_DEFAULT_VGAIN1 0x1A7200
#define DSCOPE20_DEFAULT_VGAIN2 0x164200
#define DSCOPE20_DEFAULT_VGAIN3 0x131800
#define DSCOPE20_DEFAULT_VGAIN4 0xBD000
#define DSCOPE20_DEFAULT_VGAIN5 0x7AD00
#define DSCOPE20_DEFAULT_VGAIN6 0x48800
#define DSCOPE20_DEFAULT_VGAIN7 0x12000

#define CALI_VGAIN_RANGE 100
#define CALI_VOFF_RANGE (1024-DSCOPE20_DEFAULT_TRANS)

#define DSO_AUTOTRIG_THRESHOLD 16

#define TRIG_CHECKID 0xa500005a
#define DSO_PKTID 0xa500

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

static const struct DSL_profile supported_DSCope[3] = {
    /*
     * DSCope
     */
    {0x2A0E, 0x0002, NULL, "DSCope", NULL,
     "DSCope.fw",
     "DSCope.bin",
     "DSCope.bin",
     DEV_CAPS_16BIT},

    {0x2A0E, 0x0004, NULL, "DSCope20", NULL,
     "DSCope20.fw",
     "DSCope20.bin",
     "DSCope20.bin",
     DEV_CAPS_16BIT},

    { 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

static const gboolean default_ms_en[DSO_MS_END - DSO_MS_BEGIN] = {
    FALSE, /* DSO_MS_BEGIN */
    TRUE,  /* DSO_MS_FREQ */
    FALSE, /* DSO_MS_PERD */
    TRUE,  /* DSO_MS_VMAX */
    TRUE,  /* DSO_MS_VMIN */
    FALSE, /* DSO_MS_VRMS */
    FALSE, /* DSO_MS_VMEA */
    FALSE, /* DSO_MS_VP2P */
    FALSE, /* DSO_MS_END */
};

enum {
    DSL_ERROR = -1,
    DSL_INIT = 0,
    DSL_START = 1,
    DSL_READY = 2,
    DSL_TRIGGERED = 3,
    DSL_DATA = 4,
    DSL_STOP = 5,
    DSL_FINISH = 6,
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
    uint64_t actual_samples;

	/* Operational settings */
	gboolean sample_wide;
    gboolean clock_type;
    gboolean clock_edge;
    gboolean rle_mode;
    gboolean instant;
    uint16_t op_mode;
    uint16_t ch_mode;
    uint16_t samplerates_size;
    uint16_t samplecounts_size;
    uint16_t th_level;
    double vth;
    uint16_t filter;
	uint16_t trigger_mask[NUM_TRIGGER_STAGES];
	uint16_t trigger_value[NUM_TRIGGER_STAGES];
	int trigger_stage;
	uint16_t trigger_buffer[NUM_TRIGGER_STAGES];
    uint64_t timebase;
    uint8_t max_height;
    uint8_t trigger_channel;
    uint8_t trigger_slope;
    uint8_t trigger_source;
    uint8_t trigger_hrate;
    uint32_t trigger_hpos;
    uint32_t trigger_holdoff;
    uint8_t trigger_margin;
    gboolean zero;
    gboolean cali;
    int zero_stage;
    int zero_pcnt;
    int zero_comb;
    gboolean stream;
    gboolean roll;
    gboolean data_lock;
    uint8_t dso_bits;

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
    gboolean abort;
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
    uint32_t trig_count0[NUM_TRIGGER_STAGES];
    uint32_t trig_count1_header;             // 29
    uint32_t trig_count1[NUM_TRIGGER_STAGES];
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

struct DSL_vga {
    int key;
    uint64_t vgain0;
    uint64_t vgain1;
    uint16_t voff0;
    uint16_t voff1;
};

#endif
