//
// Created by root on 18-7-29.
//

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>
#include "sysqos_token_reqgrp.h"
#include "list_head.h"
#include "memory_cache.h"
#include "sysqos_interface.h"
#include "sysqos_common.h"

static void free_token(token_req_t *resource,
                       memory_cache_t *resource_cache)
{
    assert(resource && resource_cache);
    resource_cache->free(resource_cache, resource);
}

static token_req_t *
alloc_token(token_reqgrp_t *permission,
            memory_cache_t *resource_cache,
            resource_t *rs)
{
    token_req_t *resource = NULL;
    assert(permission && resource_cache && rs);
    
    resource =
            (token_req_t *) resource_cache->alloc(resource_cache);
    if ( !resource )
    {
        return NULL;
    }
    LISTHEAD_INIT(&resource->list_reqgrp);
    resource->token_grp  = permission;
    resource->stat       = app_token_stat_init;
    resource->token_need.rs.cost = rs->cost;
    resource->token_need.rs.id   = rs->id;
    LISTHEAD_INIT(&resource->token_need.list);
    return resource;
}

bool _to_got(token_req_t *rip)
{
    return __sync_bool_compare_and_swap(&rip->stat,
                                        app_token_stat_init,
                                        app_token_stat_got);
    
}

bool _to_fail(token_req_t *rip)
{
    return __sync_bool_compare_and_swap(&rip->stat,
                                        app_token_stat_init,
                                        app_token_stat_failed);
}

/**********************************************************************************************************/

/**********************************************************************************************************/
static int to_got(token_reqgrp_t *token_grp, token_req_t *rip)
{
    int err = QOS_ERROR_PENDING;
    assert(token_grp && rip);
    if ( _to_got(rip) && 0 == __sync_sub_and_fetch(&token_grp->req_pending_nr, 1) )
    {
        if ( token_grp->failed_exist )
        {
            token_grp->err = err = QOS_ERROR_FAILEDNODE;
        }
        else
        {
            token_grp->err = err = QOS_ERROR_OK;
        }
    }
    return err;
}

static int to_failed(token_reqgrp_t *token_grp,
                     token_req_t *rip)
{
    int err = QOS_ERROR_PENDING;
    assert(token_grp && rip);
    token_grp->failed_exist = true;
    if ( _to_fail(rip) && 0 == __sync_sub_and_fetch(&token_grp->req_pending_nr, 1) )
    {
        token_grp->err = err = QOS_ERROR_FAILEDNODE;
    }
    return err;
}

/********************************************************************************************************/

static void clear_token_grp_resource_from_list(struct list_head *head,
                                               memory_cache_t *resource_cache)
{
    struct list_head *pos = NULL;
    struct list_head *tmp = NULL;
    list_for_each_safe(pos, tmp, head)
    {
        token_req_t
                *resource = container_of(pos, token_req_t, list_reqgrp);
        list_del(pos);
        assert(resource);
        free_token(resource, resource_cache);
    }
}

static int init_rip_list_by_rs_list(token_reqgrp_t *token_grp,
                                    memory_cache_t *resource_cache,
                                    struct list_head *rs_list)
{
    int              err  = QOS_ERROR_OK;
    struct list_head *pos = NULL;
    struct list_head *tmp = NULL;
    list_for_each_safe(pos, tmp, rs_list)
    {
        resource_list_t *rs = container_of(pos, resource_list_t, list);
        assert(rs);
        token_req_t *resource = alloc_token(
                token_grp, resource_cache, &rs->rs);
        if ( !resource )
            end_func(err, QOS_ERROR_MEMORY, alloc_resource_failed);
        
        list_add(&resource->list_reqgrp, &token_grp->lhead_reqgrp);
        ++token_grp->req_total_nr;
        ++token_grp->req_pending_nr;
    }
alloc_resource_failed:
    return err;
}

static inline void fill_token_grp_func(token_reqgrp_t *token_grp)
{
    token_grp->to_got    = to_got;
    token_grp->to_failed = to_failed;
}

