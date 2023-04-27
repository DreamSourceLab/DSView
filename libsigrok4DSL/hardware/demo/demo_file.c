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

#include "demo_file.h"

static void check_file(struct sr_dev_inst *sdi)
{
    struct session_vdev *vdev = sdi->priv;
    if(sdi->mode == LOGIC)
    {
        if (cur_sample_generator != PATTERN_RANDOM)
        {
            vdev->archive = unzOpen64(sdi->path);
            if (NULL == vdev->archive)
            {
                sr_err("Failed to open session file '%s'\n",sdi->path);
                set_sample_generator(sdi,pre_sample_generator);
            }
        }
    }
}

static void init_data(struct sr_dev_inst *sdi)
{
    assert(sdi);
    assert(sdi->priv);

    uint8_t random_val;
    struct session_vdev *vdev = sdi->priv;
    switch (sdi->mode)
    {
    case LOGIC:
        switch (cur_sample_generator)
        {
            case PATTERN_RANDOM:
                vdev->logic_buf_len = 10000000;
                if(!(vdev->logic_buf = g_try_malloc0(vdev->logic_buf_len)))
                {
                    return;
                }
                for(int i = 0 ; i < vdev->logic_buf_len ;i++)
                {
                    random_val = abs(rand())%256;
                    if(random_val >= 128)
                    {
                        *(uint8_t*)(vdev->logic_buf + i) = (uint8_t)255;
                    }
                    else
                    {
                        *(uint8_t*)(vdev->logic_buf + i) = (uint8_t)0;
                    }
                }
                break;
            case PATTERN_UART:
                break;
            case PATTERN_SPI:
                break;
            case PATTERN_EEPROM:
                break;
            default:
                break;
        }  
        break;
    case DSO:
        switch (cur_sample_generator)
        {
            case PATTERN_RANDOM:
                break;
            case PATTERN_SINE:
                break;
            case PATTERN_SQUARE:
                break;
            case PATTERN_TRIANGLE:
                break;
            case PATTERN_SAWTOOTH:
                break;
            default:
                break;
        }  
        break;
    case ANALOG:
        switch (cur_sample_generator)
        {
            case PATTERN_RANDOM:
                break;
            case PATTERN_SINE:
                break;
            case PATTERN_SQUARE:
                break;
            case PATTERN_TRIANGLE:
                break;
            case PATTERN_SAWTOOTH:
                break;
            default:
                break;
        }  
        break;
    default:
        break;
    }
}

static void set_mode(struct sr_dev_inst *sdi,int mode)
{
    assert(sdi);

    pre_mode = cur_mode;
    sdi->mode = cur_mode = mode;
    
    if (sdi->path)
    {
        g_free(sdi->path);
    }
    char str[500];
    strcpy(str,DS_RES_PATH);

    switch (sdi->mode)
    {
    case LOGIC:
        cur_sample_generator = pre_sample_generator = PATTERN_UART;
        sdi->path = g_strdup(strcat(str,"demo/demo_uart.dsl"));
        break;
    case DSO:
        cur_sample_generator = pre_sample_generator = PATTERN_SINE;
        sdi->path = g_strdup(strcat(str,"demo/demo_sine_dso.dsl"));
        break;
    case ANALOG:
        cur_sample_generator = pre_sample_generator = PATTERN_SINE;
        sdi->path = g_strdup(strcat(str,"demo/demo_sine_analog.dsl"));
        break;
    default:
        break;
    }
}

