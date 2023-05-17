/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2010 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2011 Olivier Fauchon <olivier@aixmarseille.com>
 * Copyright (C) 2012 Alexandru Gagniuc <mr.nuke.me@gmail.com>
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

#include "demo.h"
#include <math.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <inttypes.h>
#include <unistd.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#define pipe(fds) _pipe(fds, 4096, _O_BINARY)
#endif

#include "../../log.h"

/* Message logging helpers with subsystem-specific prefix string. */

#undef LOG_PREFIX
#define LOG_PREFIX "demo: "

/* The size of chunks to send through the session bus. */
/* TODO: Should be configurable. */
#define BUFSIZE                512*1024
#define DSO_BUFSIZE            10*1024

static struct DEMO_channels channel_modes[] = {
    // LA Stream
    {DEMO_LOGIC100x16,  LOGIC,  SR_CHANNEL_LOGIC,  16, 1, SR_MHZ(1), SR_Mn(1),
     SR_KHZ(10), SR_MHZ(100), "Use 16 Channels (Max 20MHz)"},
    //updata

    // DAQ
    {DEMO_ANALOG10x2,   ANALOG, SR_CHANNEL_ANALOG,  2,  8, SR_MHZ(1), SR_Mn(1),
     SR_HZ(10),  SR_MHZ(10), "Use Channels 0~1 (Max 10MHz)"},

    // OSC
    {DEMO_DSO200x2,     DSO,    SR_CHANNEL_DSO,     2,  8, SR_MHZ(100), SR_Kn(10),
     SR_HZ(100), SR_MHZ(200), "Use Channels 0~1 (Max 200MHz)"},
};


/* Private, per-device-instance driver context. */
/* TODO: struct context as with the other drivers. */

/* List of struct sr_dev_inst, maintained by dev_open()/dev_close(). */
SR_PRIV struct sr_dev_driver demo_driver_info;
static struct sr_dev_driver *di = &demo_driver_info;

extern struct ds_trigger *trigger;

static void init_analog_random_data(struct session_vdev * vdev)
{
    if(vdev->analog_buf != NULL)
    {
        g_safe_free(vdev->analog_buf);
        vdev->analog_buf = NULL;
    }

    vdev->analog_buf = g_try_malloc0(DSO_BUF_LEN);
    if (vdev->analog_buf == NULL)
    {
        sr_err("%s: vdev->analog_buf malloc failed", __func__);
        return SR_ERR_MALLOC;
    }
    for(int i = 0;i < DSO_BUF_LEN ;i++)
    {
        if(i % 2 == 0)
            *(uint8_t*)(vdev->analog_buf + i) = ANALOG_RANDOM_DATA;
        else
            *(uint8_t*)(vdev->analog_buf + i) = *(uint8_t*)(vdev->analog_buf + i -1);
    }
    vdev->analog_buf_len = DSO_BUF_LEN;
}

static void delay_time()
{
    gdouble packet_elapsed = g_timer_elapsed(packet_interval, NULL);
    gdouble waittime = packet_time - packet_elapsed;
    if(waittime > 0)
    {
        g_usleep(SR_MS(waittime));
    }
}

static void get_last_packet_len(struct sr_datafeed_logic *logic,const struct session_vdev * vdev)
{
    assert(vdev);
    int last_packet_len = post_data_len - (logic->length / enabled_probe_num);
    last_packet_len = (vdev->total_samples/8) - last_packet_len;
    logic->length = last_packet_len * enabled_probe_num;
    post_data_len = vdev->total_samples/8;
}

static void reset_enabled_probe_num(struct sr_dev_inst *sdi)
{
    struct sr_channel *probe;
    enabled_probe_num = 0;
    for(GSList *l = sdi->channels; l; l = l->next)
    {
        probe = (struct sr_channel *)l->data;
        if(probe->enabled)
        {
            enabled_probe_num++;
        }
    }
}

static void init_pattern_mode_list()
{
    int i;
    if(pattern_logic_count != 1)
    {
        for(i = 1 ;i < pattern_logic_count ; i++)
        {
            if(pattern_strings_logic[i] != NULL)
            {
                g_safe_free(pattern_strings_logic[i]);
                pattern_strings_logic[i] =NULL;
            }
        }
    }
    if(pattern_dso_count != 1)
    {
        for(i = 1 ;i < pattern_dso_count ; i++)
        {
            if(pattern_strings_dso[i] != NULL)
            {
                g_safe_free(pattern_strings_dso[i]);
                pattern_strings_dso[i] =NULL;
            }
        }
    }
    if(pattern_analog_count != 1)
    {
        for(i = 1 ;i < pattern_analog_count ; i++)
        {
            if(pattern_strings_analog[i] != NULL)
            {
                g_safe_free(pattern_strings_analog[i]);
                pattern_strings_analog[i] =NULL;
            }
        }
    }
}

static int get_pattern_mode_index_by_string(uint8_t device_mode , const char* str)
{
    int index = PATTERN_INVALID,
    i = PATTERN_RANDOM;
    if (device_mode == LOGIC)
    {
        while (pattern_strings_logic[i] != NULL)
        {
            if(!strcmp(str,pattern_strings_logic[i]))
            {
                index = i;
                break;
            }
            else
                i++;
        }
    }
    else if (device_mode == DSO)
    {
        while (pattern_strings_dso[i] != NULL)
        {
            if(!strcmp(str,pattern_strings_dso[i]))
            {
                index = i;
                break;
            }
            else
                i++;
        }
    }
    else
    {
        while (pattern_strings_analog[i] != NULL)
        {
            if(!strcmp(str,pattern_strings_analog[i]))
            {
                index = i;
                break;
            }
            else
                i++;
        }
    }

    if(index == PATTERN_INVALID)
        index = PATTERN_RANDOM;
    return index;
}

static void get_pattern_mode_from_file(uint8_t device_mode)
{
    const gchar * filename = NULL;
    char dir_str[500];
    int index = 1;

    strcpy(dir_str,DS_RES_PATH);
    strcat(dir_str,"../");
    strcat(dir_str,"demo/");

    if(device_mode == LOGIC)
        strcat(dir_str,"logic/");
    else if(device_mode == DSO)
        strcat(dir_str,"dso/");
    else if(device_mode == ANALOG)
        strcat(dir_str,"analog/");

    GDir *dir  = NULL;
    dir = g_dir_open(dir_str,0,NULL);
    if(dir == NULL)
    {
        return 0;
    }

    while ((filename = g_dir_read_name(dir)) != NULL)
    {
        if (FALSE == g_file_test(filename,G_FILE_TEST_IS_DIR))
        {
            if(strstr(filename,".demo") != NULL)
            {
                char *tmp_file_name = g_try_malloc0(strlen(filename)-strlen(".demo")+1);
                snprintf(tmp_file_name, strlen(filename)-strlen(".demo")+1 , "%s", filename);
                if(device_mode == LOGIC)
                    pattern_strings_logic[index] = tmp_file_name;
                else if(device_mode == DSO)
                    pattern_strings_dso[index] = tmp_file_name;
                else if(device_mode == ANALOG)
                    pattern_strings_analog[index] = tmp_file_name;
                if(index < 99)
                    index++;
            }
        }
    }

    g_dir_close(dir);

    if(device_mode == LOGIC)
        pattern_logic_count = index;
    else if(device_mode == DSO)
        pattern_dso_count = index;
    else if(device_mode == ANALOG)
        pattern_analog_count = index;

    return 0;
}

static void scan_dsl_file(struct sr_dev_inst *sdi)
{
    init_pattern_mode_list();

    get_pattern_mode_from_file(LOGIC);
    get_pattern_mode_from_file(DSO);
    get_pattern_mode_from_file(ANALOG);

    if(PATTERN_RANDOM <get_pattern_mode_index_by_string(LOGIC, DEFAULT_LOGIC_FILE))
    {
        int index = get_pattern_mode_index_by_string(LOGIC, DEFAULT_LOGIC_FILE);
        char * str =  pattern_strings_logic[index];
        pattern_strings_logic[index] = pattern_strings_logic[PATTERN_DEFAULT];
        pattern_strings_logic[PATTERN_DEFAULT] = str;
        sample_generator = PATTERN_DEFAULT;
        sdi->mode = LOGIC;
        reset_dsl_path(sdi,LOGIC,PATTERN_DEFAULT);
    }
    else
    {
        sample_generator = PATTERN_RANDOM;
        sdi->mode = LOGIC;
        reset_dsl_path(sdi,LOGIC,PATTERN_RANDOM);
    }
}

static int reset_dsl_path(struct sr_dev_inst *sdi,uint8_t device_mode ,uint8_t pattern_mode)
{
    unzFile archive = NULL;
    if(sdi->path != NULL)
        g_safe_free(sdi->path);
    
    char *str = g_try_malloc0(500);
    strcpy(str,DS_RES_PATH);
    strcat(str,"../");
    strcat(str,"demo/");

    if (pattern_mode != PATTERN_RANDOM)
    {
        switch (device_mode)
        {
        case LOGIC:
            if(NULL != pattern_strings_logic[pattern_mode])
            {
                strcat(str,"logic/");
                strcat(str,pattern_strings_logic[pattern_mode]);
            }
            break;
        case DSO:
            if(NULL != pattern_strings_dso[pattern_mode])
            {
                strcat(str,"dso/");
                strcat(str,pattern_strings_dso[pattern_mode]);
            }
            break;
        case ANALOG:
            if(NULL != pattern_strings_analog[pattern_mode])
            {
                strcat(str,"analog/");
                strcat(str,pattern_strings_analog[pattern_mode]);
            }
            break;
        default:
            break;
        }
        strcat(str,".demo");
    }

    if(pattern_mode != PATTERN_RANDOM)
    {
        archive = unzOpen64(str);
        if (NULL != archive){
            sdi->path = str;
            return SR_OK;
        }
        else{
            g_safe_free(str);
            sdi->path = g_strdup("");
            return SR_ERR;
        }

    }
    else{
        g_safe_free(str);
        sdi->path = g_strdup("");
        return SR_OK;
    }
}

