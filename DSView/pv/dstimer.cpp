
#include "dstimer.h" 
#include <assert.h>

DsTimer::DsTimer(){
    _binded = false;
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

void DsTimer::SetCallback(CALLBACL_FUNC f)
 {
    assert(!_binded);
    _binded = true;

    _call = f;
    connect(&_timer, SIGNAL(timeout()), this, SLOT(on_timeout()));
 }

 void DsTimer::Start(int millsec, CALLBACL_FUNC f)
 {
     assert(!_binded);
     _binded = true;

     _call = f; 
      connect(&_timer, SIGNAL(timeout()), this, SLOT(on_timeout()));
     _timer.start(millsec);
 }

 void DsTimer::Start(int millsec)
 {  
     //check if connectb
     assert(_binded);
    _timer.start(millsec);
 }

 void DsTimer::Stop()
 {
     _timer.stop();
 }