static void set_sample_generator(struct sr_dev_inst *sdi,uint8_t sample_generator)
{
    assert(sdi);

    pre_sample_generator = cur_sample_generator;
    cur_sample_generator = sample_generator;
    
    if (sdi->path)
    {
        g_free(sdi->path);
    }
    char str[500];
    strcpy(str,DS_RES_PATH);

    switch (sdi->mode)
    {
    case LOGIC:
        switch (cur_sample_generator)
        {
            case PATTERN_RANDOM:
                sdi->path = g_strdup(strcat(str,"demo/demo_random_logic.dsl"));
                break;
            case PATTERN_UART:
                sdi->path = g_strdup(strcat(str,"demo/demo_uart.dsl"));
                break;
            case PATTERN_SPI:
                sdi->path = g_strdup(strcat(str,"demo/demo_spi.dsl"));
                break;
            case PATTERN_EEPROM:
                sdi->path = g_strdup(strcat(str,"demo/demo_eeprom.dsl"));
                break;
            default:
                break;
        }  
        break;
    case DSO:
        switch (cur_sample_generator)
        {
            case PATTERN_RANDOM:
                sdi->path = g_strdup(strcat(str,"demo/demo_random_dso.dsl"));
                break;
            case PATTERN_SINE:
                sdi->path = g_strdup(strcat(str,"demo/demo_sine_dso.dsl"));
                break;
            case PATTERN_SQUARE:
                sdi->path = g_strdup(strcat(str,"demo/demo_square_dso.dsl"));
                break;
            case PATTERN_TRIANGLE:
                sdi->path = g_strdup(strcat(str,"demo/demo_triangle_dso.dsl"));
                break;
            case PATTERN_SAWTOOTH:
                sdi->path = g_strdup(strcat(str,"demo/demo_sawtooth_dso.dsl"));
                break;
            default:
                break;
        }  
        break;
    case ANALOG:
        switch (cur_sample_generator)
        {
            case PATTERN_RANDOM:
                sdi->path = g_strdup(strcat(str,"demo/demo_random_analog.dsl"));
                break;
            case PATTERN_SINE:
                sdi->path = g_strdup(strcat(str,"demo/demo_sine_analog.dsl"));
                break;
            case PATTERN_SQUARE:
                sdi->path = g_strdup(strcat(str,"demo/demo_square_analog.dsl"));
                break;
            case PATTERN_TRIANGLE:
                sdi->path = g_strdup(strcat(str,"demo/demo_triangle_analog.dsl"));
                break;
            case PATTERN_SAWTOOTH:
                sdi->path = g_strdup(strcat(str,"demo/demo_sawtooth_analog.dsl"));
                break;
            default:
                break;
        }  
        break;
    default:
        break;
    }
}  


static void adjust_samplerate_file(struct sr_dev_inst *sdi)
{
    if(sdi->mode == LOGIC && cur_sample_generator > PATTERN_RANDOM)
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
           channel_modes_file[cur_mode].max_samplerate)
        vdev->samplerates_max_index--;

    vdev->samplerates_min_index = 0;
    while (samplerates[vdev->samplerates_min_index] <
           channel_modes_file[cur_mode].min_samplerate)
        vdev->samplerates_min_index++;

    assert(vdev->samplerates_max_index >= vdev->samplerates_min_index);

    if (vdev->samplerate > samplerates[vdev->samplerates_max_index])
        vdev->samplerate = samplerates[vdev->samplerates_max_index];

    if (vdev->samplerate < samplerates[vdev->samplerates_min_index])
        vdev->samplerate = samplerates[vdev->samplerates_min_index];

}