static inline void fill_normal_val(token_reqgrp_t *token_grp)
{
    LISTHEAD_INIT(&token_grp->lhead_reqgrp);
    LISTHEAD_INIT(&token_grp->lhead_reqgrep_got);
    
    token_grp->err          = QOS_ERROR_PENDING;
    token_grp->failed_exist = false;
    token_grp->req_total_nr       = 0;
    token_grp->req_pending_nr     = 0;
    
}

/************************************************************************************/

static void clear_token_grp_resource(token_reqgrp_t *token_grp,
                                     memory_cache_t *resource_cache)
{
    clear_token_grp_resource_from_list(&token_grp->lhead_reqgrp,
                                       resource_cache);
}

int token_reqgrp_init(token_reqgrp_t *token_grp, memory_cache_t *resource_cache,
                      struct list_head *rs_list, void *pri)
{
    int err = QOS_ERROR_OK;
    assert(token_grp && resource_cache && rs_list);
    token_grp->pri = pri;
    fill_normal_val(token_grp);
    fill_token_grp_func(token_grp);
    
    err = init_rip_list_by_rs_list(token_grp, resource_cache, rs_list);
    if ( err )
    {
        goto alloc_resource_failed;
    }
    return QOS_ERROR_OK;
alloc_resource_failed:
    clear_token_grp_resource(token_grp, resource_cache);
    return err;
}

void token_reqgrp_exit(token_reqgrp_t *token_grp,
                       memory_cache_t *resource_cache)
{
    assert(token_grp && resource_cache);
    clear_token_grp_resource(token_grp, resource_cache);
}

/**for test*********************************************************************/
#ifdef APP_TOKEN_GRP_TEST

#include <stdio.h>

#define MAX_RESOURCE_NUM             15
#define MAX_RESOURCE_PER_PERMISSION  10

static memory_cache_t cache;

static void
_fill_rs_list_head(long len, resource_list_t rs[], struct list_head *rs_list)
{
    long i = 0;
    LISTHEAD_INIT(rs_list);
    for ( i = 0; i < len; ++i )
    {
        rs[i].rs.cost = i + 1;
        rs[i].rs.id   = (void *) i;
        LISTHEAD_INIT(&rs[i].list);
        list_add(&rs[i].list, rs_list);
    }
}

static void
test_case_init_exit(token_reqgrp_t *permission, memory_cache_t *cache)
{
    int              err = 0;
    resource_list_t  rs[MAX_RESOURCE_NUM * 2];
    struct list_head rs_list;
    
    _fill_rs_list_head(MAX_RESOURCE_NUM * 2, rs, &rs_list);
    err = token_reqgrp_init(permission, cache, &rs_list, (void *) 123);
    assert(err);
    
    _fill_rs_list_head(MAX_RESOURCE_NUM, rs, &rs_list);
    err = token_reqgrp_init(permission, cache, &rs_list, (void *) 123);
    assert(err == 0);
    
    token_reqgrp_exit(permission, cache);
#ifdef CACHE_OPEN_CNT
    assert(cache->alloc_cnt == cache->free_cnt);
#endif
    _fill_rs_list_head(MAX_RESOURCE_NUM, rs, &rs_list);
    err = token_reqgrp_init(permission, cache, &rs_list, (void *) 123);
    assert(err == 0);
    token_reqgrp_exit(permission, cache);
    printf("[test_case_init_exit] %s[OK]%s\n", GREEN, RESET);
}

