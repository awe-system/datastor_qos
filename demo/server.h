//
// Created by root on 18-9-13.
//

#ifndef DATASTOR_QOS_SERVER_H
#define DATASTOR_QOS_SERVER_H


#include <json_obj.h>
#include <list>
#include "task.h"
#include "sysqos_interface.h"

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
    std::mutex disk_m;
    int rs;
    std::list<task *> disk_task;
    std::list<task *> waitgrp_task;
    server_submit_hook *hook;
    void complete_task(task *t);
    bool could_rcv(task *t);
    
    int net_piece_cnt;
    int net_hit_cnt;
    
    int piece_cnt;
    int hit_cnt;
public:
    bool is_stop;
    int last_percent;
    int net_last_percent;
    sysqos_dispatcher_ops_t ops;
    //每个接受的io至少经历多少微妙us
    server(const string &name, bool is_qos_open, int max_resource_num,
           int usec_perio,server_submit_hook *_hook);
    
    json_obj to_json_obj();
    
    //处理接收过程并执行rcvd
    bool run_once(long &usecs_guess_wake_up);
    
    void sync_disks();
    
    void submit_to_disk(task *t);
    
    void submit_task(task *t);
    
    string name() const;
    
    
    void try_to_erase_task(task *t);
};


#endif //DATASTOR_QOS_SERVER_H