static int close_archive(struct session_vdev *vdev)
{
    if(cur_mode == LOGIC && cur_sample_generator != PATTERN_RANDOM)
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

static void send_error_packet(const struct sr_dev_inst *cb_sdi, struct session_vdev *vdev, struct sr_datafeed_packet *packet)
{
    packet->type = SR_DF_END;
    packet->status = SR_PKT_SOURCE_ERROR;
    ds_data_forward(cb_sdi, packet);
    sr_session_source_remove(-1);
    close_archive(vdev);
}

static int receive_data_analog(int fd, int revents, const struct sr_dev_inst *sdi)
{
    struct session_vdev *vdev = sdi->priv;
    struct sr_datafeed_packet packet;
    struct sr_datafeed_analog analog;
    struct sr_channel *probe = NULL;

    assert(sdi);
    assert(sdi->priv);

    (void)fd;
    (void)revents;

    sr_detail("Feed chunk.");

    packet.status = SR_PKT_OK;

    assert(vdev->unit_bits > 0);

    g_timer_start(packet_interval);

    if(is_first)
    {
        gdouble rate = 103 / (gdouble)2048 ;
        uint64_t buf_len = rate * vdev->total_samples;

        if(buf_len % 103 != 0)
        {
            buf_len = buf_len / 103 * 103;
        }
        buf_len = buf_len * 2;

        if(vdev->buf != NULL)
        {
            g_free(vdev->buf);
            vdev->buf = NULL;
        }
        vdev->buf = (g_try_malloc0(buf_len));
        if (vdev->buf == NULL)
        {
            return SR_ERR_MALLOC;
        }
        vdev->buf_len = buf_len;

        uint64_t per_block_after_expend = buf_len /206;

        probe = g_slist_nth(sdi->channels, 0)->data;
        uint64_t p0_vdiv = probe->vdiv;
        probe = g_slist_nth(sdi->channels, 1)->data;
        uint64_t p1_vdiv = probe->vdiv;
        uint64_t vdiv;
        uint8_t val = 0;
        uint16_t tem;
        uint64_t cur_l = 0;

       uint8_t* analog_data;
       switch (cur_sample_generator)
       {
        case PATTERN_RANDOM:
            analog_data = analog_random_data;
            break;
        case PATTERN_SINE:
            analog_data = analog_sine_data;
            break;
        case PATTERN_SQUARE:
            analog_data = analog_square_data;
            break;
        case PATTERN_TRIANGLE:
            analog_data = analog_triangle_data;
            break;
        case PATTERN_SAWTOOTH:
            analog_data = analog_sawtooth_data;
            break;
        default:
            break;
       }

        for(int i = 0 ; i < 206;i++)
        {
            if(i == 0 || i % 2 == 0)
            {
                vdiv = p0_vdiv;
            }
            else
            {
                vdiv = p1_vdiv;
            }
            tem = 0;

            uint8_t temp_value = *(analog_data + i);

            if(temp_value > 128)
            {
                val = temp_value - 128;
                if(vdiv == 2000)
                {
                    tem = val/2;
                    tem = 128 + tem;
                }
                else if(vdiv == 500)
                {
                    tem = val*2;
                    tem = 128 + tem;
                }
                else if(vdiv == 200)
                {
                    tem = val*5;
                    tem = 128 + tem;
                }
                else if(vdiv == 100)
                {
                    tem = val*10;
                    tem = 128 + tem;
                }
                else if(vdiv == 50)
                {
                    tem = val*20;
                    tem = 128 + tem;
                }
                else if(vdiv == 20)
                {
                    tem = val*50;
                    tem = 128 + tem;
                }
                else if(vdiv == 10)
                {
                    tem = val*100;
                    tem = 128 + tem;
                }
                else if (vdiv == 1000)
                    tem = temp_value;
                if(tem >= 255)
                    temp_value = 255;
                else
                    temp_value = tem;
            }
            else if(temp_value < 128 && temp_value != 0)
            {
                val = 128 - temp_value;
                if(vdiv == 2000)
                {
                    tem = val/2;
                    tem = 128 - tem;
                }
                else if(vdiv == 500)
                {
                    tem = val*2;
                    tem = 128 - tem;
                }
                else if(vdiv == 200)
                {
                    tem = val*5;
                    tem = 128 - tem;
                }
                else if(vdiv == 100)
                {
                    tem = val*10;
                    tem = 128 - tem;
                }
                else if(vdiv == 50)
                {
                    tem = val*20;
                    tem = 128 - tem;
                }
                else if(vdiv == 20)
                {
                    tem = val*50;
                    tem = 128 - tem;
                }
                else if(vdiv == 10)
                {
                    tem = val*100;
                    tem = 128 - tem;
                }
                else if (vdiv == 1000)
                    tem = temp_value;
                if(tem == 0)
                    temp_value = 1;
                else
                    temp_value = tem;
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
                memset(vdev->buf + cur_l,temp_value,1);
            } 
        }
        is_first = FALSE;
    }


    void* analog_buf;
    if(analog_read_pos + packet_len >= vdev->buf_len - 1 )
    {
        analog_buf = g_try_malloc0(packet_len);
        uint64_t back_len = vdev->buf_len - analog_read_pos;
        for (int i = 0; i < back_len; i++)
        {
            uint8_t temp_val = *((uint8_t*)vdev->buf + analog_read_pos + i);
            memset(analog_buf + i,temp_val,1);
        }

        uint64_t front_len = packet_len - back_len;
        for (int i = 0; i < front_len; i++)
        {
            uint8_t temp_val = *((uint8_t*)vdev->buf + i);
            memset(analog_buf + back_len + i,temp_val,1);
        }
        analog_read_pos = front_len;
    }
    else
    {
        analog_buf = (uint8_t*) vdev->buf + analog_read_pos;
        analog_read_pos += packet_len;
    }

    packet.type = SR_DF_ANALOG;
    packet.payload = &analog;
    analog.probes = sdi->channels;
    analog.num_samples = packet_len / vdev->num_probes;
    analog.unit_bits = vdev->unit_bits;
    analog.mq = SR_MQ_VOLTAGE;
    analog.unit = SR_UNIT_VOLT;
    analog.mqflags = SR_MQFLAG_AC;
    analog.data = analog_buf;

    gdouble packet_elapsed = g_timer_elapsed(packet_interval, NULL);
    gdouble waittime = packet_time - packet_elapsed;
    if(waittime > 0){
        g_usleep(waittime*1000000);
    }

    ds_data_forward(sdi, &packet);

    return TRUE;
}

int cal_file_len(const struct sr_dev_inst *sdi)
{
    struct session_vdev *vdev = sdi->priv;
    char file_name[32];
    uint64_t time = 0;
    unz_file_info64 fileInfo;
    char szFilePath[15];
    int num_block = vdev->num_blocks;
    int checkfile = 0;
    int cur_probe = 0;
    for(int i = 0 ;i < num_block ; i++)
    {
        while (1)
        {
            snprintf(file_name, sizeof(file_name)-1, "L-%d/%d", cur_probe,i);
            if (unzLocateFile(vdev->archive, file_name, 0) == UNZ_OK){
                checkfile = 1;
                break;
            }
            cur_probe ++;
        }

        if (!checkfile){
            return -1;
        }

        if (unzGetCurrentFileInfo64(vdev->archive, &fileInfo, szFilePath,
                                    sizeof(szFilePath), NULL, 0, NULL, 0) != UNZ_OK){
            return -1;
        }
            time += fileInfo.uncompressed_size;
    }
    return time;
}

static int receive_data_logic(int fd, int revents, const struct sr_dev_inst *sdi)
{
    struct session_vdev *vdev = NULL;
    struct sr_datafeed_packet packet;
    struct sr_datafeed_logic logic;

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
//    assert(vdev->archive);

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

    if (vdev->packet_buffer == NULL){
        vdev->cur_block = 0;

        vdev->packet_buffer = g_try_malloc0(sizeof(struct session_packet_buffer));
        if (vdev->packet_buffer == NULL){
            sr_err("%s: vdev->packet_buffer malloc failed", __func__);
            return SR_ERR_MALLOC;
        }
    }
    pack_buffer = vdev->packet_buffer;


    // Make packet.
    read_chan_index = 0;
    dir_index = 0;

    if(post_data_len >= vdev->total_samples/8)
    {
        bToEnd = 1;
    }
 
    if(!bToEnd)
    {
        packet.type = SR_DF_LOGIC;
        packet.payload = &logic;
        logic.format = LA_CROSS_DATA;
        logic.index = 0;
        logic.order = 0;
        logic.length = chan_num * packet_len;
        post_data_len += logic.length / enabled_probe_num;
        if(post_data_len >= vdev->total_samples/8)
        {
            int last_packet_len = post_data_len - (logic.length / enabled_probe_num);
            last_packet_len = (vdev->total_samples/8) - last_packet_len;
            logic.length = last_packet_len * enabled_probe_num;
        }

        uint64_t random = vdev->logic_buf_len - logic.length;
        random = abs(rand()) %random;
        logic.data = vdev->logic_buf + random;

        gdouble packet_elapsed = g_timer_elapsed(packet_interval, NULL);
        gdouble waittime = packet_time - packet_elapsed;
        if(waittime > 0)
        {
            g_usleep(waittime*1000000);
        }
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
    }
    pack_buffer = vdev->packet_buffer;

    if(pack_buffer->post_buf_len != chan_num * packet_len)
    {
        if (sdi->mode == LOGIC)
            pack_buffer->post_buf_len = chan_num * packet_len;
        if(pack_buffer->post_buf != NULL){
            g_free(pack_buffer->post_buf);
            pack_buffer->post_buf = g_try_malloc0(pack_buffer->post_buf_len + 1);
            if (pack_buffer->post_buf == NULL){
            sr_err("%s: pack_buffer->post_buf malloc failed", __func__);
            return SR_ERR_MALLOC;
        }
        }
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
                                g_free(pack_buffer->block_bufs[malloc_chan_index]);
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
        post_data_len += logic.length / enabled_probe_num;
        if(post_data_len >= vdev->total_samples/8)
        {
            int last_packet_len = post_data_len - (logic.length / enabled_probe_num);
            last_packet_len = (vdev->total_samples/8) - last_packet_len;
            logic.length = last_packet_len * enabled_probe_num;
        }
        logic.data = pack_buffer->post_buf;
        
        gdouble packet_elapsed = g_timer_elapsed(packet_interval, NULL);
        gdouble waittime = packet_time - packet_elapsed;
        if(waittime > 0){
           g_usleep(waittime*1000000);
        }

        ds_data_forward(sdi, &packet);
        pack_buffer->post_len = 0;
    }

    if (bToEnd || revents == -1)
    {
        test = FALSE;
        packet.type = SR_DF_END;
        ds_data_forward(sdi, &packet);
        sr_session_source_remove(-1);
        close_archive(vdev);
    }

    return TRUE;
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
    // assert(vdev->archive);

    //2 probe
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

        if (sdi->mode == LOGIC)
            vdev->packet_buffer->post_buf_len = 8 * chan_num * 1000;
        else
            vdev->packet_buffer->post_buf_len = chan_num * 10000;

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
    }
    pack_buffer = vdev->packet_buffer;

    // Make packet.
    read_chan_index = 0;
    dir_index = 0;

    if(vdiv_change)
    {
        while (pack_buffer->post_len < pack_buffer->post_buf_len)
        {
            if(cur_sample_generator == PATTERN_SINE)
            {
                for(int i = 0 ; i < 100;i++)
                {
                    int cur_l = i * sizeof(dso_sine_data);
                    memcpy(pack_buffer->post_buf + cur_l,dso_sine_data,sizeof(dso_sine_data));
                }
                pack_buffer->post_len = pack_buffer->post_buf_len;
            }
            else if(cur_sample_generator == PATTERN_SQUARE)
            {
                for(int i = 0 ; i < 100;i++)
                {
                    int cur_l = i * sizeof(dso_square_data);
                    memcpy(pack_buffer->post_buf + cur_l,dso_square_data,sizeof(dso_square_data));
                }
                pack_buffer->post_len = pack_buffer->post_buf_len;

            }
            else if(cur_sample_generator == PATTERN_TRIANGLE)
            {
                for(int i = 0 ; i < 100;i++)
                {
                    int cur_l = i * sizeof(dso_triangle_data);
                    memcpy(pack_buffer->post_buf + cur_l,dso_triangle_data,sizeof(dso_triangle_data));
                }
                pack_buffer->post_len = pack_buffer->post_buf_len;

            }
            else if(cur_sample_generator == PATTERN_SAWTOOTH)
            {
                for(int i = 0 ; i < 100;i++)
                {
                    int cur_l = i * sizeof(dso_sawtooth_data);
                    memcpy(pack_buffer->post_buf + cur_l,dso_sawtooth_data,sizeof(dso_sawtooth_data));
                }
                pack_buffer->post_len = pack_buffer->post_buf_len;

            }
            else if(cur_sample_generator == PATTERN_RANDOM)
            {
                for(int i = 0 ; i < 100;i++)
                {
                    int cur_l = i * sizeof(dso_random_data);
                    memcpy(pack_buffer->post_buf + cur_l,dso_random_data,sizeof(dso_random_data));
                }
                pack_buffer->post_len = pack_buffer->post_buf_len;
            }
        }

        for(int i = 0 ; i < pack_buffer->post_buf_len; i++)
        {
            tem = 0;
            if(i % 2 == 0)
            {
                probe = g_slist_nth(sdi->channels, 0)->data;
            }
            else
            {
                probe = g_slist_nth(sdi->channels, 1)->data;
                    
            }
            vdiv = probe->vdiv;
            uint8_t temp_val = *((uint8_t*)pack_buffer->post_buf + i);
            if(temp_val>128)
            {
                val = temp_val - 128;
                if(vdiv == 2000)
                {
                    tem = val/2;
                    tem = 128 + tem;
                }
                else if(vdiv == 500)
                {
                    tem = val*2;
                    tem = 128 + tem;
                }
                else if(vdiv == 200)
                {
                    tem = val*5;
                    tem = 128 + tem;
                }
                else if(vdiv == 100)
                {
                    tem = val*10;
                    tem = 128 + tem;
                }
                else if(vdiv == 50)
                {
                    tem = val*20;
                    tem = 128 + tem;
                }
                else if(vdiv == 20)
                {
                    tem = val*50;
                    tem = 128 + tem;
                }
                else if(vdiv == 10)
                {
                    tem = val*100;
                    tem = 128 + tem;
                }
                else if (vdiv == 1000)
                    tem = temp_val;
                if(tem >= 255)
                    temp_val = 255;
                else
                    temp_val = tem;
            }
            else if(temp_val < 128 && temp_val != 0)
            {
                val = 128 - temp_val;
                if(vdiv == 2000)
                {
                    tem = val/2;
                    tem = 128 - tem;
                }
                else if(vdiv == 500)
                {
                    tem = val*2;
                    tem = 128 - tem;
                }
                else if(vdiv == 200)
                {
                    tem = val*5;
                    tem = 128 - tem;
                }
                else if(vdiv == 100)
                {
                    tem = val*10;
                    tem = 128 - tem;
                }
                else if(vdiv == 50)
                {
                    tem = val*20;
                    tem = 128 - tem;
                }
                else if(vdiv == 20)
                {
                    tem = val*50;
                    tem = 128 - tem;
                }
                else if(vdiv == 10)
                {
                    tem = val*100;
                    tem = 128 - tem;
                }
                else if (vdiv == 1000)
                    tem = temp_val;
                if(tem == 0)
                    temp_val = 0;
                else
                    temp_val = tem;
            }
            *((uint8_t*)pack_buffer->post_buf + i) = temp_val;
        }
        vdiv_change = FALSE;
    }    
   
   uint8_t top0;
   uint8_t top1;
   if(cur_sample_generator == PATTERN_RANDOM)
   {
       top0 = *((uint8_t*)pack_buffer->post_buf + 19998);
       top1 = *((uint8_t*)pack_buffer->post_buf + 19999);
   }
   else
   {
       top0 = *((uint8_t*)pack_buffer->post_buf + 198);
       top1 = *((uint8_t*)pack_buffer->post_buf + 199);
   }


   for(int i = pack_buffer->post_len -1;  i > 1; i -= 2){
       *((uint8_t*)pack_buffer->post_buf + i) = *((uint8_t*)pack_buffer->post_buf + i - 2);
   }

   for(int i = pack_buffer->post_len -2;  i > 0; i -= 2){
       *((uint8_t*)pack_buffer->post_buf + i) = *((uint8_t*)pack_buffer->post_buf + i - 2);
   }

   *(uint8_t*)pack_buffer->post_buf = top0;
   *((uint8_t*)pack_buffer->post_buf + 1)= top1;


    if (pack_buffer->post_len >= byte_align * chan_num)
    {
        packet.type = SR_DF_DSO;
        packet.payload = &dso;
        dso.probes = sdi->channels;
        dso.mq = SR_MQ_VOLTAGE;
        dso.unit = SR_UNIT_VOLT;
        dso.mqflags = SR_MQFLAG_AC;
        dso.num_samples = pack_buffer->post_len / chan_num;
        dso.data = pack_buffer->post_buf;
        
        gdouble packet_elapsed = g_timer_elapsed(packet_interval, NULL);
        gdouble waittime = packet_time - packet_elapsed;
        if(waittime > 0){
            g_usleep(waittime*1000000);
        }
        g_timer_start(packet_interval);
        // Send data back.
        ds_data_forward(sdi, &packet);
        // pack_buffer->post_len = 0;
    }

    if(instant)
    {
        bToEnd = 1;
        instant = FALSE;
    }

    if (bToEnd || revents == -1)
    {
        packet.type = SR_DF_END;
        ds_data_forward(sdi, &packet);
        sr_session_source_remove(-1);
        // close_archive(vdev);
    }

    return TRUE;
}

