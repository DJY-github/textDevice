#ifndef _UTIL_TIMER_H_
#define _UTIL_TIMER_H_

#include   <stdio.h>
#include   <time.h>
#include   <signal.h>
#include "source_config.h"


#ifdef IM_UTIL_EXPORTS
#define UTILTIMER DLL_EXPORT
#else
#define UTILTIMER DLL_IMPORT
#endif

namespace util {


/*
注意：TIMER_HANDLE 会在新的线程中被调用
*/

typedef void (*TIMER_HANDLE)(union sigval  v);
class UTILTIMER IMTimer
{
public:
    IMTimer();
    ~IMTimer();

    bool setTimer(int sig_id, int seconds, TIMER_HANDLE handle);
    void delTimer();

private:
    timer_t     tid_;

};



}

#endif
