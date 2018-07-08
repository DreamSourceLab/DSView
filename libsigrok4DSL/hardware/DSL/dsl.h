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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <assert.h>

#include <sys/stat.h>
#include <inttypes.h>

#undef min
#define min(a,b) ((a)<(b)?(a):(b))
#undef max
#define max(a,b) ((a)>(b)?(a):(b))

/* Message logging helpers with subsystem-specific prefix string. */
#define LOG_PREFIX "DSL Hardware: "
#define sr_log(l, s, args...) sr_log(l, LOG_PREFIX s, ## args)
#define sr_spew(s, args...) sr_spew(LOG_PREFIX s, ## args)
#define sr_dbg(s, args...) sr_dbg(LOG_PREFIX s, ## args)
#define sr_info(s, args...) sr_info(LOG_PREFIX s, ## args)
#define sr_warn(s, args...) sr_warn(LOG_PREFIX s, ## args)
#define sr_err(s, args...) sr_err(LOG_PREFIX s, ## args)

#define USB_INTERFACE		0
#define USB_CONFIGURATION	1
#define NUM_TRIGGER_STAGES	16
#define NUM_SIMUL_TRANSFERS	64

#define DSL_REQUIRED_VERSION_MAJOR	2
#define DSL_REQUIRED_VERSION_MINOR	0

/* hardware Capabilities */
#define CAPS_MODE_LOGIC (1 << 0)
#define CAPS_MODE_ANALOG (1 << 1)
#define CAPS_MODE_DSO (1 << 2)

#define CAPS_FEATURE_NONE 0
// voltage threshold
#define CAPS_FEATURE_VTH (1 << 0)
// with external buffer
#define CAPS_FEATURE_BUF (1 << 1)
// pre offset control
#define CAPS_FEATURE_PREOFF (1 << 2)
// small startup eemprom
#define CAPS_FEATURE_SEEP (1 << 3)
// zero calibration ability
#define CAPS_FEATURE_ZERO (1 << 4)
/* end */


#define DSLOGIC_ATOMIC_BITS 6
#define DSLOGIC_ATOMIC_SAMPLES (1 << DSLOGIC_ATOMIC_BITS)
#define DSLOGIC_ATOMIC_SIZE (1 << (DSLOGIC_ATOMIC_BITS - 3))
#define DSLOGIC_ATOMIC_MASK (0xFFFF << DSLOGIC_ATOMIC_BITS)

/*
 * for basic configuration
 */
#define TRIG_EN_BIT 0
#define CLK_TYPE_BIT 1
#define CLK_EDGE_BIT 2
#define RLE_MODE_BIT 3
#define DSO_MODE_BIT 4
#define HALF_MODE_BIT 5
#define QUAR_MODE_BIT 6
#define ANALOG_MODE_BIT 7
#define FILTER_BIT 8
#define INSTANT_BIT 9
#define STRIG_MODE_BIT 11
#define STREAM_MODE_BIT 12
#define LPB_TEST_BIT 13
#define EXT_TEST_BIT 14
#define INT_TEST_BIT 15

#define bmNONE          0
#define bmEEWP          (1 << 0)
#define bmFORCE_RDY     (1 << 1)
#define bmFORCE_STOP    (1 << 2)
#define bmSCOPE_SET     (1 << 3)
#define bmSCOPE_CLR     (1 << 4)

/*
 * packet content check
 */
#define TRIG_CHECKID 0x55555555
#define DSO_PKTID 0xa500

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
#define DSCOPE_CONSTANT_BIAS 160
#define DSCOPE_TRANS_CMULTI   10
#define DSCOPE_TRANS_FMULTI   100.0

/*
 * for DSCope20 device
 * trans: the whole windows offset map to the offset pwm(1024 total)
 * voff: offset pwm constant bias to balance circuit offset
 */
#define CALI_VGAIN_RANGE 100

struct DSL_caps {
    uint64_t mode_caps;
    uint64_t feature_caps;
    uint64_t channels;
    uint64_t hw_depth;
    uint64_t dso_depth;
    uint8_t intest_channel;
    const uint64_t *vdivs;
    uint8_t vga_id;
    uint16_t default_channelmode;
    uint64_t default_samplerate;
    uint64_t default_samplelimit;
    uint16_t default_pwmtrans;
    uint16_t default_pwmmargin;
};

