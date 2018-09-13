//
// Created by root on 18-9-5.
//


#include "token_manager.h"

#define TABLE_LEN 127

#define TEST_RESOURCE_PER_PERMISSION 3
#define MAX_QUEUE_NUM MAX(TABLE_LEN*TEST_RESOURCE_PER_PERMISSION,MIN_RS_NUM)
#define MAX_PERMISSION_NUM MAX_QUEUE_NUM
#define MAX_RESOURCE_NUM  MAX_PERMISSION_NUM * TEST_RESOURCE_PER_PERMISSION
#define MAX_NODE_NUM      MAX_QUEUE_NUM
static token_reqgrp_manager_t manager_static;
static token_reqgrp_manager_t *manager = &manager_static;

typedef struct test_context
{
    void            *queue[2 * MAX_QUEUE_NUM + 1];
    int             begin;
    int             end;
    int             err;
    resource_list_t rs[MAX_QUEUE_NUM * 3];
}                             test_context_t;

static void test_ctx_init(test_context_t *test, bool is_pending)
{
    int i = 0;
    test->begin = test->end = 0;
    test->err   = QOS_ERROR_OK;
    memset(test->queue, 0, (2 * MAX_QUEUE_NUM + 1) * sizeof(void *));
    for ( i = 0; i < MAX_QUEUE_NUM * 3; ++i )
    {
        long id = i % (MAX_QUEUE_NUM);
        LISTHEAD_INIT(&test->rs[i].list);
        test->rs[i].rs.id = (void *) id;
        if ( is_pending )
        {
            test->rs[i].rs.cost = MIN_RS_NUM;
        }
        else
        {
            test->rs[i].rs.cost = 1;
        }
    }
    
    for ( i = MAX_QUEUE_NUM; i < MAX_QUEUE_NUM * 2; ++i )
    {
        list_add(&test->rs[i].list, &test->rs[(i + 1) % (MAX_QUEUE_NUM)].list);
    }
    
    for ( i = MAX_QUEUE_NUM * 2; i < MAX_QUEUE_NUM * 3; ++i )
    {
        list_add(&test->rs[i].list, &test->rs[(i + 2) % (MAX_QUEUE_NUM)].list);
    }
}

static void test_enqueue(test_context_t *test, void *val)
{
    test->queue[test->end] = val;
    if ( ++test->end > 2 * MAX_QUEUE_NUM )
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
    if ( ++test->begin > 2 * MAX_QUEUE_NUM )
    {
        test->begin = 0;
    }
    return res;
}

static void test_token_grp_got(void *token_grp, void *pri, int err)
{
    test_context_t *test = pri;
    if ( err )
    {
        test->err = err;
    }
    test_enqueue(test, token_grp);
}

static applicant_event_ops_t test_app_event =
                                     {
                                             .token_grp_got = test_token_grp_got,
                                     };

static msg_ops_t test_msg_ops =
                         {
                                 .snd_msg = NULL,
                                 .compare = test_compare,
                                 .hash_id = test_hash,
                         };


static bool test_are_empty(token_reqgrp_manager_t *manager)
{
    int i = 0;
    for ( i = 0; i < manager->app_node_table->tab_len; ++i )
    {
        struct list_head *pos  = NULL;
        struct list_head *head = &manager->app_node_table->hash_table[i].list;
        list_for_each(pos, head)
        {
            dispatch_node_t
                    *item = container_of(pos, qos_container_item_t, list)->pri;
            assert(item);
            if ( item->lhead_nodereq.press != 0 )
            {
                return false;
            }
        }
    }
    return true;
}

static bool test_are_alloc_small_total(token_reqgrp_manager_t *manager)
{
    int i = 0;
    for ( i = 0; i < manager->app_node_table->tab_len; ++i )
    {
        struct list_head *pos  = NULL;
        struct list_head *head = &manager->app_node_table->hash_table[i].list;
        list_for_each(pos, head)
        {
            dispatch_node_t
                    *item = container_of(pos, qos_container_item_t, list)->pri;
            assert(item);
            if ( item->base_node.token_inuse >
                 item->base_node.token_quota_target )
            {
                return false;
            }
        }
    }
    return true;
}

