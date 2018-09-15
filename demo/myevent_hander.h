//
// Created by root on 18-9-13.
//

#ifndef DATASTOR_QOS_MYEVENT_HANDER_H
#define DATASTOR_QOS_MYEVENT_HANDER_H

#include "demo.h"
#include "demo_obj.h"

class myevent_hander : public sysqos_demo_cmds::event_handler
{
    demo_obj *demo;
private:
    void add_task(const json_obj &obj);

public:
    int tasks(json_obj &tasks_info) override;

public:
    myevent_hander(demo_obj *_demo);
    
    int servers(json_obj &ino) override;
    
    int dumps(OUT json_obj &all_info)override ;
    
    int add_tasks(string &tasks_string) override;
};


#endif //DATASTOR_QOS_MYEVENT_HANDER_H
