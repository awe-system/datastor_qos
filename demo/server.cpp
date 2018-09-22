//
// Created by root on 18-9-13.
//
#include <awe_conf/env.h>
#include <thread>
#include "client.h"
#include "server.h"
#include "demo.h"

static env is_fix_quota("demo", "is_fix_quota");


void server::submit_to_disk(task *t)
{
    unique_lock<mutex> lck(m);
    if ( is_qos_open )
    {
        assert(!t->is_final());
        assert(t->state == task_stat_wait_grp);
    }
    if ( t->is_wait_grp() )
    {
        t->set_stat(task_stat_wait_disk);
//    if ( t->cli->name() == string("c5") )
//    {
//        cout << RED << "server::submit_to_disk (" << RESET << t->task_id << ")"
//                << t->cli->name() << " -> " << name()
//                << ": before push_back task"
//                << endl;
//    }
        disk_task.push_back(t);
    }
    
}

env max_server_piece("demo", "max_server_piece");

void server::submit_task(task *t)
{
//    if ( t->cli->name() == string("c5") )
//    {
//        cout << YELLOW << "submit_task (" << RESET << t->task_id << ")"
//                << t->cli->name() << " -> " << name()
//                << ": before unique_lock<mutex> lck(m)"
//                << endl;
//    }
    unique_lock<mutex> lck(m);
    rs += t->cost;
    t->set_stat(task_stat_wait_grp);
    waitgrp_task.push_back(t);
}


bool server::run_once(long &usecs_guess_wake_up)
{
    usecs_guess_wake_up = 0;
//    sleep(1);
//    cout << RED << "server::run_once"
//    << name()
//    << ": before  unique_lock<mutex> lck"
//    << endl;
    
    unique_lock<mutex> lck(m);
    if ( ++net_piece_cnt >= max_server_piece.get_int() )
    {
        net_last_percent = 100 * net_hit_cnt / max_server_piece.get_int();
        net_piece_cnt    = 0;
        net_hit_cnt      = 0;
    }
    
//    cout << YELLOW << "server::run_once"
//    << name()
//    << ": before  unique_lock<mutex> lck"
//    << endl;
    if ( waitgrp_task.empty() )
    {
//        cout << YELLOW << "server::run_once"
//                << name()
//                << ": pending_task.empty()"
//                << endl;
        return false;
    }
    ++net_hit_cnt;
    task *t = waitgrp_task.front();
//    if ( t->cli->name() == string("c5") )
//    {
//        cout << GREEN << "server::run_once (" << RESET << t->task_id << ")"
//                << t->cli->name() << " -> " << name()
//                << ": before set_stat(task_stat_net_rcv)"
//                << endl;
//    }
    waitgrp_task.pop_front();
    
    if ( t->grp->is_failed )
    {
        ++t->retry_num;
        t->set_stat(task_stat_net_rcv);
        rs -= t->cost;
        lck.unlock();
        complete_task(t);
        return true;
    }
    if ( !could_rcv(t) )
    {
        if ( is_qos_open )
        {
            abort();
        }
        ++t->retry_num;
        rs -= t->cost;
        t->set_stat(task_stat_net_rcv);
        lck.unlock();
        complete_task(t);
        return true;
    }
   
    if ( is_qos_open && !is_fix_quota.get_int() )
    {
        long len = t->cli->ops
                .snd_msg_len(&t->cli->ops, (void *) srv_name.c_str());
        assert(len <= 1024);
        unsigned char buf[1024];
        t->cli->ops
                .snd_msg_buf(&t->cli->ops, (void *) srv_name.c_str(), len, buf);
//        if ( t->cli->name() == string("c5") )
//        {
//            cout << RED << "server::run_once (" << RESET << t->task_id << ")"
//                    << t->cli->name() << " -> " << name()
//                    << ": before rcvd"
//                    << endl;
//        }
        ops.rcvd(&ops, (void *) t->cli->name().c_str(), len, buf);
//        if ( t->cli->name() == string("c5") )
//        {
//            cout << BLUE << "server::run_once (" << RESET << t->task_id << ")"
//                    << t->cli->name() << " -> " << name()
//                    << ": after rcvd"
//                    << endl;
//        }
    }
    if ( t->grp->is_grp_ready() )
    {
        lck.unlock();
        t->grp->set_stat(task_group_stat_wait_disk);
//        if ( t->cli->name() == string("c5") )
//        {
//            cout << BLUE << "server::run_once (" << RESET << t->task_id << ")"
//                    << t->cli->name() << " -> " << name()
//                    << ": is_grp_ready true"
//                    << endl;
//        }
    }
//    else
//    {
//        if ( t->cli->name() == string("c5") )
//        {
//            cout << BLUE << "server::run_once (" << RESET << t->task_id << ")"
//                    << t->cli->name() << " -> " << name()
//                    << ": not ready"
//                    << endl;
//        }
//    }
    return true;
}

