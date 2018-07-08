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

#ifndef LIBDSL_HARDWARE_DEMO_H
#define LIBDSL_HARDWARE_DEMO_H

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

#include <unistd.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#define pipe(fds) _pipe(fds, 4096, _O_BINARY)
#endif

#undef min
#define min(a,b) ((a)<(b)?(a):(b))
#undef max
#define max(a,b) ((a)>(b)?(a):(b))

/* Message logging helpers with subsystem-specific prefix string. */
#define LOG_PREFIX "demo: "
#define sr_log(l, s, args...) sr_log(l, LOG_PREFIX s, ## args)
#define sr_spew(s, args...) sr_spew(LOG_PREFIX s, ## args)
#define sr_dbg(s, args...) sr_dbg(LOG_PREFIX s, ## args)
#define sr_info(s, args...) sr_info(LOG_PREFIX s, ## args)
#define sr_warn(s, args...) sr_warn(LOG_PREFIX s, ## args)
#define sr_err(s, args...) sr_err(LOG_PREFIX s, ## args)

/* hardware Capabilities */
#define CAPS_MODE_LOGIC (1 << 0)
#define CAPS_MODE_ANALOG (1 << 1)
#define CAPS_MODE_DSO (1 << 2)

#define CAPS_FEATURE_NONE 0
// zero calibration ability
#define CAPS_FEATURE_ZERO (1 << 4)
/* end */

static struct sr_dev_mode mode_list[] = {
    {"LA", LOGIC},
    {"DAQ", ANALOG},
    {"OSC", DSO},
};

/* Supported patterns which we can generate */
enum DEMO_PATTERN {
    PATTERN_SINE = 0,
    PATTERN_SQUARE = 1,
    PATTERN_TRIANGLE = 2,
    PATTERN_SAWTOOTH = 3,
    PATTERN_RANDOM = 4,
};

static const char *pattern_strings[] = {
    "Sine",
    "Square",
    "Triangle",
    "Sawtooth",
    "Random",
};

struct DEMO_caps {
    uint64_t mode_caps;
    uint64_t feature_caps;
    uint64_t channels;
    uint64_t hw_depth;
    uint64_t dso_depth;
    uint8_t intest_channel;
    const uint64_t *vdivs;
    uint8_t vga_id;
    uint16_t default_channelmode;
    enum DEMO_PATTERN default_pattern;
    uint64_t default_timebase;
};

struct DEMO_profile {
    const char *vendor;
    const char *model;
    const char *model_version;

    struct DEMO_caps dev_caps;
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

enum CHANNEL_ID {
    DEMO_LOGIC100x16 = 0,
    DEMO_ANALOG10x2,
    DEMO_DSO200x2,
};

struct DEMO_channels {
    enum CHANNEL_ID id;
    enum OPERATION_MODE mode;
    enum CHANNEL_TYPE type;

    uint16_t num;
    uint8_t unit_bits;
    uint64_t default_samplerate;
    uint64_t default_samplelimit;
    uint64_t min_samplerate;
    uint64_t max_samplerate;

    const char *descr;
};

static const struct DEMO_channels channel_modes[] = {
    // LA Stream
    {DEMO_LOGIC100x16,  LOGIC,  SR_CHANNEL_LOGIC,  16, 1, SR_MHZ(1), SR_Mn(1),
     SR_KHZ(10), SR_MHZ(100), "Use 16 Channels (Max 20MHz)"},

    // DAQ
    {DEMO_ANALOG10x2,   ANALOG, SR_CHANNEL_ANALOG,  2,  8, SR_MHZ(1), SR_Mn(1),
     SR_HZ(10),  SR_MHZ(10), "Use Channels 0~1 (Max 10MHz)"},

    // OSC
    {DEMO_DSO200x2,     DSO,    SR_CHANNEL_DSO,     2,  8, SR_MHZ(100), SR_Kn(10),
     SR_HZ(100), SR_MHZ(200), "Use Channels 0~1 (Max 200MHz)"}
};

static const struct DEMO_profile supported_Demo[] = {
    /*
     * Demo
     */
    {"DreamSourceLab", "Demo Device", NULL,
     {CAPS_MODE_LOGIC | CAPS_MODE_ANALOG | CAPS_MODE_DSO,
      CAPS_FEATURE_NONE,
      (1 << DEMO_LOGIC100x16) |
      (1 << DEMO_ANALOG10x2) |
      (1 << DEMO_DSO200x2),
      SR_Mn(100),
      SR_Kn(20),
      0,
      vdivs10to2000,
      0,
      DEMO_LOGIC100x16,
      PATTERN_SINE,
      SR_NS(500)}
    },

    { 0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}
};

struct demo_context {
    const struct DEMO_profile *profile;

