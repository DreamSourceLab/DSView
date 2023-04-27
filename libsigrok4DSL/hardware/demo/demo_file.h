/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
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

#include "libsigrok-internal.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <assert.h>
#include <string.h>
#include <minizip/unzip.h>
#include "log.h"
#include "./hardware/demo/demo.h"

/* size of payloads sent across the session bus */
/** @cond PRIVATE */
#define CHUNKSIZE (512 * 1024)
#define UNITLEN 64
/** @endcond */

static uint64_t samplerates_file[1];
static uint64_t samplecounts_file[1];

static const char *maxHeights_s[] = {
    "1X",
    "2X",
    "3X",
    "4X",
    "5X",
};

static const uint64_t vdivs[] = {
    SR_mV(10),
    SR_mV(20),
    SR_mV(50),
    SR_mV(100),
    SR_mV(200),
    SR_mV(500),
    SR_V(1),
    SR_V(2),
};

struct session_packet_buffer;

struct session_vdev
{ 
    int version;
    unzFile archive; // zip document
    int capfile;     // current inner file open status

    uint16_t samplerates_min_index;
    uint16_t samplerates_max_index;

    void *buf;
    uint64_t buf_len;
    void *logic_buf;
    uint64_t logic_buf_len;


    int cur_channel;
    int cur_block;
    int num_blocks;
    uint64_t samplerate;
    uint64_t total_samples;
    int64_t trig_time;
    uint64_t trig_pos;
    int cur_probes;
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
};

uint64_t samplerate_file;
uint64_t total_samples_file;

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

static uint64_t analog_read_pos = 0;

static gboolean vdiv_change;

static gboolean instant = FALSE;

static const int hwoptions_s[] = {
    SR_CONF_PATTERN_MODE,
    SR_CONF_MAX_HEIGHT,
};

static const int32_t sessions_s[] = {
    SR_CONF_PATTERN_MODE,
    SR_CONF_MAX_HEIGHT,
};

static const int32_t probeOptions_s[] = {
    SR_CONF_PROBE_MAP_UNIT,
    SR_CONF_PROBE_MAP_MIN,
    SR_CONF_PROBE_MAP_MAX,
};

static const char *probeMapUnits_s[] = {
    "V",
    "A",
    "℃",
    "℉",
    "g",
    "m",
    "m/s",
};

static void check_file(struct sr_dev_inst *sdi);

static void init_data(struct sr_dev_inst *sdi);

static void set_mode(struct sr_dev_inst *sdi,int mode);

static void set_sample_generator(struct sr_dev_inst *sdi,uint8_t sample_generator);

static int close_archive(struct session_vdev *vdev);

static void send_error_packet(const struct sr_dev_inst *cb_sdi, struct session_vdev *vdev, struct sr_datafeed_packet *packet);

static int receive_data_logic(int fd, int revents, const struct sr_dev_inst *sdi);

static int receive_data_logic_decoder(int fd, int revents, const struct sr_dev_inst *sdi);

static int receive_data_dso(int fd, int revents, const struct sr_dev_inst *sdi);

static int dev_open_file(struct sr_dev_inst *sdi);

static int config_get_file(int id, GVariant **data, const struct sr_dev_inst *sdi,
                      const struct sr_channel *ch,
                      const struct sr_channel_group *cg);

static int config_set_file(int id, GVariant *data, struct sr_dev_inst *sdi,
                      struct sr_channel *ch,
                      struct sr_channel_group *cg);

static int config_list_file(int key, GVariant **data,
                       const struct sr_dev_inst *sdi,
                       const struct sr_channel_group *cg);            

static int dev_status_get(const struct sr_dev_inst *sdi, struct sr_status *status, gboolean prg);

static int dev_close_file(struct sr_dev_inst *sdi);

static int dev_acquisition_start_file(struct sr_dev_inst *sdi, void *cb_data);

static int dev_acquisition_stop_file(const struct sr_dev_inst *sdi, void *cb_data);

static int load_virtual_device_session(struct sr_dev_inst *sdi);

