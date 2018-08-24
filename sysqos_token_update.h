//
// Created by root on 18-8-11.
//

#ifndef _QOS_SYSQOS_POLICY_H
#define _QOS_SYSQOS_POLICY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "list_head.h"
#include "sysqos_common.h"
#include "sysqos_interface.h"
#include "sysqos_policy_param.h"

typedef void (*set_tototal_func)(void *manager, resource_t *rs);

typedef struct token_update_ctx
{
    unsigned long    token_left_min;
    unsigned long    token_left;
    unsigned long    max_token_once;
    struct list_head lhead_update_node;
    
    void (*add_resource)(struct token_update_ctx *policy, resource_t *rs,
                         unsigned long p_press, limit_t *p_limt);
    
    void (*update)(struct token_update_ctx *policy);
    
    void (*for_each_do)(struct token_update_ctx *policy, void *manager,
                        set_tototal_func func);
    
    void (*reset)(struct token_update_ctx *policy);
/**cache******************************/
} token_update_ctx_t;

int token_update_ctx_init(token_update_ctx_t *ctx,
                          unsigned long max_node_num,
                          unsigned long max_grp_num);


void token_update_ctx_exit(token_update_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif //TEST_QOS_SYSQOS_POLICY_H
