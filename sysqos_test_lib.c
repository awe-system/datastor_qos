//
// Created by root on 18-7-24.
//

#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include "sysqos_test_lib.h"
#include "sysqos_alloc.h"
#include "sysqos_common.h"

static void test_thread_init(test_thread_t *thread,
                             int thread_num, test_thread_func func,
                             void *pri)
{
    assert(thread && thread_num && func &&
           thread_num < MAX_TEST_THREAD_PER_TYPE);
    thread->thread_num = thread_num;
    thread->threads =
            qos_alloc(MAX_TEST_THREAD_PER_TYPE * sizeof(pthread_t));
    thread->pri = pri;
    assert(thread->threads);
    thread->func = func;
}

static void test_thread_exit(test_thread_t *thread)
{
    assert(thread && thread->thread_num && thread->threads && thread->func);
    qos_free(thread->threads);
}

static void test_run(test_threads_t *threads)
{
    int i = 0, j = 0;
    for ( i = 0; i < threads->thread_num; ++i )
    {
        test_thread_t *thread = &threads->threads[i];
        for ( j = 0; j < thread->thread_num; ++j )
        {
            pthread_create(&thread->threads[j], NULL, thread->func,
                           thread->pri);
        }
    }
    
    for ( i = 0; i < threads->thread_num; ++i )
    {
        test_thread_t *thread = &threads->threads[i];
        for ( j = 0; j < thread->thread_num; ++j )
        {
            pthread_join(thread->threads[j], NULL);
        }
    }
}

static void test_add_type(test_threads_t *threads,
                          int thread_num, test_thread_func func,
                          void *pri)
{
    test_thread_t *thread = &threads->threads[threads->thread_num++];
    test_thread_init(thread, thread_num, func, pri);
}

void test_threads_init(test_threads_t *thread)
{
    assert(thread);
    memset(thread, 0, sizeof(test_threads_t));
    thread->thread_num = 0;
    thread->run        = test_run;
    thread->add_type   = test_add_type;
}

void test_threads_exit(test_threads_t *thread)
{
    int i = 0;
    assert(thread);
    for ( i = 0; i < thread->thread_num; ++i )
    {
        test_thread_exit(&thread->threads[i]);
    }
}

/****************************************************************************************************/

static void test_try_get_items_erase(struct test_map *tm,
                                     INOUT unsigned long *max_pair_num,
                                     OUT test_key_pair_t pairs[])
{
    unsigned long i = 0, j = 0;
    assert(max_pair_num && *max_pair_num);
    
    pthread_rwlock_wrlock(&tm->lck);
    for ( i = 0, j = 0; i < tm->max_key_num && j < *max_pair_num; ++i )
    {
        if ( tm->in_tab[i] && !tm->lock_tab[i] )
        {
            tm->in_tab[i] = false;
            pairs[j].key = i;
            pairs[j].val = tm->vals[i];
            ++j;
        }
    }
    pthread_rwlock_unlock(&tm->lck);
    *max_pair_num = j;
}

static void test_complete_erase(struct test_map *tm,
                                IN unsigned long pair_num, IN
                                test_key_pair_t pairs[])
{
    unsigned long i = 0;
    assert(pair_num);
    
    pthread_rwlock_wrlock(&tm->lck);
    for ( i = 0; i < pair_num; ++i )
    {
        tm->out_tab[pairs[i].key] = true;
    }
    pthread_rwlock_unlock(&tm->lck);
    
}

static void test_try_get_items_insert(struct test_map *tm, INOUT
                                      unsigned long *max_pair_num, OUT
                                      test_key_pair_t pairs[])
{
    unsigned long i = 0, j = 0;
    assert(max_pair_num && *max_pair_num);
    
    pthread_rwlock_wrlock(&tm->lck);
    for ( i = 0, j = 0; i < tm->max_key_num && j < *max_pair_num; ++i )
    {
        if ( tm->out_tab[i] )
        {
            tm->out_tab[i] = false;
            pairs[j].key = i;
            pairs[j].val = tm->vals[i];
            ++j;
        }
    }
    pthread_rwlock_unlock(&tm->lck);
    *max_pair_num = j;
}

static void test_complete_insert(struct test_map *tm,
                                 IN unsigned long pair_num, IN
                                 test_key_pair_t pairs[])
{
    unsigned long i = 0;
    assert(pair_num);
    
    pthread_rwlock_wrlock(&tm->lck);
    for ( i = 0; i < pair_num; ++i )
    {
        tm->in_tab[pairs[i].key] = true;
    }
    pthread_rwlock_unlock(&tm->lck);
}