static void
test_case_init_got(token_reqgrp_t *permission, memory_cache_t *cache)
{
    int              err  = 0;
    struct list_head *pos = NULL;
    struct list_head *tmp = NULL;
    resource_list_t  rs[MAX_RESOURCE_NUM];
    struct list_head rs_list;
    
    _fill_rs_list_head(MAX_RESOURCE_NUM, rs, &rs_list);
    err = token_reqgrp_init(permission, cache, &rs_list, (void *) 123);
    assert(err == 0);
    
    list_for_each_safe(pos, tmp, &permission->lhead_reqgrp)
    {
        token_req_t
                *rip = app_token_by_grp_list(pos);
        int     err  = QOS_ERROR_OK;
        assert(rip->stat == app_token_stat_init);
        err = permission->to_got(permission, rip);
        assert(rip->stat == app_token_stat_got);
        assert(permission->err == err);
        if ( tmp != &permission->lhead_reqgrp )
        {
            assert(err == QOS_ERROR_PENDING);
        }
        else
        {
            assert(err == QOS_ERROR_OK);
        }
    }
    assert(err == QOS_ERROR_OK && permission->failed_exist == false
           && permission->req_pending_nr == 0 && permission->err == err);
    
    token_reqgrp_exit(permission, cache);
#ifdef CACHE_OPEN_CNT
    assert(cache->alloc_cnt == cache->free_cnt);
#endif
    printf("[test_case_init_got] %s[OK]%s\n", GREEN, RESET);
}

static void
test_case_init_failed(token_reqgrp_t *permission, memory_cache_t *cache)
{
    int              err  = 0;
    struct list_head *pos = NULL;
    struct list_head *tmp = NULL;
    resource_list_t  rs[MAX_RESOURCE_NUM];
    struct list_head rs_list;
    
    _fill_rs_list_head(MAX_RESOURCE_NUM, rs, &rs_list);
    err = token_reqgrp_init(permission, cache, &rs_list, (void *) 123);
    assert(err == 0);
    
    list_for_each_safe(pos, tmp, &permission->lhead_reqgrp)
    {
        token_req_t
                *rip = app_token_by_grp_list(pos);
        err = permission->to_failed(permission, rip);
        assert(permission->err == err);
        if ( tmp != &permission->lhead_reqgrp )
        {
            assert(err == QOS_ERROR_PENDING &&
                   rip->stat == app_token_stat_failed
                   && permission->failed_exist);
        }
        
    }
    assert(err == QOS_ERROR_FAILEDNODE && permission->failed_exist
           && permission->req_pending_nr == 0 && permission->pri == (void *) 123
    );
    
    token_reqgrp_exit(permission, cache);
#ifdef CACHE_OPEN_CNT
    assert(cache->alloc_cnt == cache->free_cnt);
#endif
    printf("[test_case_init_failed] %s[OK]%s\n", GREEN, RESET);
}

static void
test_case_init_part_failed(token_reqgrp_t *permission, memory_cache_t *cache)
{
    int              err  = 0;
    int              i    = 0;
    struct list_head *pos = NULL;
    struct list_head *tmp = NULL;
    resource_list_t  rs[MAX_RESOURCE_NUM];
    struct list_head rs_list;
    
    _fill_rs_list_head(MAX_RESOURCE_NUM, rs, &rs_list);
    err = token_reqgrp_init(permission, cache, &rs_list, (void *) 123);
    assert(err == 0);
    
    list_for_each_safe(pos, tmp, &permission->lhead_reqgrp)
    {
        token_req_t
                *rip = app_token_by_grp_list(pos);
        if ( ++i % 2 == 0 )
        {
            assert(rip->stat == app_token_stat_init);
            err = permission->to_got(permission, rip);
            assert(rip->stat == app_token_stat_got);
        }
        else
        {
            assert(rip->stat == app_token_stat_init);
            err = permission->to_failed(permission, rip);
            assert(rip->stat == app_token_stat_failed);
        }
        assert(permission->err == err);
        if ( tmp != &permission->lhead_reqgrp )
        {
            assert(err == QOS_ERROR_PENDING && permission->failed_exist);
        }
        
    }
    assert(err == QOS_ERROR_FAILEDNODE && permission->failed_exist
           && permission->req_pending_nr == 0 && permission->pri == (void *) 123
    );
    token_reqgrp_exit(permission, cache);
#ifdef CACHE_OPEN_CNT
    assert(cache->alloc_cnt == cache->free_cnt);
#endif
    printf("[test_case_init_part_failed] %s[OK]%s\n", GREEN, RESET);
}

