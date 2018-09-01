//
// Created by GaoHualong on 2018/7/9.
//

#include <assert.h>
#include <stddef.h>
#include <pthread.h>
#include <string.h>
#include "sysqos_token_reqgrp_manager.h"
#include "sysqos_dispatch_node.h"

/*******************************************************************************************************************/
static inline void check_snd_msg(token_reqgrp_manager_t *manager,
                                 app2dispatch_t *atd)
{
    if ( manager->msg_ops->snd_msg )
    {
        manager->msg_ops->snd_msg(manager->msg_ops, sizeof(app2dispatch_t),
                                  (unsigned char *) &atd);
    }
}

static inline void end_token_grps(token_reqgrp_manager_t *manager,
                                  struct list_head *permission_list)
{
    struct list_head *pos = NULL;
    struct list_head *tmp = NULL;
    list_for_each_safe(pos, tmp, permission_list)
    {
        token_reqgrp_t *token_grp = container_of(pos, token_reqgrp_t,
                                                  lhead_reqgrep_got);
        manager->app_ops->token_grp_got(token_grp, token_grp->pri,
                                        token_grp->err);
    }
}

/*******************************************************************************************************************/

static int
try_to_alloc_from_manager_nolock(struct token_reqgrp_manager *manager,
                                 token_reqgrp_t *token_grp,
                                 token_req_t *rip)
{
    void *item = NULL;
    int  err   = QOS_ERROR_OK;
    assert(manager && rip);
    
    err = manager->app_node_table->find(manager->app_node_table, rip->token_need.rs.id, &item);
    if ( err )
    {
        return token_grp->to_failed(token_grp, rip);
    }
    
    err = ((dispatch_node_t *) item)->alloc_resource(item, &rip->token_need);
    return err;
}

static void try_to_free_to_manager_nolock(struct token_reqgrp_manager *manager,
                                          resource_list_t *rs,
                                          struct list_head *relative_token_grp_list)
{
    void *item = NULL;
    int  err   = QOS_ERROR_OK;
    assert(manager && rs);
    
    err = manager->app_node_table->find(manager->app_node_table, rs->rs.id, &item);
    if ( err )
    {
        return;
    }
    
    ((dispatch_node_t *) item)
            ->free_resource(item, rs, relative_token_grp_list);
}

/*******************************************************************************************************************/
static inline token_reqgrp_manager_t *
token_grp_manager_by_msg_event(struct msg_event_ops *ops)
{
    assert(ops);
    return container_of(ops, token_reqgrp_manager_t, msg_event);
}

static int try_to_get_tokens(struct token_reqgrp_manager *manager,
                             token_reqgrp_t *token_grp)
{
    int                      err   = QOS_ERROR_OK;
    struct list_head         *pos  = NULL;
    struct list_head         *tmp  = NULL;
    struct list_head         to_fail_wait_list;
    dispatch_node_t *item = NULL;
    LISTHEAD_INIT(&to_fail_wait_list);
    
    list_for_each_safe(pos, tmp, &token_grp->lhead_reqgrp)
    {
        token_req_t
                        *rip = app_token_by_grp_list(pos);
        resource_list_t *rs  = &rip->token_need;
        
        err = try_to_alloc_from_manager_nolock(manager, token_grp, rip);
    }
    return err;
}

static void try_to_put_tokens(struct token_reqgrp_manager *manager,
                              token_reqgrp_t *token_grp,
                              struct list_head *relative_token_grp_list)
{
    int              err  = QOS_ERROR_OK;
    struct list_head *pos = NULL;
    struct list_head *tmp = NULL;
    
    list_for_each_safe(pos, tmp, &token_grp->lhead_reqgrp)
    {
        token_req_t
                *rip = app_token_by_grp_list(pos);
        if ( rip->stat == app_token_stat_got )
        {
            resource_list_t *rs = &rip->token_need;
            try_to_free_to_manager_nolock(manager, rs,
                                          relative_token_grp_list);
        }
    }
}

