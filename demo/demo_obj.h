//
// Created by root on 18-9-13.
//

#ifndef DATASTOR_QOS_DEMO_OBJ_H
#define DATASTOR_QOS_DEMO_OBJ_H

#include <json_obj.h>
#include <list>
#include <mutex>
#include <condition_variable>
#include "client_group.h"
#include "server_group.h"
#include "cli_ser.h"
#include "client.h"


class demo_obj : public complete_task_hook
{
private:
    bool is_stop;
    long task_cnt;
    long grp_cnt;
    long failed_num;
    long aver_task_usec;
    long aver_grp_usec;
    
    map<string,cli_ser *> cli_sers;
    int cur_task_id;//只在 外部命令发送产生新task时增加 所以无需加锁
    list<task_group *> to_snd_task_list;
    map<int ,task_group *> wait_task_map;
    list<task_group *> to_del_list;
    std::mutex run_lck;
    std::condition_variable cond;
    void update_demo_analysis(task_group *grp);
    void try_free_grp(task_group *grp);
    void try_increase_grp(task_group *grp);
public:
    client_group * clients;
    server_group * servers;
    json_obj to_json_obj();
    
    json_obj task_detail();
    
    json_obj json_by_cs();
    
    void run();
    
    void addtasks(const int task_num, const int cost,
                 const json_obj& c_s_pairs);
    demo_obj( client_group * clis,server_group * servs);
    
    void task_final(task_group *tgrp, bool is_fail);
};


#endif //DATASTOR_QOS_DEMO_OBJ_H
