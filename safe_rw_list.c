//
// Created by root on 18-7-24.
//


#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include "sysqos_common.h"
#include "safe_rw_list.h"
#include "sysqos_container_item.h"

static int rwfind_nolock(struct safe_rw_list *list, void *id, void **pri)
{
    int              err   = safe_rw_list_error_notfound;
    struct list_head *head = NULL;
    struct list_head *pos  = NULL;
    assert(list && pri && list->compare);
    
    head = &list->list;
    list_for_each(pos, head)
    {
        qos_container_item_t
                *item = container_of(pos, qos_container_item_t, list);
        if ( 0 == list->compare(item->id, id) )
        {
            *pri = item->pri;
            err = safe_rw_list_error_ok;
            break;
        }
    }
    return err;
}

static int rwinsert(struct safe_rw_list *list, void *id, void *pri)
{
    int                  err   = safe_rw_list_error_exist;
    qos_container_item_t *item = NULL;
    int                  index = 0;
    struct list_head     *head = NULL;
    void                 *tmp_pri;
    assert(list);
    
    ////当插入经常失败时需要打开此部分内容
//    if(0 == rwfind(list_reqgrp,id,&tmp_pri))
//        return err;
    
    item = list->cache->alloc(list->cache);
    assert(item);
    qos_container_item_init(item, id, pri);
    
    head = &list->list;
    
    pthread_rwlock_wrlock(&list->rwlock);
    if ( 0 != rwfind_nolock(list, id, &tmp_pri) )
    {
        list_add(&item->list, head);
        err = safe_rw_list_error_ok;
    }
    pthread_rwlock_unlock(&list->rwlock);
    
    if ( err )
    {
        list->cache->free(list->cache, item);
    }
    return err;
}

static int rwerase(struct safe_rw_list *list, void *id, void **pri)
{
    int              err   = safe_rw_list_error_notfound;
    struct list_head *head = NULL;
    struct list_head *pos  = NULL;
    assert(list && list->compare);
    
    head = &list->list;
    pthread_rwlock_wrlock(&list->rwlock);
    list_for_each(pos, head)
    {
        qos_container_item_t
                *item = container_of(pos, qos_container_item_t, list);
        if ( 0 == list->compare(item->id, id) )
        {
            list_del(&item->list);
            pthread_rwlock_unlock(&list->rwlock);
            if ( pri )
            {
                *pri = item->pri;
            }
            list->cache->free(list->cache, item);
            return safe_rw_list_error_ok;
        }
    }
    pthread_rwlock_unlock(&list->rwlock);
    return err;
}

static int rwfind(struct safe_rw_list *list, void *id, void **pri)
{
    int err = safe_rw_list_error_ok;
    pthread_rwlock_rdlock(&list->rwlock);
    err = rwfind_nolock(list, id, pri);
    pthread_rwlock_unlock(&list->rwlock);
    return err;
}


static void rwset_compare(struct safe_rw_list *list, compare_id_func compare)
{
    list->compare = compare;
}

static void for_each_do(struct safe_rw_list *list, void *ctx,
               for_each_dofunc_t dofunc)
{
    struct list_head *pos = NULL;
    pthread_rwlock_rdlock(&list->rwlock);
    list_for_each(pos, &list->list)
    {
        qos_container_item_t
                *item = container_of(pos, qos_container_item_t, list);
        dofunc(ctx,item->id,item->pri);
    }
    pthread_rwlock_unlock(&list->rwlock);
}

static void rwclear(struct safe_rw_list *list)
{
    int              i     = 0;
    struct list_head *head = &list->list;
    struct list_head *pos  = NULL;
    assert(list);
    pthread_rwlock_wrlock(&list->rwlock);
    list_for_each(pos, head)
    {
        qos_container_item_t
                *item = container_of(pos, qos_container_item_t, list);
        list->cache->free(list->cache, item);
    }
    LISTHEAD_INIT(head);
    pthread_rwlock_unlock(&list->rwlock);
}

void safe_rw_list_exit(safe_rw_list_t *list)
{
    assert(list);
    rwclear(list);
    pthread_rwlock_destroy(&list->rwlock);
}

#ifdef SAFE_RW_LIST_TEST

static void rwdump(struct safe_rw_list *list);

#endif