json_obj server::to_json_obj()
{
    std::unique_lock<std::mutex> lck(m);
    json_obj                     obj("rs_hold", rs);
    json_obj                     grp_array, disk_array;
    grp_array.set_array();
    for ( auto t : waitgrp_task )
    {
        grp_array.append(t->to_json_obj());
    }
    obj.merge(json_obj("waitgrp_task", grp_array));
    lck.unlock();
    
    disk_array.set_array();
    std::unique_lock<std::mutex> disk_lck(disk_m);
    for ( auto                   t : disk_task )
    {
        disk_array.append(t->to_json_obj());
    }
    obj.merge(json_obj("waitdisk_task", disk_array));
    return obj;
}

string server::name() const
{
    return srv_name;
}

static void *disk_func(void *data)
{
    server *srv = (server *) data;
    while ( !srv->is_stop )
    {
        int cnt = 0;
        while ( ++cnt < 10 )
        {
            srv->sync_disks();
        }
//        usleep(1);
    }
}


server::server(const string &_name, bool qos_open, int max_resource_num,
               int usec_per, server_submit_hook *_hook) :
        srv_name(_name), usecs_perio(usec_per), max_rs(max_resource_num),
        is_qos_open(qos_open), rs(0), hook(_hook), is_stop(false),
        last_percent(0), piece_cnt(0), hit_cnt(0), net_piece_cnt(0),
        net_hit_cnt(0),net_last_percent(0)

{
assert(0 == sysqos_dispatch_init(&ops, demo_compare, demo_hash));
ops.
resource_increase(&ops, max_resource_num
);
thread *th = new thread(disk_func, this);
}

void server::complete_task(task *t)
{
    t->cli->complete_task(t);
}

bool server::could_rcv(task *t)
{
    if ( is_qos_open )
    {
        //FIXME alloc free 若不靠考虑 掉结点不会生效
        return t->cost + rs <= max_rs;
    }
    else
    {
        return t->cost + rs <= max_rs;
    }
}

void server::sync_disks()
{
    usleep(1);
    assert(max_server_piece.get_int());
    if ( ++piece_cnt >= max_server_piece.get_int() )
    {
        last_percent = 100 * hit_cnt / max_server_piece.get_int();
        piece_cnt    = 0;
        hit_cnt      = 0;
    }
    std::unique_lock<std::mutex> lck(m);
    if ( disk_task.empty() )
    {
        return;
    }
    
    task *t = disk_task.front();
    disk_task.pop_front();
    if ( !t->grp->is_failed )
    {
        ++hit_cnt;
    }
//    disk_lck.unlock();
//    std::unique_lock<std::mutex> lck(m);
    
    t->set_stat(task_stat_net_rcv);
    rs -= t->cost;
    assert(rs >= 0);
    lck.unlock();

//    if ( t->cli->name() == string("c5") )
//    {
//        cout << RED << "server::sync_disks (" << RESET << t->task_id << ")"
//                << t->cli->name() << " -> " << name()
//                << ": before complete task"
//                << endl;
//    }
    
    complete_task(t);
//    if ( t->cli->name() == string("c5") )
//    {
//        cout << BLUE << "server::sync_disks (" << RESET << t->task_id << ")"
//                << t->cli->name() << " -> " << name()
//                << ": after complete task"
//                << endl;
//    }
}

void server::try_to_erase_task(task *t)
{
    std::unique_lock<std::mutex> lck(m);
    if ( t->state == task_stat_net_send || t->state == task_stat_wait_token ||
         t->state == task_stat_out_standing || t->state == task_stat_complete )
    {
        return;
    }
    if ( t->state == task_stat_net_rcv )
    {
        return;
    }
    if ( t->state == task_stat_wait_grp )
    {
        for ( std::list<task *>::iterator it = waitgrp_task.begin();
              it != waitgrp_task.end(); ++it )
        {
            if ( (*it)->task_id == t->task_id )
            {
                waitgrp_task.erase(it);
                break;
            }
        }
        t->set_stat(task_stat_wait_disk);
        disk_task.push_back(t);
    }
    lck.unlock();
    //mute
}
