
#include <libsigrok.h>
#include "../log.h"
#include <stdio.h>
#include <QDebug>
#include <log/xlog.h>
#include <QString>

#include <chrono>
#include <thread>
#include <vector>

using namespace std;

#undef LOG_PREFIX 
#define LOG_PREFIX "test_main: "

int b_exit = 0;

void print_log(const char *data, int len){
  if (len > 0){
    //*(data + len-1) = 0;
    QString s(data);
   // qDebug()<<s;
  } 
}


void ctrl()
{
   char c = 0;

   qDebug()<<"wait command:";
    int i = 0;

    while (!b_exit)
    {   
       c = getchar();
       if (c == 'x'){    
         sr_info("exit.");
         break;
       }
       qDebug()<<"b";
        this_thread::sleep_for(chrono::milliseconds(2000));
    }

   qDebug()<<"exit command waitting";
}

int main()
{ 
    xlog_context *log_ctx = xlog_new2(0);

    ds_log_level(XLOG_LEVEL_INFO);
    ds_log_set_context(log_ctx);

    xlog_add_receiver(log_ctx, print_log, 0);

    sr_lib_init();    

    thread t1(ctrl);
    t1.join();    
    
    sr_lib_exit();
    qDebug()<<"exit.";

    return 0;
}
