//
// Created by root on 18-8-11.
//

#ifndef _QOS_DISPATCH_TOKEN_MANAGER_H
#define _QOS_DISPATCH_TOKEN_MANAGER_H

#include "sysqos_interface.h"
#include "hash_table.h"
#include "wait_increase_list.h"
#include "sysqos_token_global.h"
#include "count_controller.h"
#include "fence_executor.h"
#include "sysqos_token_update.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct sysqos_disp_manager
{
    /*********************************************************************/
    msg_event_ops_t msg_event;
    
    int (*alloc_tokens)(struct sysqos_disp_manager *manager,
                        resource_t *rs, long *fence_id);
    
    void (*free_tokens)(struct sysqos_disp_manager *manager,
                        resource_t *rs, long fence_id, bool is_fail);
    
    void (*resource_increase)(struct sysqos_disp_manager *manager,
                              unsigned long cost);
    
    void (*resource_reduce)(struct sysqos_disp_manager *manager, long cost);
    
    void (*set_dispatcher_event_ops)(struct sysqos_disp_manager *manager,
                                     dispatcher_event_ops_t *ops);
    
    void (*set_msg_ops)(struct sysqos_disp_manager *manager, msg_ops_t *ops);
    
    /**私有属性或方法*******************************************************************************************/
    token_global_t         tokens;
    dispatcher_event_ops_t *disp_ops;
    msg_ops_t              *msg_ops;
    
    wait_increase_list_t lhead_wait_increase;
    
    hash_table_t       *tab;
    memory_cache_t     token_node_cache;
    pthread_rwlock_t   lck;
    unsigned long      default_token_min;
    unsigned long      fence_id;
    count_controller_t cnt_controller;
    fence_executor_t   executor;
    struct token_update_ctx update_ctx;
} sysqos_disp_manager_t;

int sysqos_disp_manager_init(sysqos_disp_manager_t *manager, int table_len,
                             unsigned long max_node_num, int update_interval,
                             unsigned long min_rs_num, int rebalance_ratio);

void sysqos_disp_manager_exit(sysqos_disp_manager_t *manager);

void test_sysqos_disp_manager();

#ifdef __cplusplus
}
#endif
#endif //TEST_QOS_DISPATCH_TOKEN_MANAGER_H
