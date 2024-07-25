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

/* Message logging helpers with subsystem-specific prefix string. */

#undef LOG_PREFIX
#define LOG_PREFIX "virtual-session: "

/* size of payloads sent across the session bus */
/** @cond PRIVATE */
#define CHUNKSIZE (512 * 1024)
#define UNITLEN 64
/** @endcond */

extern struct sr_session *session;
extern SR_PRIV struct sr_dev_driver session_driver;

static int sr_load_virtual_device_session(struct sr_dev_inst *sdi);

static uint64_t samplerates[1];
static uint64_t samplecounts[1];

static const char *maxHeights[] = {
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

static void get_file_short_name(const char *file, char *buf, int buflen)
{
    if (file == NULL || *file == '\0' || buf == NULL || buflen < 1)
        return;

    int len = strlen(file);
    int pos = len;
    char *wr = buf;
    char c;

    while (pos >= 0)
    {
        pos--;
        c = *(file + pos);
        if (c == '/' || c == '\\')
        {
            pos++;
            break;
        }
    }

    while (pos < len && (wr - buf) <= buflen)
    {
        *wr = *(file + pos);
        wr++;
        pos++;
    }
    *wr = '\0';
}

struct session_packet_buffer;

struct session_vdev
{ 
    int version;
    unzFile archive; // zip document
    int capfile;     // current inner file open status

    void *buf;
    void *logic_buf;
    int cur_channel;
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
};

#define SESSION_MAX_CHANNEL_COUNT 512

struct session_packet_buffer
{
    void       *post_buf;
    uint64_t    post_buf_len;
    uint64_t    post_write_len; 
    
    void       *block_bufs[SESSION_MAX_CHANNEL_COUNT]; // Current block of all channel.
    uint64_t    block_buf_len;  //Current block buffer length.
    uint64_t    block_data_len; 
    uint64_t    block_read_len; //Current block read position.
    int         channel_dir_map[SESSION_MAX_CHANNEL_COUNT];
};

static const int hwoptions[] = {
    SR_CONF_MAX_HEIGHT,
};

static const int32_t sessions[] = {
    SR_CONF_MAX_HEIGHT,
};

static const int32_t probeOptions[] = {
    SR_CONF_PROBE_MAP_UNIT,
    SR_CONF_PROBE_MAP_MIN,
    SR_CONF_PROBE_MAP_MAX,
};

static const char *probeMapUnits[] = {
    "V",
    "A",
    "℃",
    "℉",
    "g",
    "m",
    "m/s",
};

static void free_temp_buffer(struct session_vdev *vdev);

static int trans_data(const struct sr_dev_inst *sdi)
{
    // translate for old format
    struct session_vdev *vdev = sdi->priv;
    GSList *l;
    struct sr_channel *probe;

    assert(vdev->buf != NULL);
    assert(vdev->logic_buf != NULL);
    assert(CHUNKSIZE % UNITLEN == 0);

    // int bytes = ceil(vdev->num_probes / 8.0);
    int bytes = 2;
    uint8_t *src_ptr = (uint8_t *)vdev->buf;
    uint64_t *dest_ptr = (uint64_t *)vdev->logic_buf;
    
    for (int k = 0; k < CHUNKSIZE / (UNITLEN * bytes); k++)
    {
        src_ptr = (uint8_t *)vdev->buf + (k * bytes * UNITLEN);
        for (l = sdi->channels; l; l = l->next)
        {
            probe = l->data;
            if (!probe->enabled)
                continue;
            uint64_t mask = 1ULL << probe->index;
            uint64_t result = 0;
            for (int j = 0; j < UNITLEN; j++)
            {
                if (*(uint64_t *)(src_ptr + j * bytes) & mask)
                    result += 1ULL << j;
            }
            *dest_ptr++ = result;
        }
    }

    return SR_OK;
}

static int close_archive(struct session_vdev *vdev)
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
    return SR_OK;
}

static void send_error_packet(const struct sr_dev_inst *cb_sdi, struct session_vdev *vdev, struct sr_datafeed_packet *packet)
{
    packet->type = SR_DF_END;
    packet->status = SR_PKT_SOURCE_ERROR;
    ds_data_forward(cb_sdi, packet);
    sr_session_source_remove(-1);
    close_archive(vdev);
}