static void test_check_snd_buf_legal(token_reqgrp_manager_t *manager,
                                     long version,
                                     long press,
                                     long alloced)
{
    int i = 0;
    for ( i = 0; i < manager->app_node_table->tab_len; ++i )
    {
        struct list_head *pos  = NULL;
        struct list_head *head = &manager->app_node_table->hash_table[i].list;
        list_for_each(pos, head)
        {
            qos_container_item_t *container_item =
                                         container_of(pos, qos_container_item_t,
                                                      list);
            dispatch_node_t
                                 *item           = container_item->pri;
            app2dispatch_t       atd;
            assert(item);
            assert(sizeof(app2dispatch_t) == manager->msg_event
                    .snd_msg_len(&manager->msg_event, container_item->id));
            assert(0 == manager->msg_event.snd_msg_buf(&manager->msg_event,
                                                       container_item->id,
                                                       sizeof(app2dispatch_t),
                                                       (unsigned char *) &atd));
            if ( atd.token_in_use != alloced )
            {
                assert(atd.token_in_use == alloced);
            }
            if ( version >= 0 )
            {
                assert(atd.version == version);
            }
            assert(atd.token_in_use == alloced);
            assert(atd.press.fifo.depth == press);
        }
    }
}


static void test_case_get_token_grp_right_now()
{
    static int     exe_time   = 0;
    test_context_t test;
    unsigned long  i          = 0;
    token_reqgrp_t *token_grp = NULL;
    
    test_ctx_init(&test, false);
    
    for ( i = 0; i < MAX_QUEUE_NUM; ++i )
    {
        int err = manager->get_token_reqgrp(manager, &test.rs[i], &test,
                                            &token_grp);
        assert(err == QOS_ERROR_OK);
        test_enqueue(&test, token_grp);
    }
    assert(test_are_empty(manager));
    i = 0;
    while ( test.begin != test.end )
    {
        token_grp = (token_reqgrp_t *) test_dequeue(&test);
        manager->put_token_reqgrp(manager, token_grp, NULL);
        ++i;
    }
    assert(i == MAX_QUEUE_NUM);

#ifdef CACHE_OPEN_CNT
    assert(manager->resource_cache.alloc_cnt ==
           manager->resource_cache.free_cnt);
    assert(manager->token_grp_cache.alloc_cnt ==
           manager->token_grp_cache.free_cnt);
    assert(manager->app_node_table->cache.alloc_cnt ==
           manager->app_node_table->cache.free_cnt + MAX_QUEUE_NUM);
    assert(manager->manager_item_cache.alloc_cnt ==
           manager->manager_item_cache.free_cnt + MAX_QUEUE_NUM);
#endif
    printf("[test_case_get_token_grp_right_now for NO.%d] %s[OK]%s\n",
           ++exe_time, GREEN, RESET);
}

static void test_case_get_token_grp_pending()
{
    static int     exe_time      = 0;
    test_context_t test;
    int            i             = 0;
    bool           pending_exist = false;
    token_reqgrp_t *token_grp    = NULL;
    
    test_ctx_init(&test, true);
    
    for ( i = 0; i < MAX_QUEUE_NUM; ++i )
    {
        int err = manager->get_token_reqgrp(manager, &test.rs[i], &test,
                                            &token_grp);
        
        assert(err == QOS_ERROR_OK || err == QOS_ERROR_PENDING);
        if ( err == QOS_ERROR_OK )
        {
            test_enqueue(&test, token_grp);
        }
        else
        {
            pending_exist = true;
        }
    }
    assert(pending_exist);
    assert(!test_are_empty(manager));
    i = 0;
    while ( test.begin != test.end )
    {
        token_grp = (token_reqgrp_t *) test_dequeue(&test);
        manager->put_token_reqgrp(manager, token_grp, NULL);
        ++i;
    }
    assert(i == MAX_QUEUE_NUM);

#ifdef CACHE_OPEN_CNT
    assert(manager->resource_cache.alloc_cnt ==
           manager->resource_cache.free_cnt);
    assert(manager->token_grp_cache.alloc_cnt ==
           manager->token_grp_cache.free_cnt);
    assert(manager->app_node_table->cache.alloc_cnt ==
           manager->app_node_table->cache.free_cnt + MAX_QUEUE_NUM);
    assert(manager->manager_item_cache.alloc_cnt ==
           manager->manager_item_cache.free_cnt + MAX_QUEUE_NUM);
#endif
    printf("[test_case_get_token_grp_pending for NO.%d] %s[OK]%s\n",
           ++exe_time, GREEN, RESET);
}

