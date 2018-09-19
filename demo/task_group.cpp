//
// Created by root on 18-9-13.
//

#include <iostream>
#include <sysqos_common.h>
#include "client.h"

static string string_bygrpstat(task_group_stat stat)
{
    switch ( stat )
    {
        case task_group_stat_out_standing:
            return "task_group_stat_out_standing";
        case task_group_stat_wait_token:
            return "task_group_stat_wait_token";
        case task_group_stat_wait_disk:
            return "task_group_stat_wait_disk";
    }
}

task_group::task_group(const int task_cost,
                       const map<server *, client *> &c_s_pairs,
                       int _task_id) :
        task_id(_task_id), state(task_group_stat_out_standing),
        final_cnt(0)
{
    
    //NOTE:所有cli都相同才能为一组
    for ( auto cspair : c_s_pairs )
    {
        cli = cspair.second;
        server *srv = cspair.first;
        task   *t   = new task(cli, srv, task_id, (unsigned long) task_cost,
                               this);
        tasks.push_back(t);
    }
//    cout << RED << "intit task_group (" << RESET << task_id << ")"
//            << tasks[0]->cli->name() << ":"
//            << string_bygrpstat(state)
//            << "| retry_num " << retry_num << endl;
    
}


task_group::~task_group()
{
    for ( auto t : tasks )
    {
        delete t;
    }
}


json_obj task_group::to_json_obj() const
{
    json_obj obj("task_id", task_id);
    obj.merge(json_obj("state", string_bygrpstat(state)));
    json_obj task_array;
    task_array.set_array();
    for ( auto t : tasks )
    {
        task_array.append(t->to_json_obj());
    }
    obj.merge(json_obj("tasks", task_array));
    return obj;
}

void task_group::set_stat(task_group_stat new_stat)
{
//    cout << "task_group (" << task_id << ")" << tasks[0]->cli->name() << ":"
//            << string_bygrpstat(state) << "->" << string_bygrpstat(new_stat)
//            << "| retry_num " << retry_num << endl;
    state = new_stat;
    if ( new_stat == task_group_stat_wait_token )
    {
        for ( auto it : tasks )
        {
            it->set_stat(task_stat_wait_token);
        }
        unique_lock<std::mutex> lck(m);
        final_cnt = 0;
    }
}

long task_group::usecs()
{
    utime endtime;
    return endtime.usec_elapsed_since(begin_time);
}

bool task_group::is_final()
{
    unique_lock<std::mutex> lck(m);
    
    int new_cnt = ++final_cnt;
    
//    cout << "is_final (" << task_id << ")" << tasks[0]->cli->name() << ":"
//            << string_bygrpstat(state) << "| retry_num " << retry_num
//            << "|new_cnt:" << new_cnt << endl;
    
    return tasks.size() == new_cnt;
}

bool task_group::is_failed()
{
    for ( auto t : tasks )
    {
        if ( t->retry_num > 0)
        {
            return true;
        }
    }
    return false;
}
