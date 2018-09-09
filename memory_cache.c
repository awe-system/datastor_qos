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
    sysqos_spin_lock(&cache->lck);
    if ( cache->free_cnt + cache->cache_unit - cache->alloc_cnt > 0 )
    {
        ++cache->alloc_cnt;
        sysqos_spin_unlock(&cache->lck);
#endif
        //刷0
        res = qos_alloc(cache->unit_size);
#ifdef CACHE_OPEN_CNT
        sysqos_spin_lock(&cache->lck);
        if ( res == NULL )
        {
            --cache->alloc_cnt;
        }
    }
    sysqos_spin_unlock(&cache->lck);
#endif
    return res;
}

static void cache_free(struct memory_cache *cache, void *data)
{
#ifdef CACHE_OPEN_CNT
    sysqos_spin_lock(&cache->lck);
    ++cache->free_cnt;
    sysqos_spin_unlock(&cache->lck);
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
    sysqos_spin_init(&cache->lck);
#endif
    return 0;
}

void memory_cache_exit(memory_cache_t *cache)
{
#ifdef CACHE_OPEN_CNT
    sysqos_spin_destroy(&cache->lck);
#endif
    
    assert(cache);
}

