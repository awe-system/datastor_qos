//
// Created by root on 18-9-13.
//

#ifndef DATASTOR_QOS_CLIENT_GROUP_H
#define DATASTOR_QOS_CLIENT_GROUP_H


#include <json_obj.h>
#include <condition_variable>
#include "task_group.h"
#include "circle_increse_safe.h"
#include "client.h"

class client_group : public complete_task_hook
{
    bool                is_stop;
    int                 max_num;
    circle_increse_safe clientnum;
public:
    void task_completed(void);
    
    void task_final(task_group *tgrp, bool is_fail);

private:
    std::mutex          m;
    condition_variable  cond;
    complete_task_hook *hook;
public:
    vector<client *>      clients_vec;
    map<string, client *> clients;
    
    client_group(int max_cli_num, bool is_qos_open, int grp_retry,
                 int task_retry);
    
    void set_hook(complete_task_hook *_hook);
    json_obj to_json_obj() const;
    
    //具体任务执行
    void run();
    
    void submit_tasks(task_group *task_grp);
    
    client *client_byname(const string &name);
};


#endif //DATASTOR_QOS_CLIENT_GROUP_H
