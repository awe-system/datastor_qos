//
// Created by root on 18-9-4.
//

#include <test_frame.h>
#include "node_inapp.h"

#define MAX_PERMISSION_NUM_INTEST 16
int                    test_version = 0;
static dispatch_node_t item_static;
static dispatch_node_t *item        = &item_static;

static void test_case_init_exit(dispatch_node_t *item)
{
    int err = dispatch_node_init(item, test_version++);
    assert(err == QOS_ERROR_OK && item->fence_id == test_version - 1);
    dispatch_node_exit(item);
    err = dispatch_node_init(item, test_version++);
    assert(err == QOS_ERROR_OK && item->fence_id == test_version - 1);
    printf("[test_case_init_exit] %s[OK]%s\n", GREEN, RESET);
}

static int
test_alloc_permissions(token_reqgrp_t *permissions[], memory_cache_t *percache,
                       memory_cache_t *rscache,
                       bool is_rand)
{
    int              i = 0;
    resource_list_t  rs1;
    resource_list_t  rs2;
    struct list_head head;
    //用同一个队列挂同一个资源来模拟事实上不会存在这样的使用情况
    LISTHEAD_INIT(&rs1.list);
    LISTHEAD_INIT(&rs2.list);
    LISTHEAD_INIT(&head);
    list_add(&rs1.list, &head);
    list_add(&rs2.list, &head);
    
    rs1.rs.id = rs2.rs.id = NULL;
    if ( is_rand )
    {
        rs1.rs.cost = (rand() % MIN_RS_NUM + MIN_RS_NUM) % MIN_RS_NUM;
        rs2.rs.cost = (rand() % MIN_RS_NUM + MIN_RS_NUM) % MIN_RS_NUM;
    }
    else
    {
        rs1.rs.cost = 1;
        rs2.rs.cost = 1;
    }
    
    for ( i = 0; i < MAX_PERMISSION_NUM_INTEST; ++i )
    {
        permissions[i] = percache->alloc(percache);
        assert(permissions[i]);
        assert(0 == token_reqgrp_init(permissions[i], rscache, &head, NULL));
    }
}

static void
test_free_permissions(token_reqgrp_t *permissions[], memory_cache_t *percache,
                      memory_cache_t *rscache)
{
    int i = 0;
    for ( i = 0; i < MAX_PERMISSION_NUM_INTEST; ++i )
    {
        token_reqgrp_exit(permissions[i], rscache);
        percache->free(percache, permissions[i]);
    }
#ifdef CACHE_OPEN_CNT
    assert(percache->alloc_cnt == percache->free_cnt);
    assert(rscache->alloc_cnt == rscache->free_cnt);
#endif
}

typedef struct test_for_permission_item
{
    memory_cache_t permission_cache;
    memory_cache_t resource_cache;
    token_reqgrp_t *permissions[MAX_PERMISSION_NUM_INTEST];
    void           *queue[MAX_PERMISSION_NUM_INTEST * 2 + 1];
    int            begin;
    int            end;
}                      test_for_permission_item_t;

static void test_enqueue(test_for_permission_item_t *test, void *val)
{
    test->queue[test->end] = val;
    if ( ++test->end > MAX_PERMISSION_NUM_INTEST * 2 )
    {
        test->end = 0;
    }
    assert(test->end != test->begin);
}

static void *test_dequeue(test_for_permission_item_t *test)
{
    void *res = NULL;
    if ( test->end == test->begin )
    {
        return res;
    }
    res = test->queue[test->begin];
    if ( ++test->begin > MAX_PERMISSION_NUM_INTEST * 2 )
    {
        test->begin = 0;
    }
    return res;
}

static void
test_for_permission_item_init(test_for_permission_item_t *test, bool is_rand)
{
    test->end   = 0;
    test->begin = 0;
    memset(test->queue, 0, MAX_PERMISSION_NUM_INTEST * 2);
    assert(0 ==
           memory_cache_init(&test->permission_cache, sizeof(token_reqgrp_t),
                             MAX_PERMISSION_NUM_INTEST));
    assert(0 ==
           memory_cache_init(&test->resource_cache,
                             sizeof(token_req_t),
                             MAX_PERMISSION_NUM_INTEST * 2));
    test_alloc_permissions(test->permissions, &test->permission_cache,
                           &test->resource_cache, is_rand);
}