static void test_try_find_items_lock_erase(struct test_map *tm, INOUT
                                           unsigned long *max_pair_num, OUT
                                           test_key_pair_t pairs[])
{
    unsigned long i = 0, j = 0;
    assert(max_pair_num && *max_pair_num);
    
    pthread_rwlock_wrlock(&tm->lck);
    for ( i = 0, j = 0; i < tm->max_key_num && j < *max_pair_num; ++i )
    {
        if ( tm->in_tab[i] )
        {
            assert(tm->lock_tab[i] >= 0);
            ++tm->lock_tab[i];
            pairs[j].key = i;
            pairs[j].val = tm->vals[i];
            ++j;
        }
    }
    pthread_rwlock_unlock(&tm->lck);
    *max_pair_num = j;
}

static void test_try_update_items_lock_erase(struct test_map *tm, INOUT
                                             unsigned long *max_pair_num, OUT
                                             test_key_pair_t pairs[])
{
    unsigned long i = 0, j = 0;
    assert(max_pair_num && *max_pair_num);
    
    pthread_rwlock_wrlock(&tm->lck);
    for ( i = 0, j = 0; i < tm->max_key_num && j < *max_pair_num; ++i )
    {
        if ( tm->in_tab[i] && !tm->lock_tab[i] )
        {
            tm->vals[i] = rand();
            ++tm->lock_tab[i];
            pairs[j].key = i;
            pairs[j].val = tm->vals[i];
            ++j;
        }
    }
    pthread_rwlock_unlock(&tm->lck);
    *max_pair_num = j;
}


static void test_unlock_erase(struct test_map *tm, IN unsigned long pair_num, IN
                              test_key_pair_t pairs[])
{
    unsigned long i = 0;
    assert(pair_num);
    pthread_rwlock_wrlock(&tm->lck);
    for ( i = 0; i < pair_num; ++i )
    {
        --tm->lock_tab[pairs[i].key];
    }
    pthread_rwlock_unlock(&tm->lck);
}

static void fill_random_array(unsigned long a[], unsigned long num)
{
    unsigned long i = 0;
    for ( i = 0; i < num; ++i )
    {
        a[i] = i;
    }
}

static void test_dump(test_map_t *tm)
{
    unsigned long i = 0;
    
    printf(GREEN"--------i,tm->vals[i],tm->lock_tab[i],tm->in_tab[i],tm->out_tab[i]-----------------------------------\n"RESET);
    for ( i = 0; i < tm->max_key_num; ++i )
    {
        printf("[%lu]:%lu:%lu:[%d]:[%d]\t", i, tm->vals[i], tm->lock_tab[i],
               tm->in_tab[i] ? 1 : 0, tm->out_tab[i] ? 1l : 0);
        if ( i % 8 == 7 )
        { printf("\n"); }
    }
    printf("\n");
}

void test_map_init(test_map_t *tm, unsigned long max_key_num)
{
    assert(tm && max_key_num);
    
    tm->max_key_num = max_key_num;
//    srand(time(0));
    pthread_rwlock_t lck;
    tm->vals = (unsigned long *) qos_alloc(sizeof(unsigned long) * max_key_num);
    assert(tm->vals);
    memset(tm->vals, 0, sizeof(unsigned long) * max_key_num);
    
    tm->in_tab = (bool *) qos_alloc(sizeof(bool) * max_key_num);
    assert(tm->in_tab);
    memset(tm->in_tab, 0, sizeof(bool) * max_key_num);
    
    tm->out_tab = (bool *) qos_alloc(sizeof(bool) * max_key_num);
    assert(tm->out_tab);
    memset(tm->out_tab, 0xff, sizeof(bool) * max_key_num);
    
    tm->lock_tab =
            (unsigned long *) qos_alloc(sizeof(unsigned long) * max_key_num);
    assert(tm->lock_tab);
    memset(tm->lock_tab, 0, sizeof(bool) * max_key_num);
    
    
    assert(0 == pthread_rwlock_init(&tm->lck, NULL));
    
    tm->try_get_items_erase         = test_try_get_items_erase;
    tm->complete_erase              = test_complete_erase;
    tm->try_get_items_insert        = test_try_get_items_insert;
    tm->complete_insert             = test_complete_insert;
    tm->try_find_items_lock_erase   = test_try_find_items_lock_erase;
    tm->try_update_items_lock_erase = test_try_update_items_lock_erase;
    tm->unlock_erase                = test_unlock_erase;
    tm->dump                        = test_dump;
    
    fill_random_array(tm->vals, max_key_num);
}

void test_map_exit(test_map_t *tm)
{
    pthread_rwlock_destroy(&tm->lck);
    qos_free(tm->vals);
    qos_free(tm->in_tab);
    qos_free(tm->out_tab);
    qos_free(tm->lock_tab);
}

void test_dump_pairs(unsigned long num, test_key_pair_t *pairs)
{
    unsigned long i = 0;
    printf(GREEN"----num%lu------------------------------------------\n"RESET,
           num);
    for ( i = 0; i < num; ++i )
    {
        printf("%lu:%lu ", pairs[i].key, pairs->val);
        if ( i % 16 == 15 )
        { printf("\n"); }
    }
    printf("\n");
}
