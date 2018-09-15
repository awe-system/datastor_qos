//
// Created by root on 18-9-13.
//

#include "myevent_hander.h"

int myevent_hander::dumps(json_obj &all_info)
{
    try
    {
        all_info = demo->to_json_obj();
    }
    catch (...)
    {
        return -1;
    }
    return 0;
}


int myevent_hander::add_tasks(string &task_string)
{
    json_obj obj;
    obj.loads(task_string);
    try
    {
        for(const json_obj &task_obj : obj.array_val)
        {
            add_task(task_obj);
        }
        return 0;
    }
    catch (...)
    {
        return -1;
    }
    
}

myevent_hander::myevent_hander(demo_obj *_demo)
{
    demo = _demo;
}

void myevent_hander::add_task(const json_obj &obj)
{
    demo->addtasks((const int) obj["task_num"].get_number(),
                   (const int) obj["task_cost"].get_number(),
                   obj["c_s_pairs"]);
}

int myevent_hander::tasks(json_obj &tasks_info)
{
    try
    {
        tasks_info = demo->task_detail();
    }
    catch (...)
    {
        return -1;
    }
    return 0;
}

int myevent_hander::servers(json_obj &info)
{
    try
    {
        info = demo->servers->to_json_obj();
    }
    catch (...)
    {
        return -1;
    }
    return 0;
}