static void node_online(struct msg_event_ops *ops, void *id)
{
    token_reqgrp_manager_t      *manager = token_grp_manager_by_msg_event(ops);
    dispatch_node_t *item    = NULL;
    long                     atd_len  = sizeof(app2dispatch_t);
    app2dispatch_t            atd;
    init_app2dispatch(&atd);
    int err = QOS_ERROR_OK;
    
    item = (dispatch_node_t *) manager->manager_item_cache
            .alloc(&manager->manager_item_cache);
    if ( !item )
    {
        goto manager_item_alloc_failed;
    }
    err = dispatch_node_init(item,
                             __sync_fetch_and_add(&manager->version,
                                                  1));
    if ( err )
    {
        goto token_grp_manager_item_init_failed;
    }
    
    atd.press.val = 0;
    atd.version       = 0;
    
    pthread_rwlock_wrlock(&manager->lck);
    do
    {
        err = manager->app_node_table->insert(manager->app_node_table, id, item);
        if ( err )
        {
            break;
        }
        //        item->get_protocol(item,&atd);
        //        assert(atd_len == sizeof(atd));
    } while ( 0 );
    pthread_rwlock_unlock(&manager->lck);
    if ( err )
    {
        goto insert_failed;
    }
    //    check_snd_msg(manager,&atd);
    return;
insert_failed:
    dispatch_node_exit(item);
token_grp_manager_item_init_failed:
    manager->manager_item_cache.free(&manager->manager_item_cache, item);
manager_item_alloc_failed:
    return;
}


static void node_offline(struct msg_event_ops *ops, void *id)
{
    token_reqgrp_manager_t *manager = token_grp_manager_by_msg_event(ops);
    int                 err      = QOS_ERROR_OK;
    void                *item    = NULL;
    
    struct list_head relative_token_grps;
    LISTHEAD_INIT(&relative_token_grps);
    
    pthread_rwlock_wrlock(&manager->lck);
    do
    {
        err = manager->app_node_table->find(manager->app_node_table, id, &item);
        if ( err )
        {
            break;
        }
        ((dispatch_node_t *) item)
                ->pop_all(item, &relative_token_grps);
        
        err = manager->app_node_table->erase(manager->app_node_table, id, &item);
        assert(err == 0);
    } while ( 0 );
    pthread_rwlock_unlock(&manager->lck);
    if ( err == 0 )
    {
        dispatch_node_exit(item);
        manager->manager_item_cache.free(&manager->manager_item_cache, item);
    }
    end_token_grps(manager, &relative_token_grps);
}

static void node_reset(struct msg_event_ops *ops, void *id)
{
    token_reqgrp_manager_t *manager = token_grp_manager_by_msg_event(ops);
    void                *item    = NULL;
    int                 err      = QOS_ERROR_OK;
    pthread_rwlock_wrlock(&manager->lck);
    do
    {
        err = manager->app_node_table->find(manager->app_node_table, id, &item);
        if ( err )
        {
            break;
        }
        ((dispatch_node_t *) item)
                ->reset((dispatch_node_t *) item);
    } while ( 0 );
    pthread_rwlock_unlock(&manager->lck);
}

#define RCVD_FAIL_TIMES 2

static void rcvd(struct msg_event_ops *ops,
                 void *id, long len, unsigned char *buf)
{
    int                 err_cnt  = RCVD_FAIL_TIMES;
    int                 err      = QOS_ERROR_OK;
    token_reqgrp_manager_t *manager = token_grp_manager_by_msg_event(ops);
    void                *item    = NULL;
    bool                is_reset = false;
    dispatch2app_t       dta;
    struct list_head    relative_token_grps;
    LISTHEAD_INIT(&relative_token_grps);
    
    assert(ops && sizeof(dispatch2app_t) == len);
    memcpy(&dta, buf, len);
    do
    {
        pthread_rwlock_rdlock(&manager->lck);
        do
        {
            err = manager->app_node_table->find(manager->app_node_table, id, &item);
            if ( err )
            {
                break;
            }
            is_reset = ((dispatch_node_t *) item)
                    ->resource_changed(item, &dta, &relative_token_grps);
        } while ( 0 );
        pthread_rwlock_unlock(&manager->lck);
        
        if ( err && --err_cnt > 0 )
        {
            ops->node_online(ops, id);
            continue;
        }
    } while ( 0 );
    
    
    if ( is_reset )
    {
        ops->node_reset(ops, id);
    }
    end_token_grps(manager, &relative_token_grps);
}

