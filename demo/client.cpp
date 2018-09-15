//
// Created by root on 18-9-13.
//

#include <iostream>
#include <sysqos_common.h>
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
        cout << RED << "client::run_once " << RESET"(" << t->task_id << ")"
                << name() << "->" << t->srv->name() << " retry_num("
                << t->retry_num
                << ")task_retry:" << task_retry << endl;
        if ( t->retry_num > 0 && t->retry_num < task_retry )
        {
            retry_task(t);
        }
        else
        {
            final_task(t);
        }
    }
    if ( !tasks_list.empty() )
    {
        task_group *tgrp = tasks_list.front();
        tasks_list.pop_front();
        if ( could_snd(tgrp) )
        {
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
    t->set_stat(task_stat_net_send);
    t->srv->submit_task(t);
}

void client::complete_task(task *t)
{
    unique_lock<std::mutex> lck(m);
    complete_list.push_back(t);
    cout << YELLOW << "client::complete_task " << RESET << name() << " -> "
            << t->srv->name() << ":"
            << t->string_bytaskstat(t->state)
            << "| retry_num " << t->retry_num << "| grp_retry "
            << t->grp->retry_num << "| complete_list:"
            << complete_list.size() << endl;
    lck.unlock();
    hook->task_completed();
}

void client::final_task(task *t)
{
    bool is_ok = false;
    cout << BLUE << "client::final_task " << RESET << name() << " -> "
            << t->srv->name() << ":"
            << t->string_bytaskstat(t->state)
            << "| retry_num " << t->retry_num << "| grp_retry "
            << t->grp->retry_num << "| complete_list:"
            << complete_list.size() << endl;
    t->set_stat(task_stat_complete);
    if ( !is_grp_final(t->grp, is_ok) )
    {
        return;
    }
    if ( is_ok )
    {
        hook->task_final(t->grp, false);
    }
    else if ( t->grp->retry_num < grp_retry )
    {
        ++t->grp->retry_num;
        t->grp->set_stat(task_group_stat_wait_token);
        tasks_list.push_back(t->grp);
    }
    else
    {
        hook->task_final(t->grp, true);
    }
}

bool client::is_grp_final(task_group *grp, bool &is_ok)
{
    for ( auto t : grp->tasks )
    {
        if ( t->state != task_stat_complete )
        {
            return false;
        }
        if ( t->retry_num > 0 )
        {
            is_ok = false;
        }
    }
    return true;
}
