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

#pragma once

#include <string>

struct sr_context;

namespace pv{
    class DeviceManager;
    class SigSession;
}

class AppControl
{
private:
    explicit AppControl();
    ~AppControl();
    AppControl(AppControl &o);

public:
    static AppControl* Instance();

    void Destroy();

    bool Init();

    bool Start();

    void Stop();

    void UnInit();

    const char* GetLastError();

    void SetLogLevel(int level);

    inline pv::SigSession*  GetSession()
        { return _session;}

    inline pv::DeviceManager& GetDeviceManager()
        { return *_device_manager;}

public:
    std::string        _open_file_name; 

private:
    std::string         m_error;
    struct sr_context   *sr_ctx;
    pv::DeviceManager   *_device_manager;
    pv::SigSession      *_session;
};