static struct DEMO_channels channel_modes_file[] = {
    // LA Stream
    {DEMO_LOGIC100x16,  LOGIC,  SR_CHANNEL_LOGIC,  16, 1, SR_MHZ(1), SR_Mn(1),
     SR_KHZ(10), SR_MHZ(100), "Use 16 Channels (Max 20MHz)"},

    // DAQ
    {DEMO_ANALOG10x2,   ANALOG, SR_CHANNEL_ANALOG,  2,  8, SR_MHZ(1), SR_Mn(1),
     SR_HZ(10),  SR_MHZ(10), "Use Channels 0~1 (Max 10MHz)"},

    // OSC
    {DEMO_DSO200x2,     DSO,    SR_CHANNEL_DSO,     2,  8, SR_MHZ(100), SR_Kn(10),
     SR_HZ(100), SR_MHZ(200), "Use Channels 0~1 (Max 200MHz)"},
};

extern uint8_t cur_sample_generator;
extern uint8_t pre_sample_generator;
extern int pre_mode;
extern int cur_mode;
static gboolean is_auto_open = FALSE;
static GTimer *packet_interval = NULL;

static gboolean is_first = TRUE;

static gboolean test = TRUE;

static gboolean is_change = TRUE;

static int enabled_probe_num;

static uint64_t packet_len;
static gdouble packet_time;
static int post_data_len;

extern char DS_RES_PATH[500];

static uint8_t analog_random_data[]={124,124,175,175, 79, 79,125,125, 99, 99,129,129, 89, 89,164,164,154,154,126,126,
                                     108,108, 79, 79,144,144,157,157, 98, 98,156,156,122,122,171,171, 86, 86,176,176,
                                      90, 90,135,135,108,108,118,118, 94, 94,131,131,174,174,150,150,156,156, 79, 79,
                                     162,162, 99, 99,163,163,138,138,145,145,176,176,122,122,174,174,146,146,119,119,
                                      96, 96,144,144,174,174,132,132, 85, 85, 89, 89, 95, 95,152,152, 85, 85,121,121,
                                      84, 84,137,137,145,145,140,140, 86, 86,130,130,178,178,120,120,155,155,136,136,
                                     140,140,154,154,140,140,177,177,130,130,104,104,161,161,162,162,174,174,116,116,
                                     160,160, 97, 97,110,110,107,107,129,129,114,114,177,177,161,161,163,163, 92, 92,
                                     148,148,157,157,142,142,129,129,116,116,148,148,157,157,170,170, 94, 94,148,148,
                                      88, 88,116,116, 83, 83,104,104,164,164,174,174, 94, 94, 89, 89, 96, 96,101,101,
                                     120,120,171,171,104,104};

static uint8_t analog_sine_data[]={128,128,130,130,131,131,134,134,137,137,140,140,144,144,146,146,149,149,152,152,
                                   155,155,158,158,161,161,162,162,165,165,167,167,169,169,170,170,172,172,173,173,
                                   175,175,176,176,177,177,177,177,177,177,178,178,178,178,178,178,178,178,177,177,
                                   177,177,176,176,175,175,174,174,172,172,171,171,169,169,167,167,165,165,163,163,
                                   161,161,158,158,156,156,153,153,150,150,147,147,146,146,143,143,140,140,137,137,
                                   134,134,130,130,127,127,124,124,121,121,118,118,115,115,113,113,110,110,107,107,
                                   104,104,102,102, 99, 99, 97, 97, 94, 94, 92, 92, 90, 90, 87, 87, 86, 86, 84, 84,
                                    83, 83, 82, 82, 81, 81, 80, 80, 79, 79, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78,
                                    79, 79, 79, 79, 80, 80, 81, 81, 82, 82, 83, 83, 85, 85, 87, 87, 88, 88, 90, 90,
                                    91, 91, 94, 94, 96, 96, 98, 98,101,101,104,104,107,107,110,110,112,112,116,116,
                                   120,120,122,122,125,125};

