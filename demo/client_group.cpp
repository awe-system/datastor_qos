//
// Created by root on 18-9-13.
//
#include "client.h"
#include "client_group.h"

void client_group::run()
{
    while ( !is_stop )
    {
        for ( int                    idle_num = 0; idle_num < max_num;
              ++idle_num )
        {
            if ( clients_vec[clientnum.get_next()]->run_once() )
            {
                idle_num = 0;
            }
        }
        usleep(1);
//        std::unique_lock<std::mutex> lck(m);
//        cond.wait(lck);
    }
}

void client_group::submit_tasks(task_group *task_grp)
{
    task_grp->cli->submit_tasks(task_grp);
//    std::unique_lock<std::mutex> lck(m);
//    cond.notify_all();
}

json_obj client_group::to_json_obj() const
{
    return json_obj("");
}

client *client_group::client_byname(const string &name)
{
    auto it = clients.find(name);
    if ( it != clients.end() )
    {
        return clients.find(name)->second;
    }
    else
    { return NULL; }
}

client_group::client_group(int max_cli_num, bool qos_open, int grp_retry,
                           int task_retry) :
        is_stop(false), clientnum(max_cli_num), max_num(max_cli_num)
{
    for ( int i = 0; i < max_cli_num; ++i )
    {
        string cli_name = string("c") + to_string(i);
        client *cli     = new client(cli_name, qos_open, grp_retry, task_retry,
                                     this);
        clients.insert(make_pair(cli_name, cli));
        clients_vec.push_back(cli);
    }
}

void client_group::task_completed(void)
{
    std::unique_lock<std::mutex> lck(m);
    cond.notify_all();
}

void client_group::task_final(task_group *tgrp)
{
    for(auto t : tgrp->tasks)
    {
        t->set_stat(task_stat_complete);
    }
    hook->task_final(tgrp);
}

void client_group::set_hook(complete_task_hook *_hook)
{
    hook = _hook;
}
