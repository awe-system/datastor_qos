//
// Created by root on 18-9-13.
//

#include <iostream>
#include "client.h"
#include "task.h"
#include "task_group.h"

task_group::task_group(const int task_cost,
                       const map<server *, client *> &c_s_pairs,
                       int _task_id) :
        task_id(_task_id), state(task_group_stat_out_standing), retry_num(0)
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
}


task_group::~task_group()
{
    for ( auto t : tasks )
    {
        delete t;
    }
}

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
    cout << "task_group (" << task_id << ")" << tasks[0]->cli->name() << ":"
            << string_bygrpstat(state) << "->" << string_bygrpstat(new_stat)
            << "| retry_num " << retry_num << endl;
    state = new_stat;
    if ( new_stat == task_group_stat_wait_token )
    {
        for ( auto it : tasks )
        {
            it->set_stat(task_stat_wait_token);
        }
    }
}

long task_group::usecs()
{
    utime endtime;
    return endtime.usec_elapsed_since(begin_time);
}