void *test_thread_got(void *data)
{
    token_req_t     *rip        = (token_req_t *) data;
    token_reqgrp_t *permission = rip->token_grp;
    int             err         = permission->to_got(permission, rip);
    if ( err == QOS_ERROR_FAILEDNODE )
    {
        assert(err == permission->err && err == QOS_ERROR_FAILEDNODE &&
               permission->failed_exist
               && permission->req_pending_nr == 0 && permission->pri == (void *) 123);
    }
    else if ( err == QOS_ERROR_OK )
    {
        assert(err == permission->err && err == QOS_ERROR_OK &&
               !permission->failed_exist
               && permission->req_pending_nr == 0 && permission->pri == (void *) 123);
    }
    else
    {
        assert(err == QOS_ERROR_PENDING);
    }
}

void *test_thread_failed(void *data)
{
    token_req_t     *rip        = (token_req_t *) data;
    token_reqgrp_t *permission = rip->token_grp;
    int             err         =
                            permission->to_failed(permission, rip);
    if ( err == QOS_ERROR_FAILEDNODE )
    {
        assert(err == permission->err && err == QOS_ERROR_FAILEDNODE &&
               permission->failed_exist
               && permission->req_pending_nr == 0 && permission->pri == (void *) 123);
    }
    else
    {
        assert(err == QOS_ERROR_PENDING);
    }
}

static void
test_case_concurrency_got(token_reqgrp_t *permission, memory_cache_t *cache)
{
    int              err  = 0;
    int              i    = 0;
    struct list_head *pos = NULL;
    struct list_head *tmp = NULL;
    resource_list_t  rs[MAX_RESOURCE_NUM];
    struct list_head rs_list;
    test_threads_t   threads;
    test_threads_init(&threads);
    
    _fill_rs_list_head(MAX_RESOURCE_NUM, rs, &rs_list);
    err = token_reqgrp_init(permission, cache, &rs_list, (void *) 123);
    assert(err == 0);
    
    list_for_each_safe(pos, tmp, &permission->lhead_reqgrp)
    {
        token_req_t
                *rip = app_token_by_grp_list(pos);
        if ( ++i % 2 == 0 )
        {
            assert(rip->stat == app_token_stat_init);
            err = permission->to_got(permission, rip);
            assert(rip->stat == app_token_stat_got);
        }
        else
        {
            threads.add_type(&threads, 1, test_thread_got, rip);
        }
    }
    //启动线程
    threads.run(&threads);
    test_threads_exit(&threads);
    assert(permission->err == QOS_ERROR_OK
           && permission->req_pending_nr == 0 && permission->pri == (void *) 123);
    token_reqgrp_exit(permission, cache);
#ifdef CACHE_OPEN_CNT
    assert(cache->alloc_cnt == cache->free_cnt);
#endif
    printf("[test_case_concurrency_got] %s[OK]%s\n", GREEN, RESET);
}

static void
test_case_concurrency_failed(token_reqgrp_t *permission, memory_cache_t *cache)
{
    int              err  = 0;
    int              i    = 0;
    struct list_head *pos = NULL;
    struct list_head *tmp = NULL;
    resource_list_t  rs[MAX_RESOURCE_NUM];
    struct list_head rs_list;
    test_threads_t   threads;
    test_threads_init(&threads);
    
    _fill_rs_list_head(MAX_RESOURCE_NUM, rs, &rs_list);
    err = token_reqgrp_init(permission, cache, &rs_list, (void *) 123);
    assert(err == 0);
    
    list_for_each_safe(pos, tmp, &permission->lhead_reqgrp)
    {
        token_req_t
                *rip = app_token_by_grp_list(pos);
        if ( ++i % 4 == 0 )
        {
            assert(rip->stat == app_token_stat_init);
            err = permission->to_got(permission, rip);
            assert(rip->stat == app_token_stat_got);
        }
        else if ( i % 4 == 1 )
        {
            assert(rip->stat == app_token_stat_init);
            err = permission->to_failed(permission, rip);
            assert(rip->stat == app_token_stat_failed);
        }
        else if ( i % 4 == 2 )
        {
            threads.add_type(&threads, 1, test_thread_got, rip);
        }
        else
        {
            threads.add_type(&threads, 1, test_thread_failed, rip);
        }
    }
    //启动线程
    threads.run(&threads);
    test_threads_exit(&threads);
    assert(permission->err == QOS_ERROR_FAILEDNODE
           && permission->failed_exist
           && permission->req_pending_nr == 0
           && permission->pri == (void *) 123);
    token_reqgrp_exit(permission, cache);
#ifdef CACHE_OPEN_CNT
    assert(cache->alloc_cnt == cache->free_cnt);
#endif
    printf("[test_case_concurrency_failed] %s[OK]%s\n", GREEN, RESET);
}