static int receive_data(int fd, int revents, const struct sr_dev_inst *sdi)
{
    struct session_vdev *vdev = NULL;
    struct sr_datafeed_packet packet;
    struct sr_datafeed_logic logic;
    struct sr_datafeed_dso dso;
    struct sr_datafeed_analog analog;
    int ret;
    char file_name[32];
    struct sr_channel *probe = NULL;
    GSList *pl;
    int channel; 

    assert(sdi);
    assert(sdi->priv);

    (void)fd;
    (void)revents;

    sr_detail("Feed chunk.");

    ret = 0;
    packet.status = SR_PKT_OK;

    vdev = sdi->priv;
    
    assert(vdev->unit_bits > 0);
    assert(vdev->cur_channel >= 0);
    assert(vdev->archive);

    if (vdev->cur_channel < vdev->num_probes)
    {
        if (vdev->version == 1)
        {
            ret = unzReadCurrentFile(vdev->archive, vdev->buf, CHUNKSIZE);
            if (-1 == ret)
            {
                sr_err("%s: Read inner file error!", __func__);
                send_error_packet(sdi, vdev, &packet);
                return FALSE;
            }
        }
        else if (vdev->version > 1)
        {
            channel = vdev->cur_channel;
            pl = sdi->channels;

            while (channel--){
                pl = pl->next;
            }

            probe = (struct sr_channel *)pl->data;

            if (vdev->capfile == 0)
            {
                char *type_name = (probe->type == SR_CHANNEL_LOGIC) ? "L" : (probe->type == SR_CHANNEL_DSO)  ? "O"
                                                                        : (probe->type == SR_CHANNEL_ANALOG) ? "A"
                                                                                                                : "U";

                snprintf(file_name, sizeof(file_name)-1, "%s-%d/%d", type_name,
                            sdi->mode == LOGIC ? probe->index : 0, vdev->cur_block);

                if (unzLocateFile(vdev->archive, file_name, 0) != UNZ_OK)
                {
                    sr_err("can't locate zip inner file:\"%s\"", file_name);
                    send_error_packet(sdi, vdev, &packet);
                    return FALSE;
                }
                if (unzOpenCurrentFile(vdev->archive) != UNZ_OK)
                {
                    sr_err("can't open zip inner file:\"%s\"", file_name);
                    send_error_packet(sdi, vdev, &packet);
                    return FALSE;
                }
                vdev->capfile = 1;
            }

            if (vdev->capfile)
            {
                ret = unzReadCurrentFile(vdev->archive, vdev->buf, CHUNKSIZE);

                if (-1 == ret)
                {
                    sr_err("read zip inner file error:\"%s\"", file_name);
                    send_error_packet(sdi, vdev, &packet);
                    return FALSE;
                }
            }
        }

        if (ret > 0)
        {
            if (sdi->mode == DSO)
            {
                packet.type = SR_DF_DSO;
                packet.payload = &dso;
                dso.num_samples = ret / vdev->num_probes;
                dso.data = vdev->buf;
                dso.probes = sdi->channels;
                dso.mq = SR_MQ_VOLTAGE;
                dso.unit = SR_UNIT_VOLT;
                dso.mqflags = SR_MQFLAG_AC;
            }
            else if (sdi->mode == ANALOG)
            {
                packet.type = SR_DF_ANALOG;
                packet.payload = &analog;
                analog.probes = sdi->channels;
                analog.num_samples = ret / vdev->num_probes / ((vdev->unit_bits + 7) / 8);
                analog.unit_bits = vdev->unit_bits;
                analog.mq = SR_MQ_VOLTAGE;
                analog.unit = SR_UNIT_VOLT;
                analog.mqflags = SR_MQFLAG_AC;
                analog.data = vdev->buf;
            }
            else if (sdi->mode == LOGIC)
            {   
                // Only supports version 1 file.
                if (vdev->version > 1){
                    assert(0);
                }

                packet.type = SR_DF_LOGIC;
                packet.payload = &logic;
                logic.length = ret;
                logic.format = LA_CROSS_DATA;
                if (probe)
                    logic.index = probe->index;
                else
                    logic.index = 0;
                logic.order = vdev->cur_channel;

                logic.length = ret / 16 * vdev->enabled_probes;
                logic.data = vdev->logic_buf;
                trans_data(sdi);
            }

            ds_data_forward(sdi, &packet);
        }
        else
        {
            /* done with this capture file */
            unzCloseCurrentFile(vdev->archive);
            vdev->capfile = 0;

            if (vdev->version == 1)
            {
                vdev->cur_channel++;
            }
            else if (vdev->version > 1)
            {
                vdev->cur_block++;
                // if read to the last block, move to next channel
                if (vdev->cur_block == vdev->num_blocks)
                {
                    vdev->cur_block = 0;
                    vdev->cur_channel++;
                }
            }
        }
    }    

    if (vdev->cur_channel >= vdev->num_probes || revents == -1)
    {
        packet.type = SR_DF_END;
        ds_data_forward(sdi, &packet);
        sr_session_source_remove(-1);

        if (NULL != vdev)
        {
            // abort
            close_archive(vdev);
        }
    }

    return TRUE;
}

