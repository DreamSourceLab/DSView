
#ifndef _DS_TIMER_H
#define _DS_TIMER_H

#include <QObject>
#include <functional>
#include <QTimer>

typedef std::function<void()> CALLBACL_FUNC;

class DsTimer : public QObject
{
    Q_OBJECT

public:
    DsTimer();

    void TimeOut(int millsec, CALLBACL_FUNC f);

    void SetCallback(CALLBACL_FUNC f);

    void Start(int millsec, CALLBACL_FUNC f);

    void Start(int millsec);

    void Stop(); 

private slots:
    void on_timeout();

private:
    CALLBACL_FUNC   _call;
    QTimer          _timer;
    bool            _binded;
};

#endif