static void adjust_samplerate(struct sr_dev_inst *sdi)
{
    if(sdi->mode == LOGIC && sample_generator > PATTERN_RANDOM)
    {
        return;
    }

    struct session_vdev *vdev = sdi->priv;
    int cur_mode = -1;
    if(sdi->mode == LOGIC)
        cur_mode = 0;
    else if(sdi->mode == DSO)
        cur_mode = 2;
    else if(sdi->mode == ANALOG)
        cur_mode = 1;

    if(cur_mode == -1)
        return;

    vdev->samplerates_max_index = ARRAY_SIZE(samplerates) - 1;
    while (samplerates[vdev->samplerates_max_index] >
           channel_modes[cur_mode].max_samplerate)
        vdev->samplerates_max_index--;

    vdev->samplerates_min_index = 0;
    while (samplerates[vdev->samplerates_min_index] <
           channel_modes[cur_mode].min_samplerate)
        vdev->samplerates_min_index++;

    assert(vdev->samplerates_max_index >= vdev->samplerates_min_index);

    if (vdev->samplerate > samplerates[vdev->samplerates_max_index])
        vdev->samplerate = samplerates[vdev->samplerates_max_index];

    if (vdev->samplerate < samplerates[vdev->samplerates_min_index])
        vdev->samplerate = samplerates[vdev->samplerates_min_index];

}

static void init_random_data(struct session_vdev * vdev,struct sr_dev_inst *sdi)
{
    int cur_probe = 0;
    int probe_count[LOGIC_MAX_PROBE_NUM] = {0};
    uint8_t probe_status[LOGIC_MAX_PROBE_NUM] = {LOGIC_HIGH_LEVEL};

    memset(probe_status,LOGIC_HIGH_LEVEL,16);
    memset(vdev->logic_buf,0,LOGIC_BUF_LEN);

    for(int i = 0 ;i < enabled_probe_num;i++)
    {
        probe_count[i] = rand()%SR_KB(5);
    }

    for(int i = 0 ; i < vdev->logic_buf_len ;i++)
    {
        if(i % 8 == 0 && i != 0)
        {
            cur_probe++;
            if(cur_probe >enabled_probe_num-1)
                cur_probe = 0;
        }
        if(probe_count[cur_probe]> 0)
        {
            memset(vdev->logic_buf+i,probe_status[cur_probe],1);
            probe_count[cur_probe] -= 1;
        }
        else
        {
            if(probe_status[cur_probe] == LOGIC_HIGH_LEVEL)
                probe_status[cur_probe] = LOGIC_LOW_LEVEL;
            else
                probe_status[cur_probe] = LOGIC_HIGH_LEVEL;
            probe_count[cur_probe] = rand()%SR_KB(5);
            memset(vdev->logic_buf+i,probe_status[cur_probe],1);
            probe_count[cur_probe] -= 1;
        }
    }
    return 0;
}


static int hw_init(struct sr_context *sr_ctx)
{
    return std_hw_init(sr_ctx, di, LOG_PREFIX);
}

static GSList *hw_scan(GSList *options)
{
    struct sr_dev_inst *sdi;
    struct session_vdev *vdev;
    GSList *devices;
    char str[500];

    (void)options;
    devices = NULL;

    sr_info("%s", "Scan demo device.");

    vdev = g_try_malloc0(sizeof(struct session_vdev));
    if (vdev == NULL)
    {
        sr_err("%s: sdi->priv malloc failed", __func__);
        return devices;
    }

    sdi = sr_dev_inst_new(LOGIC, SR_ST_INACTIVE,
                          supported_Demo[0].vendor,
                          supported_Demo[0].model,
                          supported_Demo[0].model_version);
    if (!sdi) {
        g_safe_free(vdev);
        sr_err("Device instance creation failed.");
        return NULL;
    }
    sdi->priv = vdev;
    sdi->driver = di;
    sdi->dev_type = DEV_TYPE_DEMO;

    vdev->is_loop = 0;

    devices = g_slist_append(devices, sdi);

    return devices;
}

static const GSList *hw_dev_mode_list(const struct sr_dev_inst *sdi)
{
    // struct demo_context *devc;
    GSList *l = NULL;
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(sr_mode_list); i++) {
        // if (devc->profile->dev_caps.mode_caps & (1 << i))
        if (supported_Demo[0].dev_caps.mode_caps & (1 << i))
            l = g_slist_append(l, &sr_mode_list[i]);
    }

    return l;
}

static int hw_dev_open(struct sr_dev_inst *sdi)
{
    int ret;
    assert(sdi);
    assert(sdi->priv);

    if (sdi->status == SR_ST_ACTIVE){
        return SR_OK;
    }

    scan_dsl_file(sdi);
    struct session_vdev* vdev = sdi->priv;

    vdev->trig_pos = 0;
    vdev->trig_time = 0;
    vdev->cur_block = 0;
    vdev->cur_channel = 0;
    vdev->cur_probes = 0;
    vdev->num_blocks = 0;
    if(sdi->mode == LOGIC)
    {
        vdev->unit_bits = 1;
    }
    else
    {
        vdev->unit_bits = 8;
    }

    vdev->ref_min = 1;
    vdev->ref_max = 255;

    vdev->timebase = SR_NS(500);
    vdev->max_timebase = MAX_TIMEBASE;
    vdev->min_timebase = MIN_TIMEBASE;

    vdev->mstatus.measure_valid = TRUE;
    vdev->archive = NULL;
    vdev->capfile = 0;
    vdev->packet_buffer = NULL;
    vdev->logic_buf = NULL;
    sdi->status = SR_ST_ACTIVE;

    if(vdev->logic_buf != NULL)
    {
        g_safe_free(vdev->logic_buf);
    }
    vdev->logic_buf = g_try_malloc0(LOGIC_BUF_LEN);
    if(vdev->logic_buf == NULL)
    {
        sr_err("%s: vdev->logic_buf malloc failed", __func__);
        return SR_ERR_MALLOC;  
    }
    

    vdev->logic_buf_len = LOGIC_BUF_LEN;

    packet_interval =  g_timer_new();
    run_time =  g_timer_new();
    
    ret = load_virtual_device_session(sdi);
    if (ret != SR_OK)
    {
        sr_err("%s", "Error!Load session file failed.");
        return ret;
    }

    return SR_OK;
}

static int hw_dev_close(struct sr_dev_inst *sdi)
{
    struct session_vdev *vdev;
    int i;
    struct session_packet_buffer *pack_buf;

    if (sdi && sdi->priv)
    {
        vdev = sdi->priv;

        if (vdev->packet_buffer != NULL){
            pack_buf = vdev->packet_buffer;

            g_safe_free(pack_buf->post_buf);

            for (i = 0; i < SESSION_MAX_CHANNEL_COUNT; i++){
                if (pack_buf->block_bufs[i] != NULL){
                    g_safe_free(pack_buf->block_bufs[i]);
                    pack_buf->block_bufs[i] = NULL;
                }
                else{
                    break;
                }
            }
        }

        g_safe_free(vdev->packet_buffer);
        g_safe_free(vdev->logic_buf);
        g_safe_free(vdev->analog_buf);
        g_safe_free(sdi->path);
        g_safe_free(packet_interval);
        g_safe_free(run_time);

        sdi->status = SR_ST_INACTIVE;
        return SR_OK;
    }

    return SR_ERR_CALL_STATUS;
}

static int dev_destroy(struct sr_dev_inst *sdi)
{
    assert(sdi);
    hw_dev_close(sdi);
    sr_dev_inst_free(sdi);
}

static int config_get(int id, GVariant **data, const struct sr_dev_inst *sdi,
                      const struct sr_channel *ch,
                      const struct sr_channel_group *cg)
{
    (void)cg;

    assert(sdi);
    assert(sdi->priv);

    struct session_vdev *vdev = sdi->priv;

    switch (id)
    {
    case SR_CONF_SAMPLERATE:
        *data = g_variant_new_uint64(vdev->samplerate);
        break;
    case SR_CONF_LIMIT_SAMPLES:
        *data = g_variant_new_uint64(vdev->total_samples);
        break;
    case SR_CONF_DEVICE_MODE:
        *data = g_variant_new_int16(sdi->mode);
        break;
    case SR_CONF_DEMO_CHANGE:
        *data = g_variant_new_boolean(is_change);
        break;
    case SR_CONF_INSTANT:
        *data = g_variant_new_boolean(instant);
        break;
    case SR_CONF_PATTERN_MODE:
        if(sdi->mode == LOGIC)
            *data = g_variant_new_string(pattern_strings_logic[sample_generator]);
        else if(sdi->mode == DSO)
            *data = g_variant_new_string(pattern_strings_dso[sample_generator]);
        else
            *data = g_variant_new_string(pattern_strings_analog[sample_generator]);
        break;
    case SR_CONF_MAX_HEIGHT:
        *data = g_variant_new_string(maxHeights[vdev->max_height]);
        break;
    case SR_CONF_MAX_HEIGHT_VALUE:
        *data = g_variant_new_byte(vdev->max_height);
        break;
    case SR_CONF_PROBE_OFFSET:
        if (ch)
            *data = g_variant_new_uint16(ch->offset);
        break;
    case SR_CONF_PROBE_HW_OFFSET:
        if (ch)
            *data = g_variant_new_uint16(ch->hw_offset);
        break;
    case SR_CONF_PROBE_VDIV:
        if (ch)
            *data = g_variant_new_uint64(ch->vdiv);
        break;
    case SR_CONF_PROBE_FACTOR:
        if (ch)
            *data = g_variant_new_uint64(ch->vfactor);
        break;
    case SR_CONF_TIMEBASE:
        *data = g_variant_new_uint64(vdev->timebase);
        break;
    case SR_CONF_MAX_TIMEBASE:
        *data = g_variant_new_uint64(MAX_TIMEBASE);
        break;
    case SR_CONF_MIN_TIMEBASE:
        *data = g_variant_new_uint64(MIN_TIMEBASE);
        break;
    case SR_CONF_PROBE_COUPLING:
        if (ch)
            *data = g_variant_new_byte(ch->coupling);
        break;
    case SR_CONF_TRIGGER_VALUE:
        if (ch)
            *data = g_variant_new_byte(ch->trig_value);
        break;
    case SR_CONF_PROBE_EN:
        if (ch)
            *data = g_variant_new_boolean(ch->enabled);
        break;
    case SR_CONF_MAX_DSO_SAMPLERATE:
        *data = g_variant_new_uint64(SR_MHZ(200));
        break;
    case SR_CONF_MAX_DSO_SAMPLELIMITS:
        *data = g_variant_new_uint64(SR_Kn(20));
        break;
    case SR_CONF_HW_DEPTH:
        switch (sdi->mode)
        {
        case LOGIC:
            if(sample_generator == PATTERN_RANDOM)
                *data = g_variant_new_uint64(LOGIC_HW_DEPTH);
            else
                *data = g_variant_new_uint64(vdev->total_samples);
            break;
        case DSO:
                *data = g_variant_new_uint64(vdev->total_samples);
            break;
        case ANALOG:
                *data = g_variant_new_uint64(ANALOG_HW_DEPTH);
            break;
        default:
            break;
        }
        break;
    case SR_CONF_UNIT_BITS:
         *data = g_variant_new_byte(vdev->unit_bits);
        break;
    case SR_CONF_PROBE_MAP_DEFAULT:
        if (!sdi || !ch)
            return SR_ERR;
        *data = g_variant_new_boolean(ch->map_default);
        break;
    case SR_CONF_PROBE_MAP_UNIT:
        if (!sdi || !ch)
            return SR_ERR;
        *data = g_variant_new_string(ch->map_unit);
        break;
    case SR_CONF_PROBE_MAP_MIN:
        if (!sdi || !ch)
            return SR_ERR;
        *data = g_variant_new_double(ch->map_min);
        break;
    case SR_CONF_PROBE_MAP_MAX:
        if (!sdi || !ch)
            return SR_ERR;
        *data = g_variant_new_double(ch->map_max);
        break;
    case SR_CONF_VLD_CH_NUM:
        *data = g_variant_new_int16(vdev->num_probes);
        break;
    case SR_CONF_HAVE_ZERO:
        *data = g_variant_new_boolean(sample_generator != FALSE);
        break;
    case SR_CONF_LOAD_DECODER:
        *data = g_variant_new_boolean(sample_generator != PATTERN_RANDOM);
        break;
    case SR_CONF_REF_MIN:
        *data = g_variant_new_uint32(vdev->ref_min);
        break;
    case SR_CONF_REF_MAX:
        *data = g_variant_new_uint32(vdev->ref_max);
        break;
    default:
        return SR_ERR_NA;
    }

    return SR_OK;
}

