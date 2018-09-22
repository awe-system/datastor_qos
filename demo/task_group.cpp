//
// Created by root on 18-9-13.
//

#include <iostream>
#include <sysqos_common.h>
#include "client.h"
#include "server.h"

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
        case task_group_stat_wait_grp:
            return "task_group_stat_wait_grp";
    }
}

task_group::task_group(const int task_cost,
                       const map<server *, client *> &c_s_pairs,
                       int _task_id) :
        task_id(_task_id), state(task_group_stat_out_standing),
        final_cnt(0),wait_grp_cnt(0),is_failed(false)
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
//            << endl;
    state = new_stat;
    if ( new_stat == task_group_stat_wait_token )
    {
        for ( auto it : tasks )
        {
            it->set_stat(task_stat_wait_token);
        }
    }
    if ( new_stat == task_group_stat_wait_disk )
    {
        for ( auto it : tasks )
        {
            it->srv->submit_to_disk(it);
        }
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
//
//    cout << "is_final (" << task_id << ")" << tasks[0]->cli->name() << ":"
//            << string_bygrpstat(state)
//            << "|new_cnt:" << new_cnt << endl;
   
    return tasks.size() == new_cnt;
}

bool task_group::is_grp_ready()
{
    unique_lock<std::mutex> lck(m);
    int new_cnt = ++wait_grp_cnt;
    
//    cout << "is_final (" << task_id << ")" << tasks[0]->cli->name() << ":"
//            << string_bygrpstat(state)
//            << "|new_cnt:" << new_cnt << endl;
    return tasks.size() == new_cnt + final_cnt;
}

void task_group::set_failed()
{
    is_failed = true;
    for ( auto it : tasks )
    {
        it->srv->try_to_erase_task(it);
    }
}