static uint8_t analog_square_data[]={178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,
                                     178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,
                                     178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,
                                     178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,
                                     178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,
                                     178,178,178,178, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78,
                                      78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78,
                                      78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78,
                                      78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78,
                                      78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78,
                                      78, 78, 78, 78, 78, 78};

static uint8_t analog_triangle_data[]={128,128,129,129,130,130,132,132,134,134,136,136,138,138,140,140,142,142,144,144,
                                       146,146,148,148,151,151,152,152,154,154,156,156,158,158,160,160,162,162,164,164,
                                       166,166,168,168,170,170,172,172,173,173,175,175,177,177,177,177,175,175,173,173,
                                       171,171,169,169,167,167,165,165,162,162,161,161,159,159,157,157,155,155,153,153,
                                       151,151,149,149,147,147,145,145,143,143,141,141,140,140,138,138,136,136,134,134,
                                       132,132,130,130,128,128,126,126,124,124,122,122,120,120,119,119,117,117,115,115,
                                       113,113,111,111,109,109,107,107,105,105,103,103,101,101, 98, 98, 97, 97, 95, 95,
                                        93, 93, 91, 91, 89, 89, 87, 87, 85, 85, 83, 83, 81, 81, 79, 79, 79, 79, 80, 80,
                                        82, 82, 84, 84, 86, 86, 88, 88, 90, 90, 92, 92, 94, 94, 96, 96, 98, 98,100,100,
                                       101,101,103,103,105,105,107,107,109,109,111,111,113,113,115,115,117,117,119,119,
                                       122,122,123,123,125,125};

static uint8_t analog_sawtooth_data[]={128,128,128,128,129,129,130,130,131,131,132,132,133,133,134,134,135,135,136,136,
                                       137,137,138,138,139,139,140,140,141,141,142,142,143,143,144,144,145,145,146,146,
                                       147,147,148,148,149,149,150,150,150,150,151,151,152,152,153,153,154,154,155,155,
                                       156,156,157,157,158,158,159,159,161,161,161,161,162,162,163,163,164,164,165,165,
                                       166,166,167,167,168,168,169,169,170,170,171,171,172,172,173,173,174,174,175,175,
                                       176,176,177,177, 78, 78, 80, 80, 81, 81, 82, 82, 83, 83, 83, 83, 84, 84, 85, 85,
                                        86, 86, 87, 87, 88, 88, 89, 89, 90, 90, 91, 91, 92, 92, 94, 94, 94, 94, 95, 95,
                                        96, 96, 97, 97, 98, 98, 99, 99,100,100,101,101,102,102,103,103,104,104,105,105,
                                       106,106,107,107,108,108,109,109,110,110,111,111,112,112,113,113,114,114,115,115,
                                       115,115,116,116,117,117,118,118,119,119,120,120,121,121,122,122,123,123,124,124,
                                       126,126,126,126,127,127};

static uint8_t dso_sine_data[]={ 78, 78, 78, 78, 78, 78, 79, 79, 79, 79, 80, 80, 81, 81, 83, 83, 84, 84, 86, 86,
                                 87, 87, 89, 89, 91, 91, 94, 94, 96, 96, 98, 98,101,101,104,104,107,107,110,110,
                                112,112,116,116,119,119,122,122,125,125,128,128,131,131,134,134,137,137,140,140,
                                144,144,146,146,149,149,152,152,155,155,158,158,160,160,162,162,165,165,167,167,
                                169,169,170,170,172,172,173,173,175,175,176,176,177,177,177,177,178,178,178,178,
                                178,178,178,178,178,178,177,177,176,176,175,175,174,174,173,173,172,172,170,170,
                                168,168,166,166,164,164,162,162,159,159,157,157,154,154,152,152,149,149,146,146,
                                143,143,140,140,137,137,134,134,130,130,127,127,124,124,121,121,118,118,115,115,
                                112,112,109,109,106,106,103,103,100,100, 98, 98, 95, 95, 93, 93, 91, 91, 89, 89,
                                 87, 87, 85, 85, 84, 84, 82, 82, 81, 81, 80, 80, 79, 79, 79, 79, 78, 78, 78, 78};


