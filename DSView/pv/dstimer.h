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

#ifndef _DS_TIMER_H
#define _DS_TIMER_H

#include <QObject>
#include <functional>
#include <QTimer>
#include <chrono>

using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

typedef std::function<void()> CALLBACL_FUNC;


class DsTimer : protected QObject
{
     Q_OBJECT

public:
    DsTimer();

    void TimeOut(int millsec, CALLBACL_FUNC f);

    void TimeOut(int millsec);

    void SetCallback(CALLBACL_FUNC f);

    void Start(int millsec, CALLBACL_FUNC f);

    void Start(int millsec);

    void Stop(); 

    inline bool IsActived(){
        return _isActived;
    }

    long long GetActiveTimeLong();

    void ResetActiveTime();

private slots:
    void on_timeout();

private:    
    QTimer          _timer;
    bool            _binded;
    bool            _isActived;
    high_resolution_clock::time_point _beginTime;
    CALLBACL_FUNC   _call;
};

#endif