    int pipe_fds[2];
    GIOChannel *channel;
    uint64_t cur_samplerate;
    uint64_t limit_samples;
    uint64_t limit_samples_show;
    uint64_t limit_msec;
    uint8_t sample_generator;
    uint64_t samples_counter;
    void *cb_data;
    int64_t starttime;
    int stop;
    uint64_t timebase;
    enum CHANNEL_ID ch_mode;
    uint16_t samplerates_min_index;
    uint16_t samplerates_max_index;
    gboolean instant;
    uint8_t max_height;
    uint64_t samples_not_sent;

    uint16_t *buf;
    uint64_t pre_index;
    struct sr_status mstatus;

    int trigger_stage;
    uint16_t trigger_mask;
    uint16_t trigger_value;
    uint16_t trigger_edge;
    uint8_t trigger_slope;
    uint8_t trigger_source;
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

static const char *probeMapUnits[] = {
    "V",
    "A",
    "°C",
    "°F",
    "g",
    "m",
    "m/s",
};

/* We name the probes 0-7 on our demo driver. */
static const char *probe_names[] = {
    "0", "1", "2", "3",
    "4", "5", "6", "7",
    "8", "9", "10", "11",
    "12", "13", "14", "15",
    NULL,
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

static const char *maxHeights[] = {
    "1X",
    "2X",
    "3X",
    "4X",
    "5X",
};

static const int const_dc = 1.95 / 10 * 255;
static const int sinx[] = {
  0,   2,   3,   5,   6,   8,   9,  11,  12,  14,  16,  17,  18,  20,  21,  23,  24,  26,  27,  28,
 30,  31,  32,  33,  34,  35,  37,  38,  39,  40,  41,  41,  42,  43,  44,  45,  45,  46,  47,  47,
 48,  48,  49,  49,  49,  49,  50,  50,  50,  50,  50,  50,  50,  50,  50,  49,  49,  49,  48,  48,
 47,  47,  46,  46,  45,  44,  44,  43,  42,  41,  40,  39,  38,  37,  36,  35,  34,  33,  31,  30,
 29,  28,  26,  25,  24,  22,  21,  19,  18,  16,  15,  13,  12,  10,   9,   7,   6,   4,   2,   1,
 -1,  -2,  -4,  -6,  -7,  -9, -10, -12, -13, -15, -16, -18, -19, -21, -22, -24, -25, -26, -28, -29,
-30, -31, -33, -34, -35, -36, -37, -38, -39, -40, -41, -42, -43, -44, -44, -45, -46, -46, -47, -47,
-48, -48, -49, -49, -49, -50, -50, -50, -50, -50, -50, -50, -50, -50, -49, -49, -49, -49, -48, -48,
-47, -47, -46, -45, -45, -44, -43, -42, -41, -41, -40, -39, -38, -37, -35, -34, -33, -32, -31, -30,
-28, -27, -26, -24, -23, -21, -20, -18, -17, -16, -14, -12, -11,  -9,  -8,  -6,  -5,  -3,  -2,   0,
};

static const int sqrx[] = {
 50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,
 50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,
 50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,
 50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,
 50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,  50,
-50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
-50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
-50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
-50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
-50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
};

static const int trix[] = {
  0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,
 20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,
 40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  49,  48,  47,  46,  45,  44,  43,  42,  41,
 40,  39,  38,  37,  36,  35,  34,  33,  32,  31,  30,  29,  28,  27,  26,  25,  24,  23,  22,  21,
 20,  19,  18,  17,  16,  15,  14,  13,  12,  11,  10,   9,   8,   7,   6,   5,   4,   3,   2,   1,
  0,  -1,  -2,  -3,  -4,  -5,  -6,  -7,  -8,  -9, -10, -11, -12, -13, -14, -15, -16, -17, -18, -19,
-20, -21, -22, -23, -24, -25, -26, -27, -28, -29, -30, -31, -32, -33, -34, -35, -36, -37, -38, -39,
-40, -41, -42, -43, -44, -45, -46, -47, -48, -49, -50, -49, -48, -47, -46, -45, -44, -43, -42, -41,
-40, -39, -38, -37, -36, -35, -34, -33, -32, -31, -30, -29, -28, -27, -26, -25, -24, -23, -22, -21,
-20, -19, -18, -17, -16, -15, -14, -13, -12, -11, -10,  -9,  -8,  -7,  -6,  -5,  -4,  -3,  -2,  -1,
};

static const int sawx[] = {
  0,   0,   1,   1,   2,   2,   3,   3,   4,   4,   5,   5,   6,   6,   7,   7,   8,   8,   9,   9,
 10,  10,  11,  11,  12,  12,  13,  13,  14,  14,  15,  15,  16,  16,  17,  17,  18,  18,  19,  19,
 20,  20,  21,  21,  22,  22,  23,  23,  24,  24,  25,  25,  26,  26,  27,  27,  28,  28,  29,  29,
 30,  30,  31,  31,  32,  32,  33,  33,  34,  34,  35,  35,  36,  36,  37,  37,  38,  38,  39,  39,
 40,  40,  41,  41,  42,  42,  43,  43,  44,  44,  45,  45,  46,  46,  47,  47,  48,  48,  49,  50,
-50, -49, -48, -48, -47, -47, -46, -46, -45, -45, -44, -44, -43, -43, -42, -42, -41, -41, -40, -40,
-39, -39, -38, -38, -37, -37, -36, -36, -35, -35, -34, -34, -33, -33, -32, -32, -31, -31, -30, -30,
-29, -29, -28, -28, -27, -27, -26, -26, -25, -25, -24, -24, -23, -23, -22, -22, -21, -21, -20, -20,
-19, -19, -18, -18, -17, -17, -16, -16, -15, -15, -14, -14, -13, -13, -12, -12, -11, -11, -10, -10,
 -9,  -9,  -8,  -8,  -7,  -7,  -6,  -6,  -5,  -5,  -4,  -4,  -3,  -3,  -2,  -2,  -1,  -1,   0,   0,
};

static const int ranx[] = {
 -4,  47, -49,  -1,  -3,   6, -29,  26,   1,  14, -39, -38,  36,  17,  26, -37,  -2,  27, -20, -15,
-49, -46,  36,  16,  29,  23, -30,  -3,  28,  -2,  -6,  46,  43,  50, -42,  30,  48, -50, -38, -30,
  7, -36, -20, -24, -10, -34, -24,   3, -48,  46, -11,  22,  19,  28,  39, -49, -31,  34,   2, -29,
  9,  35,   8,  10,  38,  30,  17,  48,  -3,  -6, -28,  46, -19,  18, -43,  -9, -31, -32, -41,  16,
-10,  46,  -4,   4, -32, -43, -45, -39, -33,  28,  24, -17, -43,  42,  -7,  36, -44,  -5,   9,  39,
 17, -40,  12,  16, -42,  -1,   2,  -9,  50,  -8,  27,  27,  14,   8, -18,  12,  -8,  26,  -8,  12,
-35,  49,  35,   2, -26, -24, -31,  33,  15, -47,  34,  46,  -1, -12,  14,  32, -25, -31, -35, -18,
-48, -21,  -5,   1, -27, -14,  12,  49, -11,  33,  31,  35, -36,  19,  20,  44,  29, -48,  14, -43,
  1,  30, -12,  44,  20,  49,  29, -43,  42,  30, -34,  24,  20, -40,  33, -12,  13, -45,  45, -24,
-41,  36,  -8,  46,  47, -34,  28, -39,   7, -32,  38, -27,  28,  -3,  -8,  43, -37, -24,   6,   3,
};

#endif