static void check_node_in_when_get(struct token_reqgrp_manager *manager,
                                   struct list_head *phead)
{
    struct list_head *pos      = NULL;
    struct list_head *tmp_list = NULL;
    assert(manager && phead);
    void *tmp = NULL;
    
    list_for_each_safe(pos, tmp_list, phead)
    {
        resource_list_t *rs = container_of(pos, resource_list_t, list);
        int             err = manager->app_node_table->find(manager->app_node_table, rs->rs.id, &tmp);
        if ( err )
        {
            manager->msg_event.node_online(&manager->msg_event, rs->rs.id);
        }
    }
}

static int get_token_grp(struct token_reqgrp_manager *manager,
                         resource_list_t *rs,
                         void *pri,
                         token_reqgrp_t **pp_token_grp)
{
    int              err = QOS_ERROR_OK;
    struct list_head head;
    struct list_head tmp_got_list;
    struct list_head relative_token_grp;
    assert(manager && rs && pp_token_grp);
    
    LISTHEAD_INIT(&head);
    LISTHEAD_INIT(&tmp_got_list);
    LISTHEAD_INIT(&relative_token_grp);
    
    list_add(&head, &rs->list);
    *pp_token_grp = (token_reqgrp_t *) manager->token_grp_cache
            .alloc(&manager->token_grp_cache);
    if ( !*pp_token_grp )
        end_func(err, QOS_ERROR_MEMORY, token_grp_cache_alloc_failed);
    
    err = token_reqgrp_init(*pp_token_grp, &manager->resource_cache, &head, pri);
    
    if ( err )
        end_func(err, QOS_ERROR_MEMORY, permission_init_failed);
    
    check_node_in_when_get(manager, &head);
    
    pthread_rwlock_rdlock(&manager->lck);
    err = try_to_get_tokens(manager, *pp_token_grp);
    pthread_rwlock_unlock(&manager->lck);
    
    
    if ( err && err != QOS_ERROR_PENDING )
    {
        manager->put_token_reqgrp(manager, *pp_token_grp, NULL);
    }
    
    list_del(&head);
    return err;
    //try_alloc_permision_failed:
    //    token_reqgrp_exit(*pp_token_grp,&manager->resource_cache);
permission_init_failed:
    manager->token_grp_cache.free(&manager->token_grp_cache, *pp_token_grp);
token_grp_cache_alloc_failed:
    list_del(&head);
    return err;
}


static void check_node_out_when_put(struct token_reqgrp_manager *manager,
                                    struct list_head *phead)
{
    struct list_head *pos      = NULL;
    struct list_head *tmp_list = NULL;
    assert(manager && phead);
    void *tmp = NULL;
    
    list_for_each_safe(pos, tmp_list, phead)
    {
        resource_list_t *rs = container_of(pos, resource_list_t, list);
        int             err = manager->app_node_table->find(manager->app_node_table, rs->rs.id, &tmp);
        if ( !err )
        {
            manager->msg_event.node_offline(&manager->msg_event, rs->rs.id);
        }
    }
}


static void put_token_grp(struct token_reqgrp_manager *manager,
                          token_reqgrp_t *p_token_grp,
                          resource_list_t *fail_rs)
{
    struct list_head fail_head;
    struct list_head relative_token_grp_list;
    
    LISTHEAD_INIT(&relative_token_grp_list);
    LISTHEAD_INIT(&fail_head);
    if ( fail_rs )
    {
        list_add(&fail_head, &fail_rs->list);
    }
    check_node_out_when_put(manager, &fail_head);
    
    pthread_rwlock_rdlock(&manager->lck);
    try_to_put_tokens(manager, p_token_grp, &relative_token_grp_list);
    pthread_rwlock_unlock(&manager->lck);
    end_token_grps(manager, &relative_token_grp_list);
    
    token_reqgrp_exit(p_token_grp, &manager->resource_cache);
    manager->token_grp_cache.free(&manager->token_grp_cache, p_token_grp);
}