static uint8_t dso_square_data[]={178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,
                                  178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,
                                  178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,
                                  178,178,178,178, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78,
                                   78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78,
                                   78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78,
                                   78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78,
                                   78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78, 78,
                                   78, 78,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,
                                  178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178,178};

static uint8_t dso_triangle_data[]={101,101,103,103,105,105,107,107,109,109,111,111,113,113,115,115,117,117,119,119,
                                    121,121,123,123,125,125,128,128,130,130,132,132,134,134,136,136,138,138,140,140,
                                    142,142,144,144,146,146,148,148,150,150,152,152,154,154,156,156,158,158,160,160,
                                    162,162,164,164,166,166,168,168,170,170,172,172,174,174,176,176,178,178,176,176,
                                    174,174,172,172,170,170,168,168,166,166,164,164,162,162,160,160,158,158,156,156,
                                    154,154,152,152,150,150,148,148,146,146,144,144,142,142,140,140,138,138,136,136,
                                    134,134,132,132,130,130,128,128,126,126,124,124,122,122,120,120,118,118,116,116,
                                    114,114,112,112,110,110,108,108,106,106,104,104,102,102,100,100, 98, 98, 96, 96,
                                     94, 94, 92, 92, 90, 90, 88, 88, 86, 86, 84, 84, 82, 82, 80, 80, 78, 78, 80, 80,
                                     82, 82, 84, 84, 86, 86, 88, 88, 90, 90, 92, 92, 94, 94, 96, 96, 98, 98,100,100};

static uint8_t dso_sawtooth_data[]={149,149,150,150,151,151,152,152,153,153,154,154,155,155,156,156,157,157,158,158,
                                    159,159,160,160,161,161,162,162,163,163,164,164,165,165,166,166,167,167,168,168,
                                    169,169,170,170,171,171,172,172,173,173,174,174,175,175,176,176,177,177, 78, 78,
                                     80, 80, 81, 81, 82, 82, 83, 83, 84, 84, 85, 85, 86, 86, 87, 87, 88, 88, 89, 89,
                                     90, 90, 91, 91, 92, 92, 93, 93, 94, 94, 95, 95, 96, 96, 97, 97, 98, 98, 99, 99,
                                    100,100,101,101,102,102,103,103,104,104,105,105,106,106,107,107,108,108,109,109,
                                    110,110,111,111,112,112,113,113,114,114,115,115,116,116,117,117,118,118,119,119,
                                    120,120,121,121,122,122,123,123,124,124,125,125,126,126,127,127,128,128,128,128,
                                    129,129,130,130,131,131,132,132,133,133,134,134,135,135,136,136,137,137,138,138,
                                    139,139,140,140,141,141,142,142,143,143,144,144,145,145,146,146,147,147,148,148};

static uint8_t dso_random_data[]={ 91, 91,131,131,123,123,177,177,104,104, 80, 80,137,137,117,117,134,134,174,174,
                                  167,167,161,161,124,124,117,117,145,145,159,159,175,175,150,150, 88, 88,163,163,
                                   79, 79,147,147,140,140, 92, 92,127,127,156,156,144,144,147,147,125,125,167,167,
                                   86, 86,148,148,134,134, 79, 79,127,127,172,172, 99, 99, 97, 97,130,130,157,157,
                                  154,154,162,162,119,119, 80, 80,129,129,130,130,178,178,142,142,142,142, 99, 99,
                                  120,120, 85, 85, 89, 89,137,137,155,155,129,129, 90, 90,163,163,155,155,158,158,
                                  164,164,136,136,142,142,116,116,145,145,138,138,136,136,172,172,154,154,166,166,
                                  110,110,148,148, 91, 91,158,158,140,140,177,177,126,126,145,145,120,120,157,157,
                                  155,155,176,176,154,154, 85, 85,108,108,125,125,120,120,170,170,113,113,122,122,
                                  140,140,158,158, 79, 79,100,100, 93, 93, 94, 94, 82, 82,174,174,177,177,152,152};