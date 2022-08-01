
#include <libsigrok.h>
#include "../log.h"
#include <stdio.h>

#undef LOG_PREFIX 
#define LOG_PREFIX "test_main: "

int main()
{
    int ret = 0;

    if ((ret = sr_lib_init()) != SR_OK)
    {
        return 0;
    }   
    sr_info("%s", "start.");

    char c = 0;

    while (1)
    {
       c = getchar();

       if (c == 'x'){
         sr_lib_exit();
         sr_info("%s", "exit.");
         break;
       }
    }
    

    return 0;
}