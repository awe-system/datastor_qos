//
// Created by root on 18-7-25.
//
#include "sysqos_dispatch_base_node.h"
#include "hash_table.h"
#include "sysqos_policy_param.h"
#include "sysqos_resource.h"

#ifndef QOS_TOKEN_MANAGER_H
#define QOS_TOKEN_MANAGER_H
#ifdef __cplusplus
extern "C" {
#endif

typedef struct nodereq_list
{
    unsigned long    press;//token_list里所有等待资源的总和
    struct list_head token_list;
    pthread_rwlock_t lck;
    
    /********************************************/
    press_t (*get_press)(struct nodereq_list *tokens);
    
    void (*push_back)(struct nodereq_list *tokens, resource_list_t *rs);
    
    resource_list_t *(*front)(struct nodereq_list *tokens);
    
    int (*erase)(struct nodereq_list *tokens, resource_list_t *rs);
    
    void (*pop_all)(struct nodereq_list *tokens, struct list_head *head);
    
    void (*pop_front)(struct nodereq_list *tokens);
} nodereq_list_t;

int nodereq_list_init(nodereq_list_t *tokens);

void nodereq_list_exit(nodereq_list_t *tokens);

#ifdef __cplusplus
}
#endif

#endif //TEST_QOS_TOKEN_MANAGER_H