static int receive_data_logic_dso_v2(int fd, int revents, const struct sr_dev_inst *sdi)
{
    struct session_vdev *vdev = NULL;
    struct sr_datafeed_packet packet;
    struct sr_datafeed_logic logic;
    struct sr_datafeed_dso dso;

    int ret;
    char file_name[32]; 
    int ch_index, malloc_chan_index;
    struct session_packet_buffer *pack_buffer;
    unz_file_info64 fileInfo;
    char szFilePath[15];
    int bToEnd;
    int read_chan_index; 
    int chan_num;
    uint8_t *ptrWrite;
    uint64_t *ptrWrite_64;
    int byte_align;
    int channel_dex;
    const int file_max_channel_count = 128;

    assert(sdi);
    assert(sdi->priv);
    (void)fd;

    ret = 0;
    bToEnd = 0;
    packet.status = SR_PKT_OK;
    vdev = sdi->priv;
    
    assert(vdev->unit_bits > 0); 
    assert(vdev->archive);

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
        memset(vdev->packet_buffer, 0, sizeof(struct session_packet_buffer));

        for (ch_index = 0; ch_index <= chan_num; ch_index++){
            vdev->packet_buffer->block_bufs[ch_index] = NULL;
        }

        if (sdi->mode == LOGIC)
            vdev->packet_buffer->post_buf_len = chan_num * 8 * 200000;
        else
            vdev->packet_buffer->post_buf_len = chan_num * 10000;

        vdev->packet_buffer->post_buf = g_try_malloc0(vdev->packet_buffer->post_buf_len + 1);
        if (vdev->packet_buffer->post_buf == NULL){
            sr_err("%s: vdev->packet_buffer->post_buf malloc failed", __func__);
            return SR_ERR_MALLOC;
        }

        pack_buffer = vdev->packet_buffer;
        pack_buffer->post_write_len = 0;
        pack_buffer->block_buf_len = 0;
        pack_buffer->block_data_len = 0;
        pack_buffer->block_read_len = 0;

        if (sdi->mode == DSO)
          vdev->num_blocks = 1; // Only one data file.

        //Make dir index map
        channel_dex = 0;

        for (ch_index = 0; ch_index < chan_num; ch_index++)
        {
            while (1)
            {
                if (sdi->mode == LOGIC)
                    snprintf(file_name, sizeof(file_name)-1, "L-%d/0", channel_dex++);
                else if (sdi->mode == DSO)
                    snprintf(file_name, sizeof(file_name)-1, "O-%d/0", channel_dex++);
                
                if (unzLocateFile(vdev->archive, file_name, 0) == UNZ_OK){
                    pack_buffer->channel_dir_map[ch_index] = channel_dex - 1;
                    break;
                }
                else if (channel_dex > file_max_channel_count){
                    break;
                }                    
            }
        }
    }
    pack_buffer = vdev->packet_buffer;

    // Make packet. 
    read_chan_index = 0;

    while (pack_buffer->post_write_len < pack_buffer->post_buf_len)
    { 
        // The current block is readed end, or the buffer is empty.
        if (pack_buffer->block_read_len >= pack_buffer->block_data_len)
        { 
            // The block index to end.
            if (vdev->cur_block >= vdev->num_blocks){
                bToEnd = 1;
                break;
            }

           // Load the block file data of each channel to buffer.
           for (ch_index = 0; ch_index < chan_num; ch_index++)
            { 
                if (sdi->mode == LOGIC){
                    snprintf(file_name, sizeof(file_name)-1, "L-%d/%d", 
                        pack_buffer->channel_dir_map[ch_index], vdev->cur_block);
                }
                else if (sdi->mode == DSO){
                    snprintf(file_name, sizeof(file_name)-1, "O-%d/0", 
                        pack_buffer->channel_dir_map[ch_index]);
                }
                    
                if (unzLocateFile(vdev->archive, file_name, 0) != UNZ_OK){
                    sr_err("can't locate zip inner file:\"%s\"", file_name);
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
                    // Alloc the buffer for each channel to read block file data.
                    pack_buffer->block_data_len = fileInfo.uncompressed_size;
                    
                    if (pack_buffer->block_data_len > pack_buffer->block_buf_len)
                    {
                        for (malloc_chan_index = 0; malloc_chan_index < chan_num; malloc_chan_index++){   
                            // Release the old buffer.
                            if (pack_buffer->block_bufs[malloc_chan_index] != NULL){
                                g_free(pack_buffer->block_bufs[malloc_chan_index]);
                                pack_buffer->block_bufs[malloc_chan_index] = NULL;
                            }

                            pack_buffer->block_bufs[malloc_chan_index] = g_try_malloc(pack_buffer->block_data_len + 1);
                            if (pack_buffer->block_bufs[malloc_chan_index] == NULL){
                                sr_err("%s: block buffer malloc failed", __func__);
                                send_error_packet(sdi, vdev, &packet);
                                return FALSE;
                            }                                                    
                        }

                        pack_buffer->block_buf_len = pack_buffer->block_data_len;
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
                    sr_err("can't open zip inner file:\"%s\"", file_name);
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
            }

           // sr_info("Load block %d end.",  vdev->cur_block);
            vdev->cur_block++;
            pack_buffer->block_read_len = 0;  
        }

        //------------Read data to buffer to send back.
        if (sdi->mode == LOGIC)
        {    
            ptrWrite_64 = (uint64_t*)((uint8_t*)pack_buffer->post_buf + pack_buffer->post_write_len);

            while ((pack_buffer->post_buf_len - pack_buffer->post_write_len) >= 8 
                    && (pack_buffer->block_data_len - pack_buffer->block_read_len) >= 8)
            {
                *ptrWrite_64++ = *(uint64_t*)((uint8_t*)pack_buffer->block_bufs[read_chan_index] 
                                        + pack_buffer->block_read_len);
                pack_buffer->post_write_len += 8;

                read_chan_index++;
                if (read_chan_index == chan_num){
                    read_chan_index = 0;
                    pack_buffer->block_read_len += 8;
                }
            }

            if ((pack_buffer->block_data_len - pack_buffer->block_read_len) % 8 != 0)
            {
                sr_err("The block data is not align with 8 byte.");
                bToEnd = 1;
                break;
            }  
        }
        else{
            ptrWrite = (uint8_t*)pack_buffer->post_buf + pack_buffer->post_write_len;

            while (pack_buffer->post_write_len < pack_buffer->post_buf_len
                    && pack_buffer->block_read_len < pack_buffer->block_data_len)
            {
                *ptrWrite++ = *((uint8_t*)pack_buffer->block_bufs[read_chan_index] 
                                    + pack_buffer->block_read_len);
                pack_buffer->post_write_len++;

                read_chan_index++;                
                if (read_chan_index == chan_num){
                    read_chan_index = 0;
                    pack_buffer->block_read_len++;
                }
            }
        }

        //sr_info("Fill packet end.");
    }
    
    // Send back.
    if (pack_buffer->post_write_len >= (uint64_t)(byte_align * chan_num))
    {  
        if (sdi->mode == LOGIC)
        {
            packet.type = SR_DF_LOGIC;
            packet.payload = &logic;
            logic.format = LA_CROSS_DATA;
            logic.index = 0;
            logic.order = 0;
            logic.length = pack_buffer->post_write_len;
            logic.data = pack_buffer->post_buf;
        }
        else{
            packet.type = SR_DF_DSO;
            packet.payload = &dso;            
            dso.probes = sdi->channels;
            dso.mq = SR_MQ_VOLTAGE;
            dso.unit = SR_UNIT_VOLT;
            dso.mqflags = SR_MQFLAG_AC; 
            dso.num_samples = pack_buffer->post_write_len / chan_num;
            dso.data = pack_buffer->post_buf;
        }

        // Send data back.
        ds_data_forward(sdi, &packet);
        pack_buffer->post_write_len = 0;        
    }

    // Check if is complete.
    if (bToEnd || revents == -1)
    {
        packet.type = SR_DF_END;
        ds_data_forward(sdi, &packet);
        sr_session_source_remove(-1);
        close_archive(vdev);
        free_temp_buffer(vdev);
    }

    return TRUE;
}


/* driver callbacks */
static int dev_clear(void);

static int init(struct sr_context *sr_ctx)
{
    (void)sr_ctx;

    return SR_OK;
}

static const GSList *dev_mode_list(const struct sr_dev_inst *sdi)
{
    GSList *l = NULL;
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(sr_mode_list); i++)
    {
        if (sdi->mode == sr_mode_list[i].mode)
            l = g_slist_append(l, (gpointer)&sr_mode_list[i]);
    }

    return l;
}

static int dev_clear(void)
{
    return SR_OK;
}

static int dev_open(struct sr_dev_inst *sdi)
{
    int ret;
    struct session_vdev *vdev;

    assert(sdi);

    if (sdi->status == SR_ST_ACTIVE){
        // Is opened.
        return SR_OK;
    }

    assert(sdi->priv == NULL);

    vdev = g_try_malloc(sizeof(struct session_vdev));
    if (vdev == NULL)
    {
        sr_err("%s: sdi->priv malloc failed", __func__);
        return SR_ERR_MALLOC;
    }
    memset(vdev, 0, sizeof(struct session_vdev));

    vdev->max_timebase = MAX_TIMEBASE;
    vdev->min_timebase = MIN_TIMEBASE;
    vdev->mstatus.measure_valid = TRUE;
    vdev->unit_bits = 1;

    sdi->priv = vdev;
    sdi->status = SR_ST_ACTIVE;

    vdev->buf = g_try_malloc0(CHUNKSIZE + sizeof(uint64_t));
    if (vdev->buf == NULL){
        sr_err("%s: vdev->buf malloc failed", __func__);
        return SR_ERR_MALLOC;
    }

    ret = sr_load_virtual_device_session(sdi);
    if (ret != SR_OK)
    {
        sr_err("Error!Load session file failed.");
        return ret;
    }

    return SR_OK;
}

static void free_temp_buffer(struct session_vdev *vdev)
{   
    struct session_packet_buffer *pack_buf;
    int i;

    assert(vdev);

    pack_buf = vdev->packet_buffer;

    if (pack_buf != NULL)
    {
        safe_free(pack_buf->post_buf);

        for (i = 0; i < SESSION_MAX_CHANNEL_COUNT; i++){
            if (pack_buf->block_bufs[i] != NULL){
                g_free(pack_buf->block_bufs[i]);
                pack_buf->block_bufs[i] = NULL;
            }
            else{
                break;
            }
        }
    }   

    safe_free(vdev->packet_buffer);    
    safe_free(vdev->buf);
    safe_free(vdev->logic_buf);
}

