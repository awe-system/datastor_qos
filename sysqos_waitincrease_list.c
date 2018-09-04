//
// Created by root on 18-8-11.
//

#include <assert.h>
#include <memory.h>
#include <stdlib.h>
#include "sysqos_waitincrease_list.h"
#include "sysqos_common.h"
#include "sysqos_interface.h"

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

static void dequeue(struct wait_increase_list *q)
{
    struct list_head *frist = q->head.next;
   
    assert(q);
    if ( list_empty(&q->head) )
    {
        return;
    }
    
    list_del(frist);
    LISTHEAD_INIT(frist);
    list_add_tail(frist, &q->left);
}

int wait_increase_list_init(wait_increase_list_t *q,
                            unsigned long max_queue_depth)
{
    int err = 0, i = 0;
    
    q->insert        = enqueue;
    q->dequeue        = dequeue;
    q->erase          = erase;
    q->is_first_exist = is_first_exist;
    
    LISTHEAD_INIT(&q->head);
    
    return QOS_ERROR_OK;
}

void wait_increase_list_exit(wait_increase_list_t *q)
{
}
/** for test****************************************************************************/
#ifdef LIST_HEAD_QUEUE_TEST

#include "stdio.h"

#define  MAX_QUEUE_NUM 3
typedef struct test_context
{
    void *queue[MAX_QUEUE_NUM + 1];
    int  begin;
    int  end;
} test_context_t;

static void test_init(test_context_t *test)
{
    int i = 0;
    test->begin = test->end = 0;
    memset(test->queue, 0, (MAX_QUEUE_NUM + 1) * sizeof(void *));
}

static void test_enqueue(test_context_t *test, void *val)
{
    test->queue[test->end] = val;
    if ( ++test->end > MAX_QUEUE_NUM )
    {
        test->end = 0;
    }
    assert(test->end != test->begin);
}

static void *test_dequeue(test_context_t *test)
{
    void *res = NULL;
    if ( test->end == test->begin )
    {
        return res;
    }
    res = test->queue[test->begin];
    if ( ++test->begin > MAX_QUEUE_NUM )
    {
        test->begin = 0;
    }
    return res;
}

static void test_case_init_exit(wait_increase_list_t *q)
{
    int err = wait_increase_list_init(q, MAX_QUEUE_NUM);
    assert(err == 0);
    
    wait_increase_list_exit(q);
    err = wait_increase_list_init(q, MAX_QUEUE_NUM);
    assert(err == 0);
    printf("[test_case_init_exit] %s[OK]%s\n", GREEN, RESET);
}

static bool test_check_dequeue(wait_increase_list_t *q, test_context_t *test)
{
    void *tmp_test = test_dequeue(test);
    assert(q && test);
    if ( tmp_test )
    {
        void *tmp_q = NULL;
        //FIXME
//        assert(q->is_first_exist(q, &tmp_q));
        assert(tmp_q == tmp_test);
        q->dequeue(q);
        return true;
    }
    else
    {
        return false;
    }
}

static void test_case_endequeue(wait_increase_list_t *q)
{
    int            i     = 0;
    test_context_t test;
    void           *tmp;
//    q_item_t       *item = NULL;
    
    test_init(&test);
    
    for ( i = 0; i < MAX_QUEUE_NUM; i++ )
    {
        long id = i + 1;
//        item = q->insert(q, (void *) id);
//        assert(item->data == (void *) id);
        test_enqueue(&test, (void *) id);
    }
    
    while ( test_check_dequeue(q, &test) );
    
    
    for ( i = 0; i < MAX_QUEUE_NUM; i++ )
    {
        long id = i + 1;
//        item = q->insert(q, (void *) id);
//        assert(item->data == (void *) id);
        test_enqueue(&test, (void *) id);
        if ( rand() % 2 == 0 )
        {
            test_check_dequeue(q, &test);
        }
    }
    
    while ( test_check_dequeue(q, &test) );
    
//    assert(!q->is_first_exist(q, &tmp));
    printf("[test_case_endequeue] %s[OK]%s\n", GREEN, RESET);
}


static void test_case_enerasequeue(wait_increase_list_t *q)
{
    int            i     = 0;
    test_context_t test;
    test_context_t test_item;
    void           *tmp;
//    q_item_t       *item = NULL;
//    q_item_t       *item1 = NULL;
//    q_item_t       *item2 = NULL;
    
    q->insert(q, (void *) 1);
    q->insert(q, (void *) 2);
    q->insert(q, (void *) 2);
//
//    q->erase(q,item2);
//    q->erase(q,item1);
//    q->erase(q,item);
    
//    assert(!q->is_first_exist(q, &tmp));
    
    
    test_init(&test);
    test_init(&test_item);
    
    for ( i = 0; i < MAX_QUEUE_NUM; i++ )
    {
        long id = i + 1;
        q->insert(q, (void *) id);
//        assert(item->data == (void *) id);
        test_enqueue(&test, (void *) id);
//        test_enqueue(&test_item, item);
    }
    tmp     = test_dequeue(&test);
    while ( tmp )
    {
        void *tmp1 = NULL;
        void *tmp2 = test_dequeue(&test_item);
        
//        assert(is_first_exist(q, &tmp1));
        assert(tmp1 == tmp);
        q->erase(q, tmp2);
        tmp = test_dequeue(&test);
    }
    
//    assert(!q->is_first_exist(q, &tmp));
    printf("[test_case_enerasequeue] %s[OK]%s\n", GREEN, RESET);
}

#endif

void test_wait_increase_list()
{
    //FIXME:重写此部分测试
#ifdef LIST_HEAD_QUEUE_TEST
    int err = 0;
    printf(YELLOW"--------------test_wait_increase_list----------------------:\n"RESET);
    wait_increase_list_t q;
    
    test_case_init_exit(&q);
    
    test_case_endequeue(&q);
    
    test_case_enerasequeue(&q);
    
    wait_increase_list_exit(&q);
    printf(BLUE"----------------test_wait_increase_list end-----------------\n"RESET);
#endif
}