static void test_for_permission_item_exit(test_for_permission_item_t *test)
{
    assert(test->end == test->begin);
    test_free_permissions(test->permissions, &test->permission_cache,
                          &test->resource_cache);
    memory_cache_exit(&test->permission_cache);
    memory_cache_exit(&test->resource_cache);
}

static void test_case_alloc_free_small()
{
    int                        alloc_success = 0;
    int                        err           = QOS_ERROR_OK;
    int                        i             = 0;
    test_for_permission_item_t test;
    test_for_permission_item_init(&test, false);
    
    for ( i = 0; i < MIN_RS_NUM / 2; ++i )
    {
        token_req_t *rip =
                            app_token_by_grp_list(
                                    test.permissions[i]
                                            ->lhead_reqgrp.next);
        err = item->alloc_resource(item, &rip->token_need);
        test_enqueue(&test, rip);
        assert(err == QOS_ERROR_PENDING);
    }
    
    for ( i = 0; i < MIN_RS_NUM / 2; ++i )
    {
        token_req_t *rip =
                            app_token_by_grp_list(
                                    test.permissions[i]
                                            ->lhead_reqgrp.next
                                            ->next);
        err = item->alloc_resource(item, &rip->token_need);
        test_enqueue(&test, rip);
        if ( err == QOS_ERROR_OK )
        {
            ++alloc_success;
        }
        assert(err == QOS_ERROR_OK);
    }
    
    do
    {
        struct list_head *pos = NULL;
        struct list_head got_list;
        LISTHEAD_INIT(&got_list);
        token_req_t *rip = test_dequeue(&test);
        if ( !rip )
        {
            break;
        }
        
        item->free_resource(item, &rip->token_need, &got_list);
        assert(list_empty(&got_list));
    } while ( 1 );
    
    assert(alloc_success == MIN_RS_NUM / 2);
    test_for_permission_item_exit(&test);
    item->reset(item);
    printf("[test_case_alloc_free_small] %s[OK]%s\n", GREEN, RESET);
}

static int
test_try_resource_changed(dispatch_node_t *item, long total,
                          int version)
{
    int              alloc_success = 0;
    struct list_head *pos          = NULL;
    struct list_head got_list;
    LISTHEAD_INIT(&got_list);
    dispatch2app_t dta;
    dta.version     = version;
    dta.token_quota = total;
    
    if ( version < item->base_node.version )
    {
        assert(true == item->resource_changed(item, &dta, &got_list));
        assert(list_empty(&got_list));
    }
    else
    {
        assert(false == item->resource_changed(item, &dta, &got_list));
        list_for_each(pos, &got_list)
        {
            ++alloc_success;
        }
    }
    return alloc_success;
    
}

static int test_try_pop_all(dispatch_node_t *item)
{
    int              alloc_failed = 0;
    struct list_head *pos         = NULL;
    struct list_head failed_list;
    LISTHEAD_INIT(&failed_list);
    
    item->pop_all(item, &failed_list);
    list_for_each(pos, &failed_list)
    {
        ++alloc_failed;
    }
    
    return alloc_failed;
}

static int
test_try_free(dispatch_node_t *item, test_for_permission_item_t *test,
              int max_num)
{
    int alloc_success = 0;
    int i             = 0;
    for ( i = 0; i < max_num; ++i )
    {
        struct list_head *pos = NULL;
        struct list_head got_list;
        LISTHEAD_INIT(&got_list);
        token_req_t *rip = test_dequeue(test);
        if ( !rip )
        {
            break;
        }
        
        item->free_resource(item, &rip->token_need, &got_list);
        list_for_each(pos, &got_list)
        {
            ++alloc_success;
        }
    }
    return alloc_success;
}