static void test_case_concurrency_got_or_failed(token_reqgrp_t *permission,
                                                memory_cache_t *cache)
{
    int              err  = 0;
    int              i    = 0;
    struct list_head *pos = NULL;
    struct list_head *tmp = NULL;
    resource_list_t  rs[MAX_RESOURCE_NUM];
    struct list_head rs_list;
    test_threads_t   threads;
    test_threads_init(&threads);
    
    _fill_rs_list_head(MAX_RESOURCE_NUM, rs, &rs_list);
    err = token_reqgrp_init(permission, cache, &rs_list, (void *) 123);
    assert(err == 0);
    
    list_for_each_safe(pos, tmp, &permission->lhead_reqgrp)
    {
        token_req_t
                *rip = app_token_by_grp_list(pos);
        if ( ++i % 4 == 0 )
        {
            assert(rip->stat == app_token_stat_init);
            err = permission->to_got(permission, rip);
            assert(rip->stat == app_token_stat_got);
        }
        else if ( i % 4 == 1 )
        {
            assert(rip->stat == app_token_stat_init);
            err = permission->to_failed(permission, rip);
            assert(rip->stat == app_token_stat_failed);
        }
        else if ( i % 4 == 2 )
        {
            threads.add_type(&threads, 1, test_thread_got, rip);
            threads.add_type(&threads, 1, test_thread_failed, rip);
        }
        else
        {
            threads.add_type(&threads, 1, test_thread_failed, rip);
            threads.add_type(&threads, 1, test_thread_got, rip);
        }
    }
    //启动线程
    threads.run(&threads);
    test_threads_exit(&threads);
    assert(permission->err == QOS_ERROR_FAILEDNODE
           && permission->failed_exist
           && permission->req_pending_nr == 0
           && permission->pri == (void *) 123);
    token_reqgrp_exit(permission, cache);
#ifdef CACHE_OPEN_CNT
    assert(cache->alloc_cnt == cache->free_cnt);
#endif
    printf("[test_case_concurrency_got_or_failed] %s[OK]%s\n", GREEN, RESET);
}


#endif

void test_token_reqgrp()
{
#ifdef APP_TOKEN_GRP_TEST
    int err = 0;
    printf(YELLOW"--------------test_token_reqgrp----------------------:\n"RESET);
    token_reqgrp_t permission;
    err = memory_cache_init(&cache, sizeof(struct token_req),
                            MAX_RESOURCE_NUM);
    assert(err == 0);
    printf("[memory_cache_init] %s[OK]%s\n", GREEN, RESET);
    test_case_init_exit(&permission, &cache);
    
    test_case_init_got(&permission, &cache);
    
    test_case_init_failed(&permission, &cache);
    
    test_case_init_part_failed(&permission, &cache);
    
    test_case_concurrency_got(&permission, &cache);
    
    test_case_concurrency_failed(&permission, &cache);
    
    test_case_concurrency_got_or_failed(&permission, &cache);
    
    memory_cache_exit(&cache);
    token_reqgrp_exit(&permission, &cache);
    printf(BLUE"----------------test_token_reqgrp end-----------------\n"RESET);
#endif
}