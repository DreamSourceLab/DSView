
#include "dstimer.h" 
#include <assert.h>

 
DsTimer::DsTimer(){
    _binded = false;
    _isActived = false;
}

void DsTimer::on_timeout(){
     _call(); //call back
}

void DsTimer::TimeOut(int millsec, CALLBACL_FUNC f)
{
    _call = f;
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