static void test_case_token_grp_outof_memory()
{
    static int     exe_time      = 0;
    test_context_t test;
    int            i             = 0;
    int            j             = 0;
    bool           pending_exist = false;
    token_reqgrp_t *token_grp    = NULL;
    
    token_reqgrp_manager_exit(manager);
    
    int err = token_reqgrp_manager_init(manager, TABLE_LEN, MAX_QUEUE_NUM / 2,
                                        MAX_RESOURCE_NUM, MAX_NODE_NUM);
    assert(err == 0);
    manager->set_app_event_ops(manager, &test_app_event);
    manager->set_msg_ops(manager, &test_msg_ops);
    
    test_ctx_init(&test, false);
    
    for ( i = 0; i < MAX_QUEUE_NUM; ++i )
    {
        err = manager->get_token_reqgrp(manager, &test.rs[i], &test,
                                  &token_grp);
        if ( err == QOS_ERROR_MEMORY )
        {
            while ( test.begin != test.end )
            {
                token_grp = (token_reqgrp_t *) test_dequeue(&test);
                manager->put_token_reqgrp(manager, token_grp, NULL);
                ++j;
            }
            --i;
            continue;
        }
        assert(err == QOS_ERROR_OK || err == QOS_ERROR_PENDING);
        if ( err == QOS_ERROR_OK )
        {
            test_enqueue(&test, token_grp);
        }
        else
        {
            pending_exist = true;
        }
    }
    assert(!pending_exist);
    assert(test_are_empty(manager));
    
    while ( test.begin != test.end )
    {
        token_grp = (token_reqgrp_t *) test_dequeue(&test);
        manager->put_token_reqgrp(manager, token_grp, NULL);
        ++j;
    }
    assert(j == MAX_QUEUE_NUM);
#ifdef CACHE_OPEN_CNT
    assert(manager->resource_cache.alloc_cnt ==
           manager->resource_cache.free_cnt);
    assert(manager->token_grp_cache.alloc_cnt ==
           manager->token_grp_cache.free_cnt);
    assert(manager->app_node_table->cache.alloc_cnt ==
           manager->app_node_table->cache.free_cnt + MAX_QUEUE_NUM);
    assert(manager->manager_item_cache.alloc_cnt ==
           manager->manager_item_cache.free_cnt + MAX_QUEUE_NUM);
#endif
    token_reqgrp_manager_exit(manager);
    err = token_reqgrp_manager_init(manager, TABLE_LEN, MAX_PERMISSION_NUM,
                                    MAX_RESOURCE_NUM, MAX_NODE_NUM);
    assert(err == 0);
    manager->set_app_event_ops(manager, &test_app_event);
    manager->set_msg_ops(manager, &test_msg_ops);
    
    printf("[test_case_token_grp_outof_memory for NO.%d] %s[OK]%s\n",
           ++exe_time, GREEN, RESET);
}


static void test_case_resource_outof_memory()
{
    static int     exe_time      = 0;
    test_context_t test;
    int            i             = 0;
    int            j             = 0;
    bool           pending_exist = false;
    token_reqgrp_t *token_grp    = NULL;
    
    token_reqgrp_manager_exit(manager);
    
    int err = token_reqgrp_manager_init(manager, TABLE_LEN, MAX_PERMISSION_NUM,
                                        MAX_QUEUE_NUM, MAX_NODE_NUM);
    assert(err == 0);
    manager->set_app_event_ops(manager, &test_app_event);
    manager->set_msg_ops(manager, &test_msg_ops);
    
    test_ctx_init(&test, false);
    
    for ( i = 0; i < MAX_QUEUE_NUM; ++i )
    {
        err = manager->get_token_reqgrp(manager, &test.rs[i], &test,
                                  &token_grp);
        if ( err == QOS_ERROR_MEMORY )
        {
            while ( test.begin != test.end )
            {
                token_grp = (token_reqgrp_t *) test_dequeue(&test);
                manager->put_token_reqgrp(manager, token_grp, NULL);
                ++j;
            }
            --i;
            continue;
        }
        assert(err == QOS_ERROR_OK || err == QOS_ERROR_PENDING);
        if ( err == QOS_ERROR_OK )
        {
            test_enqueue(&test, token_grp);
        }
        else
        {
            pending_exist = true;
        }
    }
    assert(!pending_exist);
    assert(test_are_empty(manager));
    
    while ( test.begin != test.end )
    {
        token_grp = (token_reqgrp_t *) test_dequeue(&test);
        manager->put_token_reqgrp(manager, token_grp, NULL);
        ++j;
    }
    assert(j == MAX_QUEUE_NUM);

#ifdef CACHE_OPEN_CNT
    assert(manager->resource_cache.alloc_cnt ==
           manager->resource_cache.free_cnt);
    assert(manager->token_grp_cache.alloc_cnt ==
           manager->token_grp_cache.free_cnt);
    assert(manager->app_node_table->cache.alloc_cnt ==
           manager->app_node_table->cache.free_cnt + MAX_QUEUE_NUM);
    assert(manager->manager_item_cache.alloc_cnt ==
           manager->manager_item_cache.free_cnt + MAX_QUEUE_NUM);
#endif
    token_reqgrp_manager_exit(manager);
    
    err = token_reqgrp_manager_init(manager, TABLE_LEN, MAX_PERMISSION_NUM,
                                    MAX_RESOURCE_NUM, MAX_NODE_NUM);
    assert(err == 0);
    manager->set_app_event_ops(manager, &test_app_event);
    manager->set_msg_ops(manager, &test_msg_ops);
    
    printf("[test_case_resource_outof_memory for NO.%d] %s[OK]%s\n",
           ++exe_time, GREEN, RESET);
}


