//
// Created by root on 18-9-13.
//

#include "demo.h"
#include "sysqos_demo_cmds.h"

class myevent_hander: public sysqos_demo_cmds::event_handler
{
public:
    myevent_hander()
    {
    
    }
    
    int dumps(OUT json_obj &all_info)
    {
        all_info.set_dic();
        all_info.merge(json_obj("key","val"));
        return 0;
    }
};

int main(int argc, char *argv[])
{
    myevent_hander handler;
    sysqos_demo_cmds::event event1(&handler);
    while(1)
    {
        sleep(500);
    }
    return 0;
}