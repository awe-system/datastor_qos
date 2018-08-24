//
// Created by root on 18-8-11.
//

#ifndef _QOS_SYSQOS_DISP_TOKEN_NODE_H
#define _QOS_SYSQOS_DISP_TOKEN_NODE_H

#include <sys/param.h>
#include "sysqos_policy_param.h"
#include "sysqos_interface.h"
#include "sysqos_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif
struct app_node;

typedef void enqueue_func_t(void *pri, struct app_node *node);

typedef struct app_node
{
    /**public**************************************/
    void (*reset)(struct app_node *node);
    
    int (*alloc)(struct app_node *node, resource_t *rs,
                 long *fence_id);
    
    void (*free)(struct app_node *node, resource_t *rs,
                 long fence_id);
    
    //更新press以及调整根据回馈调整 token_quota 返回是否需要reset 暂时不需要返回true
    bool (*rcvd)(struct app_node *node, app2dispatch_t *a2d,
                 unsigned long *free_size);
    
    //若已经存在队中则返回false
    void (*update_tototal)(struct app_node *node,
                        unsigned long tototal,
                        void *pri, enqueue_func_t enqueue_func);
    
    void (*get_protocol)(struct app_node *node, dispatch2app_t *d2a);
    
    unsigned long (*get_press)(struct app_node *node);
    
    unsigned long (*get_zonetotal)(struct app_node *node);
    
    void (*get_limit)(struct app_node *node, limit_t *limit);

/**private**************************************/
    struct list_head list_wait_increase;
    
    
    //用来防止用一个id的node下线后资源被释放到另一个node中去
    unsigned long      token_inuse;
    unsigned long      token_quota;
    unsigned long      token_quota_new;
    unsigned long      press;
    limit_t            limit;
    long               Version;
    long               fence_id;
    pthread_spinlock_t lck;
}app_node_t;

int app_node_init(app_node_t *node, unsigned long min_units,
                  long fence_id);

void app_node_exit(app_node_t *node);

void test_app_node();

#ifdef __cplusplus
}
#endif

#endif //TEST_QOS_SYSQOS_DISP_TOKEN_NODE_H
