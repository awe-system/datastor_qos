//
// Created by root on 18-7-23.
//

#include <assert.h>
#include "sysqos_alloc.h"
#include <string.h>
#include <pthread.h>
#include "memory_cache.h"

//FIXME: update the class to prepare alloc

static void *cache_alloc(struct memory_cache *cache)
{
    void *res = NULL;
#ifdef CACHE_OPEN_CNT
    pthread_spin_lock(&cache->lck);
    if ( cache->free_cnt + cache->cache_unit - cache->alloc_cnt > 0 )
    {
        ++cache->alloc_cnt;
        pthread_spin_unlock(&cache->lck);
#endif
        //刷0
        res = qos_alloc(cache->unit_size);
#ifdef CACHE_OPEN_CNT
        pthread_spin_lock(&cache->lck);
        if ( res == NULL )
        {
            --cache->alloc_cnt;
        }
    }
    pthread_spin_unlock(&cache->lck);
#endif
    return res;
}

static void cache_free(struct memory_cache *cache, void *data)
{
#ifdef CACHE_OPEN_CNT
    pthread_spin_lock(&cache->lck);
    ++cache->free_cnt;
    pthread_spin_unlock(&cache->lck);
#endif
    qos_free(data);
}

int memory_cache_init(memory_cache_t *cache, unsigned long unit_size,
                      unsigned long cache_unit)
{
    assert(cache);
    memset(cache, 0, sizeof(memory_cache_t));
    cache->unit_size  = unit_size;
    cache->cache_unit = cache_unit;
    cache->alloc      = cache_alloc;
    cache->free       = cache_free;
#ifdef CACHE_OPEN_CNT
    cache->alloc_cnt = 0;
    cache->free_cnt  = 0;
    pthread_spin_init(&cache->lck, PTHREAD_PROCESS_PRIVATE);
#endif
    return 0;
}

void memory_cache_exit(memory_cache_t *cache)
{
#ifdef CACHE_OPEN_CNT
    pthread_spin_destroy(&cache->lck);
#endif
    
    assert(cache);
}

/**for test****************************************************************************************************/


#ifdef MEMORY_CACHE_TEST

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include "list_head.h"
#include "sysqos_alloc.h"
#include "sysqos_common.h"
#include "sysqos_test_lib.h"

const unsigned long test_cache_unit       = 10000;
const unsigned long test_unit_size        = sizeof(struct list_head);
const int           test_alloc_thread_num = 100;
const int           test_level_time       = 1000;


static void dumps(memory_cache_t *cache)
{
    printf(YELLOW"[cache]:\n"RESET);
    printf(" .unit_size =%lu:\n", cache->unit_size);
    printf(" .cache_unit=%lu:\n", cache->cache_unit);
#ifdef CACHE_OPEN_CNT
    printf(" .alloc_cnt =%lu:\n", cache->alloc_cnt);
    printf(" .free_cnt  =%lu:\n", cache->free_cnt);
#endif
    printf(" .alloc     =%p:\n", cache->alloc);
    printf(" .free      =%p:\n", cache->free);
}

static void *alloc_thread(void *arg)
{
    memory_cache_t *cache = (memory_cache_t *) arg;
    unsigned long  i      = 0;
    for ( i = 0;
          i < test_level_time * cache->cache_unit / test_alloc_thread_num; ++i )
    {
        void *tmp = cache->alloc(cache);
        if ( !tmp )
        {
            --i;
            continue;
        }
        
        if ( i & 1 )
        {
            usleep(1);
        }
        cache->free(cache, tmp);
    }
}

static int test_case_alloc_free(memory_cache_t *cache)
{
    test_threads_t threads;
    test_threads_init(&threads);
    threads.add_type(&threads, test_alloc_thread_num, alloc_thread, cache);
    threads.run(&threads);
    //    dumps(cache);
#ifdef CACHE_OPEN_CNT
    assert(cache->free_cnt == cache->alloc_cnt);
#endif
    test_threads_exit(&threads);
    return 0;
}

#endif

void test_memory_cache()
{
#ifdef MEMORY_CACHE_TEST
    int err = 0;
    printf(YELLOW"--------------test_memory_cache----------------------:\n"RESET);
    memory_cache_t cache;
    err = memory_cache_init(&cache, test_unit_size, test_cache_unit);
    assert(err == 0);
    printf("[memory_cache_init] %s[OK]%s\n", GREEN, RESET);
    err = test_case_alloc_free(&cache);
    assert(err == 0);
    printf("[test_case_alloc_free] %s[OK]%s\n", GREEN, RESET);
    memory_cache_exit(&cache);
    printf("[memory_cache_exit] %s[OK]%s\n", GREEN, RESET);
    printf(BLUE"----------------test_memory_cache end-----------------\n"RESET);
#endif
}

