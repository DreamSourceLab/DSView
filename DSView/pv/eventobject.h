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

#ifndef _EVENT_OBJECT_H
#define _EVENT_OBJECT_H

#include <QObject>

class EventObject : public QObject
{
    Q_OBJECT

public:
    EventObject(); 


signals:
    void show_error(QString error);
    void capture_state_changed(int state);
    void data_updated();

    void session_error();
    void signals_changed();
    void receive_trigger(quint64 trigger_pos);
    void frame_ended();
    void frame_began();

    void decode_done();
    void receive_data_len(quint64 len);
    void cur_snap_samplerate_changed();
    void trigger_message(int msg);
};


class DeviceEventObject : public QObject
{
    Q_OBJECT

public:
    DeviceEventObject(); 


signals: 
    void device_updated();
};

#endif