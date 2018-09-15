//
// Created by root on 18-9-15.
//

#ifndef DATASTOR_QOS_TIMER_H
#define DATASTOR_QOS_TIMER_H

#include <mutex>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include "utime.h"

class Timercallback
{
public:
    virtual void timer_on() = 0;
};

class Timer
{
    bool is_pending;
    utime last_start;
    long usecs;
public:
    Timercallback *cb;
public:
    Timer(Timercallback *_cb);
    void timer_on();
    bool try_start(long _usecs);
    
};

#endif //DATASTOR_QOS_TIMER_H
