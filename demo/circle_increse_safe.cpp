//
// Created by root on 18-9-14.
//

#include <assert.h>
#include "circle_increse_safe.h"

circle_increse_safe::circle_increse_safe(int _max_size):
        cnt(-1), max_size(_max_size)
{
}

int circle_increse_safe::get_next()
{
    assert(max_size>0);
    std::unique_lock<std::mutex> lck(m);
    if(cnt >= max_size-1)
    {
        cnt = -1;
    }
    return ++cnt;
}