static void
test_rcvd_increased(token_reqgrp_manager_t *manager)
{
    int i = 0;
    for ( i = 0; i < MAX_QUEUE_NUM; ++i )
    {
        dispatch_node_t *item;
        void            **ppitem = (void **) &item;
        long            id       = i;
        dispatch2app_t  dta;
        int             err      = manager->app_node_table
                ->find(manager->app_node_table, (void *) id, ppitem);
        assert(err == 0);
        dta.version     = item->base_node.get_version(&item->base_node) + 1;
        dta.token_quota = MAX_QUEUE_NUM;
        manager->msg_event
                .rcvd(&manager->msg_event, (void *) id, sizeof(dispatch2app_t),
                      (unsigned char *) &dta);
    }
}

static void
test_rcvd_decreased(token_reqgrp_manager_t *manager)
{
    int i = 0;
    for ( i = 0; i < MAX_QUEUE_NUM; ++i )
    {
        dispatch_node_t *item;
        void            **ppitem = (void **) &item;
        long            id       = i;
        dispatch2app_t  dta;
        int             err      = manager->app_node_table
                ->find(manager->app_node_table, (void *) id, ppitem);
        assert(err == 0);
        dta.version     = item->base_node.get_version(&item->base_node) + 1;
        dta.token_quota = MIN_RS_NUM;
        manager->msg_event
                .rcvd(&manager->msg_event, (void *) id, sizeof(dispatch2app_t),
                      (unsigned char *) &dta);
    }
}

static void
test_rcvd_reset(token_reqgrp_manager_t *manager)
{
    int i = 0;
    for ( i = 0; i < MAX_QUEUE_NUM; ++i )
    {
        dispatch_node_t *item;
        void            **ppitem = (void **) &item;
        long            id       = i;
        dispatch2app_t  dta;
        int             err      = manager->app_node_table
                ->find(manager->app_node_table,
                       (void *) id,
                       ppitem);
        assert(err == 0);
        dta.version     = 0;
        dta.token_quota = 0;
        manager->msg_event
                .rcvd(&manager->msg_event, (void *) id, sizeof(dispatch2app_t),
                      (unsigned char *) &dta);
    }
}

static void test_case_rcvd_increased()
{
    static int     exe_time   = 0;
    test_context_t test;
    int            i          = 0;
    token_reqgrp_t *token_grp = NULL;
    
    test_ctx_init(&test, true);
    
    for ( i = 0; i < MAX_QUEUE_NUM; ++i )
    {
        int err = manager->get_token_reqgrp(manager, &test.rs[i], &test,
                                            &token_grp);
        assert(err == QOS_ERROR_OK || err == QOS_ERROR_PENDING);
        if ( err == QOS_ERROR_OK )
        {
            test_enqueue(&test, token_grp);
        }
    }
    
    test_check_snd_buf_legal(manager, 0, MIN_RS_NUM * 2, MIN_RS_NUM);
    test_rcvd_increased(manager);
    test_check_snd_buf_legal(manager, 1, 0, MIN_RS_NUM * 3);
    assert(test_are_empty(manager));
    
    i = 0;
    while ( test.begin != test.end )
    {
        token_grp = (token_reqgrp_t *) test_dequeue(&test);
        manager->put_token_reqgrp(manager, token_grp, NULL);
        ++i;
    }
    assert(i == MAX_QUEUE_NUM);

#ifdef CACHE_OPEN_CNT
    assert(manager->resource_cache.alloc_cnt ==
           manager->resource_cache.free_cnt);
    assert(manager->token_grp_cache.alloc_cnt ==
           manager->token_grp_cache.free_cnt);
    assert(manager->app_node_table->cache.alloc_cnt ==
           manager->app_node_table->cache.free_cnt + MAX_QUEUE_NUM);
    assert(manager->manager_item_cache.alloc_cnt ==
           manager->manager_item_cache.free_cnt + MAX_QUEUE_NUM);
#endif
    test_rcvd_reset(manager);
    printf("[test_case_rcvd_increased for NO.%d] %s[OK]%s\n", ++exe_time, GREEN,
           RESET);
}


