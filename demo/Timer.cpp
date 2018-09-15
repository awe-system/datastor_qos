//
// Created by root on 18-9-15.
//

#include <signal.h>
#include <sys/time.h>
#include <thread>
#include "Timer.h"
#include "demo.h"


static void *thread_func(void *data );

class Timer_thread
{
public:
    mutex m;
    list<Timer *> timer_list;
    bool is_stop;
    Timer_thread():is_stop(false)
    {
        std::thread *t = new thread(thread_func,this);
    }
};

static void* thread_func(void *data )
{
    Timer_thread * thread = (Timer_thread *) data;
    while(!thread->is_stop)
    {
        list<Timer *> new_timer_list;
        std::unique_lock<std::mutex> lck(thread->m);
        while(!thread->timer_list.empty())
        {
            Timer *t = thread->timer_list.front();
            thread->timer_list.pop_front();
            lck.unlock();
            t->timer_on();
            lck.lock();
        }
        lck.unlock();
        usleep(1);
    }
    return NULL;
}

Timer_thread th;


static void real_start(Timer * t)
{
    std::unique_lock<std::mutex> lck(th.m);
    th.timer_list.push_back(t);
}

Timer::Timer(Timercallback *_cb) :cb(_cb),is_pending(false)
{
}

bool Timer::try_start(long _usecs)
{
    if(__sync_bool_compare_and_swap(&is_pending, false, true))
    {
        usecs = _usecs;
        last_start.record();
        real_start(this);
        return true;
    }
    return false;
}

void Timer::timer_on()
{
    utime now_time;
    if(now_time.usec_elapsed_since(last_start) >= usecs )
    {
        assert(__sync_bool_compare_and_swap(&is_pending,true, false));
        cb->timer_on();
    }
    else
    {
        real_start(this);
    }
   
}