struct DSL_profile {
    uint16_t vid;
    uint16_t pid;

    const char *vendor;
    const char *model;
    const char *model_version;

    const char *firmware;

    const char *fpga_bit33;
    const char *fpga_bit50;

    struct DSL_caps dev_caps;
};

static const uint64_t vdivs10to2000[] = {
    SR_mV(10),
    SR_mV(20),
    SR_mV(50),
    SR_mV(100),
    SR_mV(200),
    SR_mV(500),
    SR_V(1),
    SR_V(2),
    0,
};

struct DSL_vga {
    uint8_t id;
    uint64_t key;
    uint64_t vgain;
    uint16_t voff;
    uint16_t voff_comp;
};
static const struct DSL_vga vga_defaults[] = {
    {1, 10,   0x162400, (32<<10)+558, (32<<10)+558},
    {1, 20,   0x14C000, (32<<10)+558, (32<<10)+558},
    {1, 50,   0x12E800, (32<<10)+558, (32<<10)+558},
    {1, 100,  0x118000, (32<<10)+558, (32<<10)+558},
    {1, 200,  0x102400, (32<<10)+558, (32<<10)+558},
    {1, 500,  0x2E800,  (32<<10)+558, (32<<10)+558},
    {1, 1000, 0x18000,  (32<<10)+558, (32<<10)+558},
    {1, 2000, 0x02400,  (32<<10)+558, (32<<10)+558},

    {2, 10,   0x1DA800, 45, 1024-920-45},
    {2, 20,   0x1A7200, 45, 1024-920-45},
    {2, 50,   0x164200, 45, 1024-920-45},
    {2, 100,  0x131800, 45, 1024-920-45},
    {2, 200,  0xBD000,  45, 1024-920-45},
    {2, 500,  0x7AD00,  45, 1024-920-45},
    {2, 1000, 0x48800,  45, 1024-920-45},
    {2, 2000, 0x12000,  45, 1024-920-45},

    {3, 10,   0x1C5C00, 45, 1024-920-45},
    {3, 20,   0x19EB00, 45, 1024-920-45},
    {3, 50,   0x16AE00, 45, 1024-920-45},
    {3, 100,  0x143D00, 45, 1024-920-45},
    {3, 200,  0xB1000,  45, 1024-920-45},
    {3, 500,  0x7F000,  45, 1024-920-45},
    {3, 1000, 0x57200,  45, 1024-920-45},
    {3, 2000, 0x2DD00,  45, 1024-920-45},

    {0, 0, 0, 0, 0}
};

enum CHANNEL_ID {
    DSL_STREAM20x16 = 0,
    DSL_STREAM25x12,
    DSL_STREAM50x6,
    DSL_STREAM100x3,
    DSL_BUFFER100x16,
    DSL_BUFFER200x8,
    DSL_BUFFER400x4,
    DSL_ANALOG10x2,
    DSL_DSO200x2,
};

struct DSL_channels {
    enum CHANNEL_ID id;
    enum OPERATION_MODE mode;
    enum CHANNEL_TYPE type;
    gboolean stream;
    uint16_t num;
    uint8_t unit_bits;
    uint64_t min_samplerate;
    uint64_t max_samplerate;
    uint64_t hw_min_samplerate;
    uint64_t hw_max_samplerate;
    const char *descr;
};

