//
// Created by root on 18-8-11.
//

#ifndef _QOS_LIST_HEAD_QUEUE_H
#define _QOS_LIST_HEAD_QUEUE_H

#include <stdbool.h>
#include "sysqos_type.h"
#include "memory_cache.h"
#include "sysqos_app_node.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wait_increase_list
{
    /**public********************************/
    void (*erase)(struct wait_increase_list *, struct app_node *node);
    
    void (*insert)(struct wait_increase_list *, struct app_node *node);
    
    bool (*is_first_exist)(struct wait_increase_list *, struct app_node **node);
    
    
    /**private*******************************/
    struct list_head head;
    //for test
//    void (*dequeue)(struct wait_increase_list *);
} wait_increase_list_t;

int
wait_increase_list_init(wait_increase_list_t *q, unsigned long max_queue_depth);

void wait_increase_list_exit(wait_increase_list_t *queue);

#ifdef __cplusplus
}
#endif

#endif //TEST_QOS_LIST_HEAD_QUEUE_H
