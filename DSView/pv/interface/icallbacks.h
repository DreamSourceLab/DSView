
/*
 * This file is part of the DSView project.
 * DSView is based on PulseView.
 *
 * Copyright (C) 2021 DreamSourceLab <support@dreamsourcelab.com>
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

#ifndef _I_CALLBACKS_
#define _I_CALLBACKS_


class ISessionCallback
{
public: 
    virtual void session_error()=0;
    virtual void session_save()=0;
    virtual void data_updated()=0;
    virtual void update_capture()=0;
    virtual void cur_snap_samplerate_changed()=0;
    virtual void signals_changed()=0;
    virtual void receive_trigger(quint64 trigger_pos)=0;
    virtual void frame_ended()=0;
    virtual void frame_began()=0;
    virtual void show_region(uint64_t start, uint64_t end, bool keep)=0;
    virtual void show_wait_trigger()=0;
    virtual void repeat_hold(int percent)=0;
    virtual void decode_done()=0;
    virtual void receive_data_len(quint64 len)=0;    
    virtual void receive_header()=0;
    virtual void trigger_message(int msg)=0;  
    virtual void delay_prop_msg(QString strMsg)=0;
};

class ISessionDataGetter
{
public:
    virtual bool genSessionData(std::string &str) = 0;
};


class IDlgCallback
{
public:
    virtual void OnDlgResult(bool bYes)=0;
};

class IMainForm{
public:
    virtual void switchLanguage(int language)=0;
};


#define DSV_MSG_START_COLLECT_WORK_PREV 5001
#define DSV_MSG_START_COLLECT_WORK      5002
#define DSV_MSG_COLLECT_START           5003
#define DSV_MSG_COLLECT_END             5004
#define DSV_MSG_END_COLLECT_WORK_PREV   5005
#define DSV_MSG_END_COLLECT_WORK        5006
#define DSV_MSG_REV_END_PACKET          5007

#define DSV_MSG_DEVICE_LIST_UPDATED     6000
#define DSV_MSG_BEGIN_DEVICE_OPTIONS    6001 //Begin show device options dialog.
#define DSV_MSG_END_DEVICE_OPTIONS      6002
#define DSV_MSG_DEVICE_OPTIONS_UPDATED  6003
#define DSV_MSG_DEVICE_DURATION_UPDATED 6004
#define DSV_MSG_DEVICE_MODE_CHANGED     6005
#define DSV_MSG_CURRENT_DEVICE_CHANGE_PREV  6006
#define DSV_MSG_CURRENT_DEVICE_CHANGED  6007
#define DSV_MSG_NEW_USB_DEVICE          6008
#define DSV_MSG_CURRENT_DEVICE_DETACHED 6009
#define DSV_MSG_DEVICE_CONFIG_UPDATED   6010
#define DSV_MSG_DEMO_OPERATION_MODE_CHNAGED   6011
#define DSV_MSG_COLLECT_MODE_CHANGED    6012
#define DSV_MSG_DATA_POOL_CHANGED       6013

#define DSV_MSG_TRIG_NEXT_COLLECT       7001
#define DSV_MSG_SAVE_COMPLETE           7002
#define DSV_MSG_STORE_CONF_PREV         7003

#define DSV_MSG_CLEAR_DECODE_DATA       8001

#define DSV_MSG_APP_OPTIONS_CHANGED     9001
#define DSV_MSG_FONT_OPTIONS_CHANGED    9002

class IMessageListener
{
public:
    virtual void OnMessage(int msg)=0;
};

class IDecoderPannel
{
public:
    virtual void update_deocder_item_name(void *trace_handel, const char *name)=0;
};

class IFontForm
{
public:
    virtual void update_font()=0;
};

#endif
