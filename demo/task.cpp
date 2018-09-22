//
// Created by root on 18-9-13.
//
#include <iostream>
#include <sysqos_common.h>
#include "client.h"
#include "server.h"
#include "task.h"

task::task(client *_cli, server *_srv, int _task_id, unsigned long _cost,
           task_group *tgrp)
        : cli(_cli), srv(_srv), task_id(_task_id), cost(_cost), retry_num(0),
          state(task_stat_out_standing), grp(tgrp)
{
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
//    cout << RED<<"init task ("<<RESET << task_id << ")" << cli->name() << " -> " << srv->name()
//            << ":"
//            << string_bytaskstat(state)
//            << "| retry_num " << retry_num
//            << endl;
}

string task::string_bytaskstat(task_stat stat) const
{
    switch ( stat )
    {
        case task_stat_out_standing:
            return "task_stat_out_standing";
        case task_stat_wait_token:
            return "task_stat_wait_token";
        case task_stat_net_send:
            return "task_stat_net_send";
        case task_stat_wait_disk:
            return "task_stat_wait_disk";
        case task_stat_net_rcv:
            return "task_stat_net_rcv";
        case task_stat_wait_grp:
            return "task_stat_wait_grp";
        case task_stat_complete:
            return "task_stat_complete";
    }
}

json_obj task::to_json_obj() const
{
    json_obj obj("tid", task_id);
    obj.merge(json_obj("client", cli->name()));
    obj.merge(json_obj("server", srv->name()));
    obj.merge(json_obj("cost", (long long) cost));
    obj.merge(json_obj("retry", (long long) retry_num));
    obj.merge(json_obj("stat", string_bytaskstat(state)));
    return obj;
}

void task::set_stat(task_stat new_stat)
{
//    cout << "task (" << task_id << ")" << cli->name() << " -> " << srv->name()
//            << ":"
//            << string_bytaskstat(state) << "->" << string_bytaskstat(new_stat)
//            << "| retry_num " << retry_num
//            << endl;
    unique_lock<mutex> lck(m);
    state = new_stat;
    
    switch ( new_stat )
    {
        case task_stat_out_standing:
            out_standing_point.record();
            break;
        case task_stat_wait_token:
            wait_token_point.record();
            break;
        case task_stat_net_send:
            net_send_point.record();
            break;
        case task_stat_wait_grp:
            wait_grp_point.record();
        case task_stat_wait_disk:
            wait_disk_point.record();
            break;
        case task_stat_net_rcv:
            net_rcv_point.record();
            break;
        case task_stat_complete:
            stat_complete_point.record();
            break;
    }
}

long task::usecs()
{
    return stat_complete_point.usec_elapsed_since(wait_token_point);
}

bool task::is_final()
{
    unique_lock<mutex> lck(m);
    return state == task_stat_complete;
}

bool task::is_wait_grp()
{
    unique_lock<mutex> lck(m);
    return state == task_stat_wait_grp;
}

