//
// Created by root on 18-9-14.
//

#ifndef DATASTOR_QOS_CLI_SER_H
#define DATASTOR_QOS_CLI_SER_H

#include <map>
#include <mutex>
#include "json_obj.h"
#include "task_group.h"

class client;
class server;
class task;

class cli_ser
{
    client *cli;
    server *srv;
    std::mutex m;
    std::map<int,task *> tasks;
    int fail_num;
    long task_num;
    double ave_task_usec;
private:
    json_obj quotas();
    json_obj history();
    json_obj analysis_tasks();
public:
    cli_ser(client *_cli,server *_srv);
    void update(task *t);
    void add_task(task *t);
    void rm_task(task *t);
    
    json_obj to_json_obj();
};


#endif //DATASTOR_QOS_CLI_SER_H