static long snd_msg_len(struct msg_event_ops *ops, void *id)
{
    return sizeof(app2dispatch_t);
}

static int snd_msg_buf(struct msg_event_ops *ops, void *id,
                       long len, unsigned char *buf)
{
    int                 err      = QOS_ERROR_OK;
    app2dispatch_t       atd;
    void                *item    = NULL;
    token_reqgrp_manager_t *manager = token_grp_manager_by_msg_event(ops);
    
    assert(ops && len == sizeof(app2dispatch_t) && buf);
    
    pthread_rwlock_rdlock(&manager->lck);
    err = manager->app_node_table->find(manager->app_node_table, id, &item);
    if ( err )
        end_func(err, QOS_ERROR_FAILEDNODE, find_failed);
    ((dispatch_node_t *) item)
            ->get_protocol((dispatch_node_t *) item, &atd);
    memcpy(buf, &atd, len);

find_failed:
    pthread_rwlock_unlock(&manager->lck);
    return err;
}

void set_app_event_ops(struct token_reqgrp_manager *manager,
                       applicant_event_ops_t *ops)
{
    assert(manager && ops);
    manager->app_ops = ops;
}

void set_msg_ops(struct token_reqgrp_manager *manager, msg_ops_t *ops)
{
    assert(manager && ops);
    manager->msg_ops = ops;//用作snd_msg
    manager->app_node_table->set_hash(manager->app_node_table, ops->hash_id);
    manager->app_node_table->set_compare(manager->app_node_table, ops->compare);
}

int token_reqgrp_manager_init(token_reqgrp_manager_t *manager, int tab_len,
                              int max_token_grp_num, int max_resource_num,
                              int max_node_num)
{
    int err = QOS_ERROR_OK;
    manager->app_node_table = alloc_hash_table(tab_len, max_node_num);
    if ( !manager->app_node_table )
        end_func(err, QOS_ERROR_MEMORY, alloc_hash_table_failed);
    err = memory_cache_init(&manager->manager_item_cache,
                            sizeof(dispatch_node_t), max_node_num);
    if ( err )
        end_func(err, QOS_ERROR_MEMORY, manager_item_cache_init_failed);
    err = memory_cache_init(&manager->token_grp_cache, sizeof(token_reqgrp_t),
                            max_token_grp_num);
    if ( err )
        end_func(err, QOS_ERROR_MEMORY, token_grp_cache_init_failed);
    err = memory_cache_init(&manager->resource_cache,
                            sizeof(token_req_t), max_resource_num);
    if ( err )
        end_func(err, QOS_ERROR_MEMORY, resource_in_permission_init_failed);
    
    err = pthread_rwlock_init(&manager->lck, NULL);
    if ( err )
        end_func(err, QOS_ERROR_MEMORY, rwlock_init_failed);
    manager->get_token_reqgrp     = get_token_grp;
    manager->put_token_reqgrp     = put_token_grp;
    manager->set_app_event_ops = set_app_event_ops;
    manager->set_msg_ops       = set_msg_ops;
    
    manager->msg_event.rcvd         = rcvd;
    manager->msg_event.node_offline = node_offline;
    manager->msg_event.node_online  = node_online;
    manager->msg_event.node_reset   = node_reset;
    manager->msg_event.snd_msg_buf  = snd_msg_buf;
    manager->msg_event.snd_msg_len  = snd_msg_len;
    
    manager->version = 0;
    
    return err;
    pthread_rwlock_destroy(&manager->lck);
rwlock_init_failed:
    memory_cache_exit(&manager->resource_cache);
resource_in_permission_init_failed:
    memory_cache_exit(&manager->token_grp_cache);
token_grp_cache_init_failed:
    memory_cache_exit(&manager->manager_item_cache);
manager_item_cache_init_failed:
    free_hash_table(manager->app_node_table);
alloc_hash_table_failed:
    return err;
}

