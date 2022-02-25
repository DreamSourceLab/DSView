
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