static int config_set(int id, GVariant *data, struct sr_dev_inst *sdi,
                      struct sr_channel *ch,
                      struct sr_channel_group *cg)
{
 (void)cg;

    struct session_vdev *vdev;
    const char *stropt;
    unsigned int i;

    assert(sdi);
    assert(sdi->priv);

    vdev = sdi->priv;

    switch (id)
    {
    case SR_CONF_SAMPLERATE:
        vdev->samplerate = g_variant_get_uint64(data);
        if(sdi->mode == LOGIC && sample_generator >PATTERN_RANDOM)
        {
            samplerates_file[0] = vdev->samplerate;
        }
        sr_dbg("Setting samplerate to %llu.", vdev->samplerate);
        break;
    case SR_CONF_LIMIT_SAMPLES:
        vdev->total_samples = g_variant_get_uint64(data);
        if(sdi->mode == LOGIC && sample_generator >PATTERN_RANDOM)
        {
            samplecounts_file[0] = vdev->total_samples;
        }
        sr_dbg("Setting limit samples to %llu.", vdev->total_samples);
        break;
    case SR_CONF_LIMIT_MSEC:
        break;
    case SR_CONF_DEVICE_MODE:
        sdi->mode = g_variant_get_int16(data);
        switch (sdi->mode)
        {
            case LOGIC:
                if(SR_OK == reset_dsl_path(sdi,sdi->mode ,PATTERN_DEFAULT))
                    sample_generator = PATTERN_DEFAULT;
                else
                    sample_generator = PATTERN_RANDOM;
                break;
            case DSO:
                reset_dsl_path(sdi,sdi->mode ,PATTERN_RANDOM);
                sample_generator = PATTERN_RANDOM;
                break;
            case ANALOG:
                reset_dsl_path(sdi,sdi->mode ,PATTERN_RANDOM);
                sample_generator = PATTERN_RANDOM;
                break;
            default:
                break;
        }
        load_virtual_device_session(sdi);
        break;
    case SR_CONF_PATTERN_MODE:
        stropt = g_variant_get_string(data, NULL);
        sample_generator = get_pattern_mode_index_by_string(sdi->mode , stropt);
        if(SR_OK != reset_dsl_path(sdi,sdi->mode,sample_generator))
        {
            sample_generator = PATTERN_RANDOM;
        }

        sr_dbg("%s: setting pattern to %d",
            __func__, sample_generator);
        break;

    case SR_CONF_MAX_HEIGHT:
        stropt = g_variant_get_string(data, NULL);
        for (i = 0; i < ARRAY_SIZE(maxHeights); i++)
        {
            if (!strcmp(stropt, maxHeights[i]))
            {
                vdev->max_height = i;
                break;
            }
        }
        sr_dbg("%s: setting Signal Max Height to %d",
               __func__, vdev->max_height);
        break;
    case SR_CONF_PROBE_EN:
        ch->enabled = g_variant_get_boolean(data);
        break;
    case SR_CONF_PROBE_VDIV:
        ch->vdiv = g_variant_get_uint64(data);
        if(sdi->mode == DSO)
        {
            if(vdev->packet_buffer)
            {
                vdev->packet_buffer->post_len = 0;
                vdev->packet_buffer->block_read_positions[0] = 0;
                vdev->packet_buffer->block_read_positions[1] = 0;
                vdev->packet_buffer->block_chan_read_pos = 0;
            }
            vdiv_change = TRUE;
        }
        break;
    case SR_CONF_PROBE_FACTOR:
        ch->vfactor = g_variant_get_uint64(data);
        break;
    case SR_CONF_PROBE_OFFSET:
        ch->offset = g_variant_get_uint16(data);
        break;
    case SR_CONF_PROBE_HW_OFFSET:
        ch->hw_offset = g_variant_get_uint16(data);
        break;
    case SR_CONF_TIMEBASE:
        vdev->timebase = g_variant_get_uint64(data);
        if(sdi->mode == DSO)
        {
            g_timer_start(run_time);
            timebase_change = TRUE;
        } 
        sr_dbg("Setting timebase to %llu.", vdev->timebase);
        break;
    case SR_CONF_PROBE_COUPLING:
        ch->coupling = g_variant_get_byte(data);
        if(sdi->mode == ANALOG)
        {
            if(ch->coupling == 0)
                ch->hw_offset = 178;
            else
                ch->hw_offset = 128;
        }
        else if(sdi->mode == DSO)
        {
            if(ch->coupling == 0)
                ch->hw_offset = 178;
            else
                ch->hw_offset = 128;
        }
        break;
    case SR_CONF_TRIGGER_SOURCE:
        break;
    case SR_CONF_TRIGGER_SLOPE:
        break;
    case SR_CONF_TRIGGER_VALUE:
        ch->trig_value = g_variant_get_byte(data);
        break;
    case SR_CONF_PROBE_MAP_DEFAULT:
        ch->map_default = g_variant_get_boolean(data);
        if (ch->map_default) {
            ch->map_unit = probeMapUnits[0];
            ch->map_min = -(ch->vdiv * ch->vfactor * DS_CONF_DSO_VDIVS / 2000.0);
            ch->map_max = ch->vdiv * ch->vfactor * DS_CONF_DSO_VDIVS / 2000.0;
        }
        break;
    case SR_CONF_PROBE_MAP_UNIT:
        ch->map_unit = g_variant_get_string(data, NULL);
        break;
    case SR_CONF_PROBE_MAP_MIN:
        ch->map_min = g_variant_get_double(data);
        break;
    case SR_CONF_PROBE_MAP_MAX:
        ch->map_max = g_variant_get_double(data);
        if(sdi->mode == ANALOG)
        break;
    case SR_CONF_NUM_BLOCKS:
        vdev->num_blocks = g_variant_get_uint64(data);
        sr_dbg("Setting block number to %llu.", vdev->num_blocks);
        break;
    case SR_CONF_CAPTURE_NUM_PROBES:
        vdev->num_probes = g_variant_get_uint64(data);
        break;
    case SR_CONF_INSTANT:
        instant = g_variant_get_boolean(data);
        break;
    case SR_CONF_DEMO_CHANGE:
        is_change = g_variant_get_boolean(data);
        break;
    case SR_CONF_DEMO_INIT:
        load_virtual_device_session(sdi);
        break;
    case SR_CONF_LOOP_MODE:
        vdev->is_loop = g_variant_get_boolean(data);
        sr_info("Set demo loop mode:%d", vdev->is_loop);
        break;
    default:
        sr_err("Unknown capability: %d.", id);
        return SR_ERR_NA;
    }

    return SR_OK;

}

