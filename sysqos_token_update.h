//
// Created by root on 18-8-11.
//

#ifndef _QOS_SYSQOS_TOKEN_UPDATE_H
#define _QOS_SYSQOS_TOKEN_UPDATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "list_head.h"
#include "sysqos_common.h"
#include "sysqos_interface.h"
#include "sysqos_app_node.h"
#include "sysqos_token_global.h"
#include "sysqos_waitincrease_list.h"

typedef unsigned long (*set_quota_new_func)(void *manager, unsigned long quota,
                                   app_node_t *node);

struct sysqos_disp_manager;
typedef struct token_update_ctx
{
    /**private**********************************/
    unsigned long    total_weight;
    unsigned long    token_total;
    unsigned long    token_static;
//    unsigned long    max_token_once;
//    int              rebalance_ratio;
    struct list_head lhead_update_node;
    pthread_rwlock_t lck;
    
    /**public*****************************************/
    void (*add_resource)(struct token_update_ctx *ctx, app_node_t *node);
    
    void (*update)(struct token_update_ctx *ctx,
                   struct sysqos_disp_manager *manager,
                   set_quota_new_func func);
/**cache******************************/
} token_update_ctx_t;

int token_update_ctx_init(token_update_ctx_t *ctx/*,
                          unsigned long max_token_once,
                          int rebalance_ratio*/);


void token_update_ctx_exit(token_update_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif //TEST_QOS_SYSQOS_POLICY_H