static int dev_open_file(struct sr_dev_inst *sdi)
{
    int ret;

    assert(sdi);

   if (sdi->status == SR_ST_ACTIVE){
       return SR_OK;
   }

   check_file(sdi);

    struct session_vdev *vdev = sdi->priv;
    if(cur_sample_generator != pre_sample_generator)
    {
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
        
        vdev->ref_min = 0;
        vdev->ref_max = 0;

        vdev->timebase = SR_NS(500);
        vdev->max_timebase = MAX_TIMEBASE;
        vdev->min_timebase = MIN_TIMEBASE;

        vdev->mstatus.measure_valid = TRUE;
        vdev->archive = NULL;
        vdev->capfile = 0;
        vdev->packet_buffer = NULL;
        vdev->logic_buf = NULL;
        sdi->status = SR_ST_ACTIVE;

        init_data(sdi);

        ret = load_virtual_device_session(sdi);
        if (ret != SR_OK)
        {
            sr_err("%s", "Error!Load session file failed.");
            return ret;
        }

        pre_sample_generator = cur_sample_generator;
        is_change = TRUE;
    }
    else if(cur_mode != pre_mode)
    {
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
        vdev->ref_min = 0;
        vdev->ref_max = 0;

        vdev->timebase = SR_NS(500);
        vdev->max_timebase = MAX_TIMEBASE;
        vdev->min_timebase = MIN_TIMEBASE;

        vdev->mstatus.measure_valid = TRUE;
        vdev->archive = NULL;
        vdev->capfile = 0;
        vdev->packet_buffer = NULL;
        vdev->logic_buf = NULL;
        sdi->status = SR_ST_ACTIVE;

        init_data(sdi);

        ret = load_virtual_device_session(sdi);
        if (ret != SR_OK)
        {
            sr_err("%s", "Error!Load session file failed.");
            return ret;
        }

        pre_mode = cur_mode;
        is_change = TRUE;
    }
    
    return SR_OK;
}

