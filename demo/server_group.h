//
// Created by root on 18-9-13.
//

#ifndef DATASTOR_QOS_SERVER_GROUP_H
#define DATASTOR_QOS_SERVER_GROUP_H

#include <json_obj.h>
#include <condition_variable>
#include "server.h"
#include "circle_increse_safe.h"
#include "Timer.h"


class server_group:public Timercallback,public server_submit_hook
{
    bool                is_stop;
    int                 max_num;
    circle_increse_safe servernum;
    vector<server *>    servers_vec;
    mutex               m;
    condition_variable  cond;
    Timer server_timer;
private:
    void wake_up_work();
public:
    map<string, server *> servers;
    
    void timer_on() override;
    
    server_group(int max_server_num, bool is_qos_open, int max_rs,
                 int usec_perio);
    
    json_obj to_json_obj() const;
    
    void run();
    
    server *server_byname(const string &name);

    void tasks_submited(void);
};


#endif //DATASTOR_QOS_SERVER_GROUP_H