static int config_list(int key, GVariant **data, const struct sr_dev_inst *sdi,
                       const struct sr_channel_group *cg)
{
    (void)cg;

    GVariant *gvar;
    GVariantBuilder gvb;

    (void)sdi;
    struct session_vdev *vdev = sdi->priv;

    switch (key)
    {
    case SR_CONF_DEVICE_OPTIONS:
        *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                                        hwoptions, ARRAY_SIZE(hwoptions) * sizeof(int32_t), TRUE, NULL, NULL);
        break;
    case SR_CONF_DEVICE_SESSIONS:
        *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                                        sessions, ARRAY_SIZE(sessions) * sizeof(int32_t), TRUE, NULL, NULL);
        break;
    case SR_CONF_SAMPLERATE:
        g_variant_builder_init(&gvb, G_VARIANT_TYPE("a{sv}"));
        if(sdi->mode == LOGIC && sample_generator != PATTERN_RANDOM)
        {
             gvar = g_variant_new_from_data(G_VARIANT_TYPE("at"),
                                       samplerates_file, ARRAY_SIZE(samplerates_file) * sizeof(uint64_t), TRUE, NULL, NULL);
        }
        else
        {
            gvar = g_variant_new_from_data(G_VARIANT_TYPE("at"),
                                       samplerates + vdev->samplerates_min_index , (vdev->samplerates_max_index - vdev->samplerates_min_index + 1) * sizeof(uint64_t), TRUE, NULL, NULL);
        }
        g_variant_builder_add(&gvb, "{sv}", "samplerates", gvar);
        *data = g_variant_builder_end(&gvb);
        break;
    case SR_CONF_PATTERN_MODE:
        if(sdi->mode == LOGIC)
        {
            *data = g_variant_new_strv(pattern_strings_logic, pattern_logic_count);
        }
        else if (sdi->mode == DSO)
        {
            *data = g_variant_new_strv(pattern_strings_dso, pattern_dso_count);
        }
        else
        {
            *data = g_variant_new_strv(pattern_strings_analog, pattern_analog_count);
        }
        break;
    case SR_CONF_MAX_HEIGHT:
        *data = g_variant_new_strv(maxHeights, ARRAY_SIZE(maxHeights));
        break;
    case SR_CONF_PROBE_CONFIGS:
        *data = g_variant_new_from_data(G_VARIANT_TYPE("ai"),
                                        probeOptions, ARRAY_SIZE(probeOptions) * sizeof(int32_t), TRUE, NULL, NULL);
        break;
    case SR_CONF_PROBE_VDIV:
        g_variant_builder_init(&gvb, G_VARIANT_TYPE("a{sv}"));
        gvar = g_variant_new_from_data(G_VARIANT_TYPE("at"),
                                       vdivs10to2000, (ARRAY_SIZE(vdivs10to2000)-1) * sizeof(uint64_t), TRUE, NULL, NULL);
        g_variant_builder_add(&gvb, "{sv}", "vdivs", gvar);
        *data = g_variant_builder_end(&gvb);
        break;
    case SR_CONF_PROBE_COUPLING:
        g_variant_builder_init(&gvb, G_VARIANT_TYPE("a{sv}"));
        gvar = g_variant_new_from_data(G_VARIANT_TYPE("ay"),
                probeCoupling, ARRAY_SIZE(probeCoupling)*sizeof(uint8_t), TRUE, NULL, NULL);
        g_variant_builder_add(&gvb, "{sv}", "coupling", gvar);
        *data = g_variant_builder_end(&gvb);
        break;
    case SR_CONF_PROBE_MAP_UNIT:
        *data = g_variant_new_strv(probeMapUnits, ARRAY_SIZE(probeMapUnits));
        break;
    default:
        return SR_ERR_ARG;
    }

    return SR_OK;
}

static int hw_dev_acquisition_start(struct sr_dev_inst *sdi,
        void *cb_data)
{

     (void)cb_data;

    struct session_vdev *vdev;
    struct sr_datafeed_packet packet;
    int ret;
    GSList *l;
    struct sr_channel *probe;

    assert(sdi);
    assert(sdi->priv);


    vdev = sdi->priv;
    vdev->enabled_probes = 0;
    packet.status = SR_PKT_OK;

    vdev->cur_block = 0;
    vdev->cur_channel = 0;

    if(sample_generator != PATTERN_RANDOM)
    {
        if (vdev->archive != NULL)
        {
            sr_err("history archive is not closed.");
        }

        sr_dbg("Opening archive file %s", sdi->path);

        vdev->archive = unzOpen64(sdi->path);

        if (NULL == vdev->archive)
        {
            sr_err("Failed to open session file '%s': "
                "zip error %d\n",
                sdi->path, ret);
            return SR_ERR;
        }
    }

    if (sdi->mode == LOGIC)
        vdev->cur_channel = 0;
    else
        vdev->cur_channel = vdev->num_probes - 1;

    for (l = sdi->channels; l; l = l->next)
    {
        probe = l->data;
        if (probe->enabled)
            vdev->enabled_probes++;
    }

    /* Send header packet to the session bus. */
    std_session_send_df_header(sdi, LOG_PREFIX);

    /* Send trigger packet to the session bus */
    if (vdev->trig_pos != 0)
    {
        struct ds_trigger_pos session_trigger;
        if (sdi->mode == DSO)
            session_trigger.real_pos = vdev->trig_pos * vdev->enabled_probes / vdev->num_probes;
        else
            session_trigger.real_pos = vdev->trig_pos;
        packet.type = SR_DF_TRIGGER;
        packet.payload = &session_trigger;
        ds_data_forward(sdi, &packet);
    }


    if(sdi->mode == LOGIC)
    {
        reset_enabled_probe_num(sdi);
        post_data_len = 0;
        packet_len = LOGIC_PACKET_LEN(vdev->samplerate);
        packet_time = LOGIC_PACKET_TIME(LOGIC_PACKET_NUM_PER_SEC);
        if(packet_len < LOGIC_MIN_PACKET_LEN)
        {
            packet_len = LOGIC_MIN_PACKET_LEN;
            packet_time = LOGIC_MIN_PACKET_TIME(vdev->samplerate);
        }

        
        if(sample_generator == PATTERN_RANDOM)
        {
            init_random_data(vdev,sdi);
            sr_session_source_add(-1, 0, 0, receive_data_logic, sdi);
        }
        else
        {
            sr_session_source_add(-1, 0, 0, receive_data_logic_decoder, sdi);
        }
    }
    else if(sdi->mode == DSO)
    {
        vdiv_change = TRUE;
        if(instant){
            post_data_len = 0;

            gdouble total_time = vdev->timebase /(gdouble)SR_SEC(1)*(gdouble)10;
            uint64_t post_data_per_sec = DSO_PACKET_LEN/total_time;
            packet_len = 2;
            uint64_t packet_num = post_data_per_sec/packet_len;
            packet_time = SEC/(gdouble)packet_num;
        }
        else{
            packet_time = DSO_PACKET_TIME;
        }
        
        g_timer_start(run_time);
        
        sr_session_source_add(-1, 0, 0, receive_data_dso, sdi);
    }
    else if(sdi->mode == ANALOG)
    {
        load_data = TRUE;
        packet_len = ANALOG_PACKET_LEN(vdev->samplerate);

        if(packet_len < ANALOG_MIN_PACKET_LEN)
        {
            packet_len = ANALOG_MIN_PACKET_LEN;
            packet_time = ANALOG_PACKET_TIME(ANALOG_MIN_PACKET_NUM(vdev->samplerate));
        }
        else
        {
            if (packet_len % ANALOG_PACKET_ALIGN != 0)
            {
                packet_len += 1;
            }
            packet_time = ANALOG_PACKET_TIME(ANALOG_PACKET_NUM_PER_SEC);
        }
        if(sample_generator == PATTERN_RANDOM)
            init_analog_random_data(vdev);
            
        vdev->analog_read_pos = 0;       

        sr_session_source_add(-1, 0, 0, receive_data_analog, sdi);
    }

    return SR_OK;
}

static int hw_dev_acquisition_stop(const struct sr_dev_inst *sdi, void *cb_data)
{
    struct session_vdev *vdev = sdi->priv;
    struct sr_datafeed_packet packet;
    packet.status = SR_PKT_OK;

    if(sdi->mode != LOGIC)
    {
        packet.type = SR_DF_END;
        ds_data_forward(sdi, &packet);
        close_archive(vdev);
    }

    return SR_OK;
}

static int hw_dev_status_get(const struct sr_dev_inst *sdi, struct sr_status *status, gboolean prg)
{
    (void)prg;

    struct session_vdev *vdev;

    if (sdi)
    {
        vdev = sdi->priv;
        *status = vdev->mstatus;
        return SR_OK;
    }
    else
    {
        return SR_ERR;
    }
}

static int receive_data_logic(int fd, int revents, const struct sr_dev_inst *sdi)
{
    assert(sdi);
    assert(sdi->priv);

    (void)fd;
    (void)revents;

    struct session_vdev *vdev = sdi->priv;
    struct sr_datafeed_packet packet;
    struct sr_datafeed_logic logic;

    int bToEnd;
    int chan_num;

    bToEnd = 0;

    vdev = sdi->priv;

    chan_num = enabled_probe_num;

    if (chan_num < 1){
        sr_err("%s: channel count < 1.", __func__);
        return SR_ERR_ARG;
    }
    if (chan_num > SESSION_MAX_CHANNEL_COUNT){
        sr_err("%s: channel count is to big.", __func__);
        return SR_ERR_ARG;
    }

     g_timer_start(packet_interval);

    if(!vdev->is_loop){
        if(post_data_len >= vdev->total_samples/8){
            bToEnd = 1;
        }
    }

    if(!bToEnd)
    {
        packet.status = SR_PKT_OK;
        packet.type = SR_DF_LOGIC;
        packet.payload = &logic;
        logic.format = LA_CROSS_DATA;
        logic.index = 0;
        logic.order = 0;
        logic.length = chan_num * packet_len;

        if(!vdev->is_loop){
            post_data_len += logic.length / enabled_probe_num;
            if(post_data_len >= vdev->total_samples/8){
                get_last_packet_len(&logic,vdev);
            }
        }
       
        uint64_t random = vdev->logic_buf_len - logic.length;
        random = rand() % random;
        int index = enabled_probe_num * 8;
        random = floor(random/index)*index;
        logic.data = vdev->logic_buf + random;
        ds_data_forward(sdi, &packet);
        delay_time();
    }

    if (bToEnd || revents == -1)
    {
        packet.type = SR_DF_END;
        ds_data_forward(sdi, &packet);
        sr_session_source_remove(-1);
    }

    return TRUE;
}

