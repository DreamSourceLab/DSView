
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
    virtual void show_error(QString error)=0;
    virtual void session_error()=0;
    virtual void capture_state_changed(int state)=0;
    virtual void device_attach()=0;
    virtual void device_detach()=0;

    virtual void session_save()=0;
    virtual void data_updated()=0;
    virtual void repeat_resume()=0;
    virtual void update_capture()=0;
    virtual void cur_snap_samplerate_changed()=0;

    virtual void device_setted()=0;
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
    virtual void data_received()=0;
  
};

class ISessionDataGetter
{
public:
    virtual bool genSessionData(std::string &str) = 0;
};

#endif