static void test_case_rcvd_decreased()
{
    static int     exe_time   = 0;
    test_context_t test;
    int            i          = 0;
    token_reqgrp_t *token_grp = NULL;
    
    test_ctx_init(&test, true);
    
    for ( i = 0; i < MAX_QUEUE_NUM; ++i )
    {
        int err = manager->get_token_reqgrp(manager, &test.rs[i], &test,
                                            &token_grp);
        assert(err == QOS_ERROR_OK || err == QOS_ERROR_PENDING);
        if ( err == QOS_ERROR_OK )
        {
            test_enqueue(&test, token_grp);
        }
    }
    
    test_check_snd_buf_legal(manager, 0, MIN_RS_NUM * 2, MIN_RS_NUM);
    assert(test_are_alloc_small_total(manager));
    test_rcvd_increased(manager);
    test_check_snd_buf_legal(manager, 1, 0, MIN_RS_NUM * 3);
    assert(test_are_empty(manager));
    test_rcvd_decreased(manager);
    assert(test_are_alloc_small_total(manager));
    test_check_snd_buf_legal(manager, 2, 0, MIN_RS_NUM * 3);
    i = 0;
    while ( test.begin != test.end )
    {
        token_grp = (token_reqgrp_t *) test_dequeue(&test);
        manager->put_token_reqgrp(manager, token_grp, NULL);
        ++i;
    }
    assert(i == MAX_QUEUE_NUM);

#ifdef CACHE_OPEN_CNT
    assert(manager->resource_cache.alloc_cnt ==
           manager->resource_cache.free_cnt);
    assert(manager->token_grp_cache.alloc_cnt ==
           manager->token_grp_cache.free_cnt);
    assert(manager->app_node_table->cache.alloc_cnt ==
           manager->app_node_table->cache.free_cnt + MAX_QUEUE_NUM);
    assert(manager->manager_item_cache.alloc_cnt ==
           manager->manager_item_cache.free_cnt + MAX_QUEUE_NUM);
#endif
    test_rcvd_reset(manager);
    printf("[test_case_rcvd_decreased for NO.%d] %s[OK]%s\n", ++exe_time, GREEN,
           RESET);
}

static void test_case_rcvd_reset()
{
    static int     exe_time   = 0;
    test_context_t test;
    int            i          = 0;
    token_reqgrp_t *token_grp = NULL;
    
    test_ctx_init(&test, true);
    
    for ( i = 0; i < MAX_QUEUE_NUM; ++i )
    {
        int err = manager->get_token_reqgrp(manager, &test.rs[i], &test,
                                            &token_grp);
        assert(err == QOS_ERROR_OK || err == QOS_ERROR_PENDING);
        if ( err == QOS_ERROR_OK )
        {
            test_enqueue(&test, token_grp);
        }
    }
    test_check_snd_buf_legal(manager, -1, MIN_RS_NUM * 2, MIN_RS_NUM);
    assert(test_are_alloc_small_total(manager));
    test_rcvd_increased(manager);
    test_check_snd_buf_legal(manager, -1, 0, MIN_RS_NUM * 3);
    assert(test_are_empty(manager));
    test_rcvd_reset(manager);
    assert(!test_are_alloc_small_total(manager));
    test_check_snd_buf_legal(manager, 0, 0, MIN_RS_NUM * 3);
    
    i = 0;
    while ( test.begin != test.end )
    {
        token_grp = (token_reqgrp_t *) test_dequeue(&test);
        manager->put_token_reqgrp(manager, token_grp, NULL);
        ++i;
    }
    assert(i == MAX_QUEUE_NUM);

#ifdef CACHE_OPEN_CNT
    assert(manager->resource_cache.alloc_cnt ==
           manager->resource_cache.free_cnt);
    assert(manager->token_grp_cache.alloc_cnt ==
           manager->token_grp_cache.free_cnt);
    assert(manager->app_node_table->cache.alloc_cnt ==
           manager->app_node_table->cache.free_cnt + MAX_QUEUE_NUM);
    assert(manager->manager_item_cache.alloc_cnt ==
           manager->manager_item_cache.free_cnt + MAX_QUEUE_NUM);
#endif
    printf("[test_case_rcvd_reset for NO.%d] %s[OK]%s\n", ++exe_time, GREEN,
           RESET);
}