static int receive_data_logic_decoder(int fd, int revents, const struct sr_dev_inst *sdi)
{
    struct session_vdev *vdev = NULL;
    struct sr_datafeed_packet packet;
    struct sr_datafeed_logic logic;
    struct sr_datafeed_dso dso;

    int ret;
    char file_name[32];
    int channel;
    int ch_index, malloc_chan_index;
    struct session_packet_buffer *pack_buffer;
    unz_file_info64 fileInfo;
    char szFilePath[15];
    int bToEnd;
    int read_chan_index;
    int chan_num;
    uint8_t *p_wr;
    uint8_t *p_rd;
    int byte_align;
    int dir_index;
    int bCheckFile;
    const int file_max_channel_count = 128;

    assert(sdi);
    assert(sdi->priv);

    (void)fd;
    (void)revents;

    sr_detail("Feed chunk.");

    ret = 0;
    bToEnd = 0;
    packet.status = SR_PKT_OK;

    vdev = sdi->priv;

    assert(vdev->unit_bits > 0);
    assert(vdev->archive);

    chan_num = enabled_probe_num;
    byte_align = sdi->mode == LOGIC ? 8 : 1;

    if (chan_num < 1){
        sr_err("%s: channel count < 1.", __func__);
        return SR_ERR_ARG;
    }
    if (chan_num > SESSION_MAX_CHANNEL_COUNT){
        sr_err("%s: channel count is to big.", __func__);
        return SR_ERR_ARG;
    }

    g_timer_start(packet_interval);

    // Make buffer
    if (vdev->packet_buffer == NULL){
        vdev->cur_block = 0;

        vdev->packet_buffer = g_try_malloc0(sizeof(struct session_packet_buffer));
        if (vdev->packet_buffer == NULL){
            sr_err("%s: vdev->packet_buffer malloc failed", __func__);
            return SR_ERR_MALLOC;
        }

        for (ch_index = 0; ch_index <= chan_num; ch_index++){
            vdev->packet_buffer->block_bufs[ch_index] = NULL;
            vdev->packet_buffer->block_read_positions[ch_index] = 0;
        }

        vdev->packet_buffer->post_buf_len = chan_num * packet_len;

        vdev->packet_buffer->post_buf = g_try_malloc0(vdev->packet_buffer->post_buf_len + 1);
        if (vdev->packet_buffer->post_buf == NULL){
            sr_err("%s: vdev->packet_buffer->post_buf malloc failed", __func__);
            return SR_ERR_MALLOC;
        }

        pack_buffer = vdev->packet_buffer;
        pack_buffer->post_len;
        pack_buffer->block_buf_len = 0;
        pack_buffer->block_data_len = 0;
        pack_buffer->block_chan_read_pos = 0;

        max_probe_num = chan_num;
    }
    pack_buffer = vdev->packet_buffer;

    if(chan_num != max_probe_num)
    {
        for(ch_index = 0 ;ch_index < chan_num; ch_index++)
        {
            if(pack_buffer->block_bufs[ch_index] != NULL)
            {
                g_safe_free(pack_buffer->block_bufs[ch_index]);
            }
            pack_buffer->block_bufs[ch_index] = NULL;
            pack_buffer->block_read_positions[ch_index] = 0;
        }
        pack_buffer->block_buf_len = 0;
        pack_buffer->block_data_len = 0;
        pack_buffer->block_chan_read_pos = 0;
        max_probe_num = chan_num;
    }

    if(pack_buffer->post_buf_len != chan_num * packet_len)
    {
        pack_buffer->post_buf_len = chan_num * packet_len;
        if(pack_buffer->post_buf != NULL)
        {
            g_safe_free(pack_buffer->post_buf);
        }

        pack_buffer->post_buf = g_try_malloc0(pack_buffer->post_buf_len);
        if (pack_buffer->post_buf == NULL)
        {
            sr_err("%s: pack_buffer->post_buf malloc failed", __func__);
            return SR_ERR_MALLOC;
        }

        pack_buffer->post_len = 0;
    }

    // Make packet.
    read_chan_index = 0;
    dir_index = 0;

    while (pack_buffer->post_len < pack_buffer->post_buf_len)
    {
        if (pack_buffer->block_chan_read_pos >= pack_buffer->block_data_len)
        {
            if(vdev->cur_block >= vdev->num_blocks){
                bToEnd = 1;
                break;
            }

            for (ch_index = 0; ch_index < chan_num; ch_index++)
            {
                bCheckFile = 0;

                while (1)
                {
                    snprintf(file_name, sizeof(file_name)-1, "L-%d/%d", dir_index++, vdev->cur_block);

                    if (unzLocateFile(vdev->archive, file_name, 0) == UNZ_OK){
                        bCheckFile = 1;
                        break;
                    }
                    else if (dir_index > file_max_channel_count){
                        break;
                    }
                }

                if (!bCheckFile)
                {
                    sr_err("cant't locate zip inner file:\"%s\"", file_name);
                    send_error_packet(sdi, vdev, &packet);
                    return FALSE;
                }

                if (unzGetCurrentFileInfo64(vdev->archive, &fileInfo, szFilePath,
                                    sizeof(szFilePath), NULL, 0, NULL, 0) != UNZ_OK)
                {
                    sr_err("%s: unzGetCurrentFileInfo64 error.", __func__);
                    send_error_packet(sdi, vdev, &packet);
                    return FALSE;
                }

                if (ch_index == 0){
                    pack_buffer->block_data_len = fileInfo.uncompressed_size;

                    if (pack_buffer->block_data_len > pack_buffer->block_buf_len)
                    {
                        for (malloc_chan_index = 0; malloc_chan_index < chan_num; malloc_chan_index++){
                            // Release the old buffer.
                            if (pack_buffer->block_bufs[malloc_chan_index] != NULL){
                                g_safe_free(pack_buffer->block_bufs[malloc_chan_index]);
                                pack_buffer->block_bufs[malloc_chan_index] = NULL;
                            }

                            pack_buffer->block_bufs[malloc_chan_index] = g_try_malloc0(pack_buffer->block_data_len + 1);
                            if (pack_buffer->block_bufs[malloc_chan_index] == NULL){
                                sr_err("%s: block buffer malloc failed", __func__);
                                send_error_packet(sdi, vdev, &packet);
                                return FALSE;
                            }
                            pack_buffer->block_buf_len = pack_buffer->block_data_len;
                        }
                    }
                }
                else
                {
                    if (pack_buffer->block_data_len != fileInfo.uncompressed_size){
                        sr_err("The block size is not coincident:%s", file_name);
                        send_error_packet(sdi, vdev, &packet);
                        return FALSE;
                    }
                }

                // Read the data to buffer.
                if (unzOpenCurrentFile(vdev->archive) != UNZ_OK)
                {
                    sr_err("cant't open zip inner file:\"%s\"", file_name);
                    send_error_packet(sdi, vdev, &packet);
                    return FALSE;
                }

                ret = unzReadCurrentFile(vdev->archive, pack_buffer->block_bufs[ch_index], pack_buffer->block_data_len);
                if (-1 == ret)
                {
                    sr_err("read zip inner file error:\"%s\"", file_name);
                    send_error_packet(sdi, vdev, &packet);
                    return FALSE;
                }
                unzCloseCurrentFile(vdev->archive);
                pack_buffer->block_read_positions[ch_index] = 0; // Reset the read position.
            }
            vdev->cur_block++;
            pack_buffer->block_chan_read_pos = 0;
        }

        p_wr = (uint8_t*)pack_buffer->post_buf + pack_buffer->post_len;
        p_rd = (uint8_t*)pack_buffer->block_bufs[read_chan_index] + pack_buffer->block_read_positions[read_chan_index];
        *p_wr = *p_rd;

        pack_buffer->post_len++;
        pack_buffer->block_read_positions[read_chan_index]++;

        if (pack_buffer->block_read_positions[read_chan_index] % byte_align == 0
                || pack_buffer->block_read_positions[read_chan_index] == pack_buffer->block_data_len)
        {
            read_chan_index++;

            if (pack_buffer->block_read_positions[read_chan_index] == pack_buffer->block_data_len){
                sr_info("Block read end.");
                if (vdev->cur_block < vdev->num_blocks){
                    sr_err("%s", "The block data is not align.");
                    break;
                }
            }

            // Each channel's data is ready.
            if (read_chan_index == chan_num){
                read_chan_index = 0;
                pack_buffer->block_chan_read_pos += byte_align;
            }
        }
    }

    if (pack_buffer->post_len >= byte_align * chan_num)
    {
        packet.type = SR_DF_LOGIC;
        packet.payload = &logic;
        logic.format = LA_CROSS_DATA;
        logic.index = 0;
        logic.order = 0;
        logic.length = pack_buffer->post_len;
        logic.data = pack_buffer->post_buf;

        delay_time();
        ds_data_forward(sdi, &packet);
        
        pack_buffer->post_len = 0;
    }

    if (bToEnd || revents == -1)
    {
        packet.type = SR_DF_END;
        ds_data_forward(sdi, &packet);
        sr_session_source_remove(-1);
        close_archive(vdev);
    }

    return TRUE;
}

int get_bit(uint64_t timebase)
{
    if(timebase < SR_MS(1) && timebase >= SR_US(20))
        return SR_MS(1)/timebase*2;
    else if(timebase < SR_MS(200) && timebase >= SR_MS(1))
        return SR_MS(200)/timebase*2;
    else if(timebase <= SR_SEC(10) && timebase > SR_MS(200))
        return SR_SEC(40)/timebase*2;
    else
        return 200;
}