int safe_rw_list_init(safe_rw_list_t *list, memory_cache_t *item_cache)
{
    int err;
    assert(list && item_cache);
    memset(list, 0, sizeof(safe_rw_list_t));
    list->cache = item_cache;
    LISTHEAD_INIT(&list->list);
    list->for_each_do = for_each_do;
    list->set_compare = rwset_compare;
    list->insert      = rwinsert;
    list->erase       = rwerase;
    list->find        = rwfind;
#ifdef SAFE_RW_LIST_TEST
    list->dump = rwdump;
#endif
    err = pthread_rwlock_init(&list->rwlock, NULL);
    if ( err )
    { err = safe_rw_list_error_initlock; }
    return err;
}
/**for test****************************************************************************************************/
#ifdef SAFE_RW_LIST_TEST

#include <stdio.h>
#include <unistd.h>
#include "sysqos_test_lib.h"

#define TEST_MAX_ITEM_NUM 4096
test_map_t test;
#define first_insert_num 20
#define first_erase_num 10
#define item_piece      10
#define second_insert_num   200000
#define insert_thread_num   10
#define second_erase_num    200010
#define erase_thread_num    10
#define second_find_num     400000
#define find_thread_num     10


static void rwdump(struct safe_rw_list *list)
{
    int              i     = 0;
    struct list_head *head = NULL;
    struct list_head *pos  = NULL;
    assert(list);
    printf(GREEN"-----rwdump-------------------------------------------------------\n"RESET);
    head = &list->list;
    list_for_each(pos, head)
    {
        qos_container_item_t
                *item = container_of(pos, qos_container_item_t, list);
        printf("%p:%p ", item->id, item->pri);
        if ( ++i % 16 == 0 )
        { printf("\n"); }
    }
    printf("\n");
}

static void test_case_init_exit(safe_rw_list_t *list, memory_cache_t *cache)
{
    assert(0 == safe_rw_list_init(list, cache));
    list->set_compare(list, test_compare);
    safe_rw_list_exit(list);
    assert(0 == safe_rw_list_init(list, cache));
    list->set_compare(list, test_compare);
    printf("[test_case_init_exit] %s[OK]%s\n", GREEN, RESET);
}

static void test_case_regular_insert_find(safe_rw_list_t *list)
{
    unsigned long   max_num  = first_insert_num;
    unsigned long   left_num = first_insert_num;
    test_key_pair_t pairs[first_insert_num];
    while ( left_num > 0 )
    {
        unsigned long tmp_len = left_num;
        test.try_get_items_insert(&test, &tmp_len,
                                  pairs + (max_num - left_num));
        insert_pairs(list, tmp_len, &pairs[max_num - left_num], &test);
        left_num -= tmp_len;
    }
    test.complete_insert(&test, max_num, pairs);
    checksum_pairs(list, max_num, pairs);
#ifdef CACHE_OPEN_CNT
    assert(list->cache->alloc_cnt == first_insert_num &&
           list->cache->free_cnt == 0);
#endif
    printf("[test_case_regular_insert_find] %s[OK]%s\n", GREEN, RESET);
}

static void test_case_regular_erase(safe_rw_list_t *list)
{
    unsigned long   max_num  = first_erase_num;
    unsigned long   left_num = first_erase_num;
    test_key_pair_t pairs[first_erase_num];
    while ( left_num > 0 )
    {
        unsigned long tmp_len = left_num;
        test.try_get_items_erase(&test, &tmp_len, pairs + (max_num - left_num));
        left_num -= tmp_len;
    }
    checksum_pairs(list, max_num, pairs);
    erase_pairs(list, max_num, pairs);
    check_pairs_not_exist(list, max_num, pairs);
    test.complete_erase(&test, max_num, pairs);
#ifdef CACHE_OPEN_CNT
    assert(list->cache->alloc_cnt == first_insert_num &&
           list->cache->free_cnt == first_erase_num);
#endif
    printf("[test_case_regular_erase] %s[OK]%s\n", GREEN, RESET);
}

