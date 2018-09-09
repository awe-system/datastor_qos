//
// Created by root on 18-8-11.
//

#ifndef QOS_SYSQOS_DISP_LEFT_TOKENS_H
#define QOS_SYSQOS_DISP_LEFT_TOKENS_H

#include "sysqos_app_node.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct token_global
{
    /**private****************************************************/
    unsigned long default_token_min;
    unsigned long token_total;
    unsigned long token_free;
    unsigned long token_used_static;
    unsigned long token_dynamic;
    unsigned long token_dynamic_new;
    unsigned long app_token_rsvr;
    unsigned long app_num_max;
    unsigned long app_num_cur;
    unsigned long app_max_token_rsvr;
    sysqos_spin_lock_t lck;
    /**public****************************************************/
    //为node分配额外的空间
    int (*online)(struct token_global *tokens, app_node_t *node);
    
    void (*offline)(struct token_global *tokens, app_node_t *node);
    
    unsigned long (*try_alloc)(struct token_global *tokens,
                               unsigned long cost);
    
    //cant del zero
    void (*free)(struct token_global *tokens, unsigned long cost);
    
    void (*increase)(struct token_global *tokens, unsigned long units);
    
    bool (*try_decrease)(struct token_global *tokens, unsigned long units);
    
    void (*set_max_rsvr)(struct token_global *tokens, unsigned long max_rsvr);
} token_global_t;

int token_global_init(token_global_t *left_tokens,
                      unsigned long max_node_nr,
                      unsigned long min_per_node);

void token_global_exit(token_global_t *left_tokens);

void test_token_global();

#ifdef __cplusplus
}
#endif

#endif //TEST_QOS_SYSQOS_DISP_LEFT_TOKENS_H
