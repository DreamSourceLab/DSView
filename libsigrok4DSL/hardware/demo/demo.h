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
#include "../../libsigrok-internal.h"
#include <minizip/unzip.h>

#define DEFAULT_LOGIC_FILE "protocol"
#define DEFAULT_ANALOG_FILE "sine"
#define DEFAULT_DSO_FILE "sine"

#define SEC 1
#define LOGIC_POST_DATA_PER_SECOND(n) ((n)/(8))
#define LOGIC_PACKET_NUM_PER_SEC (gdouble)1000
#define LOGIC_PACKET_TIME(n) (gdouble)((SEC)/(n))
#define LOGIC_PACKET_LEN(n) (ceil((LOGIC_POST_DATA_PER_SECOND(n))/(LOGIC_PACKET_NUM_PER_SEC)/(8))*(8))
#define LOGIC_MIN_PACKET_LEN 8
#define LOGIC_MIN_PACKET_NUM(n) (LOGIC_POST_DATA_PER_SECOND(n))/(LOGIC_MIN_PACKET_LEN)
#define LOGIC_MIN_PACKET_TIME(n) ((SEC)/(gdouble)(LOGIC_MIN_PACKET_NUM(n)))
#define LOGIC_MAX_PACKET_LEN 62504
#define LOGIC_MAX_PACKET_NUM(n) (LOGIC_POST_DATA_PER_SECOND(n))/(LOGIC_MAX_PACKET_LEN)
#define LOGIC_MAX_PACKET_TIME(n) ((SEC)/(gdouble)(LOGIC_MAX_PACKET_NUM(n)))

#define LOGIC_BLOCK_LEN SR_MB(2)
#define LOGIC_BUF_LEN SR_MB(2)

#define LOGIC_FREQ_LIMIT SR_MHZ(100)
#define LOGIC_MEMORY_LIMIT SR_MB(500)

#define DSO_PACKET_NUM_PER_SEC (gdouble)200
#define DSO_PACKET_TIME ((SEC)/(DSO_PACKET_NUM_PER_SEC))
#define DSO_PACKET_LEN 20000
#define DSO_RANDOM_DATA rand()%120 +68
#define DSO_BUF_LEN SR_MB(1)

#define ANALOG_PROBE_NUM 2
#define ANALOG_PACKET_NUM_PER_SEC 200
#define ANALOG_POST_DATA_PER_SECOND(n) ((n)*(ANALOG_PROBE_NUM))
#define ANALOG_PACKET_LEN(n) ((ANALOG_POST_DATA_PER_SECOND(n))/(ANALOG_PACKET_NUM_PER_SEC))
#define ANALOG_PACKET_TIME(n) (gdouble)((SEC)/(gdouble)(n))
#define ANALOG_MIN_PACKET_LEN 2
#define ANALOG_MIN_PACKET_NUM(n) ((ANALOG_POST_DATA_PER_SECOND(n))/(ANALOG_MIN_PACKET_LEN))
#define ANALOG_PACKET_ALIGN 2

#define LOGIC_HW_DEPTH (SR_GHZ(16))


#define LOGIC_MAX_PROBE_NUM 16
#define LOGIC_HIGH_LEVEL 255
#define LOGIC_LOW_LEVEL 0


#define DSO_DEFAULT_VDIV 1000
#define DSO_MID_VAL 128
#define DSO_MAX_VAL 0
#define DSO_MIN_VAL 255

#define DSO_EXPAND_MID_VAL(i) ((i)*(uint16_t)(256))
#define DSO_LIMIT 255
#define DSO_WAVE_PERIOD_LEN 200
#define DSO_WAVE_PERIOD_LEN_PER_PROBE 100


#define ANALOG_HW_DEPTH (SR_MHZ(12.5))
#define ANALOG_DEFAULT_VDIV 1000
#define ANALOG_MID_VAL 128
#define ANALOG_MAX_VAL 0
#define ANALOG_MIN_VAL 255
#define ANALOG_AC_COUL_OFFSET 128
#define ANALOG_DC_COUL_OFFSET 178

#define ANALOG_CYCLE_RATIO ((gdouble)(103) / (gdouble)(2048))
#define ANALOG_DATA_LEN_PER_CYCLE 206
#define ANALOG_RANDOM_DATA rand()%120 +68
#define ANALOG_PROBE_NUM 2

#define ANALOG_RETE(n) ((n/SR_HZ(10)))

//defult value
#define LOGIC_DEFAULT_SAMPLERATE SR_MHZ(1)
#define LOGIC_DEFAULT_TOTAL_SAMPLES SR_MHZ(1)
#define LOGIC_DEFAULT_NUM_PROBE 16

