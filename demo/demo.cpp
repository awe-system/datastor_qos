//
// Created by root on 18-9-13.
//

#include "demo.h"
#include "client_group.h"
#include "server_group.h"
#include "demo_obj.h"
#include "myevent_hander.h"

static void *client_group_func(void *pgroup)
{
    client_group *group = (client_group *) pgroup;
    group->run();
    return NULL;
}

static void *server_group_func(void *pgroup)
{
    server_group *group = (server_group *) pgroup;
    group->run();
    return NULL;
}

static void *demo_func(void *pdemo)
{
    demo_obj *demo = (demo_obj *) pdemo;
    demo->run();
    return NULL;
}

#include <awe_conf/env.h>

env demo_disp_thread("demo", "demo_disp_thread");
env demo_server_thread("demo", "demo_server_thread");
env demo_client_thread("demo", "demo_client_thread");
env demo_client_num("demo", "demo_client_num");
env demo_server_num("demo", "demo_server_num");
env demo_max_rs_inserver("demo", "demo_max_rs_inserver");
env demo_task_retry("demo", "demo_task_retry");

#define USECS_PER_IO         1
#define TASK_GRP_RETRY_NUM   3
static bool is_qos_open = false;


class timer_cb : public Timercallback
{
public:
    void timer_on() override
    {
        cout << "timer" << endl;
    }
};

int main(int argc, char *argv[])
{
    client_group
                 cli_grp
            (demo_client_num.get_int(), is_qos_open, TASK_GRP_RETRY_NUM,
             demo_task_retry.get_int());
    server_group srv_grp(demo_server_num.get_int(), is_qos_open,
                         demo_max_rs_inserver.get_int(),
                         USECS_PER_IO);
    
    demo_obj       demo(&cli_grp, &srv_grp);
    myevent_hander handler(&demo);
    
    sysqos_demo_cmds::event event1(&handler);
    
    test_threads_t threads;
    test_threads_init(&threads);
    
    threads.add_type(&threads, demo_disp_thread.get_int(), demo_func, &demo);
    threads.add_type(&threads, demo_client_thread.get_int(), client_group_func,
                     &cli_grp);
    threads.add_type(&threads, demo_server_thread.get_int(), server_group_func,
                     &srv_grp);
    threads.run(&threads);
    test_threads_exit(&threads);
    return 0;
}