static int dev_close_file(struct sr_dev_inst *sdi)
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
                    g_free(pack_buf->block_bufs[i]);
                    pack_buf->block_bufs[i] = NULL;
                }
                else{
                    break;
                }
            }
        }

        g_safe_free(vdev->packet_buffer);
        g_safe_free(vdev->buf);
        g_safe_free(vdev->logic_buf);

        sdi->status = SR_ST_INACTIVE;
        return SR_OK;
    }

    return SR_ERR_CALL_STATUS;
}

static int config_get_file(int id, GVariant **data, const struct sr_dev_inst *sdi,
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
    case SR_CONF_TEST:
        *data = g_variant_new_boolean(is_change);
        break;
    case SR_CONF_AUTO_OPEN:
        *data = g_variant_new_boolean(is_auto_open);
        break;
    case SR_CONF_INSTANT:
        *data = g_variant_new_boolean(instant);
        break;
    case SR_CONF_PATTERN_MODE:
        if(sdi->mode == LOGIC)
            *data = g_variant_new_string(pattern_strings_logic[cur_sample_generator]);
        else
            *data = g_variant_new_string(pattern_strings_dso[cur_sample_generator]);
        break;
    case SR_CONF_MAX_HEIGHT:
        *data = g_variant_new_string(maxHeights_s[vdev->max_height]);
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
        if(sdi->mode == DSO || (sdi->mode == LOGIC && cur_sample_generator != PATTERN_RANDOM))
        {
            *data = g_variant_new_uint64(vdev->total_samples);
        }
        else if(sdi->mode == ANALOG)
        {
            *data = g_variant_new_uint64(SR_MHZ(12.5));
        }
        else
        {
            *data = g_variant_new_uint64(SR_MHZ(100));
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
        break;
    default:
        return SR_ERR_NA;
    }

    return SR_OK;
}