static int receive_data_dso(int fd, int revents, const struct sr_dev_inst *sdi)
{
 struct session_vdev *vdev = NULL;
    struct sr_datafeed_packet packet;
    struct sr_datafeed_dso dso;
    struct sr_channel *probe;

    int ret;
    char file_name[32];
    int channel;
    int ch_index, malloc_chan_index;
    struct session_packet_buffer *pack_buffer;
    unz_file_info64 fileInfo;
    char szFilePath[15];
    int bToEnd;
    int read_chan_index;
    int chan_num;
    uint8_t *p_wr;
    uint8_t *p_rd;
    int byte_align;
    int dir_index;
    int bCheckFile;
    const int file_max_channel_count = 128;

    uint16_t tem;
    uint8_t val = 0;
    uint64_t vdiv;

    assert(sdi);
    assert(sdi->priv);

    (void)fd;
    (void)revents;

    sr_detail("Feed chunk.");

    ret = 0;
    bToEnd = 0;
    packet.status = SR_PKT_OK;

    vdev = sdi->priv;

    assert(vdev->unit_bits > 0);
    if(sample_generator != PATTERN_RANDOM)
    {
        assert(vdev->archive);
    }

    chan_num = vdev->num_probes;
    byte_align = sdi->mode == LOGIC ? 8 : 1;

    if (chan_num < 1){
        sr_err("%s: channel count < 1.", __func__);
        return SR_ERR_ARG;
    }
    if (chan_num > SESSION_MAX_CHANNEL_COUNT){
        sr_err("%s: channel count is to big.", __func__);
        return SR_ERR_ARG;
    }

    // Make buffer
    if (vdev->packet_buffer == NULL){
        vdev->cur_block = 0;

        vdev->packet_buffer = g_try_malloc0(sizeof(struct session_packet_buffer));
        if (vdev->packet_buffer == NULL){
            sr_err("%s: vdev->packet_buffer malloc failed", __func__);
            return SR_ERR_MALLOC;
        }

        for (ch_index = 0; ch_index <= chan_num; ch_index++){
            vdev->packet_buffer->block_bufs[ch_index] = NULL;
            vdev->packet_buffer->block_read_positions[ch_index] = 0;
        }

        vdev->packet_buffer->post_buf_len = chan_num * 10000;

        vdev->packet_buffer->post_buf = g_try_malloc0(vdev->packet_buffer->post_buf_len);
        if (vdev->packet_buffer->post_buf == NULL){
            sr_err("%s: vdev->packet_buffer->post_buf malloc failed", __func__);
            return SR_ERR_MALLOC;
        }

        pack_buffer = vdev->packet_buffer;
        pack_buffer->post_len;
        pack_buffer->block_buf_len = 0;
        pack_buffer->block_data_len = 0;
        pack_buffer->block_chan_read_pos = 0;
    }
    pack_buffer = vdev->packet_buffer;
    if(pack_buffer->post_buf_len != chan_num * 10000)
    {
        vdev->packet_buffer->post_buf_len = chan_num * 10000;
        if(pack_buffer->post_buf != NULL)
        {
            g_safe_free(pack_buffer->post_buf);
        }
        pack_buffer->post_buf = g_try_malloc0(pack_buffer->post_buf_len);
        if (pack_buffer->post_buf == NULL)
        {
            sr_err("%s: pack_buffer->post_buf malloc failed", __func__);
            return SR_ERR_MALLOC;
        }

        pack_buffer->post_len = 0;
    }

    if(chan_num != max_probe_num)
    {
        for(ch_index = 0 ;ch_index < chan_num; ch_index++)
        {
            if(pack_buffer->block_bufs[ch_index] != NULL)
            {
                g_safe_free(pack_buffer->block_bufs[ch_index]);
            }
            pack_buffer->block_bufs[ch_index] = NULL;
            pack_buffer->block_read_positions[ch_index] = 0;
        }
        pack_buffer->block_buf_len = 0;
        pack_buffer->block_data_len = 0;
        pack_buffer->block_chan_read_pos = 0;
        max_probe_num = chan_num;
    }

    // Make packet.
    read_chan_index = 0;
    dir_index = 0;

    if(vdiv_change || timebase_change)
    {
        if(sample_generator == PATTERN_RANDOM)
        {
            for(int i = 0 ; i < pack_buffer->post_buf_len ;i++)
            {
                if(i % 2 == 0)
                    *(uint8_t*)(pack_buffer->post_buf + i) = DSO_RANDOM_DATA;
                else
                    *(uint8_t*)(pack_buffer->post_buf + i) = *(uint8_t*)(pack_buffer->post_buf + i -1);
            }
            pack_buffer->post_len = pack_buffer->post_buf_len;
        }
        else
        {
            pack_buffer->post_len = 0;
            while (pack_buffer->post_len < pack_buffer->post_buf_len)
            {
                if (pack_buffer->block_chan_read_pos >= pack_buffer->block_data_len)
                {
                    if (vdev->cur_block >= vdev->num_blocks){
                        vdev->cur_block = 0;
                    }

                    for (ch_index = 0; ch_index < chan_num; ch_index++)
                    {
                        bCheckFile = 0;

                        while (1)
                        {
                            if (sdi->mode  == LOGIC)
                                snprintf(file_name, sizeof(file_name)-1, "L-%d/%d", dir_index++, vdev->cur_block);
                            else if (sdi->mode == DSO)
                                snprintf(file_name, sizeof(file_name)-1, "O-%d/0", dir_index++);

                            if (unzLocateFile(vdev->archive, file_name, 0) == UNZ_OK){
                                bCheckFile = 1;
                                break;
                            }
                            else if (dir_index > file_max_channel_count){
                                break;
                            }
                        }

                        if (!bCheckFile)
                        {
                            sr_err("cant't locate zip inner file:\"%s\"", file_name);
                            send_error_packet(sdi, vdev, &packet);
                            return FALSE;
                        }

                        if (unzGetCurrentFileInfo64(vdev->archive, &fileInfo, szFilePath,
                                            sizeof(szFilePath), NULL, 0, NULL, 0) != UNZ_OK)
                        {
                            sr_err("%s: unzGetCurrentFileInfo64 error.", __func__);
                            send_error_packet(sdi, vdev, &packet);
                            return FALSE;
                        }

                        if (ch_index == 0){
                            pack_buffer->block_data_len = fileInfo.uncompressed_size;

                            if (pack_buffer->block_data_len > pack_buffer->block_buf_len)
                            {
                                for (malloc_chan_index = 0; malloc_chan_index < chan_num; malloc_chan_index++){
                                    // Release the old buffer.
                                    if (pack_buffer->block_bufs[malloc_chan_index] != NULL){
                                        g_safe_free(pack_buffer->block_bufs[malloc_chan_index]);
                                        pack_buffer->block_bufs[malloc_chan_index] = NULL;
                                    }

                                    pack_buffer->block_bufs[malloc_chan_index] = g_try_malloc0(pack_buffer->block_data_len + 1);
                                    if (pack_buffer->block_bufs[malloc_chan_index] == NULL){
                                        sr_err("%s: block buffer malloc failed", __func__);
                                        send_error_packet(sdi, vdev, &packet);
                                        return FALSE;
                                    }
                                    pack_buffer->block_buf_len = pack_buffer->block_data_len;
                                }
                            }
                        }
                        else
                        {
                            if (pack_buffer->block_data_len != fileInfo.uncompressed_size){
                                sr_err("The block size is not coincident:%s", file_name);
                                send_error_packet(sdi, vdev, &packet);
                                return FALSE;
                            }
                        }

                        // Read the data to buffer.
                        if (unzOpenCurrentFile(vdev->archive) != UNZ_OK)
                        {
                            sr_err("cant't open zip inner file:\"%s\"", file_name);
                            send_error_packet(sdi, vdev, &packet);
                            return FALSE;
                        }

                        ret = unzReadCurrentFile(vdev->archive, pack_buffer->block_bufs[ch_index], pack_buffer->block_data_len);
                        if (-1 == ret)
                        {
                            sr_err("read zip inner file error:\"%s\"", file_name);
                            send_error_packet(sdi, vdev, &packet);
                            return FALSE;
                        }
                        unzCloseCurrentFile(vdev->archive);
                        pack_buffer->block_read_positions[ch_index] = 0; // Reset the read position.
                    }
                    vdev->cur_block++;
                    pack_buffer->block_chan_read_pos = 0;
                }

                p_wr = (uint8_t*)pack_buffer->post_buf + pack_buffer->post_len;
                p_rd = (uint8_t*)pack_buffer->block_bufs[read_chan_index] + pack_buffer->block_read_positions[read_chan_index];
                *p_wr = *p_rd;

                pack_buffer->post_len++;
                pack_buffer->block_read_positions[read_chan_index]++;

                if (pack_buffer->block_read_positions[read_chan_index] % byte_align == 0
                        || pack_buffer->block_read_positions[read_chan_index] == pack_buffer->block_data_len)
                {
                    read_chan_index++;

                    if (pack_buffer->block_read_positions[read_chan_index] == pack_buffer->block_data_len){
                        sr_info("Block read end.");
                        if (vdev->cur_block < vdev->num_blocks){
                            sr_err("%s", "The block data is not align.");
                            break;
                        }
                    }

                    // Each channel's data is ready.
                    if (read_chan_index == chan_num){
                        read_chan_index = 0;
                        pack_buffer->block_chan_read_pos += byte_align;
                    }
                }
            }
        }

        if(timebase_change || vdiv_change)
        {
            int index;
            int bit = get_bit(vdev->timebase); 

            if(sample_generator!= PATTERN_RANDOM)
            {
                void* tmp_buf = g_try_malloc0(bit);
                if(tmp_buf == NULL)
                {
                    sr_err("%s: tmp_buf malloc failed", __func__);
                    return SR_ERR_MALLOC;
                }
                for(int i = 0 ; i < bit ; i++)
                {
                    if(i%2 == 0)
                    {
                        if(bit == 10)
                            index = i * 16;
                        else
                            index = i * 100 / (bit / 2);
                    }
                    else
                    {
                        
                        if(bit == 10)
                            index = (i-1) * 16 + 1;
                        else
                            index = (i-1) * 100 / (bit / 2) + 1;
                    }
                    *((uint8_t*)tmp_buf+ i) = *((uint8_t*)pack_buffer->post_buf + index + 30);
                }

                for(int i = 0 ; i < DSO_PACKET_LEN/bit ; i++)
                {
                    memcpy(pack_buffer->post_buf+i*bit,tmp_buf,bit);
                }

                g_safe_free(tmp_buf);
            }

            for(int i = 0 ; i < pack_buffer->post_buf_len; i++)
            {
                tem = 0;
                if(i % 2 == 0)
                    probe = g_slist_nth(sdi->channels, 0)->data;
                else
                    probe = g_slist_nth(sdi->channels, 1)->data;
                if(!probe->enabled){
                    if(i % 2 == 0)
                        probe = g_slist_nth(sdi->channels, 1)->data;
                    else
                        probe = g_slist_nth(sdi->channels, 0)->data;
                }
                vdiv = probe->vdiv;


                uint8_t temp_val = *((uint8_t*)pack_buffer->post_buf + i);
                if(temp_val > DSO_MID_VAL)
                {
                    val = temp_val - DSO_MID_VAL;
                    tem = val * DSO_DEFAULT_VDIV / vdiv;
                    if(tem >= DSO_MID_VAL)
                        temp_val = DSO_MIN_VAL;
                    else
                        temp_val = DSO_MID_VAL + tem;
                }
                else if(temp_val < DSO_MID_VAL)
                {
                    val = DSO_MID_VAL - temp_val;
                    tem =  val * DSO_DEFAULT_VDIV / vdiv;

                    if(tem >= DSO_MID_VAL)
                        temp_val = DSO_MAX_VAL;
                    else
                        temp_val = DSO_MID_VAL - tem;
                }
                *((uint8_t*)pack_buffer->post_buf + i) = temp_val;
            }
            timebase_change = FALSE;
            vdiv_change = FALSE;
        }
    }

    gdouble total_time = vdev->timebase /(gdouble)SR_SEC(1)*(gdouble)10;
    gdouble total_time_elapsed = g_timer_elapsed(run_time, NULL);

    if(!instant)
    {
        if (total_time_elapsed < total_time)
        {
            gdouble percent = total_time_elapsed / total_time;
            int buf_len = percent* DSO_PACKET_LEN;
            if(buf_len %2 != 0)
                buf_len +=1;
            pack_buffer->post_len = buf_len;
        }
        else
        {
            uint8_t top0;
            uint8_t top1;
            if(sample_generator == PATTERN_RANDOM)
            {
                top0 = *((uint8_t*)pack_buffer->post_buf + pack_buffer->post_buf_len -2);
                top1 = *((uint8_t*)pack_buffer->post_buf + pack_buffer->post_buf_len -1);
            }
            else
            {
                top0 = *((uint8_t*)pack_buffer->post_buf + get_bit(vdev->timebase) -2);
                top1 = *((uint8_t*)pack_buffer->post_buf + get_bit(vdev->timebase) -1);
            }

            for(int i = pack_buffer->post_len -1; i > 1; i -= 2){
                *((uint8_t*)pack_buffer->post_buf + i) = *((uint8_t*)pack_buffer->post_buf + i - 2);
            }

            for(int i = pack_buffer->post_len -2; i > 0; i -= 2){
                *((uint8_t*)pack_buffer->post_buf + i) = *((uint8_t*)pack_buffer->post_buf + i - 2);
            }

            *(uint8_t*)pack_buffer->post_buf = top0;
            *((uint8_t*)pack_buffer->post_buf + 1)= top1;
            pack_buffer->post_len = DSO_PACKET_LEN;
        }
        
    }
    else
    {
        if(DSO_PACKET_LEN >post_data_len)
        {
            pack_buffer->post_len = packet_len;
            post_data_len += packet_len;
        }
        else
        {
            bToEnd = 1;
            instant = FALSE;
        }
    }

    if (pack_buffer->post_len >= byte_align * chan_num && !bToEnd)
    {
        packet.type = SR_DF_DSO;
        packet.payload = &dso;
        dso.probes = sdi->channels;
        dso.mq = SR_MQ_VOLTAGE;
        dso.unit = SR_UNIT_VOLT;
        dso.mqflags = SR_MQFLAG_AC;
        dso.num_samples = pack_buffer->post_len / chan_num;
        if (instant)
            dso.data = pack_buffer->post_buf+post_data_len;
        else
            dso.data = pack_buffer->post_buf;
        
        delay_time();
        g_timer_start(packet_interval);
        // Send data back.
        ds_data_forward(sdi, &packet);
    }

    if (bToEnd || revents == -1)
    {
        packet.type = SR_DF_END;
        ds_data_forward(sdi, &packet);
        sr_session_source_remove(-1);
    }

    return TRUE;
}