#define DSO_DEFAULT_SAMPLERATE SR_MHZ(100)
#define DSO_DEFAULT_TOTAL_SAMPLES SR_KHZ(10)
#define DSO_DEFAULT_NUM_PROBE 2
#define DSO_DEFAULT_NUM_BLOCK 1
#define DSO_DEFAULT_ENABLE TRUE
#define DSO_DEFAULT_COUPLING 1
#define DSO_DEFAULT_VIDV SR_V(1)
#define DSO_DEFAULT_VFACOTR SR_V(1)
#define DSO_DEFAULT_HW_OFFSET 128
#define DSO_DEFAULT_OFFSET 128
#define DSO_DEFAULT_TRIG_VAL 128

#define ANALOG_DEFAULT_SAMPLERATE SR_MHZ(1)
#define ANALOG_DEFAULT_TOTAL_SAMPLES SR_MHZ(1)
#define ANALOG_DEFAULT_NUM_PROBE 2
#define ANALOG_DEFAULT_NUM_BLOCK 1
#define ANALOG_DEFAULT_BIT 8
#define ANALOG_DEFAULT_ENABLE TRUE
#define ANALOG_DEFAULT_COUPLING 1
#define ANALOG_DEFAULT_VIDV SR_V(1)
#define ANALOG_DEFAULT_VFACOTR SR_mV(1)
#define ANALOG_DEFAULT_HW_OFFSET 128
#define ANALOG_DEFAULT_OFFSET 128
#define ANALOG_DEFAULT_TRIG_VAL 128
#define ANALOG_DEFAULT_MAP_DEFAULT TRUE
#define ANALOG_DEFAULT_MAP_UNIT "V"
#define ANALOG_DEFAULT_MAP_MIN (gdouble)-5
#define ANALOG_DEFAULT_MAP_MAX (gdouble)+5




enum DEMO_PATTERN {  
    PATTERN_RANDOM = 0,
    PATTERN_DEFAULT = 1,
};

enum DEMO_LOGIC_CHANNEL_ID {
    DEMO_LOGIC125x16 = 16,
    DEMO_LOGIC250x12 = 17,
    DEMO_LOGIC500x6 = 18,
    DEMO_LOGIC1000x3 = 19,
};

enum DEMO_CHANNEL_ID {
    DEMO_LOGIC100x16 = 0,
    DEMO_ANALOG10x2 = 1 ,
    DEMO_DSO200x2 = 2,
};

enum DEMO_LOGIC_CHANNEL_INDEX {
    LOGIC125x16 = 0,
    LOGIC250x12 = 1,
    LOGIC500x6 = 2,
    LOGIC1000x3 = 3,
};


struct session_packet_buffer;

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

struct session_vdev
{ 
    int version;
    unzFile archive; // zip document
    int capfile;     // current inner file open status

    uint16_t samplerates_min_index;
    uint16_t samplerates_max_index;
    uint8_t sample_generator;
    void        *data_buf;
    uint64_t    data_buf_len;
    
    void *analog_post_buf;
    uint64_t analog_post_buf_len;
    uint64_t analog_read_pos;

    int cur_block;
    int num_blocks;
    uint64_t samplerate;
    uint64_t total_samples;
    int64_t trig_time;
    uint64_t trig_pos;
    int num_probes;
    int enabled_probes;
    uint64_t timebase;
    uint64_t max_timebase;
    uint64_t min_timebase;
    uint8_t unit_bits;
    uint32_t ref_min;
    uint32_t ref_max;
    uint8_t max_height;
    struct sr_status mstatus;
    struct session_packet_buffer   *packet_buffer;
    uint8_t   trigger_hrate; //dso trig position.

    //time
    uint64_t packet_len;
    gdouble packet_time;

    //status
    gboolean instant;
    gboolean load_data;
    gboolean vdiv_change;
    gboolean offset_change;
    gboolean timebase_change;
    gboolean channel_mode_change;
    gboolean logic_mem_limit;

    //post
    uint64_t post_data_len;

    //logic
    uint64_t logci_cur_packet_num;  //dso or analog

    uint8_t logic_sel_probe_list[LOGIC_MAX_PROBE_NUM];
    uint8_t logic_sel_probe_num;
    enum DEMO_LOGIC_CHANNEL_ID logic_ch_mode;
    enum DEMO_LOGIC_CHANNEL_INDEX logic_ch_mode_index;

    int is_loop;
};

#define SESSION_MAX_CHANNEL_COUNT 512

struct session_packet_buffer
{
    void       *post_buf;
    uint64_t    post_buf_len;
    uint64_t    post_len; 
    
    uint64_t    block_buf_len;
    uint64_t    block_chan_read_pos;
    uint64_t    block_data_len;
    void       *block_bufs[SESSION_MAX_CHANNEL_COUNT];
    uint64_t    block_read_positions[SESSION_MAX_CHANNEL_COUNT];
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
    int default_pattern; /**enum DEMO_PATTERN type*/
    uint64_t default_timebase;
};

struct DEMO_profile {
    const char *vendor;
    const char *model;
    const char *model_version;

    struct DEMO_caps dev_caps;
};

