//
// Created by root on 18-7-23.
//

#include <assert.h>
#include "sysqos_alloc.h"
#include <string.h>
#include <pthread.h>
#include "memory_cache.h"

typedef struct memory_item
{
    struct list_head list;
    long             mem[0];
} memory_item_t;

static void *cache_alloc(struct memory_cache *cache)
{
    void *res = NULL;
    sysqos_spin_lock(&cache->lck);
#ifdef CACHE_OPEN_CNT
    
    if ( cache->free_cnt + cache->cache_unit - cache->alloc_cnt > 0 )
    {
        
        ++cache->alloc_cnt;
#endif
        if ( !list_empty(&cache->empty_pool) )
        {
            struct list_head *list = cache->empty_pool.next;
            list_del(list);
            list_add(list,&cache->used_pool);
            res = container_of(list, memory_item_t, list)->mem;
        }
#ifdef CACHE_OPEN_CNT
        if ( res == NULL )
        {
            --cache->alloc_cnt;
        }
    }
#endif
    sysqos_spin_unlock(&cache->lck);
    if ( res )
    { memset(res, 0, cache->unit_size); }
    return res;
}

static void cache_free(struct memory_cache *cache, void *data)
{
    memory_item_t *item = container_of(data,memory_item_t,mem);
    sysqos_spin_lock(&cache->lck);
#ifdef CACHE_OPEN_CNT
    ++cache->free_cnt;
#endif
    list_del(&item->list);
    LISTHEAD_INIT(&item->list);
    list_add(&item->list,&cache->empty_pool);
    sysqos_spin_unlock(&cache->lck);
}

static void clear_pool(struct list_head *head)
{
    struct list_head *pos = NULL;
    struct list_head *tmp = NULL;
    list_for_each_safe(pos, tmp, head)
    {
        list_del(pos);
        memory_item_t *item = container_of(pos, memory_item_t, list);
        qos_free(item);
    }
}
static void clear_pools(memory_cache_t *cache)
{
    clear_pool(&cache->empty_pool);
    clear_pool(&cache->used_pool);
}

static inline void memory_item_init(memory_item_t *mem)
{
    LISTHEAD_INIT(&mem->list);
}

static int alloc_empty_pool(memory_cache_t *cache)
{
    unsigned long i = 0;
    for ( i = 0; i < cache->cache_unit; ++i )
    {
        memory_item_t *mem = NULL;
        mem = qos_alloc(cache->unit_size + sizeof(memory_item_t));
        if ( !mem )
        { goto alloc_failed; }
        memory_item_init(mem);
        list_add(&mem->list, &cache->empty_pool);
    }
    return 0;
alloc_failed:
    clear_pool(&cache->empty_pool);
    return -1;
}

int memory_cache_init(memory_cache_t *cache, unsigned long unit_size,
                      unsigned long cache_unit)
{
    int err;
    assert(cache);
    memset(cache, 0, sizeof(memory_cache_t));
    cache->unit_size  = unit_size;
    cache->cache_unit = cache_unit;
    cache->alloc      = cache_alloc;
    cache->free       = cache_free;
    LISTHEAD_INIT(&cache->empty_pool);
    LISTHEAD_INIT(&cache->used_pool);
    err = alloc_empty_pool(cache);
    if ( err )
    {
        return err;
    }
#ifdef CACHE_OPEN_CNT
    cache->alloc_cnt = 0;
    cache->free_cnt  = 0;
#endif
    err = sysqos_spin_init(&cache->lck);
    assert(err == 0);
    return err;
}

void memory_cache_exit(memory_cache_t *cache)
{
    sysqos_spin_destroy(&cache->lck);
    clear_pools(cache);
    assert(cache);
}

