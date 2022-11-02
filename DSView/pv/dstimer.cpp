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

#include "dstimer.h" 
#include <assert.h>

 
DsTimer::DsTimer()
{
    _binded = false;
    _isActived = false;
}

void DsTimer::on_timeout()
{
     _call(); //call back
}

void DsTimer::TimeOut(int millsec, CALLBACL_FUNC f)
{
    _call = f;
    QTimer::singleShot(millsec, this, SLOT(on_timeout()));
}

void DsTimer::TimeOut(int millsec)
{
    if (_call == NULL){
        assert(false);
    }
    QTimer::singleShot(millsec, this, SLOT(on_timeout()));
}

void DsTimer::Start(int millsec, CALLBACL_FUNC f)
 {  
     if (_isActived)
        return;

     _call = f;

     if (!_binded){
        _binded = true;
        connect(&_timer, SIGNAL(timeout()), this, SLOT(on_timeout())); 
     }
     
     _timer.start(millsec);
     _isActived = true;
     _beginTime = high_resolution_clock::now();
}

void DsTimer::SetCallback(CALLBACL_FUNC f)
 {
    _call = f;

    if(!_binded){
        _binded = true;
        connect(&_timer, SIGNAL(timeout()), this, SLOT(on_timeout())); 
    }  
 }

 void DsTimer::Start(int millsec)
 {  
     //check if connect
     assert(_binded);

     if (_isActived)
        return;
     
    _timer.start(millsec);
    _isActived = true;
    _beginTime = high_resolution_clock::now();
 }

 void DsTimer::Stop()
 {
     if (_isActived)
        _timer.stop();
     _isActived = false;
 }

 long long DsTimer::GetActiveTimeLong()
 {
    high_resolution_clock::time_point endTime = high_resolution_clock::now();
    milliseconds timeInterval = std::chrono::duration_cast<milliseconds>(endTime - _beginTime);
    return timeInterval.count();
 }

 void DsTimer::ResetActiveTime()
 {
     _beginTime = high_resolution_clock::now();
 }
