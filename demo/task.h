//
// Created by root on 18-9-13.
//

#ifndef DATASTOR_QOS_TASK_H
#define DATASTOR_QOS_TASK_H

#include <mutex>
#include <json_obj.h>
#include "utime.h"

class client;

class server;

enum task_stat
{
    task_stat_out_standing,
    task_stat_wait_token,
    task_stat_net_send,
    task_stat_wait_grp,
    task_stat_wait_disk,
    task_stat_net_rcv,
    task_stat_complete
};

class task_group;

class task
{
    mutex m;
public:
    utime out_standing_point;
    utime wait_token_point;
    utime net_send_point;
    utime net_rcv_point;
    utime wait_grp_point;
    utime wait_disk_point;
    utime stat_complete_point;
    
    long usecs();
    
    void set_stat(task_stat new_stat);
    unsigned long cost;
    client        *cli;
    server        *srv;
    int           retry_num;
    task_stat     state;//处于那个阶段
    int           task_id;//task序号用作查询与grp相同
    task_group    *grp;
    
    string string_bytaskstat(task_stat stat)const;
    
    json_obj to_json_obj() const;
    
    task(client *_cli, server *_srv, int task_id, unsigned long cost,
         task_group *grp);
    
    
    bool is_final();
    bool is_wait_grp();
};


#endif //DATASTOR_QOS_TASK_H