static void
fullfail_put_token_grp(token_reqgrp_manager_t *manager,
                       token_reqgrp_t *permission)
{
    int              i       = 0;
    struct list_head *pos    = NULL;
    struct list_head *tmp    = NULL;
    resource_list_t  *tmp_rs = NULL;
    struct list_head tmp_head;
    
    LISTHEAD_INIT(&tmp_head);
    resource_list_t rs[TEST_RESOURCE_PER_PERMISSION];
    list_for_each_safe(pos, tmp, &permission->lhead_reqgrp)
    {
        tmp_rs = &app_token_by_grp_list(pos)->token_need;
        LISTHEAD_INIT(&rs[i].list);
        rs[i].rs.id   = tmp_rs->rs.id;
        rs[i].rs.cost = tmp_rs->rs.cost;
        list_add(&rs[i].list, &tmp_head);
        ++i;
    }
    assert(!list_empty(&tmp_head));
    tmp_rs = container_of(tmp_head.next, resource_list_t, list);
    list_del(&tmp_head);
    manager->put_token_reqgrp(manager, permission, tmp_rs);
}

static void
partfail_put_permission(token_reqgrp_manager_t *manager,
                        token_reqgrp_t *permission)
{
    int              i       = 0, j = 0;
    struct list_head *pos    = NULL;
    struct list_head *tmp    = NULL;
    resource_list_t  *tmp_rs = NULL;
    struct list_head tmp_head;
    
    LISTHEAD_INIT(&tmp_head);
    resource_list_t rs[TEST_RESOURCE_PER_PERMISSION];
    list_for_each_safe(pos, tmp, &permission->lhead_reqgrp)
    {
        tmp_rs = &app_token_by_grp_list(pos)->token_need;
        if ( ++j % 2 == 1 )
        {
            LISTHEAD_INIT(&rs[i].list);
            rs[i].rs.id   = tmp_rs->rs.id;
            rs[i].rs.cost = tmp_rs->rs.cost;
            list_add(&rs[i].list, &tmp_head);
            ++i;
        }
    }
    assert(!list_empty(&tmp_head));
    tmp_rs = container_of(tmp_head.next, resource_list_t, list);
    list_del(&tmp_head);
    manager->put_token_reqgrp(manager, permission, tmp_rs);
}

static void test_case_put_partfailed()
{
    static int     exe_time    = 0;
    test_context_t test;
    int            i           = 0;
    token_reqgrp_t *permission = NULL;
    
    test_ctx_init(&test, true);
    
    for ( i = 0; i < MAX_QUEUE_NUM; ++i )
    {
        int err = manager->get_token_reqgrp(manager, &test.rs[i], &test,
                                            &permission);
        assert(err == QOS_ERROR_OK || err == QOS_ERROR_PENDING);
        if ( err == QOS_ERROR_OK )
        {
            test_enqueue(&test, permission);
        }
    }
    assert(test_are_alloc_small_total(manager));
    assert(!test_are_empty(manager));
    
    i = 0;
    while ( test.begin != test.end )
    {
        permission = (token_reqgrp_t *) test_dequeue(&test);
        if ( i % 2 == 0 )
        {
            partfail_put_permission(manager, permission);
        }
        else
        {
            manager->put_token_reqgrp(manager, permission, NULL);
        }
        ++i;
    }
    assert(i == MAX_QUEUE_NUM);

#ifdef CACHE_OPEN_CNT
    assert(manager->resource_cache.alloc_cnt ==
           manager->resource_cache.free_cnt);
    assert(manager->token_grp_cache.alloc_cnt ==
           manager->token_grp_cache.free_cnt);
#endif
    printf("[test_case_put_partfailed for NO.%d] %s[OK]%s\n", ++exe_time, GREEN,
           RESET);
}