static const struct DSL_channels channel_modes[] = {
    // LA Stream
    {DSL_STREAM20x16,  LOGIC,  SR_CHANNEL_LOGIC,  TRUE,  16, 1, SR_KHZ(10), SR_MHZ(20),
     SR_KHZ(10), SR_MHZ(100), "Use 16 Channels (Max 20MHz)"},
    {DSL_STREAM25x12,  LOGIC,  SR_CHANNEL_LOGIC,  TRUE,  12, 1, SR_KHZ(10), SR_MHZ(25),
     SR_KHZ(10), SR_MHZ(100), "Use 12 Channels (Max 25MHz)"},
    {DSL_STREAM50x6,   LOGIC,  SR_CHANNEL_LOGIC,  TRUE,  6,  1, SR_KHZ(10), SR_MHZ(50),
     SR_KHZ(10), SR_MHZ(100), "Use 6 Channels (Max 50MHz)"},
    {DSL_STREAM100x3,  LOGIC,  SR_CHANNEL_LOGIC,  TRUE,  3,  1, SR_KHZ(10), SR_MHZ(100),
     SR_KHZ(10), SR_MHZ(100), "Use 3 Channels (Max 100MHz)"},

    // LA Buffer
    {DSL_BUFFER100x16, LOGIC,  SR_CHANNEL_LOGIC,  FALSE, 16, 1, SR_KHZ(10), SR_MHZ(100),
     SR_KHZ(10), SR_MHZ(100), "Use Channels 0~15 (Max 100MHz)"},
    {DSL_BUFFER200x8,  LOGIC,  SR_CHANNEL_LOGIC,  FALSE, 8,  1, SR_KHZ(10), SR_MHZ(200),
     SR_KHZ(10), SR_MHZ(100), "Use Channels 0~7 (Max 200MHz)"},
    {DSL_BUFFER400x4,  LOGIC,  SR_CHANNEL_LOGIC,  FALSE, 4,  1, SR_KHZ(10), SR_MHZ(400),
     SR_KHZ(10), SR_MHZ(100), "Use Channels 0~3 (Max 400MHz)"},

    // DAQ
    {DSL_ANALOG10x2,   ANALOG, SR_CHANNEL_ANALOG, TRUE,  2,  8, SR_HZ(10),  SR_MHZ(10),
     SR_KHZ(10), SR_MHZ(100), "Use Channels 0~1 (Max 10MHz)"},

    // OSC
    {DSL_DSO200x2,     DSO,    SR_CHANNEL_DSO,    FALSE, 2,  8, SR_KHZ(10), SR_MHZ(200),
     SR_KHZ(10), SR_MHZ(100), "Use Channels 0~1 (Max 200MHz)"}
};

static const struct DSL_profile supported_DSLogic[] = {
    /*
     * DSLogic
     */
    {0x2A0E, 0x0001, "DreamSourceLab", "DSLogic", NULL,
     "DSLogic.fw",
     "DSLogic33.bin",
     "DSLogic50.bin",
     {CAPS_MODE_LOGIC | CAPS_MODE_ANALOG | CAPS_MODE_DSO,
      CAPS_FEATURE_SEEP | CAPS_FEATURE_BUF,
      (1 << DSL_STREAM20x16) | (1 << DSL_STREAM25x12) | (1 << DSL_STREAM50x6) | (1 << DSL_STREAM100x3) |
      (1 << DSL_BUFFER100x16) | (1 << DSL_BUFFER200x8) | (1 << DSL_BUFFER400x4) |
      (1 << DSL_ANALOG10x2) |
      (1 << DSL_DSO200x2),
      SR_MB(256),
      SR_Mn(2),
      DSL_BUFFER100x16,
      vdivs10to2000,
      0,
      DSL_STREAM20x16,
      SR_MHZ(1),
      SR_Mn(1),
      0,
      0}
    },

    {0x2A0E, 0x0003, "DreamSourceLab", "DSLogic Pro", NULL,
     "DSLogicPro.fw",
     "DSLogicPro.bin",
     "DSLogicPro.bin",
     {CAPS_MODE_LOGIC,
      CAPS_FEATURE_SEEP | CAPS_FEATURE_VTH | CAPS_FEATURE_BUF,
      (1 << DSL_STREAM20x16) | (1 << DSL_STREAM25x12) | (1 << DSL_STREAM50x6) | (1 << DSL_STREAM100x3) |
      (1 << DSL_BUFFER100x16) | (1 << DSL_BUFFER200x8) | (1 << DSL_BUFFER400x4),
      SR_MB(256),
      0,
      DSL_BUFFER100x16,
      0,
      0,
      DSL_STREAM20x16,
      SR_MHZ(1),
      SR_Mn(1),
      0,
      0}
    },

    {0x2A0E, 0x0020, "DreamSourceLab", "DSLogic PLus", NULL,
     "DSLogicPlus.fw",
     "DSLogicPlus.bin",
     "DSLogicPlus.bin",
     {CAPS_MODE_LOGIC,
      CAPS_FEATURE_VTH | CAPS_FEATURE_BUF,
      (1 << DSL_STREAM20x16) | (1 << DSL_STREAM25x12) | (1 << DSL_STREAM50x6) | (1 << DSL_STREAM100x3) |
      (1 << DSL_BUFFER100x16) | (1 << DSL_BUFFER200x8) | (1 << DSL_BUFFER400x4),
      SR_MB(256),
      0,
      DSL_BUFFER100x16,
      0,
      0,
      DSL_STREAM20x16,
      SR_MHZ(1),
      SR_Mn(1),
      0,
      0}
    },

    {0x2A0E, 0x0021, "DreamSourceLab", "DSLogic Basic", NULL,
     "DSLogicBasic.fw",
     "DSLogicBasic.bin",
     "DSLogicBasic.bin",
     {CAPS_MODE_LOGIC,
      CAPS_FEATURE_VTH,
      (1 << DSL_STREAM20x16) | (1 << DSL_STREAM25x12) | (1 << DSL_STREAM50x6) | (1 << DSL_STREAM100x3) |
      (1 << DSL_BUFFER100x16) | (1 << DSL_BUFFER200x8) | (1 << DSL_BUFFER400x4),
      SR_KB(256),
      0,
      DSL_STREAM20x16,
      0,
      0,
      DSL_STREAM20x16,
      SR_MHZ(1),
      SR_Mn(1),
      0,
      0}
    },

    { 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}
};