static int config_set_file(int id, GVariant *data, struct sr_dev_inst *sdi,
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
        if(sdi->mode == LOGIC && cur_sample_generator >PATTERN_RANDOM)
        {
            samplerates_file[0] = vdev->samplerate;
        }
        sr_dbg("Setting samplerate to %llu.", vdev->samplerate);
        break;
    case SR_CONF_LIMIT_SAMPLES:
        vdev->total_samples = g_variant_get_uint64(data);
        if(sdi->mode == LOGIC && cur_sample_generator >PATTERN_RANDOM)
        {
            samplecounts_file[0] = vdev->total_samples;
        }
        sr_dbg("Setting limit samples to %llu.", vdev->total_samples);
        break;
    case SR_CONF_LIMIT_MSEC:
        break;
    case SR_CONF_DEVICE_MODE:
        set_mode(sdi,g_variant_get_int16(data));
        break;
    case SR_CONF_PATTERN_MODE:
        stropt = g_variant_get_string(data, NULL);
        if (!strcmp(stropt, pattern_strings_dso[PATTERN_SINE])) {
            set_sample_generator(sdi,PATTERN_SINE);
        } else if (!strcmp(stropt, pattern_strings_dso[PATTERN_SQUARE])) {
            set_sample_generator(sdi,PATTERN_SQUARE);
        } else if (!strcmp(stropt, pattern_strings_dso[PATTERN_TRIANGLE])) {
            set_sample_generator(sdi,PATTERN_TRIANGLE);
        } else if (!strcmp(stropt, pattern_strings_dso[PATTERN_SAWTOOTH])) {
            set_sample_generator(sdi,PATTERN_SAWTOOTH);
        } else if (!strcmp(stropt, pattern_strings_logic[PATTERN_RANDOM])) {
            set_sample_generator(sdi,PATTERN_RANDOM);
        } else if(!strcmp(stropt, pattern_strings_logic[PATTERN_UART])){
            set_sample_generator(sdi,PATTERN_UART);
        } else if(!strcmp(stropt, pattern_strings_logic[PATTERN_SPI])){
            set_sample_generator(sdi,PATTERN_SPI);
        } else if(!strcmp(stropt, pattern_strings_logic[PATTERN_EEPROM])){
            set_sample_generator(sdi,PATTERN_EEPROM);
        }
        else{
            sr_err("Unknown pattern: %s.", stropt);
            return SR_ERR_NA;
        }
        sdi->status = SR_ST_INACTIVE;
        
        sr_dbg("%s: setting pattern to %d",
            __func__, cur_sample_generator);
        break;
   
    case SR_CONF_MAX_HEIGHT:
        stropt = g_variant_get_string(data, NULL);
        for (i = 0; i < ARRAY_SIZE(maxHeights_s); i++)
        {
            if (!strcmp(stropt, maxHeights_s[i]))
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
    case SR_CONF_AUTO_OPEN:
        is_auto_open = g_variant_get_boolean(data);
        if(!is_auto_open)
        {
            char str[500];
            cur_mode = pre_mode = sdi->mode = LOGIC;
            cur_sample_generator = PATTERN_UART;
            pre_sample_generator = PATTERN_RANDOM;
            strcpy(str,DS_RES_PATH);
            sdi->path = g_strdup(strcat(str,"demo/demo_uart.dsl"));
        }
        break;
    case SR_CONF_TEST:
        is_change = g_variant_get_boolean(data);
        break;
    default:
        sr_err("Unknown capability: %d.", id);
        return SR_ERR_NA;
    }

    return SR_OK;
}