static void test_case_alloc_free_queue()
{
    int                        alloc_success = 0;
    int                        err           = QOS_ERROR_OK;
    int                        i             = 0;
    test_for_permission_item_t test;
    test_for_permission_item_init(&test, true);
    app2dispatch_t atd;
    
    for ( i = 0; i < MAX_PERMISSION_NUM_INTEST * 2; ++i )
    {
        token_req_t *rip = NULL;
        if ( i % 2 == 0 )
        {
            rip = app_token_by_grp_list(
                    test.permissions[i / 2]->lhead_reqgrp.next);
        }
        else
        {
            rip = app_token_by_grp_list(
                    test.permissions[i / 2]->lhead_reqgrp.next->next);
        }
        err              = item->alloc_resource(item, &rip->token_need);
        test_enqueue(&test, rip);
        if ( err == QOS_ERROR_OK )
        {
            ++alloc_success;
        }
        else
        {
            assert(err == QOS_ERROR_PENDING);
        }
    }
    
    alloc_success += test_try_free(item, &test, MAX_PERMISSION_NUM_INTEST * 2);
    assert(alloc_success == MAX_PERMISSION_NUM_INTEST);
    item->reset(item);
    item->get_protocol(item, &atd);
    assert(atd.press.fifo.depth == 0 && atd.version == 0);
    
    test_for_permission_item_exit(&test);
    printf("[test_case_alloc_free_queue] %s[OK]%s\n", GREEN, RESET);
}

static void test_case_alloc_free_queue_rand()
{
    int                        alloc_success = 0;
    int                        err           = QOS_ERROR_OK;
    int                        i             = 0;
    test_for_permission_item_t test;
    test_for_permission_item_init(&test, true);
    
    for ( i = 0; i < MAX_PERMISSION_NUM_INTEST * 3; ++i )
    {
        token_req_t *rip = NULL;
        if ( i % 3 == 0 )
        {
            rip = app_token_by_grp_list(
                    test.permissions[i / 3]->lhead_reqgrp.next);
        }
        else if ( i % 3 == 1 )
        {
            rip = app_token_by_grp_list(
                    test.permissions[i / 3]->lhead_reqgrp.next->next);
        }
        else if ( test.end - test.begin > 0 )
        {
            int free_num = rand() % ((test.end - test.begin) / 2 + 1) +
                           (test.end - test.begin) / 2 + 1;
            alloc_success += test_try_free(item, &test, free_num);
            continue;
        }
        
        err = item->alloc_resource(item, &rip->token_need);
        test_enqueue(&test, rip);
        if ( err == QOS_ERROR_OK )
        {
            ++alloc_success;
        }
        else
        {
            assert(err == QOS_ERROR_PENDING);
        }
    }
    
    alloc_success += test_try_free(item, &test, MAX_PERMISSION_NUM_INTEST * 2);
    assert(alloc_success == MAX_PERMISSION_NUM_INTEST);
    test_for_permission_item_exit(&test);
    printf("[test_case_alloc_free_queue_rand] %s[OK]%s\n", GREEN, RESET);
}

static void
test_case_alloc_free_resource_changed_enough()
{
    int                        alloc_success = 0;
    int                        err           = QOS_ERROR_OK;
    int                        i             = 0;
    app2dispatch_t             atd;
    test_for_permission_item_t test;
    test_for_permission_item_init(&test, false);
    
    for ( i = 0; i < MAX_PERMISSION_NUM_INTEST * 2; ++i )
    {
        token_req_t *rip = NULL;
        if ( i % 2 == 0 )
        {
            rip = app_token_by_grp_list(
                    test.permissions[i / 2]->lhead_reqgrp.next);
        }
        else
        {
            rip = app_token_by_grp_list(
                    test.permissions[i / 2]->lhead_reqgrp.next->next);
        }
        err              = item->alloc_resource(item, &rip->token_need);
        test_enqueue(&test, rip);
        if ( err == QOS_ERROR_OK )
        {
            ++alloc_success;
        }
        else
        {
            assert(err == QOS_ERROR_PENDING);
        }
    }
    alloc_success +=
            test_try_resource_changed(item, MAX_PERMISSION_NUM_INTEST * 2,
                                      test_version++);
    item->get_protocol(item, &atd);
    assert(atd.press.val == 0 && atd.version == test_version - 1);
    item->reset(item);
    assert(alloc_success == MAX_PERMISSION_NUM_INTEST);
    test_try_free(item, &test, MAX_PERMISSION_NUM_INTEST * 2);
    test_for_permission_item_exit(&test);
    printf("[test_case_alloc_free_resource_changed_enough] %s[OK]%s\n", GREEN,
           RESET);
}


