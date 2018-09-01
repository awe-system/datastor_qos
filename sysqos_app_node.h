//
// Created by root on 18-8-11.
//

#ifndef _QOS_SYSQOS_DISP_TOKEN_NODE_H
#define _QOS_SYSQOS_DISP_TOKEN_NODE_H

#include <sys/param.h>
#include "sysqos_policy_param.h"
#include "sysqos_interface.h"
#include "sysqos_protocol.h"
#include "sysqos_update_node.h"

#ifdef __cplusplus
extern "C" {
#endif
struct app_node;

typedef void (*insert_func_t)(void *pri, struct app_node *node);

typedef void (*erease_func_t)(void *pri, struct app_node *node);

typedef unsigned long (*try_alloc_func_t)(void *pri, unsigned long cost);

typedef struct app_node
{
    /**public**************************************/
    //return to globel free
    unsigned long (*reset)(struct app_node *node);
    
    int (*alloc)(struct app_node *node, resource_t *rs,
                 long *fence_id);
    
    void (*free)(struct app_node *node, resource_t *rs,
                 long fence_id);
    
    //更新press以及调整根据回馈调整 token_quota 返回是否需要reset 暂时不需要返回true
    bool (*rcvd)(struct app_node *node, app2dispatch_t *a2d,
                 unsigned long *free_size);
    
    //根据token_quota_new更改token_quota 若相同则返回false
    //已经移除了
    bool (*update_token_quota)(struct app_node *node,void *pri,
                               try_alloc_func_t try_alloc_func,
                               erease_func_t erase_func);
    
    //返回新设置的值
    unsigned long (*update_token_quota_new)(struct app_node *node,
                        unsigned long token_quota_new,
                        void *pri, insert_func_t insert_func);
    
    void (*get_protocol)(struct app_node *node, dispatch2app_t *d2a);
    
   update_node_t * (*init_update_node)(struct app_node *node);
/**private**************************************/
    struct list_head list_wait_increase;
    
    update_node_t      updatenode;
    //用来防止用一个id的node下线后资源被释放到另一个node中去
    unsigned long      token_inuse;
    unsigned long      token_quota;
    unsigned long      token_quota_new;
    unsigned long      press;
    limit_t            limit;
    long               version;
    long               fence_id;
    pthread_spinlock_t lck;
}app_node_t;


static app_node_t * app_node_by_update(update_node_t *updatenode)
{
    return container_of(updatenode,app_node_t,updatenode);
}

int app_node_init(app_node_t *node, unsigned long min_units,
                  long fence_id);

void app_node_exit(app_node_t *node);

void test_app_node();

#ifdef __cplusplus
}
#endif

#endif //TEST_QOS_SYSQOS_DISP_TOKEN_NODE_H