static const struct DSL_profile supported_DSCope[] = {
    /*
     * DSCope
     */
    {0x2A0E, 0x0002, "DreamSourceLab", "DSCope", NULL,
     "DSCope.fw",
     "DSCope.bin",
     "DSCope.bin",
     {CAPS_MODE_ANALOG | CAPS_MODE_DSO,
      CAPS_FEATURE_ZERO | CAPS_FEATURE_PREOFF | CAPS_FEATURE_SEEP | CAPS_FEATURE_BUF,
      (1 << DSL_ANALOG10x2) |
      (1 << DSL_DSO200x2),
      SR_MB(256),
      SR_Mn(2),
      0,
      vdivs10to2000,
      1,
      DSL_DSO200x2,
      SR_MHZ(100),
      SR_Mn(1),
      (129<<8)+167,
      1024-920}
    },

    {0x2A0E, 0x0004, "DreamSourceLab", "DSCope20", NULL,
     "DSCope20.fw",
     "DSCope20.bin",
     "DSCope20.bin",
     {CAPS_MODE_ANALOG | CAPS_MODE_DSO,
      CAPS_FEATURE_ZERO | CAPS_FEATURE_SEEP | CAPS_FEATURE_BUF,
      (1 << DSL_ANALOG10x2) |
      (1 << DSL_DSO200x2),
      SR_MB(256),
      SR_Mn(2),
      0,
      vdivs10to2000,
      2,
      DSL_DSO200x2,
      SR_MHZ(100),
      SR_Mn(1),
      920,
      1024-920}
    },

    {0x2A0E, 0x0022, "DreamSourceLab", "DSCope B20", NULL,
     "DSCopeB20.fw",
     "DSCope20.bin",
     "DSCope20.bin",
     {CAPS_MODE_ANALOG | CAPS_MODE_DSO,
      CAPS_FEATURE_ZERO | CAPS_FEATURE_BUF,
      (1 << DSL_ANALOG10x2) |
      (1 << DSL_DSO200x2),
      SR_MB(256),
      SR_Mn(2),
      0,
      vdivs10to2000,
      2,
      DSL_DSO200x2,
      SR_MHZ(100),
      SR_Mn(1),
      920,
      1024-920}
    },

    {0x2A0E, 0x0023, "DreamSourceLab", "DSCope C20", NULL,
     "DSCopeC20.fw",
     "DSCopeC20.bin",
     "DSCopeC20.bin",
     {CAPS_MODE_ANALOG | CAPS_MODE_DSO,
      CAPS_FEATURE_ZERO | CAPS_FEATURE_BUF,
      (1 << DSL_ANALOG10x2) |
      (1 << DSL_DSO200x2),
      SR_MB(256),
      SR_Mn(2),
      0,
      vdivs10to2000,
      3,
      DSL_DSO200x2,
      SR_MHZ(100),
      SR_Mn(1),
      920,
      1024-920}
    },


    {0x2A0E, 0x0024, "DreamSourceLab", "DSCope C20P", NULL,
     "DSCopeC20P.fw",
     "DSCopeC20P.bin",
     "DSCopeC20P.bin",
     {CAPS_MODE_ANALOG | CAPS_MODE_DSO,
      CAPS_FEATURE_ZERO | CAPS_FEATURE_BUF,
      (1 << DSL_ANALOG10x2) |
      (1 << DSL_DSO200x2),
      SR_MB(256),
      SR_Mn(2),
      0,
      vdivs10to2000,
      3,
      DSL_DSO200x2,
      SR_MHZ(100),
      SR_Mn(1),
      920,
      1024-920}
    },

    {0x2A0E, 0x0025, "DreamSourceLab", "DSCope C20", NULL,
     "DSCopeC20B.fw",
     "DSCopeC20B.bin",
     "DSCopeC20B.bin",
     {CAPS_MODE_ANALOG | CAPS_MODE_DSO,
      CAPS_FEATURE_ZERO,
      (1 << DSL_ANALOG10x2) |
      (1 << DSL_DSO200x2),
      SR_KB(256),
      SR_Kn(20),
      0,
      vdivs10to2000,
      3,
      DSL_DSO200x2,
      SR_MHZ(100),
      SR_Kn(10),
      920,
      1024-920}
    },

    { 0, 0, 0, 0, 0, 0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}
};