struct DEMO_channels {
    enum DEMO_CHANNEL_ID id;
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
    SR_MHZ(125),
    SR_MHZ(250),
    SR_MHZ(400),
    SR_MHZ(500),
    SR_MHZ(800),
    SR_GHZ(1),
    SR_GHZ(2),
    SR_GHZ(5),
    SR_GHZ(10),
};

/* hardware Capabilities */
#define CAPS_MODE_LOGIC (1 << 0)
#define CAPS_MODE_ANALOG (1 << 1)
#define CAPS_MODE_DSO (1 << 2)

#define CAPS_FEATURE_NONE 0
// zero calibration ability
#define CAPS_FEATURE_ZERO (1 << 4)
/* end */


static const char *maxHeights[] = {
    "1X",
    "2X",
    "3X",
    "4X",
    "5X",
};

/* We name the probes 0-7 on our demo driver. */
static const char *probe_names[] = {
    "0", "1", "2", "3",
    "4", "5", "6", "7",
    "8", "9", "10", "11",
    "12", "13", "14", "15",
    NULL,
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

static const int hwoptions[] = {
    SR_CONF_PATTERN_MODE,
    SR_CONF_MAX_HEIGHT,
};

static const int32_t sessions[] = {
    SR_CONF_SAMPLERATE,
    SR_CONF_LIMIT_SAMPLES,
    SR_CONF_PATTERN_MODE,
    SR_CONF_MAX_HEIGHT,
};

static const int32_t probeOptions[] = {
    SR_CONF_PROBE_COUPLING,
    SR_CONF_PROBE_VDIV,
    SR_CONF_PROBE_MAP_DEFAULT,
    SR_CONF_PROBE_MAP_UNIT,
    SR_CONF_PROBE_MAP_MIN,
    SR_CONF_PROBE_MAP_MAX,
};

static const int32_t probeSessions[] = {
    SR_CONF_PROBE_COUPLING,
    SR_CONF_PROBE_VDIV,
    SR_CONF_PROBE_MAP_DEFAULT,
    SR_CONF_PROBE_MAP_UNIT,
    SR_CONF_PROBE_MAP_MIN,
    SR_CONF_PROBE_MAP_MAX,
};

static const uint8_t probeCoupling[] = {
    SR_DC_COUPLING,
    SR_AC_COUPLING,
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
      PATTERN_RANDOM,
      SR_NS(500)}
    },

    { 0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}
};

static int vdev_init(struct sr_dev_inst* sdi);

static void dso_status_update(struct session_vdev *vdev);

static void logic_adjust_samplerate(struct session_vdev * vdev);

static int init_analog_random_data(struct session_vdev * vdev);

static int delay_time(struct session_vdev *vdev);

static void get_last_packet_len(struct sr_datafeed_logic *logic,struct session_vdev * vdev);

static int get_bit(uint64_t timebase);


static void scan_dsl_file(struct sr_dev_inst *sdi);

static int reset_dsl_path(struct sr_dev_inst *sdi, uint8_t pattern_mode);

static void adjust_samplerate(struct sr_dev_inst *sdi);

static int init_random_data(struct session_vdev * vdev);

static int hw_init(struct sr_context *sr_ctx);

static GSList *hw_scan(GSList *options);

static const GSList *hw_dev_mode_list(const struct sr_dev_inst *sdi);

static int hw_dev_open(struct sr_dev_inst *sdi);

static int hw_dev_close(struct sr_dev_inst *sdi);

static int dev_destroy(struct sr_dev_inst *sdi);


static int config_get(int id, GVariant **data, const struct sr_dev_inst *sdi,
                      const struct sr_channel *ch,
                      const struct sr_channel_group *cg);

static int config_set(int id, GVariant *data, struct sr_dev_inst *sdi,
                      struct sr_channel *ch,
                      struct sr_channel_group *cg);

static int config_list(int key, GVariant **data, const struct sr_dev_inst *sdi,
                       const struct sr_channel_group *cg);

static int hw_dev_acquisition_start(struct sr_dev_inst *sdi,
		void *cb_data);

static int hw_dev_acquisition_stop(const struct sr_dev_inst *sdi, void *cb_data);

static int hw_dev_status_get(const struct sr_dev_inst *sdi, struct sr_status *status, gboolean prg);

static int load_virtual_device_session(struct sr_dev_inst *sdi);

static int receive_data_logic(int fd, int revents, const struct sr_dev_inst *sdi);

static int receive_data_logic_decoder(int fd, int revents, const struct sr_dev_inst *sdi);

static int receive_data_dso(int fd, int revents, const struct sr_dev_inst *sdi);

static int receive_data_analog(int fd, int revents, const struct sr_dev_inst *sdi);

static void send_error_packet(const struct sr_dev_inst *cb_sdi, struct session_vdev *vdev, struct sr_datafeed_packet *packet);

static int close_archive(struct session_vdev *vdev);

static int dso_wavelength_updata(struct session_vdev *vdev);


#endif