static void test_case_put_fullfailed()
{
    static int     exe_time    = 0;
    test_context_t test;
    int            i           = 0;
    token_reqgrp_t *permission = NULL;
    
    test_ctx_init(&test, true);
    for ( i = 0; i < MAX_QUEUE_NUM; ++i )
    {
        int err = manager->get_token_reqgrp(manager, &test.rs[i], &test,
                                            &permission);
        assert(err == QOS_ERROR_OK || err == QOS_ERROR_PENDING);
        if ( err == QOS_ERROR_OK )
        {
            test_enqueue(&test, permission);
        }
    }
    assert(test_are_alloc_small_total(manager));
    assert(!test_are_empty(manager));
    
    test_rcvd_reset(manager);
    test_check_snd_buf_legal(manager, 0, MIN_RS_NUM * 2, MIN_RS_NUM);
    i = 0;
    while ( test.begin != test.end )
    {
        permission = (token_reqgrp_t *) test_dequeue(&test);
        fullfail_put_token_grp(manager, permission);
        ++i;
    }
    assert(i == MAX_QUEUE_NUM);
    
    for ( i = 0; i < MAX_QUEUE_NUM; ++i )
    {
        int err = manager->get_token_reqgrp(manager, &test.rs[i], &test,
                                            &permission);
        assert(err == QOS_ERROR_OK || err == QOS_ERROR_PENDING);
        if ( err == QOS_ERROR_OK )
        {
            test_enqueue(&test, permission);
        }
    }
    assert(test_are_alloc_small_total(manager));
    assert(!test_are_empty(manager));
    test_check_snd_buf_legal(manager, 0, MIN_RS_NUM * 2, MIN_RS_NUM);
    
    i = 0;
    while ( test.begin != test.end )
    {
        permission = (token_reqgrp_t *) test_dequeue(&test);
        manager->put_token_reqgrp(manager, permission, NULL);
        ++i;
    }
    test_check_snd_buf_legal(manager, 0, 0, 0);

#ifdef CACHE_OPEN_CNT
    assert(manager->resource_cache.alloc_cnt ==
           manager->resource_cache.free_cnt);
    assert(manager->token_grp_cache.alloc_cnt ==
           manager->token_grp_cache.free_cnt);
    assert(manager->app_node_table->cache.alloc_cnt ==
           manager->app_node_table->cache.free_cnt + MAX_QUEUE_NUM);
    assert(manager->manager_item_cache.alloc_cnt ==
           manager->manager_item_cache.free_cnt + MAX_QUEUE_NUM);
#endif
    printf("[test_case_put_fullfailed for NO.%d] %s[OK]%s\n", ++exe_time, GREEN,
           RESET);
}


static void test_case_put_failed_getagain()
{
    static int     exe_time   = 0;
    test_context_t test;
    int            i          = 0, cnt = 0;
    token_reqgrp_t *token_grp = NULL;
    
    test_ctx_init(&test, true);
    
    for ( i = 0; i < MAX_QUEUE_NUM; ++i )
    {
        int err = manager->get_token_reqgrp(manager, &test.rs[i], &test,
                                            &token_grp);
        assert(err == QOS_ERROR_OK || err == QOS_ERROR_PENDING);
        if ( err == QOS_ERROR_OK )
        {
            test_enqueue(&test, token_grp);
        }
    }
    assert(test_are_alloc_small_total(manager));
    assert(!test_are_empty(manager));
    
    while ( test.begin != test.end )
    {
        if ( cnt < MAX_QUEUE_NUM / 2 )
        {
            token_grp = (token_reqgrp_t *) test_dequeue(&test);
            assert(token_grp);
            fullfail_put_token_grp(manager, token_grp);
            ++cnt;
        }
        else
        {
            break;
        }
    }
    
    for ( i = 0; i < MAX_QUEUE_NUM; ++i )
    {
        int err = manager->get_token_reqgrp(manager, &test.rs[i], &test,
                                            &token_grp);
        if ( err == QOS_ERROR_MEMORY )
        {
            while ( test.begin != test.end )
            {
                token_grp = (token_reqgrp_t *) test_dequeue(&test);
                manager->put_token_reqgrp(manager, token_grp, NULL);
                ++cnt;
            }
            --i;
            continue;
        }
        assert(err == QOS_ERROR_OK || err == QOS_ERROR_PENDING);
        if ( err == QOS_ERROR_OK )
        {
            test_enqueue(&test, token_grp);
        }
    }
    
    while ( test.begin != test.end )
    {
        token_grp = (token_reqgrp_t *) test_dequeue(&test);
        manager->put_token_reqgrp(manager, token_grp, NULL);
        ++cnt;
    }
    
    assert(cnt == 2 * MAX_QUEUE_NUM);

#ifdef CACHE_OPEN_CNT
    assert(manager->resource_cache.alloc_cnt ==
           manager->resource_cache.free_cnt);
    assert(manager->token_grp_cache.alloc_cnt ==
           manager->token_grp_cache.free_cnt);
    assert(manager->app_node_table->cache.alloc_cnt ==
           manager->app_node_table->cache.free_cnt + MAX_QUEUE_NUM);
    assert(manager->manager_item_cache.alloc_cnt ==
           manager->manager_item_cache.free_cnt + MAX_QUEUE_NUM);
#endif
    printf("[test_case_put_failed_getagain for NO.%d] %s[OK]%s\n", ++exe_time,
           GREEN,
           RESET);
}