static const gboolean default_ms_en[] = {
    FALSE, /* DSO_MS_BEGIN */
    TRUE,  /* DSO_MS_FREQ */
    FALSE, /* DSO_MS_PERD */
    TRUE,  /* DSO_MS_VMAX */
    TRUE,  /* DSO_MS_VMIN */
    FALSE, /* DSO_MS_VRMS */
    FALSE, /* DSO_MS_VMEA */
    FALSE, /* DSO_MS_VP2P */
};

enum {
    DSL_ERROR = -1,
    DSL_INIT = 0,
    DSL_START = 1,
    DSL_READY = 2,
    DSL_TRIGGERED = 3,
    DSL_DATA = 4,
    DSL_STOP = 5,
    DSL_FINISH = 7,
    DSL_ABORT = 8,
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
    uint64_t actual_bytes;

	/* Operational settings */
    gboolean clock_type;
    gboolean clock_edge;
    gboolean rle_mode;
    gboolean rle_support;
    gboolean instant;
    uint16_t op_mode;
    gboolean stream;
    uint8_t  test_mode;
    uint16_t buf_options;
    enum CHANNEL_ID ch_mode;
    uint16_t samplerates_min_index;
    uint16_t samplerates_max_index;
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
    gboolean roll;
    gboolean data_lock;
    uint16_t unit_pitch;

    uint64_t num_samples;
    uint64_t num_bytes;
	int submitted_transfers;
	int empty_transfer_count;

	void *cb_data;
	unsigned int num_transfers;
	struct libusb_transfer **transfers;
	int *usbfd;

    int pipe_fds[2];
    GIOChannel *channel;

    int status;
    int trf_completed;
    gboolean mstatus_valid;
    struct sr_status mstatus;
    gboolean abort;
    gboolean overflow;
};

/*
 * hardware setting for each capture
 */
struct DSL_setting {
    uint32_t sync;

    uint16_t mode_header;                   // 0
    uint16_t mode;
    uint16_t divider_header;                // 1-2
    uint16_t div_l;
    uint16_t div_h;
    uint16_t count_header;                  // 3-4
    uint16_t cnt_l;
    uint16_t cnt_h;
    uint16_t trig_pos_header;               // 5-6
    uint16_t tpos_l;
    uint16_t tpos_h;
    uint16_t trig_glb_header;               // 7
    uint16_t trig_glb;
    uint16_t ch_en_header;                  // 8
    uint16_t ch_en;

    uint16_t trig_header;                   // 64
    uint16_t trig_mask0[NUM_TRIGGER_STAGES];
    uint16_t trig_mask1[NUM_TRIGGER_STAGES];
    uint16_t trig_value0[NUM_TRIGGER_STAGES];
    uint16_t trig_value1[NUM_TRIGGER_STAGES];
    uint16_t trig_edge0[NUM_TRIGGER_STAGES];
    uint16_t trig_edge1[NUM_TRIGGER_STAGES];
    uint16_t trig_logic0[NUM_TRIGGER_STAGES];
    uint16_t trig_logic1[NUM_TRIGGER_STAGES];
    uint32_t trig_count[NUM_TRIGGER_STAGES];

