//
// Created by root on 18-9-13.
//

#include <iostream>
#include <sysqos_common.h>
#include <awe_conf/env.h>
#include "client.h"
#include "server.h"

void client::submit_tasks(task_group *task_grp)
{
    unique_lock<std::mutex> lck(m);
    tasks_list.push_back(task_grp);
}

bool client::could_snd(task_group *tgrp)
{
    if ( is_qos_open )
    {
        //FIXME
    }
    return true;
}
env demo_retry_interval("demo","demo_retry_interval");
bool client::run_once()
{
    unique_lock<std::mutex> lck(m);
    if ( tasks_list.empty() && complete_list.empty() )
    {
        return false;
    }
    
    if ( !complete_list.empty() )
    {
        task *t = complete_list.front();
        complete_list.pop_front();
//        cout << RED << "client::run_once " << RESET"(" << t->task_id << ")"
//                << name() << "->" << t->srv->name() << " retry_num("
//                << t->retry_num
//                << ")task_retry:" << task_retry << endl;
        if ( t->retry_num > 0 && t->retry_num < task_retry )
        {
            if(t->retry_num > 1)
            {
                t->retry_num = t->retry_num;
            }
            lck.unlock();
            if(demo_retry_interval.get_int() > 0)
            {
                usleep(demo_retry_interval.get_int());
            }
            retry_task(t);
            return true;
        }
        else if ( t->retry_num > 0 )
        {
            t->set_stat(task_stat_complete);
        }
        if ( final_task(t) )
        {
            lck.unlock();
            hook->task_final(t->grp);
            lck.lock();
        }
    }
    if ( !tasks_list.empty() )
    {
        task_group *tgrp = tasks_list.front();
        tasks_list.pop_front();
        if ( could_snd(tgrp) )
        {
            lck.unlock();
            snd_tasks(tgrp);
        }
    }
    return true;
}


string client::name() const
{
    return cli_name;
}

client::client(const string &_name, bool qos_open, int _grp_retry,
               int _task_retry, complete_task_hook *_hook) :
        cli_name(_name), is_qos_open(qos_open), hook(_hook),
        grp_retry(_grp_retry), task_retry(_task_retry)
{
}

void client::snd_tasks(task_group *tgrp)
{
    tgrp->set_stat(task_group_stat_wait_disk);
    for ( auto t : tgrp->tasks )
    {
        t->set_stat(task_stat_net_send);
        t->srv->submit_task(t);
    }
}

void client::retry_task(task *t)
{
    unique_lock<std::mutex> lck(m);
//    cout << RED << "client::retry_task " << RESET"(" << t->task_id << ")"
//            << name() << "->" << t->srv->name() << " retry_num("
//            << t->retry_num
//            << ")task_retry:(" << task_retry << ") grp_retry_num:"
//            << t->grp->retry_num << endl;
    t->set_stat(task_stat_net_send);
    lck.unlock();
    t->srv->submit_task(t);
}

void client::complete_task(task *t)
{
    unique_lock<std::mutex> lck(m);
//    cout << YELLOW << "client::complete_task (" << t->task_id << ")"<<RESET
//            << name() << " -> "
//            << t->srv->name() << ":"
//            << t->string_bytaskstat(t->state)
//            << "| retry_num " << t->retry_num << "| grp_retry "
//            << t->grp->retry_num << "| complete_list:"
//            << complete_list.size() << endl;
    complete_list.push_back(t);
    lck.unlock();
//    hook->task_completed();
}

bool client::final_task(task *t)
{
    return t->grp->is_final();
}
