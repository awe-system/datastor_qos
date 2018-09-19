//
// Created by root on 18-9-13.
//

#include <unistd.h>
#include "server_group.h"

void server_group::run()
{
    
    while ( !is_stop )
    {
        long      min_time_usecs = 0;
        for ( int idle_num       = 0; idle_num < max_num;
              ++idle_num )
        {
            long guess_usecs = 0;
            if ( servers_vec[servernum.get_next()]->run_once(guess_usecs) )
            {
                idle_num = 0;
            }
            else if ( guess_usecs > 0 )
            {
                if ( min_time_usecs > 0 )
                {
                    min_time_usecs = min(min_time_usecs, guess_usecs);
                }
                else
                {
                    min_time_usecs = guess_usecs;
                }
            }
        }
        usleep(1);
//        if ( min_time_usecs > 0 )
//        {
//            usleep((__useconds_t) min_time_usecs);
//        }
//        else
//        {
//            usleep(1);
//        }
//
//        if ( min_time_usecs > 0 )
//        {
//            server_timer.try_start(min_time_usecs);
//        }
//        else
//        {
//            server_timer.try_start(100);
//        }
//        std::unique_lock<std::mutex> lck(m);
//        cond.wait(lck);
    }
}

json_obj server_group::to_json_obj() const
{
    json_obj obj;
    obj.set_array();
    for (auto s : servers)
    {
        obj.append(json_obj(s.first,s.second->to_json_obj()));
    }
    return obj;
}

server *server_group::server_byname(const string &name)
{
    auto it = servers.find(name);
    return it->second;
}

server_group::server_group(int max_server_num, bool is_qos_open, int max_rs,
                           int usec_perio) :
        servernum(max_server_num), max_num(max_server_num), server_timer(this)
{
    for ( int i = 0; i < max_server_num; ++i )
    {
        string srv_name = string("s") + to_string(i);
        server *srv     = new server(srv_name, is_qos_open, max_rs, usec_perio,
                                     this);
        servers.insert(make_pair(srv_name, srv));
        servers_vec.push_back(srv);
    }
}

void server_group::timer_on()
{
    wake_up_work();
}

void server_group::wake_up_work()
{
    std::unique_lock<std::mutex> lck(m);
    cond.notify_all();
}

void server_group::tasks_submited(void)
{
    wake_up_work();
}