static void test_case_alloc_free_pop_all()
{
    int                        alloc_success = 0;
    int                        alloc_fail    = 0;
    int                        err           = QOS_ERROR_OK;
    int                        i             = 0;
    test_for_permission_item_t test;
    test_for_permission_item_init(&test, false);
    
    for ( i = 0; i < MAX_PERMISSION_NUM_INTEST * 2; ++i )
    {
        token_req_t *rip = NULL;
        if ( i % 2 == 0 )
        {
            rip = app_token_by_grp_list(
                    test.permissions[i / 2]->lhead_reqgrp.next);
        }
        else
        {
            rip = app_token_by_grp_list(
                    test.permissions[i / 2]->lhead_reqgrp.next->next);
        }
        err              = item->alloc_resource(item, &rip->token_need);
        test_enqueue(&test, rip);
        if ( err == QOS_ERROR_OK )
        {
            ++alloc_success;
        }
        else
        {
            assert(err == QOS_ERROR_PENDING);
        }
    }
    alloc_fail += test_try_pop_all(item);
    assert(alloc_success + alloc_fail == MAX_PERMISSION_NUM_INTEST);
    assert(NULL == item->lhead_nodereq.front(&item->lhead_nodereq));
    while ( test_dequeue(&test) );
    test_for_permission_item_exit(&test);
    printf("[test_case_alloc_free_resource_changed_enough] %s[OK]%s\n", GREEN,
           RESET);
}

static void
test_case_alloc_free_resource_changed()
{
    int                        alloc_success = 0;
    int                        err           = QOS_ERROR_OK;
    int                        i             = 0;
    test_for_permission_item_t test;
    test_for_permission_item_init(&test, true);
    
    for ( i = 0; i < MAX_PERMISSION_NUM_INTEST * 2; ++i )
    {
        token_req_t *rip = NULL;
        if ( i % 2 == 0 )
        {
            rip = app_token_by_grp_list(
                    test.permissions[i / 2]->lhead_reqgrp.next);
        }
        else
        {
            rip = app_token_by_grp_list(
                    test.permissions[i / 2]->lhead_reqgrp.next->next);
        }
        err              = item->alloc_resource(item, &rip->token_need);
        test_enqueue(&test, rip);
        if ( err == QOS_ERROR_OK )
        {
            ++alloc_success;
        }
        else
        {
            assert(err == QOS_ERROR_PENDING);
        }
    }
    alloc_success +=
            test_try_resource_changed(item, MIN_RS_NUM + 1, test_version++);
    alloc_success += test_try_free(item, &test, MAX_PERMISSION_NUM_INTEST * 2);
    item->reset(item);
    assert(alloc_success == MAX_PERMISSION_NUM_INTEST);
    test_for_permission_item_exit(&test);
    printf("[test_case_alloc_free_resource_changed] %s[OK]%s\n", GREEN, RESET);
}

static int test_init()
{
    printf(YELLOW"--------------test_init------------:\n"RESET);
    
    srand(time(0));
    test_case_init_exit(item);
    return CUE_SUCCESS;
}

static int test_clean()
{
    dispatch_node_exit(item);
    printf(BLUE"--------------test_clean--------:\n"RESET);
    return CUE_SUCCESS;
}

void node_inapp_suit_init(test_frame_t *frame)
{
    test_suit_t suit;
    
    test_suit_init(&suit, "node_inapp_suit_init", test_init, test_clean);
    
    suit.add_case(&suit, "test_case_alloc_free_small",
                  test_case_alloc_free_small);
    
    suit.add_case(&suit, "test_case_alloc_free_queue",
                  test_case_alloc_free_queue);
    
    suit.add_case(&suit, "test_case_alloc_free_queue_rand",
                  test_case_alloc_free_queue_rand);
    
    suit.add_case(&suit, "test_case_alloc_free_resource_changed",
                  test_case_alloc_free_resource_changed);
    
    suit.add_case(&suit, "test_case_alloc_free_resource_changed_enough",
                  test_case_alloc_free_resource_changed_enough);
    
    suit.add_case(&suit, "test_case_alloc_free_pop_all",
                  test_case_alloc_free_pop_all);
    
    frame->add_suit(frame, &suit);
}
