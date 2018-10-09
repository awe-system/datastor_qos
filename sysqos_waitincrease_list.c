//
// Created by root on 18-8-11.
//

#include <assert.h>
#include <memory.h>
#include "sysqos_waitincrease_list.h"

static void erase(struct wait_increase_list *q, app_node_t *node)
{
    assert(q && node);
    
    list_del(&node->list_wait_increase);
    LISTHEAD_INIT(&node->list_wait_increase);
}

static void enqueue(struct wait_increase_list *q, app_node_t *node)
{
    assert(q && node);
    
    list_del(&node->list_wait_increase);
    LISTHEAD_INIT(&node->list_wait_increase);
    list_add_tail(&node->list_wait_increase, &q->head);
}

static bool is_first_exist(struct wait_increase_list *q,  app_node_t **node)
{
    struct list_head *frist = q->head.next;
    
    assert(q && node);
    if ( list_empty(&q->head) )
    {
        return false;
    }
    
    *node = container_of(frist,app_node_t,list_wait_increase);
    return true;
}

//static void dequeue(struct wait_increase_list *q)
//{
//    struct list_head *frist = q->head.next;
//
//    assert(q);
//    if ( list_empty(&q->head) )
//    {
//        return;
//    }
//
//    list_del(frist);
//    LISTHEAD_INIT(frist);
//    list_add_tail(frist, &q->left);
//}

int wait_increase_list_init(wait_increase_list_t *q,
                            unsigned long max_queue_depth)
{
//    max_queue_depth;
    q->insert        = enqueue;
//    q->dequeue        = dequeue;
    q->erase          = erase;
    q->is_first_exist = is_first_exist;
    
    LISTHEAD_INIT(&q->head);
    
    return QOS_ERROR_OK;
}

void wait_increase_list_exit(wait_increase_list_t *q)
{
}