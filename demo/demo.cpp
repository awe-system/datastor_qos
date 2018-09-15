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

#define CLIENT_GROUP_RUN_THREADS 1
#define SERVER_THREAD_NUM 6
#define CLINT_THREAD_NUM  6
#define MAX_CLIENT_NUM       6
#define MAX_SERVER_NUM       6
#define MAX_RS_IN_SERVER     10000
#define USECS_PER_IO         1
#define TASK_RETRY_NUM       3
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
    client_group cli_grp(MAX_CLIENT_NUM, is_qos_open, TASK_GRP_RETRY_NUM,
                         TASK_RETRY_NUM);
    server_group srv_grp(MAX_SERVER_NUM, is_qos_open, MAX_RS_IN_SERVER,
                         USECS_PER_IO);
    
    demo_obj       demo(&cli_grp, &srv_grp);
    myevent_hander handler(&demo);
    
    sysqos_demo_cmds::event event1(&handler);
    
    test_threads_t threads;
    test_threads_init(&threads);
    
    threads.add_type(&threads, CLIENT_GROUP_RUN_THREADS, demo_func, &demo);
    threads.add_type(&threads, CLINT_THREAD_NUM, client_group_func, &cli_grp);
    threads.add_type(&threads, SERVER_THREAD_NUM, server_group_func, &srv_grp);
    threads.run(&threads);
    test_threads_exit(&threads);
    return 0;
}