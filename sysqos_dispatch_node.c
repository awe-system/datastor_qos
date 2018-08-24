//
// Created by root on 18-7-29.
//

#include <assert.h>
#include <pthread.h>
#include "sysqos_dispatch_node.h"
#include "sysqos_token_reqgrp.h"

static struct list_head *
permission_list_if_final_by_rs_list(struct list_head *list)
{
    token_reqgrp_t *permission = permission_by_token_list(list);
    token_req_t     *rip        =
                            app_token_by_list(list);
    int             err         = permission->to_got(permission, rip);
    if ( err == QOS_ERROR_FAILEDNODE || err == QOS_ERROR_OK )
    {
        return &permission->lhead_reqgrep_got;
    }
    return NULL;
}

static void resource_increased(dispatch_node_t *item,
                               struct list_head *got_permission_list)
{
    int               err              = QOS_ERROR_OK;
    struct list_head  *permission_list = NULL;
    nodereq_list_t *tokens          = &item->lhead_nodereq;
    dispatch_base_node_t  *desc            = &item->base_node;
    resource_list_t   *rs              = NULL;
    assert(item && got_permission_list);
    
    rs = tokens->front(tokens);
    if ( !rs )
    {
        return;
    }
    
    pthread_spin_lock(&item->lck);
    while ( NULL != (rs = tokens->front(tokens)) )
    {
        err = desc->try_alloc_from_base(desc, rs->rs.cost);
        if ( err )
        {
            break;
        }
        tokens->pop_front(tokens);
        permission_list = permission_list_if_final_by_rs_list(&rs->list);
        if ( permission_list )
        {
            list_add(permission_list, got_permission_list);
        }
    }
    pthread_spin_unlock(&item->lck);
}

static bool resource_changed(dispatch_node_t *item,
                             dispatch2app_t *dta,
                             struct list_head *got_permission_list)
{
    bool is_reset = false;
    assert(item && dta && got_permission_list);
    bool is_increased = item->base_node
            .check_update_quota_version(&item->base_node, dta->total, dta->ver,
                                        &is_reset);
    if ( is_increased )
    {
        resource_increased(item, got_permission_list);
    }
    return is_reset;
}

static void reset(dispatch_node_t *item)
{
    item->base_node.reset(&item->base_node);
}

//返回的是获得
static void
free_resource(struct dispatch_node *item, resource_list_t *rs,
              struct list_head *got_permission_list)
{
    token_req_t
            *rip = app_token_by_list(&rs->list);
    assert(item && rs && got_permission_list);
    //NOTE:删除之前的资源数和当前版本的资源数无关
    if ( rip->Nodeid == item->fence_id )
    {
        item->base_node.free_to_base(&item->base_node, rs->rs.cost);
        resource_increased(item, got_permission_list);
    }
}

//失败则放入队列
static int
alloc_resource(struct dispatch_node *item, resource_list_t *rs)
{
    assert(item && rs);
    int         err  = QOS_ERROR_OK;
    token_req_t *rip = app_token_by_list(
            &rs->list);
    rip->Nodeid = item->fence_id;
    err = item->base_node.try_alloc_from_base(&item->base_node, rs->rs.cost);
    if ( err )
    {
        pthread_spin_lock(&item->lck);
        err = item->base_node.try_alloc_from_base(&item->base_node, rs->rs.cost);
        if ( err )
        {
            item->lhead_nodereq.push_back(&item->lhead_nodereq, rs);
        }
        else
        {
            token_reqgrp_t *permission = rip->token_grp;
            err                         = permission->to_got(permission, rip);
        }
        pthread_spin_unlock(&item->lck);
    }
    else
    {
        token_reqgrp_t *permission = rip->token_grp;
        err = permission->to_got(permission, rip);
    }
    
    
    return err;
}

static void get_protocol(struct dispatch_node *item,
                         app2dispatch_t *atd)
{
    atd->press   = item->lhead_nodereq.get_press(&item->lhead_nodereq);
    atd->ver     = item->base_node.get_version(&item->base_node);
    atd->alloced = item->base_node.get_token_inuse(&item->base_node);
}

static void pop_all(struct dispatch_node *item,
                    struct list_head *fail_permission_list)
{
    struct list_head *pos = NULL;
    struct list_head *tmp = NULL;
    struct list_head fail_token_list;
    LISTHEAD_INIT(&fail_token_list);
    