static int receive_data_analog(int fd, int revents, const struct sr_dev_inst *sdi)
{
    struct session_vdev *vdev = sdi->priv;
    struct sr_datafeed_packet packet;
    struct sr_datafeed_analog analog;
    struct sr_channel *probe = NULL;

    char file_name[32];
    int ret;

    assert(sdi);
    assert(sdi->priv);

    (void)fd;
    (void)revents;

    sr_detail("Feed chunk.");

    packet.status = SR_PKT_OK;

    assert(vdev->unit_bits > 0);

    g_timer_start(packet_interval);

    if(load_data)
    {
        if(sample_generator != PATTERN_RANDOM)
        {
            vdev->analog_buf_len = 0;

            void* analog_data = g_try_malloc0(ANALOG_DATA_LEN_PER_CYCLE);
            if(analog_data == NULL)
            {
                sr_err("%s:analog_data malloc failed",__func__);
                return SR_ERR_MALLOC;
            }

            snprintf(file_name, sizeof(file_name)-1, "%s-%d/%d", "A",
                         0, 0);

            if (unzLocateFile(vdev->archive, file_name, 0) != UNZ_OK)
            {
                sr_err("cant't locate zip inner file:\"%s\"", file_name);
                send_error_packet(sdi, vdev, &packet);
                return FALSE;
            }
            if (unzOpenCurrentFile(vdev->archive) != UNZ_OK)
            {
                sr_err("cant't open zip inner file:\"%s\"", file_name);
                send_error_packet(sdi, vdev, &packet);
                return FALSE;
            }

            ret = unzReadCurrentFile(vdev->archive, analog_data, ANALOG_DATA_LEN_PER_CYCLE);
            if (-1 == ret)
            {
                sr_err("read zip inner file error:\"%s\"", file_name);
                send_error_packet(sdi, vdev, &packet);
                return FALSE;
            }

            uint64_t total_buf_len = ANALOG_CYCLE_RATIO * vdev->total_samples * ANALOG_PROBE_NUM;
            if(total_buf_len % ANALOG_DATA_LEN_PER_CYCLE != 0)
            {
                total_buf_len = total_buf_len / ANALOG_DATA_LEN_PER_CYCLE * ANALOG_DATA_LEN_PER_CYCLE;
            }

            if(vdev->analog_buf != NULL)
            {
                g_safe_free(vdev->analog_buf);
                vdev->analog_buf = NULL;
            }
            vdev->analog_buf = (g_try_malloc0(total_buf_len));
            if (vdev->analog_buf == NULL)
            {
                sr_err("%s: vdev->analog_buf malloc failed", __func__);
                return SR_ERR_MALLOC;
            }
            vdev->analog_buf_len = total_buf_len;
            uint64_t per_block_after_expend = total_buf_len / ANALOG_DATA_LEN_PER_CYCLE;

            probe = g_slist_nth(sdi->channels, 0)->data;
            uint64_t p0_vdiv = probe->vdiv;
            probe = g_slist_nth(sdi->channels, 1)->data;
            uint64_t p1_vdiv = probe->vdiv;
            uint64_t vdiv;
            uint8_t val = 0;
            uint16_t tem;
            uint64_t cur_l = 0;
            for(int i = 0 ; i < ANALOG_DATA_LEN_PER_CYCLE;i++)
            {
                if(i % 2 == 0)
                    vdiv = p0_vdiv;
                else
                    vdiv = p1_vdiv;
                tem = 0;

                uint8_t temp_value = *((uint8_t*)analog_data + i);

                if(temp_value > ANALOG_MID_VAL){
                    val = temp_value - ANALOG_MID_VAL;
                    tem = val * ANALOG_DEFAULT_VDIV / vdiv;
                    if(tem >= ANALOG_MID_VAL)
                        temp_value = ANALOG_MIN_VAL;
                    else
                        temp_value = ANALOG_MID_VAL + tem;
                }
                else if(temp_value < ANALOG_MID_VAL)
                {
                    val = ANALOG_MID_VAL - temp_value;
                    tem =  val * ANALOG_DEFAULT_VDIV / vdiv;

                    if(tem >= ANALOG_MID_VAL)
                        temp_value = ANALOG_MAX_VAL;
                    else
                        temp_value = ANALOG_MID_VAL - tem;
                }

                for(int j = 0 ; j <per_block_after_expend ;j++)
                {
                    if(i % 2 == 0)
                    {
                        cur_l = i * per_block_after_expend + j * 2;
                    }
                    else
                    {
                        cur_l = 1 + (i - 1) * per_block_after_expend + j * 2;
                    }
                    memset(vdev->analog_buf + cur_l,temp_value,1);
                }
            }
            g_safe_free(analog_data);
        }
        load_data = FALSE;
    }

    void* buf = g_try_malloc0(packet_len);
    if(buf == NULL)
    {
        sr_err("%s: buf malloc failed", __func__);
        return SR_ERR_MALLOC;
    }
    if(vdev->analog_read_pos + packet_len >= vdev->analog_buf_len - 1 )
    {
        uint64_t back_len = vdev->analog_buf_len - vdev->analog_read_pos;
        for (int i = 0; i < back_len; i++)
        {
            uint8_t temp_val = *((uint8_t*)vdev->analog_buf + vdev->analog_read_pos + i);
            memset(buf + i,temp_val,1);
        }

        uint64_t front_len = packet_len - back_len;
        for (int i = 0; i < front_len; i++)
        {
            uint8_t temp_val = *((uint8_t*)vdev->analog_buf + i);
            memset(buf + back_len + i,temp_val,1);
        }
        vdev->analog_read_pos = front_len;
    }
    else
    {
        memcpy(buf,vdev->analog_buf + vdev->analog_read_pos,packet_len);
        vdev->analog_read_pos += packet_len;
    }

    packet.type = SR_DF_ANALOG;
    packet.payload = &analog;
    analog.probes = sdi->channels;
    analog.num_samples = packet_len / vdev->num_probes;
    analog.unit_bits = vdev->unit_bits;
    analog.mq = SR_MQ_VOLTAGE;
    analog.unit = SR_UNIT_VOLT;
    analog.mqflags = SR_MQFLAG_AC;
    analog.data = buf;

    delay_time();
    ds_data_forward(sdi, &packet);
    g_safe_free(buf);

    return TRUE;
}

static void send_error_packet(const struct sr_dev_inst *cb_sdi, struct session_vdev *vdev, struct sr_datafeed_packet *packet)
{
    packet->type = SR_DF_END;
    packet->status = SR_PKT_SOURCE_ERROR;
    ds_data_forward(cb_sdi, packet);
    sr_session_source_remove(-1);
    close_archive(vdev);
}


static int close_archive(struct session_vdev *vdev)
{
    if(sample_generator != PATTERN_RANDOM)
    {
        assert(vdev->archive);

        // close current inner file
        if (vdev->capfile)
        {
            unzCloseCurrentFile(vdev->archive);
            vdev->capfile = 0;
        }

        int ret = unzClose(vdev->archive);
        if (ret != UNZ_OK)
        {
            sr_err("close zip archive error!");
        }

        vdev->archive = NULL;
    }

    return SR_OK;
}