void token_reqgrp_manager_exit(token_reqgrp_manager_t *manager)
{
    pthread_rwlock_destroy(&manager->lck);
    memory_cache_exit(&manager->resource_cache);
    memory_cache_exit(&manager->token_grp_cache);
    memory_cache_exit(&manager->manager_item_cache);
    free_hash_table(manager->app_node_table);
}


/**for test*****************************************************************************************/
#ifdef TOKEN_GRP_MANAGER_TEST

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sysqos_common.h"
#include "sysqos_container_item.h"

#define TABLE_LEN 127

#define TEST_RESOURCE_PER_PERMISSION 3
#define MAX_QUEUE_NUM MAX(TABLE_LEN*TEST_RESOURCE_PER_PERMISSION,MIN_RS_NUM)
#define MAX_PERMISSION_NUM MAX_QUEUE_NUM
#define MAX_RESOURCE_NUM  MAX_PERMISSION_NUM * TEST_RESOURCE_PER_PERMISSION
#define MAX_NODE_NUM      MAX_QUEUE_NUM

typedef struct test_context
{
    void            *queue[2 * MAX_QUEUE_NUM + 1];
    int             begin;
    int             end;
    int             queue_len;
    int             err;
    int             rs_num;//实际的rs_list的个数 在之后的都是这个串的后续部分
    resource_list_t rs[MAX_QUEUE_NUM * 3];
} test_context_t;

static void test_init(test_context_t *test, bool is_pending)
{
    int i = 0;
    test->begin = test->end = test->queue_len = 0;
    test->err   = QOS_ERROR_OK;
    memset(test->queue, 0, (2 * MAX_QUEUE_NUM + 1) * sizeof(void *));
    test->rs_num = MAX_QUEUE_NUM;
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
    if ( ++test->end > 2*MAX_QUEUE_NUM )
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
    if ( ++test->begin > 2* MAX_QUEUE_NUM )
    {
        test->begin = 0;
    }
    return res;
}

static void test_token_grp_got(void *token_grp, void *pri, int err)
{
    token_reqgrp_t *p    = (token_reqgrp_t *) token_grp;
    test_context_t  *test = pri;
    if ( err )
    {
        test->err = err;
    }
    test_enqueue(test, token_grp);
}

static applicant_event_ops_t test_app_event =
                                     {
                                             .token_grp_got = test_token_grp_got,
                                             .node_changed = NULL
                                     };