static void test_case_regular_find(safe_rw_list_t *list)
{
    unsigned long   max_num  = first_insert_num - first_erase_num;
    unsigned long   left_num = first_insert_num - first_erase_num;
    test_key_pair_t pairs[first_insert_num - first_erase_num];
    while ( left_num > 0 )
    {
        unsigned long tmp_len = left_num;
        test.try_find_items_lock_erase(&test, &tmp_len,
                                       pairs + (max_num - left_num));
        left_num -= tmp_len;
    }
    checksum_pairs(list, max_num, pairs);
    test.unlock_erase(&test, max_num, pairs);
#ifdef CACHE_OPEN_CNT
    assert(list->cache->alloc_cnt == first_insert_num &&
           list->cache->free_cnt == first_erase_num);
#endif
    printf("[test_case_regular_find] %s[OK]%s\n", GREEN, RESET);
}

static void *insert_thread_func(void *arg)
{
    safe_rw_list_t  *list      = arg;
    int             insert_num = second_insert_num / insert_thread_num;
    int             left_num   = insert_num;
    test_key_pair_t pairs[item_piece];
    while ( left_num > 0 )
    {
        unsigned long tmp_len = min(left_num, item_piece);
        test.try_get_items_insert(&test, &tmp_len, pairs);
        if ( tmp_len == 0 )
        {
            usleep(100);
            continue;
        }
        insert_pairs(list, tmp_len, pairs, (&test));
        test.complete_insert(&test, tmp_len, pairs);
        left_num -= tmp_len;
        usleep(1000);
    }
}

static void *erase_thread_func(void *arg)
{
    safe_rw_list_t  *list     = arg;
    int             erase_num = second_erase_num / erase_thread_num;
    int             left_num  = erase_num;
    test_key_pair_t pairs[item_piece];
    while ( left_num > 0 )
    {
        unsigned long tmp_len = min(left_num, item_piece);
        test.try_get_items_erase(&test, &tmp_len, pairs);
        if ( tmp_len == 0 )
        {
            usleep(100);
            continue;
        }
        erase_pairs(list, tmp_len, pairs);
        test.complete_erase(&test, tmp_len, pairs);
        left_num -= tmp_len;
    }
}

static void *find_thread_func(void *arg)
{
    safe_rw_list_t  *list    = arg;
    int             find_num = second_find_num / find_thread_num;
    int             left_num = find_num;
    test_key_pair_t pairs[item_piece];
    while ( left_num > 0 )
    {
        unsigned long tmp_len = min(left_num, item_piece);
        test.try_find_items_lock_erase(&test, &tmp_len, pairs);
        if ( tmp_len == 0 )
        {
            usleep(100);
            continue;
        }
        checksum_pairs(list, tmp_len, pairs);
        test.unlock_erase(&test, tmp_len, pairs);
        left_num -= tmp_len;
    }
}

static void test_case_concurrency(safe_rw_list_t *list)
{
    clock_t        begin_time, end_time;
    test_threads_t threads;
    test_threads_init(&threads);
    threads.add_type(&threads, insert_thread_num, insert_thread_func, list);
    threads.add_type(&threads, erase_thread_num, erase_thread_func, list);
    threads.add_type(&threads, find_thread_num, find_thread_func, list);
    begin_time = clock();
    threads.run(&threads);
    end_time = clock();
#ifdef CACHE_OPEN_CNT
    assert(list->cache->free_cnt == list->cache->alloc_cnt);
#endif
    test_threads_exit(&threads);
    printf("[test_case_concurrency] %s[OK]%s takes %fs\n", GREEN, RESET,
           (double) (end_time - begin_time) / CLOCKS_PER_SEC);
};

#endif

void test_safe_rw_list()
{
#ifdef SAFE_RW_LIST_TEST
    int err = 0;
    printf(YELLOW"--------------test_safe_rw_list----------------------:\n"RESET);
    test_map_init(&test, TEST_MAX_ITEM_NUM);
    memory_cache_t cache;
    memory_cache_init(&cache, sizeof(qos_container_item_t), TEST_MAX_ITEM_NUM);
    safe_rw_list_t list;
    
    test_case_init_exit(&list, &cache);
    
    test_case_regular_insert_find(&list);
    
    test_case_regular_erase(&list);
    
    test_case_regular_find(&list);
    
    test_case_concurrency(&list);
    test_map_exit(&test);
    safe_rw_list_exit(&list);
    //FIXME for each do
    printf(BLUE"----------------test_safe_rw_list end----------------:\n"RESET);
#endif
}
