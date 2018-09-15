//
// Created by root on 18-9-13.
//
#include "client.h"
#include "server.h"

void server::submit_task(task *t)
{
    unique_lock<mutex> lck(m);
    if (!could_rcv(t) )
    {
        ++t->retry_num;
        lck.unlock();
        complete_task(t);
    }
    else
    {
        t->set_stat(task_stat_wait_disk);
        pending_task.push_back(t);
        t->retry_num = 0;
        rs += t->cost;
        lck.unlock();
        hook->tasks_submited();
    }
}

bool server::run_once(long &usecs_guess_wake_up)
{
    usecs_guess_wake_up = 0;
    unique_lock<mutex> lck(m);
    if(pending_task.empty())
        return false;
    task * t = pending_task.front();
    utime cur_time;
    long usec_eslape = cur_time.usec_elapsed_since(t->wait_disk_point);
    if(usec_eslape < usecs_perio)
    {
        usecs_guess_wake_up = usecs_perio - usec_eslape;
        return false;
    }
    
    pending_task.pop_front();
    rs -= t->cost;
    lck.unlock();
    complete_task(t);
    return true;
}

json_obj server::to_json_obj()
{
    std::unique_lock<std::mutex> lck(m);
    json_obj obj("rs_hold",rs);
    json_obj obj_array;
    obj_array.set_array();
    for(auto t : pending_task)
    {
        obj_array.append(t->to_json_obj());
    }
    obj.merge(json_obj("pending_task",obj_array));
    return obj;
}

string server::name() const
{
    return srv_name;
}

server::server(const string &_name, bool qos_open, int max_resource_num,
               int usec_per, server_submit_hook *_hook) :
        srv_name(_name), usecs_perio(usec_per), max_rs(max_resource_num),
        is_qos_open(qos_open),rs(0),hook(_hook)
{
}

void server::complete_task(task *t)
{
    t->set_stat(task_stat_net_rcv);
    t->cli->complete_task(t);
}

bool server::could_rcv(task *t)
{
    if(is_qos_open)
    {
        //FIXME
        return t->cost + rs <= max_rs;
    }
    else
        return  t->cost + rs <= max_rs;
}