    uint32_t end_sync;
};

static const uint64_t samplerates[] = {
    SR_HZ(10),
    SR_HZ(20),
    SR_HZ(50),
    SR_HZ(100),
    SR_HZ(200),
    SR_HZ(500),
    SR_KHZ(1),
    SR_KHZ(2),
    SR_KHZ(5),
    SR_KHZ(10),
    SR_KHZ(20),
    SR_KHZ(40),
    SR_KHZ(50),
    SR_KHZ(100),
    SR_KHZ(200),
    SR_KHZ(400),
    SR_KHZ(500),
    SR_MHZ(1),
    SR_MHZ(2),
    SR_MHZ(4),
    SR_MHZ(5),
    SR_MHZ(10),
    SR_MHZ(20),
    SR_MHZ(25),
    SR_MHZ(40),
    SR_MHZ(50),
    SR_MHZ(100),
    SR_MHZ(200),
    SR_MHZ(400),
    SR_MHZ(500),
    SR_MHZ(800),
    SR_GHZ(1),
    SR_GHZ(2),
    SR_GHZ(5),
    SR_GHZ(10),
};

SR_PRIV int dsl_adjust_probes(struct sr_dev_inst *sdi, int num_probes);
SR_PRIV int dsl_setup_probes(struct sr_dev_inst *sdi, int num_probes);
SR_PRIV const GSList *dsl_mode_list(const struct sr_dev_inst *sdi);
SR_PRIV void dsl_adjust_samplerate(struct DSL_context *devc);

SR_PRIV int dsl_en_ch_num(const struct sr_dev_inst *sdi);
SR_PRIV gboolean dsl_check_conf_profile(libusb_device *dev);
SR_PRIV int dsl_configure_probes(const struct sr_dev_inst *sdi);
SR_PRIV uint64_t dsl_channel_depth(const struct sr_dev_inst *sdi);

SR_PRIV int dsl_wr_reg(const struct sr_dev_inst *sdi, uint8_t addr, uint8_t value);
SR_PRIV int dsl_rd_reg(const struct sr_dev_inst *sdi, uint8_t addr, uint8_t *value);
SR_PRIV int dsl_wr_ext(const struct sr_dev_inst *sdi, uint8_t addr, uint8_t value);
SR_PRIV int dsl_rd_ext(const struct sr_dev_inst *sdi, unsigned char *ctx, uint16_t addr, uint8_t len);

SR_PRIV int dsl_wr_dso(const struct sr_dev_inst *sdi, uint64_t cmd);
SR_PRIV int dsl_wr_nvm(const struct sr_dev_inst *sdi, unsigned char *ctx, uint16_t addr, uint8_t len);
SR_PRIV int dsl_rd_nvm(const struct sr_dev_inst *sdi, unsigned char *ctx, uint16_t addr, uint8_t len);

SR_PRIV int dsl_fpga_arm(const struct sr_dev_inst *sdi);
SR_PRIV int dsl_fpga_config(struct libusb_device_handle *hdl, const char *filename);

SR_PRIV int dsl_config_get(int id, GVariant **data, const struct sr_dev_inst *sdi,
                      const struct sr_channel *ch,
                      const struct sr_channel_group *cg);
SR_PRIV int dsl_config_list(int key, GVariant **data, const struct sr_dev_inst *sdi,
                       const struct sr_channel_group *cg);

SR_PRIV int dsl_dev_open(struct sr_dev_driver *di, struct sr_dev_inst *sdi, gboolean *fpga_done);
SR_PRIV int dsl_dev_close(struct sr_dev_inst *sdi);
SR_PRIV int dsl_dev_acquisition_stop(const struct sr_dev_inst *sdi, void *cb_data);
SR_PRIV int dsl_dev_status_get(const struct sr_dev_inst *sdi, struct sr_status *status, gboolean prg, int begin, int end);

SR_PRIV unsigned int dsl_get_timeout(struct DSL_context *devc);
SR_PRIV int dsl_start_transfers(const struct sr_dev_inst *sdi);

#endif
