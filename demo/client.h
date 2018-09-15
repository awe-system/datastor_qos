//
// Created by root on 18-9-13.
//

#ifndef DATASTOR_QOS_CLIENT_H
#define DATASTOR_QOS_CLIENT_H

#include <list>
#include "json_obj.h"
#include "task.h"
#include "task_group.h"

class complete_task_hook
{
public:
    virtual void task_completed(void ){}
    virtual void task_final(task_group *tgrp,bool is_fail) = 0;
};

class client
{
    bool is_qos_open;
    string cli_name;
    std::mutex m;
    std::list<task_group *> tasks_list;
    std::list<task *> complete_list;
    complete_task_hook *hook;
    bool could_snd(task_group *tgrp);
    void retry_task(task *t);
    void snd_tasks(task_group *tgrp);
    int grp_retry;
    int task_retry;
    void final_task(task *t);
    bool is_grp_final(task_group *grp,bool &is_ok);
public:
    client(const string &_name, bool qos_open, int _grp_retry,
           int _task_retry, complete_task_hook *_hook);
    bool run_once();
    void submit_tasks(task_group *tgrp);
    void complete_task(task *t);
    string name()const;
};


#endif //DATASTOR_QOS_CLIENT_H
