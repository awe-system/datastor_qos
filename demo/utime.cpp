//
// Created by root on 18-9-15.
//

#include <time.h>
#include <chrono>
#include "utime.h"

void utime::record()
{
    tp = std::chrono::system_clock::now();
}

long utime::usec_elapsed_since(utime &last_time)
{
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            tp - last_time.tp);
    return duration.count();
}

utime::utime()
{
    record();
}

utime::utime(const utime &other)
{
    tp = other.tp;
}