static int config_list_file(int key, GVariant **data,
                       const struct sr_dev_inst *sdi,
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
        if(sdi->mode == LOGIC && cur_sample_generator != PATTERN_RANDOM)
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
            *data = g_variant_new_strv(pattern_strings_logic, ARRAY_SIZE(pattern_strings_logic));
        }
        else
        {
            *data = g_variant_new_strv(pattern_strings_dso, ARRAY_SIZE(pattern_strings_dso));
        }
        break;
    case SR_CONF_MAX_HEIGHT:
        *data = g_variant_new_strv(maxHeights_s, ARRAY_SIZE(maxHeights_s));
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

static int dev_acquisition_start_file(struct sr_dev_inst *sdi, void *cb_data)
{
    (void)cb_data;

    struct session_vdev *vdev;
    struct sr_datafeed_packet packet;
    int ret;
    GSList *l;
    struct sr_channel *probe;

    assert(sdi);
    assert(sdi->priv);

    if(sdi->mode == LOGIC)
    {
        is_auto_open = TRUE;
    }

    vdev = sdi->priv;
    vdev->enabled_probes = 0;
    packet.status = SR_PKT_OK;

    vdev->cur_block = 0;
    vdev->cur_channel = 0;

    if (sdi->mode == LOGIC && cur_sample_generator != PATTERN_RANDOM)
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
        enabled_probe_num = 0;
        for(GSList *l = sdi->channels; l; l = l->next)
        {
            probe = (struct sr_channel *)l->data;
            if(probe->enabled)
            {
                enabled_probe_num++;
            }
        }
        is_first = TRUE;
        post_data_len = 0;
        uint64_t post_date_per_second = vdev->samplerate / 8 ;
        packet_len = post_date_per_second / 22;
        packet_len = floor(packet_len/8)*8;
        packet_time = 1/(double)22;
        if(cur_sample_generator == PATTERN_RANDOM)
        {
            sr_session_source_add(-1, 0, 0, receive_data_logic, sdi);
        }
        else
        {
            sr_session_source_add(-1, 0, 0, receive_data_logic_decoder, sdi);
        }
    }
    else if(sdi->mode == DSO)
    {
        is_first = TRUE;
        vdiv_change = TRUE;
        packet_time = 1/(double)200;
        sr_session_source_add(-1, 0, 0, receive_data_dso, sdi);
    }
    else if(sdi->mode == ANALOG)
    {
        is_first = TRUE;
        gdouble total_time = vdev->total_samples/vdev->samplerate;
        uint64_t post_date_per_second = vdev->total_samples / total_time *2;
        packet_len = post_date_per_second / 22;

        if(packet_len <= 1)
        {
            packet_len = 2;
            packet_time = post_date_per_second / 2;
            packet_time = 1/packet_time;
        }
        else
        {
            if (packet_len %2 != 0)
            {
                packet_len += 1;
            }
            packet_time = 1/(double)22;
        }

        vdev->buf_len = 0;
        analog_read_pos = 0;

        sr_session_source_add(-1, 0, 0, receive_data_analog, sdi);
    }

    return SR_OK;
}

