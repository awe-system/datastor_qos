//
// Created by GaoHualong on 2018/7/9.
//

#include "hash_table.h"
#include "sysqos_interface.h"
#include "sysqos_nodereq_list.h"
#include "sysqos_token_reqgrp.h"
#include "sysqos_msg.h"
#include "sysqos_resource.h"

#ifndef QOS_QOS_PERMISSION_MANAGER_H
#define QOS_QOS_PERMISSION_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct token_reqgrp_manager
{
    /*********************************************************************/
    msg_event_ops_t msg_event;
    
    int (*get_token_reqgrp)(struct token_reqgrp_manager *manager,
                         resource_list_t *rs,
                         void *pri, token_reqgrp_t **p);
    
    void (*put_token_reqgrp)(struct token_reqgrp_manager *manager,
                          token_reqgrp_t *p, resource_list_t *fail_rs);
    
    void (*set_app_event_ops)(struct token_reqgrp_manager *manager,
                              sysqos_applicant_event_ops_t *ops);
    
    void (*set_msg_ops)(struct token_reqgrp_manager *manager, msg_ops_t *ops);
    /**测试用方法*******************************************************************************************/
    
    /**私有属性或方法*******************************************************************************************/
    sysqos_applicant_event_ops_t *app_ops;
    msg_ops_t             *msg_ops;
    
    memory_cache_t   token_grp_cache;//token_reqgrp_t item
    memory_cache_t   resource_cache;//token_req_t source item
    hash_table_t     *app_node_table;
    memory_cache_t   manager_item_cache;
    pthread_rwlock_t lck;
    int              version;
    /**尚未实现的接口*******************************************************************************************/
    //TODO:ignore
    //TODO:commit
    //TODO:区分FIFO与失败返回
} token_reqgrp_manager_t;

int token_reqgrp_manager_init(token_reqgrp_manager_t *manager,
                              int tabl_len, int max_token_grp_num,
                              int max_resource_num, int max_node_num);

void token_reqgrp_manager_exit(token_reqgrp_manager_t *manager);

#ifdef __cplusplus
}
#endif
#endif //TEST_QOS_QOS_TOKEN_H
