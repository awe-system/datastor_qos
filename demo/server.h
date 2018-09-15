//
// Created by root on 18-9-13.
//

#ifndef DATASTOR_QOS_SERVER_H
#define DATASTOR_QOS_SERVER_H


#include <json_obj.h>
#include <list>
#include "task.h"

class server_submit_hook
{
public:
    virtual void tasks_submited(void) = 0;
};

class server
{
    string srv_name;
    bool   is_qos_open;
    int max_rs;
    int usecs_perio;
    std::mutex m;
    int rs;
    std::list<task *> pending_task;
    server_submit_hook *hook;
    void complete_task(task *t);
    bool could_rcv(task *t);
public:
    //每个接受的io至少经历多少微妙us
    server(const string &name, bool is_qos_open, int max_resource_num,
           int usec_perio,server_submit_hook *_hook);
    
    json_obj to_json_obj();
    
    bool run_once(long &usecs_guess_wake_up);
    
    void submit_task(task *t);
    
    string name() const;
};


#endif //DATASTOR_QOS_SERVER_H