static int load_virtual_device_session(struct sr_dev_inst *sdi)
{
    GKeyFile *kf;
    unzFile archive = NULL;
    char szFilePath[15];
    unz_file_info64 fileInfo;

    struct sr_channel *probe;
    int ret, devcnt, i, j;
    uint16_t probenum;
    uint64_t tmp_u64, total_probes, enabled_probes;
    uint16_t p;
    int64_t tmp_64;
    char **sections, **keys, *metafile, *val;
    char probename[SR_MAX_PROBENAME_LEN + 1];
    int mode = LOGIC;
    int channel_type = SR_CHANNEL_LOGIC;
    double tmp_double;
    int version = 1;

    assert(sdi);
    if (sample_generator != PATTERN_RANDOM)
    {
        assert(sdi->path);
    }

    if (sdi->mode == LOGIC && sample_generator == PATTERN_RANDOM)
    {
        sdi->driver->config_set(SR_CONF_SAMPLERATE,
                                g_variant_new_uint64(SR_MHZ(1)), sdi, NULL, NULL);
        sdi->driver->config_set(SR_CONF_LIMIT_SAMPLES,
                                g_variant_new_uint64(SR_MHZ(1)), sdi, NULL, NULL);
        sr_dev_probes_free(sdi);
        sdi->driver->config_set(SR_CONF_CAPTURE_NUM_PROBES,
                                g_variant_new_uint64(16), sdi, NULL, NULL);
        sdi->driver->config_set(SR_CONF_NUM_BLOCKS,
                                g_variant_new_uint64(6), sdi, NULL, NULL);

        char* probe_val;
        for (int i = 0; i < 16; i++)
        {
            probe_val = probe_names[i];
            if (!(probe = sr_channel_new(i, SR_CHANNEL_LOGIC, TRUE, probe_val)))
            {
                sr_err("%s: create channel failed", __func__);
                sr_dev_inst_free(sdi);
                return SR_ERR;
            }

            sdi->channels = g_slist_append(sdi->channels, probe);
        }
        adjust_samplerate(sdi);
    }
    else if(sdi->mode == DSO)
    {
        sdi->driver->config_set(SR_CONF_SAMPLERATE,
                                g_variant_new_uint64(SR_MHZ(100)), sdi, NULL, NULL);
        sdi->driver->config_set(SR_CONF_LIMIT_SAMPLES,
                                g_variant_new_uint64(SR_KHZ(10)), sdi, NULL, NULL);
        sr_dev_probes_free(sdi);
        sdi->driver->config_set(SR_CONF_CAPTURE_NUM_PROBES,
                                g_variant_new_uint64(2), sdi, NULL, NULL);
        sdi->driver->config_set(SR_CONF_NUM_BLOCKS,
                                g_variant_new_uint64(1), sdi, NULL, NULL);
        char* probe_val;
        for (int i = 0; i < 2; i++)
        {
            probe_val = probe_names[i];
            if (!(probe = sr_channel_new(i, SR_CHANNEL_DSO, TRUE, probe_val)))
            {
                sr_err("%s: create channel failed", __func__);
                sr_dev_inst_free(sdi);
                return SR_ERR;
            }
            probe->enabled = TRUE;
            probe->coupling = 1;
            probe->vdiv = 1000;
            probe->vfactor = 1000;
            probe->hw_offset = 128;
            probe->offset = 128;
            probe->trig_value = 0.5;
            sdi->channels = g_slist_append(sdi->channels, probe);
        }
        adjust_samplerate(sdi);
    }
    else if(sdi->mode == ANALOG)
    {
        sdi->driver->config_set(SR_CONF_SAMPLERATE,
                                g_variant_new_uint64(SR_MHZ(1)), sdi, NULL, NULL);
        sdi->driver->config_set(SR_CONF_LIMIT_SAMPLES,
                                g_variant_new_uint64(SR_MHZ(1)), sdi, NULL, NULL);
        sr_dev_probes_free(sdi);
        sdi->driver->config_set(SR_CONF_CAPTURE_NUM_PROBES,
                                g_variant_new_uint64(2), sdi, NULL, NULL);
        sdi->driver->config_set(SR_CONF_NUM_BLOCKS,
                                g_variant_new_uint64(1), sdi, NULL, NULL);
        char* probe_val;
        for (int i = 0; i < 2; i++)
        {
            probe_val = probe_names[i];
            if (!(probe = sr_channel_new(i, SR_CHANNEL_ANALOG, TRUE, probe_val)))
            {
                sr_err("%s: create channel failed", __func__);
                sr_dev_inst_free(sdi);
                return SR_ERR;
            }

            probe->enabled = TRUE;
            probe->bits = 8;
            probe->vdiv = 1000;
            probe->hw_offset = 128;
            probe->offset = 128;
            probe->coupling = 1;
            probe->vfactor = 1;
            probe->trig_value = 128;
            probe->map_default = TRUE;
            probe->map_unit = "V";
            probe->map_min =  -(probe->vdiv * probe->vfactor * DS_CONF_DSO_VDIVS / 2000.0);
            probe->map_max = probe->vdiv * probe->vfactor * DS_CONF_DSO_VDIVS / 2000.0;

            sdi->channels = g_slist_append(sdi->channels, probe);
        }
        adjust_samplerate(sdi);
    }
    else
    {
        archive = unzOpen64(sdi->path);
        if (NULL == archive)
        {
            sr_err("%s: Load zip file error.", __func__);
            return SR_ERR;
        }
        if (unzLocateFile(archive, "header", 0) != UNZ_OK)
        {
            unzClose(archive);
            sr_err("%s: unzLocateFile error.", __func__);
            return SR_ERR;
        }
        if (unzGetCurrentFileInfo64(archive, &fileInfo, szFilePath,
                                    sizeof(szFilePath), NULL, 0, NULL, 0) != UNZ_OK)
        {
            unzClose(archive);
            sr_err("%s: unzGetCurrentFileInfo64 error.", __func__);
            return SR_ERR;
        }
        if (unzOpenCurrentFile(archive) != UNZ_OK)
        {
            sr_err("%s: Cant't open zip inner file.", __func__);
            unzClose(archive);
            return SR_ERR;
        }

        if (!(metafile = g_try_malloc(fileInfo.uncompressed_size)))
        {
            sr_err("%s: metafile malloc failed", __func__);
            return SR_ERR_MALLOC;
        }

        unzReadCurrentFile(archive, metafile, fileInfo.uncompressed_size);
        unzCloseCurrentFile(archive);

        if (unzClose(archive) != UNZ_OK)
        {
            sr_err("%s: Close zip archive error.", __func__);
            return SR_ERR;
        }
        archive = NULL;

        kf = g_key_file_new();
        if (!g_key_file_load_from_data(kf, metafile, fileInfo.uncompressed_size, 0, NULL))
        {
            sr_err("Failed to parse metadata.");
            return SR_ERR;
        }

        devcnt = 0;
        sections = g_key_file_get_groups(kf, NULL);

        for (i = 0; sections[i]; i++)
        {
            if (!strcmp(sections[i], "version"))
            {
                keys = g_key_file_get_keys(kf, sections[i], NULL, NULL);
                for (j = 0; keys[j]; j++)
                {
                    val = g_key_file_get_string(kf, sections[i], keys[j], NULL);
                    if (!strcmp(keys[j], "version"))
                    {
                        version = strtoull(val, NULL, 10);
                        sr_info("The 'header' file format version:%d", version);
                    }
                }
            }

            if (!strncmp(sections[i], "header", 6))
            {
                enabled_probes = total_probes = 0;
                keys = g_key_file_get_keys(kf, sections[i], NULL, NULL);

                for (j = 0; keys[j]; j++)
                {
                    val = g_key_file_get_string(kf, sections[i], keys[j], NULL);

                    if (!strcmp(keys[j], "device mode"))
                    {
                        mode = strtoull(val, NULL, 10);
                    }
                    else if (!strcmp(keys[j], "samplerate"))
                    {
                        sr_parse_sizestring(val, &tmp_u64);
                        sdi->driver->config_set(SR_CONF_SAMPLERATE,
                                                g_variant_new_uint64(tmp_u64), sdi, NULL, NULL);
                    }
                    else if (!strcmp(keys[j], "total samples"))
                    {
                        tmp_u64 = strtoull(val, NULL, 10);
                        sdi->driver->config_set(SR_CONF_LIMIT_SAMPLES,
                                                g_variant_new_uint64(tmp_u64), sdi, NULL, NULL);

                    }
                    else if (!strcmp(keys[j], "total blocks"))
                    {
                        tmp_u64 = strtoull(val, NULL, 10);
                        sdi->driver->config_set(SR_CONF_NUM_BLOCKS,
                                                g_variant_new_uint64(tmp_u64), sdi, NULL, NULL);
                    }
                    else if (!strcmp(keys[j], "total probes"))
                    {
                        sr_dev_probes_free(sdi);
                        total_probes = strtoull(val, NULL, 10);
                        sdi->driver->config_set(SR_CONF_CAPTURE_NUM_PROBES,
                                                g_variant_new_uint64(total_probes), sdi, NULL, NULL);
                    }
                    else if (!strncmp(keys[j], "probe", 5))
                    {
                        enabled_probes++;
                        tmp_u64 = strtoul(keys[j] + 5, NULL, 10);
                        channel_type = (mode == DSO) ? SR_CHANNEL_DSO : (mode == ANALOG) ? SR_CHANNEL_ANALOG
                                                                                         : SR_CHANNEL_LOGIC;
                        if (!(probe = sr_channel_new(tmp_u64, channel_type, TRUE, val)))
                        {
                            sr_err("%s: create channel failed", __func__);
                            sr_dev_inst_free(sdi);
                            return SR_ERR;
                        }

                        sdi->channels = g_slist_append(sdi->channels, probe);
                    }
                }
                adjust_samplerate(sdi);
                g_strfreev(keys);
            }
            devcnt++;
        }

        g_strfreev(sections);
        g_key_file_free(kf);
        g_safe_free(metafile);
    }

    return SR_OK;
}


SR_PRIV struct sr_dev_driver demo_driver_info = {
    .name = "virtual-demo",
    .longname = "Demo driver and pattern generator",
    .api_version = 1,
    .driver_type = DRIVER_TYPE_DEMO,
    .init = hw_init,
    .cleanup = NULL,
    .scan = hw_scan,
    .dev_mode_list = hw_dev_mode_list,
    .config_get = config_get,
    .config_set = config_set,
    .config_list = config_list,
    .dev_open = hw_dev_open,
    .dev_close = hw_dev_close,
    .dev_destroy = dev_destroy,
    .dev_status_get = hw_dev_status_get,
    .dev_acquisition_start = hw_dev_acquisition_start,
    .dev_acquisition_stop = hw_dev_acquisition_stop,
    .priv = NULL,
};
