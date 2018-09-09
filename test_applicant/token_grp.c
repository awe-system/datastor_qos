//
// Created by root on 18-9-5.
//


#include <CUnit/CUError.h>
#include <test_frame.h>
#include "base_node.h"
#include "token_grp.h"

#define MAX_RESOURCE_NUM             15
#define MAX_RESOURCE_PER_PERMISSION  10

static memory_cache_t cache_static;
static memory_cache_t *cache      = &cache_static;
static token_reqgrp_t permission_static;
static token_reqgrp_t *permission = &permission_static;


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

static void test_case_init_got()
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

static void test_case_init_failed()
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

static void test_case_init_part_failed()
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
    token_req_t    *rip        = (token_req_t *) data;
    token_reqgrp_t *permission = rip->token_grp;
    int            err         = permission->to_got(permission, rip);
    if ( err == QOS_ERROR_FAILEDNODE )
    {
        assert(err == permission->err && err == QOS_ERROR_FAILEDNODE &&
               permission->failed_exist
               && permission->req_pending_nr == 0 &&
               permission->pri == (void *) 123);
    }
    else if ( err == QOS_ERROR_OK )
    {
        assert(err == permission->err && err == QOS_ERROR_OK &&
               !permission->failed_exist
               && permission->req_pending_nr == 0 &&
               permission->pri == (void *) 123);
    }
    else
    {
        assert(err == QOS_ERROR_PENDING);
    }
    return NULL;
}

void *test_thread_failed(void *data)
{
    token_req_t    *rip        = (token_req_t *) data;
    token_reqgrp_t *permission = rip->token_grp;
    int            err         =
                           permission->to_failed(permission, rip);
    if ( err == QOS_ERROR_FAILEDNODE )
    {
        assert(err == permission->err && err == QOS_ERROR_FAILEDNODE &&
               permission->failed_exist
               && permission->req_pending_nr == 0 &&
               permission->pri == (void *) 123);
    }
    else
    {
        assert(err == QOS_ERROR_PENDING);
    }
    return NULL;
}

static void test_case_concurrency_got()
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
           && permission->req_pending_nr == 0 &&
           permission->pri == (void *) 123);
    token_reqgrp_exit(permission, cache);
#ifdef CACHE_OPEN_CNT
    assert(cache->alloc_cnt == cache->free_cnt);
#endif
    printf("[test_case_concurrency_got] %s[OK]%s\n", GREEN, RESET);
}

static void test_case_concurrency_failed()
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

static void test_case_concurrency_got_or_failed()
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

static int test_init()
{
    int err = 0;
    printf(YELLOW"--------------test_init----------------------:\n"RESET);
    
    err = memory_cache_init(cache, sizeof(struct token_req),
                            MAX_RESOURCE_NUM);
    assert(err == 0);
    printf("[memory_cache_init] %s[OK]%s\n", GREEN, RESET);
    test_case_init_exit(permission, cache);
    
    return CUE_SUCCESS;
}

static int test_clean()
{
    memory_cache_exit(cache);
    token_reqgrp_exit(permission, cache);
    printf(BLUE"----------------test_clean end-----------------\n"RESET);
    return CUE_SUCCESS;
}

void token_reqgrp_suit_init(test_frame_t *frame)
{
    test_suit_t suit;
    
    test_suit_init(&suit, "token_reqgrp_suit_init", test_init, test_clean);
    
    suit.add_case(&suit, "test_case_init_got", test_case_init_got);
    
    suit.add_case(&suit, "test_case_init_part_failed", test_case_init_failed);
    
    suit.add_case(&suit, "test_case_init_part_failed",
                  test_case_init_part_failed);
    
    suit.add_case(&suit, "test_case_concurrency_got",
                  test_case_concurrency_got);
    
    suit.add_case(&suit, "test_case_concurrency_failed",
                  test_case_concurrency_failed);
    
    suit.add_case(&suit, "test_case_concurrency_got_or_failed",
                  test_case_concurrency_got_or_failed);
    
    frame->add_suit(frame, &suit);
}