    item->lhead_nodereq.pop_all(&item->lhead_nodereq, &fail_token_list);
    list_for_each_safe(pos, tmp, &fail_token_list)
    {
        token_reqgrp_t *permission = permission_by_token_list(pos);
        token_req_t     *rip        =
                                app_token_by_list(
                                        pos);
        
        int err = permission->to_failed(permission, rip);
        if ( err == QOS_ERROR_FAILEDNODE )
        {
            list_add(&permission->lhead_reqgrep_got, fail_permission_list);
        }
    }
}

void dispatch_node_exit(dispatch_node_t *item)
{
    assert(item);
    pthread_spin_destroy(&item->lck);
    nodereq_list_exit(&item->lhead_nodereq);
    dispatch_base_node_exit(&item->base_node);
}

int dispatch_node_init(dispatch_node_t *item, int version)
{
    int err = QOS_ERROR_OK;
    assert(item);
    
    err = dispatch_base_node_init(&item->base_node);
    if ( err )
    {
        goto disp_desc_init_failed;
    }
    
    err = nodereq_list_init(&item->lhead_nodereq);
    if ( err )
    {
        goto token_list_init_failed;
    }
    
    err = pthread_spin_init(&item->lck, PTHREAD_PROCESS_PRIVATE);
    if ( err )
        end_func(err, QOS_ERROR_MEMORY, spin_init_failed);
    
    item->fence_id = version;
    
    item->resource_changed = resource_changed;
    item->free_resource    = free_resource;
    item->alloc_resource   = alloc_resource;
    item->get_protocol     = get_protocol;
    item->pop_all          = pop_all;
    item->reset            = reset;
    
    return err;
    pthread_spin_destroy(&item->lck);
spin_init_failed:
    nodereq_list_exit(&item->lhead_nodereq);
token_list_init_failed:
    dispatch_base_node_exit(&item->base_node);
disp_desc_init_failed:
    return err;
}

/**for test*****************************************************************************************/
#ifdef PERMISSION_MANAGER_ITEM_TEST

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sysqos_common.h"
#include "sysqos_container_item.h"

#define MAX_PERMISSION_NUM_INTEST 16
int test_version = 0;
#endif

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
    memory_cache_t  permission_cache;
    memory_cache_t  resource_cache;
    token_reqgrp_t *permissions[MAX_PERMISSION_NUM_INTEST];
    void            *queue[MAX_PERMISSION_NUM_INTEST * 2 + 1];
    int             begin;
    int             end;
}   test_for_permission_item_t;

static void test_enqueue(test_for_permission_item_t *test, void *val)
{
    test->queue[test->end] = val;
    if ( ++test->end > MAX_PERMISSION_NUM_INTEST*2 )
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
    if ( ++test->begin > MAX_PERMISSION_NUM_INTEST*2 )
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

static void test_case_alloc_free_small(dispatch_node_t *item)
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
    dta.ver   = version;
    dta.total = total;
    
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

static void test_case_alloc_free_queue(dispatch_node_t *item)
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
    assert(atd.press.fifo.depth == 0 && atd.ver == 0);
    
    test_for_permission_item_exit(&test);
    printf("[test_case_alloc_free_queue] %s[OK]%s\n", GREEN, RESET);
}

static void test_case_alloc_free_queue_rand(dispatch_node_t *item)
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
test_case_alloc_free_resource_changed_enough(dispatch_node_t *item)
{
    int                        alloc_success = 0;
    int                        err           = QOS_ERROR_OK;
    int                        i             = 0;
    app2dispatch_t              atd;
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
    assert(atd.press.val == 0 && atd.ver == test_version - 1);
    item->reset(item);
    assert(alloc_success == MAX_PERMISSION_NUM_INTEST);
    test_try_free(item, &test, MAX_PERMISSION_NUM_INTEST * 2);
    test_for_permission_item_exit(&test);
    printf("[test_case_alloc_free_resource_changed_enough] %s[OK]%s\n", GREEN,
           RESET);
}


static void test_case_alloc_free_pop_all(dispatch_node_t *item)
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
test_case_alloc_free_resource_changed(dispatch_node_t *item)
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

void test_dispatch_node()
{
#ifdef PERMISSION_MANAGER_ITEM_TEST
    int err = 0;
    printf(YELLOW"--------------test_dispatch_node------------:\n"RESET);
    srand(time(0));
    dispatch_node_t item;
    test_case_init_exit(&item);
    
    test_case_alloc_free_small(&item);
    
    test_case_alloc_free_queue(&item);
    
    test_case_alloc_free_queue_rand(&item);
    
    test_case_alloc_free_resource_changed(&item);
    
    test_case_alloc_free_resource_changed_enough(&item);
    
    test_case_alloc_free_pop_all(&item);
    
    dispatch_node_exit(&item);
    printf(BLUE"--------------test_dispatch_node end--------:\n"RESET);
#endif
}