static int dev_acquisition_stop_file(const struct sr_dev_inst *sdi, void *cb_data)
{
    struct session_vdev *vdev = sdi->priv;
    struct sr_datafeed_packet packet;
    packet.status = SR_PKT_OK;

    if(sdi->mode == DSO || sdi->mode == ANALOG)
    {
        packet.type = SR_DF_END;
        ds_data_forward(sdi, &packet);
        close_archive(vdev);
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

    if (sdi->mode == LOGIC && cur_sample_generator == PATTERN_RANDOM)
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
        adjust_samplerate_file(sdi);
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
            probe->vdiv = 1;
            probe->hw_offset = 128;
            probe->offset = 128;
            probe->coupling = 1;
            probe->vfactor = 1000;
            probe->map_default = TRUE;
            probe->map_unit = "V";
            probe->map_max = 5.000000;
            probe->map_max = -5.000000;

            sdi->channels = g_slist_append(sdi->channels, probe);
        }
        adjust_samplerate_file(sdi);
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
            sdi->channels = g_slist_append(sdi->channels, probe);
        }
        adjust_samplerate_file(sdi);
    }
    else
    {
        
        assert(sdi->path);

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
                    sr_info("keys:%s , val:%s", keys[j],val);

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
                adjust_samplerate_file(sdi);
                g_strfreev(keys);
            }
            devcnt++;
        }

        g_strfreev(sections);
        g_key_file_free(kf);
        g_free(metafile);
    }

    return SR_OK;
}