static int dev_close(struct sr_dev_inst *sdi)
{
    struct session_vdev *vdev;

    if (sdi && sdi->priv)
    {
        vdev = sdi->priv;
        free_temp_buffer(vdev);

        safe_free(sdi->priv);

        sdi->status = SR_ST_INACTIVE;
        return SR_OK;
    }

    return SR_ERR_CALL_STATUS;
}

static int dev_destroy(struct sr_dev_inst *sdi)
{
    assert(sdi);
    dev_close(sdi); 
    sr_dev_inst_free(sdi);
}

static int config_get(int id, GVariant **data, const struct sr_dev_inst *sdi,
                      const struct sr_channel *ch,
                      const struct sr_channel_group *cg)
{
    (void)cg;

    assert(sdi);
    assert(sdi->priv);

    struct session_vdev *vdev;

    switch (id)
    {
    case SR_CONF_SAMPLERATE:
        if (sdi)
        {
            vdev = sdi->priv;
            *data = g_variant_new_uint64(vdev->samplerate);
        }
        else
            return SR_ERR;
        break;
    case SR_CONF_LIMIT_SAMPLES:
        if (sdi)
        {
            vdev = sdi->priv;
            *data = g_variant_new_uint64(vdev->total_samples);
        }
        else
            return SR_ERR;
        break;
    case SR_CONF_TRIGGER_TIME:
        if (sdi)
        {
            vdev = sdi->priv;
            *data = g_variant_new_int64(vdev->trig_time);
        }
        else
            return SR_ERR;
        break;
    case SR_CONF_TIMEBASE:
        if (sdi)
        {
            vdev = sdi->priv;
            *data = g_variant_new_uint64(vdev->timebase);
        }
        else
            return SR_ERR;
        break;
    case SR_CONF_MAX_TIMEBASE:
        if (sdi)
        {
            vdev = sdi->priv;
            *data = g_variant_new_uint64(vdev->max_timebase);
        }
        else
            return SR_ERR;
        break;
    case SR_CONF_MIN_TIMEBASE:
        if (sdi)
        {
            vdev = sdi->priv;
            *data = g_variant_new_uint64(vdev->min_timebase);
        }
        else
            return SR_ERR;
        break;
    case SR_CONF_UNIT_BITS:
        if (sdi)
        {
            vdev = sdi->priv;
            *data = g_variant_new_byte(vdev->unit_bits);
        }
        else
            return SR_ERR;
        break;
    case SR_CONF_REF_MIN:
        if (sdi)
        {
            vdev = sdi->priv;
            if (vdev->ref_min == 0)
                return SR_ERR;
            else
                *data = g_variant_new_uint32(vdev->ref_min);
        }
        else
            return SR_ERR;
        break;
    case SR_CONF_REF_MAX:
        if (sdi)
        {
            vdev = sdi->priv;
            if (vdev->ref_max == 0)
                return SR_ERR;
            else
                *data = g_variant_new_uint32(vdev->ref_max);
        }
        else
            return SR_ERR;
        break;
    case SR_CONF_PROBE_EN:
        if (sdi && ch)
        {
            *data = g_variant_new_boolean(ch->enabled);
        }
        else
            return SR_ERR;
        break;
    case SR_CONF_PROBE_COUPLING:
        if (sdi && ch)
        {
            *data = g_variant_new_byte(ch->coupling);
        }
        else
            return SR_ERR;
        break;
    case SR_CONF_PROBE_VDIV:
        if (sdi && ch)
        {
            *data = g_variant_new_uint64(ch->vdiv);
        }
        else
            return SR_ERR;
        break;
    case SR_CONF_PROBE_FACTOR:
        if (sdi && ch)
        {
            *data = g_variant_new_uint64(ch->vfactor);
        }
        else
            return SR_ERR;
        break;
    case SR_CONF_PROBE_OFFSET:
        if (sdi && ch)
        {
            *data = g_variant_new_uint16(ch->offset);
        }
        else
            return SR_ERR;
        break;
    case SR_CONF_PROBE_HW_OFFSET:
        if (sdi && ch)
        {
            *data = g_variant_new_uint16(ch->hw_offset);
        }
        else
            return SR_ERR;
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
    case SR_CONF_TRIGGER_VALUE:
        if (sdi && ch)
        {
            *data = g_variant_new_byte(ch->trig_value);
        }
        else
            return SR_ERR;
        break;
    case SR_CONF_MAX_DSO_SAMPLERATE:
        if (!sdi)
            return SR_ERR;
        vdev = sdi->priv;
        *data = g_variant_new_uint64(vdev->samplerate);
        break;
    case SR_CONF_MAX_DSO_SAMPLELIMITS:
        if (!sdi)
            return SR_ERR;
        vdev = sdi->priv;
        *data = g_variant_new_uint64(vdev->total_samples);
        break;
    case SR_CONF_HW_DEPTH:
        if (!sdi)
            return SR_ERR;
        vdev = sdi->priv;
        *data = g_variant_new_uint64(vdev->total_samples);
        break;
    case SR_CONF_MAX_HEIGHT:
        if (!sdi)
            return SR_ERR;
        vdev = sdi->priv;
        *data = g_variant_new_string(maxHeights[vdev->max_height]);
        break;
    case SR_CONF_MAX_HEIGHT_VALUE:
        if (!sdi)
            return SR_ERR;
        vdev = sdi->priv;
        *data = g_variant_new_byte(vdev->max_height);
        break;
    case SR_CONF_VLD_CH_NUM:
        if (!sdi)
            return SR_ERR;
        vdev = sdi->priv;
        *data = g_variant_new_int16(vdev->num_probes);
        break;
    case SR_CONF_FILE_VERSION:
        if (!sdi)
            return SR_ERR;
        vdev = sdi->priv;
        *data = g_variant_new_int16(vdev->version);
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
    int pv;

    assert(sdi);
    assert(sdi->priv);

    vdev = sdi->priv;

    switch (id)
    {
    case SR_CONF_SAMPLERATE:
        vdev->samplerate = g_variant_get_uint64(data);
        samplerates[0] = vdev->samplerate;
        sr_dbg("Setting samplerate to %llu.", (u64_t)vdev->samplerate);
        break;
    case SR_CONF_TIMEBASE:
        vdev->timebase = g_variant_get_uint64(data);
        sr_dbg("Setting timebase to %llu.", (u64_t)vdev->timebase);
        break;
    case SR_CONF_MAX_TIMEBASE:
        vdev->max_timebase = g_variant_get_uint64(data);
        sr_dbg("Setting max timebase to %llu.", (u64_t)vdev->max_timebase);
        break;
    case SR_CONF_MIN_TIMEBASE:
        vdev->min_timebase = g_variant_get_uint64(data);
        sr_dbg("Setting min timebase to %llu.", (u64_t)vdev->min_timebase);
        break;
    case SR_CONF_UNIT_BITS:
        vdev->unit_bits = g_variant_get_byte(data);
        sr_dbg("Setting unit bits to %d.", vdev->unit_bits);
        break;
    case SR_CONF_REF_MIN:
        vdev->ref_min = g_variant_get_uint32(data);
        sr_dbg("Setting ref min to %d.", vdev->ref_min);
        break;
    case SR_CONF_REF_MAX:
        vdev->ref_max = g_variant_get_uint32(data);
        sr_dbg("Setting ref max to %d.", vdev->ref_max);
        break;
    case SR_CONF_FILE_VERSION:
        vdev->version = g_variant_get_int16(data);
        sr_dbg("Setting file version to '%d'.", vdev->version);
        break;
    case SR_CONF_LIMIT_SAMPLES:
        vdev->total_samples = g_variant_get_uint64(data);
        samplecounts[0] = vdev->total_samples;
        sr_dbg("Setting limit samples to %llu.", (u64_t)vdev->total_samples);
        break;
    case SR_CONF_TRIGGER_TIME:
        vdev->trig_time = g_variant_get_int64(data);
        sr_dbg("Setting trigger time to %llu.", (u64_t)vdev->trig_time);
        break;
    case SR_CONF_TRIGGER_POS:
        vdev->trig_pos = g_variant_get_uint64(data);
        sr_dbg("Setting trigger position to %llu.", (u64_t)vdev->trig_pos);
        break;
    case SR_CONF_NUM_BLOCKS:
        vdev->num_blocks = g_variant_get_uint64(data);
        sr_dbg("Setting block number to %llu.", (u64_t)vdev->num_blocks);
        break;
    case SR_CONF_CAPTURE_NUM_PROBES:
        vdev->num_probes = g_variant_get_uint64(data);
        if (vdev->version == 1)
        {
            if (sdi->mode == LOGIC)
            {
                if (!(vdev->logic_buf = g_try_malloc0(CHUNKSIZE / 16 * vdev->num_probes)))
                {
                    sr_err("%s: vdev->logic_buf malloc failed", __func__);
                }
            }
        }
        else
        {
            vdev->logic_buf = NULL;
        }
        break;
    case SR_CONF_PROBE_EN:
        ch->enabled = g_variant_get_boolean(data);
        break;
    case SR_CONF_PROBE_COUPLING:
        ch->coupling = g_variant_get_byte(data);
        break;
    case SR_CONF_PROBE_VDIV:
        ch->vdiv = g_variant_get_uint64(data);
        break;
    case SR_CONF_PROBE_FACTOR:
        ch->vfactor = g_variant_get_uint64(data);
        break;
    case SR_CONF_PROBE_OFFSET:
        ch->offset = g_variant_get_uint16(data);
        break;
    case SR_CONF_PROBE_HW_OFFSET:
        ch->hw_offset = g_variant_get_uint16(data);
        ch->offset = ch->hw_offset;
        break;
    case SR_CONF_PROBE_MAP_UNIT:
        ch->map_unit = g_variant_get_string(data, NULL);
        break;
    case SR_CONF_PROBE_MAP_MIN:
        ch->map_min = g_variant_get_double(data);
        break;
    case SR_CONF_PROBE_MAP_MAX:
        ch->map_max = g_variant_get_double(data);
        break;
    case SR_CONF_TRIGGER_VALUE:
        ch->trig_value = g_variant_get_byte(data);
        break;
    case SR_CONF_STATUS_PERIOD:
        if (ch->index == 0)
            vdev->mstatus.ch0_cyc_tlen = g_variant_get_uint32(data);
        else
            vdev->mstatus.ch1_cyc_tlen = g_variant_get_uint32(data);
        break;
    case SR_CONF_STATUS_PCNT:
        if (ch->index == 0)
            vdev->mstatus.ch0_cyc_cnt = g_variant_get_uint32(data);
        else
            vdev->mstatus.ch1_cyc_cnt = g_variant_get_uint32(data);
        break;
    case SR_CONF_STATUS_MAX:
        if (ch->index == 0)
            vdev->mstatus.ch0_max = g_variant_get_byte(data);
        else
            vdev->mstatus.ch1_max = g_variant_get_byte(data);
        break;
    case SR_CONF_STATUS_MIN:
        if (ch->index == 0)
            vdev->mstatus.ch0_min = g_variant_get_byte(data);
        else
            vdev->mstatus.ch1_min = g_variant_get_byte(data);
        break;
    case SR_CONF_STATUS_PLEN:
        if (ch->index == 0)
            vdev->mstatus.ch0_cyc_plen = g_variant_get_uint32(data);
        else
            vdev->mstatus.ch1_cyc_plen = g_variant_get_uint32(data);
        break;
    case SR_CONF_STATUS_LLEN:
        pv = g_variant_get_uint32(data);
        if (ch->index == 0)
            vdev->mstatus.ch0_cyc_llen = pv;
        else
            vdev->mstatus.ch1_cyc_llen = pv;
        break;
    case SR_CONF_STATUS_LEVEL:
        if (ch->index == 0)
            vdev->mstatus.ch0_level_valid = g_variant_get_boolean(data);
        else
            vdev->mstatus.ch1_level_valid = g_variant_get_boolean(data);
        break;
    case SR_CONF_STATUS_PLEVEL:
        if (ch->index == 0)
            vdev->mstatus.ch0_plevel = g_variant_get_boolean(data);
        else
            vdev->mstatus.ch1_plevel = g_variant_get_boolean(data);
        break;
    case SR_CONF_STATUS_LOW:
        if (ch->index == 0)
            vdev->mstatus.ch0_low_level = g_variant_get_byte(data);
        else
            vdev->mstatus.ch1_low_level = g_variant_get_byte(data);
        break;
    case SR_CONF_STATUS_HIGH:
        if (ch->index == 0)
            vdev->mstatus.ch0_high_level = g_variant_get_byte(data);
        else
            vdev->mstatus.ch1_high_level = g_variant_get_byte(data);
        break;
    case SR_CONF_STATUS_RLEN:
        if (ch->index == 0)
            vdev->mstatus.ch0_cyc_rlen = g_variant_get_uint32(data);
        else
            vdev->mstatus.ch1_cyc_rlen = g_variant_get_uint32(data);
        break;
    case SR_CONF_STATUS_FLEN:
        if (ch->index == 0)
            vdev->mstatus.ch0_cyc_flen = g_variant_get_uint32(data);
        else
            vdev->mstatus.ch1_cyc_flen = g_variant_get_uint32(data);
        break;
    case SR_CONF_STATUS_RMS:
        if (ch->index == 0)
            vdev->mstatus.ch0_acc_square = g_variant_get_uint64(data);
        else
            vdev->mstatus.ch1_acc_square = g_variant_get_uint64(data);
        break;
    case SR_CONF_STATUS_MEAN:
        if (ch->index == 0)
            vdev->mstatus.ch0_acc_mean = g_variant_get_uint32(data);
        else
            vdev->mstatus.ch1_acc_mean = g_variant_get_uint32(data);
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
    case SR_CONF_INSTANT:
    case SR_CONF_RLE:
        break;
    default:
        sr_err("Unknown capability: %d.", id);
        return SR_ERR_NA;
    }

    return SR_OK;
}

static int config_list(int key, GVariant **data,
                       const struct sr_dev_inst *sdi,
                       const struct sr_channel_group *cg)
{
    (void)cg;

    GVariant *gvar;
    GVariantBuilder gvb;

    (void)sdi;

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
        gvar = g_variant_new_from_data(G_VARIANT_TYPE("at"),
                                       samplerates, ARRAY_SIZE(samplerates) * sizeof(uint64_t), TRUE, NULL, NULL);
        g_variant_builder_add(&gvb, "{sv}", "samplerates", gvar);
        *data = g_variant_builder_end(&gvb);
        break;
    case SR_CONF_LIMIT_SAMPLES:
        g_variant_builder_init(&gvb, G_VARIANT_TYPE("a{sv}"));
        gvar = g_variant_new_from_data(G_VARIANT_TYPE("at"),
                                       samplecounts, ARRAY_SIZE(samplecounts) * sizeof(uint64_t), TRUE, NULL, NULL);
        g_variant_builder_add(&gvb, "{sv}", "samplecounts", gvar);
        *data = g_variant_builder_end(&gvb);
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
                                       vdivs, ARRAY_SIZE(vdivs) * sizeof(uint64_t), TRUE, NULL, NULL);
        g_variant_builder_add(&gvb, "{sv}", "vdivs", gvar);
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

static int dev_status_get(const struct sr_dev_inst *sdi, struct sr_status *status, gboolean prg)
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

static int dev_acquisition_start(struct sr_dev_inst *sdi, void *cb_data)
{
    (void)cb_data;

    struct session_vdev *vdev;
    struct sr_datafeed_packet packet;
    GSList *l;
    struct sr_channel *probe; 

    assert(sdi);
    assert(sdi->priv);
    assert(sdi->path);

    if (sdi->dev_type != DEV_TYPE_FILELOG)
    {
        assert(0);
    }

    vdev = sdi->priv;
    vdev->enabled_probes = 0;
    packet.status = SR_PKT_OK;

    // reset status
    vdev->cur_block = 0;
    vdev->cur_channel = 0;

    if (vdev->archive != NULL)
    {
        sr_err("history archive is not closed.");
    }

    sr_dbg("Opening archive file %s", sdi->path);

    vdev->archive = unzOpen64(sdi->path);

    if (NULL == vdev->archive)
    {
        sr_err("Failed to open session file '%s': "
               "zip error",
               sdi->path);
        return SR_ERR;
    }

    if (vdev->version == 1)
    {
        if (unzLocateFile(vdev->archive, "data", 0) != UNZ_OK)
        {
            sr_err("can't locate zip inner file:\"%s\"", "data");
            close_archive(vdev);
            return SR_ERR;
        }
        if (unzOpenCurrentFile(vdev->archive) != UNZ_OK)
        {
            sr_err("can't open zip inner file:\"%s\"", "data");
            close_archive(vdev);
            return SR_ERR;
        }
        vdev->capfile = 1;
        vdev->cur_channel = vdev->num_probes - 1;
    }
    else
    {
        if (sdi->mode == LOGIC)
            vdev->cur_channel = 0;
        else
            vdev->cur_channel = vdev->num_probes - 1;
    }

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

    /* freewheeling source */
    if ((sdi->mode == LOGIC && vdev->version > 1) 
            || (sdi->mode == DSO && vdev->version > 2)){
        sr_session_source_add(-1, 0, 0, receive_data_logic_dso_v2, sdi);
    }
    else{
        sr_session_source_add(-1, 0, 0, receive_data, sdi);
    }    

    return SR_OK;
}

SR_PRIV int sr_new_virtual_device(const char *filename, struct sr_dev_inst **out_di)
{
    struct sr_dev_inst *sdi; 
    char short_name[150];
    GKeyFile *kf;
    char **sections, **keys, *metafile, *val;
    int mode = LOGIC;
    unzFile archive;
    unz_file_info64 fileInfo;
    char szFilePath[15];
    int i, j;

    archive = NULL;
    sdi = NULL;

    if (!filename)
    {
        sr_err("%s: filename was NULL", __func__);
        return SR_ERR_ARG;
    }

    if (out_di == NULL)
    {
        sr_err("%s: @out_di was NULL", __func__);
        return SR_ERR_ARG;
    }

    archive = unzOpen64(filename);
    if (NULL == archive)
    {
        sr_err("load zip file error:%s", filename);
        return SR_ERR;
    }
    if (unzLocateFile(archive, "header", 0) != UNZ_OK)
    {
        unzClose(archive);
        sr_err("unzLocateFile error:'header', %s", filename);
        return SR_ERR;
    }
    if (unzGetCurrentFileInfo64(archive, &fileInfo, szFilePath,
                                sizeof(szFilePath), NULL, 0, NULL, 0) != UNZ_OK)
    {
        unzClose(archive);
        sr_err("unzGetCurrentFileInfo64 error,'header', %s", filename);
        return SR_ERR;
    }
    if (unzOpenCurrentFile(archive) != UNZ_OK)
    {
        sr_err("can't open zip inner file:'header',%s", filename);
        unzClose(archive);
        return SR_ERR;
    }

    if (!(metafile = g_try_malloc0(fileInfo.uncompressed_size)))
    {
        sr_err("%s: metafile malloc failed", __func__);
        return SR_ERR_MALLOC;
    }

    unzReadCurrentFile(archive, metafile, fileInfo.uncompressed_size);
    unzCloseCurrentFile(archive);

    if (unzClose(archive) != UNZ_OK)
    {
        sr_err("close zip archive error:%s", filename);
        return SR_ERR;
    }
    archive = NULL;

    kf = g_key_file_new();
    if (!g_key_file_load_from_data(kf, metafile, fileInfo.uncompressed_size, 0, NULL))
    {
        sr_err("Failed to parse metadata.");
        return SR_ERR;
    }

    // Get mode value.
    sections = g_key_file_get_groups(kf, NULL);
    for (i = 0; sections[i]; i++)
    {
        if (strncmp(sections[i], "header", 6) == 0)
        {
            keys = g_key_file_get_keys(kf, sections[i], NULL, NULL);
            for (j = 0; keys[j]; j++)
            {
                if (strcmp(keys[j], "device mode") == 0)
                {
                    val = g_key_file_get_string(kf, sections[i], keys[j], NULL);
                    mode = strtoull(val, NULL, 10);
                    break;
                }
            }
            g_strfreev(keys);
        }
    }
    g_strfreev(sections);
    g_key_file_free(kf);
    g_free(metafile);

    sdi = sr_dev_inst_new(mode, SR_ST_INACTIVE, NULL, NULL, NULL);
    sdi->driver = &session_driver;
    sdi->dev_type = DEV_TYPE_FILELOG;

    get_file_short_name(filename, short_name, sizeof(short_name) - 1);
    sdi->name = g_strdup((const char*)short_name);
    sdi->path = g_strdup(filename);

    *out_di = sdi;

    return SR_OK;
}

static int sr_load_virtual_device_session(struct sr_dev_inst *sdi)
{
    GKeyFile *kf;
    unzFile archive = NULL;
    char szFilePath[15];
    unz_file_info64 fileInfo;

    struct sr_channel *probe;
    int devcnt, i, j;
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
    assert(sdi->path);

    if (sdi->dev_type != DEV_TYPE_FILELOG)
    {
        sr_err("%s: Is not a virtual device instance.", __func__);
        return SR_ERR_ARG;
    }

    // Clear all channels.
    sr_dev_probes_free(sdi);

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

    if (!(metafile = g_try_malloc0(fileInfo.uncompressed_size)))
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
            /* device section */
            enabled_probes = total_probes = 0;
            keys = g_key_file_get_keys(kf, sections[i], NULL, NULL);

            for (j = 0; keys[j]; j++)
            {
                val = g_key_file_get_string(kf, sections[i], keys[j], NULL);

                if (!strcmp(keys[j], "device mode"))
                {
                    mode = strtoull(val, NULL, 10);
                }
                else if (!strcmp(keys[j], "capturefile"))
                {
                    sdi->driver->config_set(SR_CONF_FILE_VERSION,
                                            g_variant_new_int16(version), sdi, NULL, NULL);
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
                else if (!strcmp(keys[j], "hDiv"))
                {
                    tmp_u64 = strtoull(val, NULL, 10);
                    sdi->driver->config_set(SR_CONF_TIMEBASE,
                                            g_variant_new_uint64(tmp_u64), sdi, NULL, NULL);
                }
                else if (!strcmp(keys[j], "hDiv min"))
                {
                    tmp_u64 = strtoull(val, NULL, 10);
                    sdi->driver->config_set(SR_CONF_MIN_TIMEBASE,
                                            g_variant_new_uint64(tmp_u64), sdi, NULL, NULL);
                }
                else if (!strcmp(keys[j], "hDiv max"))
                {
                    tmp_u64 = strtoull(val, NULL, 10);
                    sdi->driver->config_set(SR_CONF_MAX_TIMEBASE,
                                            g_variant_new_uint64(tmp_u64), sdi, NULL, NULL);
                }
                else if (!strcmp(keys[j], "bits"))
                {
                    tmp_u64 = strtoull(val, NULL, 10);
                    sdi->driver->config_set(SR_CONF_UNIT_BITS,
                                            g_variant_new_byte(tmp_u64), sdi, NULL, NULL);
                }
                else if (!strcmp(keys[j], "ref min"))
                {
                    tmp_u64 = strtoull(val, NULL, 10);
                    sdi->driver->config_set(SR_CONF_REF_MIN,
                                            g_variant_new_uint32(tmp_u64), sdi, NULL, NULL);
                }
                else if (!strcmp(keys[j], "ref max"))
                {
                    tmp_u64 = strtoull(val, NULL, 10);
                    sdi->driver->config_set(SR_CONF_REF_MAX,
                                            g_variant_new_uint32(tmp_u64), sdi, NULL, NULL);
                }
                else if (!strcmp(keys[j], "trigger time"))
                {
                    tmp_64 = strtoll(val, NULL, 10);
                    sdi->driver->config_set(SR_CONF_TRIGGER_TIME,
                                            g_variant_new_int64(tmp_64), sdi, NULL, NULL);
                }
                else if (!strcmp(keys[j], "trigger pos"))
                {
                    tmp_u64 = strtoull(val, NULL, 10);
                    sdi->driver->config_set(SR_CONF_TRIGGER_POS,
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
                    total_probes = strtoull(val, NULL, 10);
                    sdi->driver->config_set(SR_CONF_CAPTURE_NUM_PROBES,
                                            g_variant_new_uint64(total_probes), sdi, NULL, NULL);
                    if (version == 1)
                    {
                        channel_type = (mode == DSO) ? SR_CHANNEL_DSO : (mode == ANALOG) ? SR_CHANNEL_ANALOG
                                                                                         : SR_CHANNEL_LOGIC;

                        for (p = 0; p < total_probes; p++)
                        {
                            snprintf(probename, SR_MAX_PROBENAME_LEN, "%u", p);
                            if (!(probe = sr_channel_new(p, channel_type, FALSE, probename)))
                            {
                                sr_err("%s: create channel failed", __func__);
                                sr_dev_inst_free(sdi); 
                                return SR_ERR;
                            }

                            sdi->channels = g_slist_append(sdi->channels, probe);
                        }
                    }
                }
                else if (!strncmp(keys[j], "probe", 5))
                {
                    enabled_probes++;
                    tmp_u64 = strtoul(keys[j] + 5, NULL, 10);
                    /* sr_session_save() */
                    if (version == 1)
                    {
                        sr_dev_probe_name_set(sdi, tmp_u64, val);
                        sr_dev_probe_enable(sdi, tmp_u64, TRUE);
                    }
                    else if (version > 1)
                    {
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
                else if (!strncmp(keys[j], "trigger", 7))
                {
                    probenum = strtoul(keys[j] + 7, NULL, 10);
                    sr_dev_trigger_set(sdi, probenum, val);
                }
                else if (!strncmp(keys[j], "enable", 6))
                {
                    probenum = strtoul(keys[j] + 6, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                     
                    if (probenum < g_slist_length(sdi->channels))
                    {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_PROBE_EN,
                                                g_variant_new_boolean(tmp_u64), sdi, probe, NULL);
                    }
                }
                else if (!strncmp(keys[j], "coupling", 8))
                {
                    probenum = strtoul(keys[j] + 8, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels))
                    {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_PROBE_COUPLING,
                                                g_variant_new_byte(tmp_u64), sdi, probe, NULL);
                    }
                }
                else if (!strncmp(keys[j], "vDiv", 4))
                {
                    probenum = strtoul(keys[j] + 4, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels))
                    {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_PROBE_VDIV,
                                                g_variant_new_uint64(tmp_u64), sdi, probe, NULL);
                    }
                }
                else if (!strncmp(keys[j], "vFactor", 7))
                {
                    probenum = strtoul(keys[j] + 7, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels))
                    {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_PROBE_FACTOR,
                                                g_variant_new_uint64(tmp_u64), sdi, probe, NULL);
                    }
                }
                else if (!strncmp(keys[j], "vOffset", 7))
                {
                    probenum = strtoul(keys[j] + 7, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels))
                    {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_PROBE_HW_OFFSET,
                                                g_variant_new_uint16(tmp_u64), sdi, probe, NULL);
                    }
                }
                else if (!strncmp(keys[j], "vTrig", 5))
                {
                    probenum = strtoul(keys[j] + 5, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels))
                    {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_TRIGGER_VALUE,
                                                g_variant_new_byte(tmp_u64), sdi, probe, NULL);
                    }
                }
                else if (!strncmp(keys[j], "period", 6))
                {
                    probenum = strtoul(keys[j] + 6, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels))
                    {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_PERIOD,
                                                g_variant_new_uint32(tmp_u64), sdi, probe, NULL);
                    }
                }
                else if (!strncmp(keys[j], "pcnt", 4))
                {
                    probenum = strtoul(keys[j] + 4, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels))
                    {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_PCNT,
                                                g_variant_new_uint32(tmp_u64), sdi, probe, NULL);
                    }
                }
                else if (!strncmp(keys[j], "max", 3))
                {
                    probenum = strtoul(keys[j] + 3, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels))
                    {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_MAX,
                                                g_variant_new_byte(tmp_u64), sdi, probe, NULL);
                    }
                }
                else if (!strncmp(keys[j], "min", 3))
                {
                    probenum = strtoul(keys[j] + 3, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels))
                    {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_MIN,
                                                g_variant_new_byte(tmp_u64), sdi, probe, NULL);
                    }
                }
                else if (!strncmp(keys[j], "plen", 4))
                {
                    probenum = strtoul(keys[j] + 4, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels))
                    {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_PLEN,
                                                g_variant_new_uint32(tmp_u64), sdi, probe, NULL);
                    }
                }
                else if (!strncmp(keys[j], "llen", 4))
                {
                    probenum = strtoul(keys[j] + 4, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels))
                    {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_LLEN,
                                                g_variant_new_uint32(tmp_u64), sdi, probe, NULL);
                    }
                }
                else if (!strncmp(keys[j], "level", 5))
                {
                    probenum = strtoul(keys[j] + 5, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels))
                    {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_LEVEL,
                                                g_variant_new_boolean(tmp_u64), sdi, probe, NULL);
                    }
                }
                else if (!strncmp(keys[j], "plevel", 6))
                {
                    probenum = strtoul(keys[j] + 6, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels))
                    {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_PLEVEL,
                                                g_variant_new_boolean(tmp_u64), sdi, probe, NULL);
                    }
                }
                else if (!strncmp(keys[j], "low", 3))
                {
                    probenum = strtoul(keys[j] + 3, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels))
                    {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_LOW,
                                                g_variant_new_byte(tmp_u64), sdi, probe, NULL);
                    }
                }
                else if (!strncmp(keys[j], "high", 4))
                {
                    probenum = strtoul(keys[j] + 4, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels))
                    {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_HIGH,
                                                g_variant_new_byte(tmp_u64), sdi, probe, NULL);
                    }
                }
                else if (!strncmp(keys[j], "rlen", 4))
                {
                    probenum = strtoul(keys[j] + 4, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels))
                    {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_RLEN,
                                                g_variant_new_uint32(tmp_u64), sdi, probe, NULL);
                    }
                }
                else if (!strncmp(keys[j], "flen", 4))
                {
                    probenum = strtoul(keys[j] + 4, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels))
                    {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_FLEN,
                                                g_variant_new_uint32(tmp_u64), sdi, probe, NULL);
                    }
                }
                else if (!strncmp(keys[j], "rms", 3))
                {
                    probenum = strtoul(keys[j] + 3, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels))
                    {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_RMS,
                                                g_variant_new_uint64(tmp_u64), sdi, probe, NULL);
                    }
                }
                else if (!strncmp(keys[j], "mean", 4))
                {
                    probenum = strtoul(keys[j] + 4, NULL, 10);
                    tmp_u64 = strtoull(val, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels))
                    {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_STATUS_MEAN,
                                                g_variant_new_uint32(tmp_u64), sdi, probe, NULL);
                    }
                }
                else if (!strncmp(keys[j], "mapUnit", 7))
                {
                    probenum = strtoul(keys[j] + 7, NULL, 10);
                    if (probenum < g_slist_length(sdi->channels))
                    {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_PROBE_MAP_UNIT,
                                                g_variant_new_string(val), sdi, probe, NULL);
                    }
                }
                else if (!strncmp(keys[j], "mapMax", 6))
                {
                    probenum = strtoul(keys[j] + 6, NULL, 10);
                    tmp_double = strtod(val, NULL);
                    if (probenum < g_slist_length(sdi->channels))
                    {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_PROBE_MAP_MAX,
                                                g_variant_new_double(tmp_double), sdi, probe, NULL);
                    }
                }
                else if (!strncmp(keys[j], "mapMin", 6))
                {
                    probenum = strtoul(keys[j] + 6, NULL, 10);
                    tmp_double = strtod(val, NULL);
                    if (probenum < g_slist_length(sdi->channels))
                    {
                        probe = g_slist_nth(sdi->channels, probenum)->data;
                        sdi->driver->config_set(SR_CONF_PROBE_MAP_MIN,
                                                g_variant_new_double(tmp_double), sdi, probe, NULL);
                    }
                }
            }
            g_strfreev(keys);
        }
        devcnt++;
    }
    g_strfreev(sections);
    g_key_file_free(kf);
    g_free(metafile);

    return SR_OK;
}

/** @private */
SR_PRIV struct sr_dev_driver session_driver = {
    .name = "virtual-session",
    .longname = "Session-emulating driver",
    .api_version = 1,
    .driver_type = DRIVER_TYPE_FILE,
    .init = init,
    .cleanup = dev_clear,
    .scan = NULL,
    .dev_mode_list = dev_mode_list,
    .config_get = config_get,
    .config_set = config_set,
    .config_list = config_list,
    .dev_open = dev_open,
    .dev_close = dev_close,
    .dev_destroy = dev_destroy,
    .dev_status_get = dev_status_get,
    .dev_acquisition_start = dev_acquisition_start,
    .dev_acquisition_stop = NULL,
    .priv = NULL,
};