static void test_case_init_exit(token_reqgrp_manager_t *manager)
{
    int err = token_reqgrp_manager_init(manager, TABLE_LEN, MAX_PERMISSION_NUM,
                                        MAX_RESOURCE_NUM, MAX_NODE_NUM);
    assert(err == 0);
    
    token_reqgrp_manager_exit(manager);
    
    err = token_reqgrp_manager_init(manager, TABLE_LEN, MAX_PERMISSION_NUM,
                                    MAX_RESOURCE_NUM, MAX_NODE_NUM);
    assert(err == 0);
    manager->set_app_event_ops(manager, &test_app_event);
    manager->set_msg_ops(manager, &test_msg_ops);
#ifdef CACHE_OPEN_CNT
    assert(manager->resource_cache.alloc_cnt ==
           manager->resource_cache.free_cnt);
    assert(manager->token_grp_cache.alloc_cnt ==
           manager->token_grp_cache.free_cnt);
    assert(manager->app_node_table->cache.alloc_cnt ==
           manager->app_node_table->cache.free_cnt);
    assert(manager->manager_item_cache.alloc_cnt ==
           manager->manager_item_cache.free_cnt);
#endif
    printf("[test_case_init_exit] %s[OK]%s\n", GREEN, RESET);
}

static int test_init()
{
    printf(YELLOW"--------------test_context_init------------:\n"RESET);
    test_case_init_exit(manager);
    return CUE_SUCCESS;
}

static int test_clean()
{
    token_reqgrp_manager_exit(manager);
    printf(BLUE"--------------test_clean--------:\n"RESET);
    return CUE_SUCCESS;
}

void token_manager_suit_init(test_frame_t *frame)
{
    test_suit_t suit;
    
    test_suit_init(&suit, "test_token_reqgrp_manager", test_init, test_clean);
    
    suit.add_case(&suit, "test_case_get_token_grp_right_now 1st",
                  test_case_get_token_grp_right_now);
    suit.add_case(&suit, "test_case_get_token_grp_right_now 2nd",
                  test_case_get_token_grp_right_now);

    suit.add_case(&suit, "test_case_get_token_grp_pending 1st",
                  test_case_get_token_grp_pending);
    suit.add_case(&suit, "test_case_get_token_grp_pending 2nd",
                  test_case_get_token_grp_pending);


    suit.add_case(&suit, "test_case_rcvd_increased 1st",
                  test_case_rcvd_increased);
    suit.add_case(&suit, "test_case_rcvd_increased 2nd",
                  test_case_rcvd_increased);

    suit.add_case(&suit, "test_case_rcvd_dereased 1st",
                  test_case_rcvd_decreased);

    suit.add_case(&suit, "test_case_rcvd_dereased 2nd",
                  test_case_rcvd_decreased);
//
    suit.add_case(&suit, "test_case_token_grp_outof_memory 1st",
                  test_case_token_grp_outof_memory);
    suit.add_case(&suit, "test_case_token_grp_outof_memory 2nd",
                  test_case_token_grp_outof_memory);
    
    suit.add_case(&suit, "test_case_resource_outof_memory 1st",
                  test_case_resource_outof_memory);
    suit.add_case(&suit, "test_case_resource_outof_memory 2nd",
                  test_case_resource_outof_memory);
    
    suit.add_case(&suit, "test_case_rcvd_reset 1st", test_case_rcvd_reset);
    suit.add_case(&suit, "test_case_rcvd_reset 2nd", test_case_rcvd_reset);
    
    suit.add_case(&suit, "test_case_put_partfailed 1st",
                  test_case_put_partfailed);
    suit.add_case(&suit, "test_case_put_partfailed 2nd",
                  test_case_put_partfailed);
    
    suit.add_case(&suit, "test_case_put_fullfailed 1st",
                  test_case_put_fullfailed);
    suit.add_case(&suit, "test_case_put_fullfailed 2nd",
                  test_case_put_fullfailed);
    
    suit.add_case(&suit, "test_case_put_failed_getagain 1st",
                  test_case_put_failed_getagain);
    suit.add_case(&suit, "test_case_put_failed_getagain 2nd",
                  test_case_put_failed_getagain);
    
    frame->add_suit(frame, &suit);
}