static msg_ops_t test_msg_ops =
                         {
                                 .snd_msg = NULL,
                                 .compare = test_compare,
                                 .hash_id = test_hash,
                                 .node_offline_done = NULL,
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
            if ( item->base_node.token_inuse > item->base_node.token_quota )
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
            app2dispatch_t        atd;
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


static void test_case_get_token_grp_right_now(token_reqgrp_manager_t *manager)
{
    static int      exe_time   = 0;
    test_context_t  test;
    int             i          = 0;
    token_reqgrp_t *token_grp = NULL;
    
    test_init(&test, false);
    
    for ( i = 0; i < MAX_QUEUE_NUM; ++i )
    {
        int err = manager->get_token_reqgrp(manager, &test.rs[i], &test,
                                         &token_grp);
        assert(err == QOS_ERROR_OK);
        if ( err == QOS_ERROR_OK )
        {
            test_enqueue(&test, token_grp);
        }
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

static void test_case_get_token_grp_pending(token_reqgrp_manager_t *manager)
{
    static int      exe_time      = 0;
    test_context_t  test;
    int             i             = 0;
    bool            pending_exist = false;
    token_reqgrp_t *token_grp    = NULL;
    
    test_init(&test, true);
    
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

static void test_case_token_grp_outof_memory(token_reqgrp_manager_t *manager)
{
    static int      exe_time      = 0;
    test_context_t  test;
    int             i             = 0;
    int             j             = 0;
    bool            pending_exist = false;
    token_reqgrp_t *token_grp    = NULL;
    
    token_reqgrp_manager_exit(manager);
    
    int err = token_reqgrp_manager_init(manager, TABLE_LEN, MAX_QUEUE_NUM / 2,
                                        MAX_RESOURCE_NUM, MAX_NODE_NUM);
    assert(err == 0);
    manager->set_app_event_ops(manager, &test_app_event);
    manager->set_msg_ops(manager, &test_msg_ops);
    
    test_init(&test, false);
    
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
    err = token_reqgrp_manager_init(manager, TABLE_LEN, MAX_PERMISSION_NUM,
                                    MAX_RESOURCE_NUM, MAX_NODE_NUM);
    assert(err == 0);
    manager->set_app_event_ops(manager, &test_app_event);
    manager->set_msg_ops(manager, &test_msg_ops);
    
    printf("[test_case_token_grp_outof_memory for NO.%d] %s[OK]%s\n",
           ++exe_time, GREEN, RESET);
}


static void test_case_resource_outof_memory(token_reqgrp_manager_t *manager)
{
    static int      exe_time      = 0;
    test_context_t  test;
    int             i             = 0;
    int             j             = 0;
    bool            pending_exist = false;
    token_reqgrp_t *token_grp    = NULL;
    
    token_reqgrp_manager_exit(manager);
    
    int err = token_reqgrp_manager_init(manager, TABLE_LEN, MAX_PERMISSION_NUM,
                                        MAX_QUEUE_NUM, MAX_NODE_NUM);
    assert(err == 0);
    manager->set_app_event_ops(manager, &test_app_event);
    manager->set_msg_ops(manager, &test_msg_ops);
    
    test_init(&test, false);
    
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
test_rcvd_increased(token_reqgrp_manager_t *manager, test_context_t *test)
{
    int i = 0;
    for ( i = 0; i < MAX_QUEUE_NUM; ++i )
    {
        dispatch_node_t *item;
        void                     **ppitem = (void **) &item;
        long                     id       = i;
        dispatch2app_t            dta;
        int                      err      = manager->app_node_table
                ->find(manager->app_node_table, (void *) id, ppitem);
        assert(err == 0);
        dta.version   = item->base_node.get_version(&item->base_node) + 1;
        dta.token_quota = MAX_QUEUE_NUM;
        manager->msg_event
                .rcvd(&manager->msg_event, (void *) id, sizeof(dispatch2app_t),
                      (unsigned char *) &dta);
    }
}

static void
test_rcvd_decreased(token_reqgrp_manager_t *manager, test_context_t *test)
{
    int i = 0;
    for ( i = 0; i < MAX_QUEUE_NUM; ++i )
    {
        dispatch_node_t *item;
        void                     **ppitem = (void **) &item;
        long                     id       = i;
        dispatch2app_t            dta;
        int                      err      = manager->app_node_table
                ->find(manager->app_node_table, (void *) id, ppitem);
        assert(err == 0);
        dta.version   = item->base_node.get_version(&item->base_node) + 1;
        dta.token_quota = MIN_RS_NUM;
        manager->msg_event
                .rcvd(&manager->msg_event, (void *) id, sizeof(dispatch2app_t),
                      (unsigned char *) &dta);
    }
}

static void
test_rcvd_reset(token_reqgrp_manager_t *manager, test_context_t *test)
{
    int i = 0;
    for ( i = 0; i < MAX_QUEUE_NUM; ++i )
    {
        dispatch_node_t *item;
        void                     **ppitem = (void **) &item;
        long                     id       = i;
        dispatch2app_t            dta;
        int                      err      = manager->app_node_table->find(manager->app_node_table,
                                                               (void *) id,
                                                               ppitem);
        assert(err == 0);
        dta.version   = 0;
        dta.token_quota = 0;
        manager->msg_event
                .rcvd(&manager->msg_event, (void *) id, sizeof(dispatch2app_t),
                      (unsigned char *) &dta);
    }
}

static void test_case_rcvd_increased(token_reqgrp_manager_t *manager)
{
    static int      exe_time   = 0;
    test_context_t  test;
    int             i          = 0;
    token_reqgrp_t *token_grp = NULL;
    
    test_init(&test, true);
    
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
    test_rcvd_increased(manager, &test);
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
    test_rcvd_reset(manager, &test);
    printf("[test_case_rcvd_increased for NO.%d] %s[OK]%s\n", ++exe_time, GREEN,
           RESET);
}


static void test_case_rcvd_decreased(token_reqgrp_manager_t *manager)
{
    static int      exe_time   = 0;
    test_context_t  test;
    int             i          = 0;
    token_reqgrp_t *token_grp = NULL;
    
    test_init(&test, true);
    
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
    test_rcvd_increased(manager, &test);
    test_check_snd_buf_legal(manager, 1, 0, MIN_RS_NUM * 3);
    assert(test_are_empty(manager));
    test_rcvd_decreased(manager, &test);
    assert(!test_are_alloc_small_total(manager));
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
    test_rcvd_reset(manager, &test);
    printf("[test_case_rcvd_decreased for NO.%d] %s[OK]%s\n", ++exe_time, GREEN,
           RESET);
}

static void test_case_rcvd_reset(token_reqgrp_manager_t *manager)
{
    static int      exe_time   = 0;
    test_context_t  test;
    int             i          = 0;
    token_reqgrp_t *token_grp = NULL;
    
    test_init(&test, true);
    
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
    test_rcvd_increased(manager, &test);
    test_check_snd_buf_legal(manager, -1, 0, MIN_RS_NUM * 3);
    assert(test_are_empty(manager));
    test_rcvd_reset(manager, &test);
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

static void test_case_put_partfailed(token_reqgrp_manager_t *manager)
{
    static int      exe_time    = 0;
    test_context_t  test;
    int             i           = 0;
    token_reqgrp_t *permission = NULL;
    
    test_init(&test, true);
    
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


static void test_case_put_fullfailed(token_reqgrp_manager_t *manager)
{
    static int      exe_time    = 0;
    test_context_t  test;
    int             i           = 0;
    token_reqgrp_t *permission = NULL;
    
    test_init(&test, true);
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
    
    test_rcvd_reset(manager, &test);
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


static void test_case_put_failed_getagain(token_reqgrp_manager_t *manager)
{
    static int      exe_time   = 0;
    test_context_t  test;
    int             i          = 0, cnt = 0;
    token_reqgrp_t *token_grp = NULL;
    
    test_init(&test, true);
    
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
    assert(manager->app_node_table->cache.alloc_cnt == manager->app_node_table->cache.free_cnt);
    assert(manager->manager_item_cache.alloc_cnt ==
           manager->manager_item_cache.free_cnt);
#endif
    printf("[test_case_init_exit] %s[OK]%s\n", GREEN, RESET);
}

#endif

void test_token_reqgrp_manager()
{
#ifdef TOKEN_GRP_MANAGER_TEST
    int err = 0;
    printf(YELLOW"--------------test_token_reqgrp_manager------------:\n"RESET);
    
    token_reqgrp_manager_t manager;
    
    test_case_init_exit(&manager);
    
    test_case_get_token_grp_right_now(&manager);
    test_case_get_token_grp_right_now(&manager);
    
    test_case_get_token_grp_pending(&manager);
    test_case_get_token_grp_pending(&manager);
    
    test_case_rcvd_increased(&manager);
    test_case_rcvd_increased(&manager);
    
    test_case_token_grp_outof_memory(&manager);
    test_case_token_grp_outof_memory(&manager);
    
    test_case_resource_outof_memory(&manager);
    test_case_resource_outof_memory(&manager);
    
    //FIXME梯度减少的用例
//    test_case_rcvd_decreased(&manager);
//    test_case_rcvd_decreased(&manager);
    
    test_case_rcvd_reset(&manager);
    test_case_rcvd_reset(&manager);
    
    test_case_put_partfailed(&manager);
    test_case_put_partfailed(&manager);
    
    test_case_put_fullfailed(&manager);
    test_case_put_fullfailed(&manager);
    
    test_case_put_failed_getagain(&manager);
    test_case_put_failed_getagain(&manager);
    
    token_reqgrp_manager_exit(&manager);
    printf(BLUE"--------------test_token_reqgrp_manager end--------:\n"RESET);
#endif
}
