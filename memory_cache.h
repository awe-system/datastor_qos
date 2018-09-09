//
// Created by root on 18-7-23.
//

#ifndef TEST_QOS_MEMORY_CACHE_H
#define TEST_QOS_MEMORY_CACHE_H

#include <sys/param.h>
#include "sysqos_type.h"

#ifdef __cplusplus
extern "C" {
#endif
#define CACHE_OPEN_CNT

//FIXME: 此类暂时之提供接口不提供 实际的与分配缓存
typedef struct memory_cache
{
    unsigned long unit_size;
    unsigned long cache_unit;
    
    /**************************************************/
    void *(*alloc)(struct memory_cache *cache);
    
    void (*free)(struct memory_cache *cache, void *data);
    /**************************************************/
#ifdef CACHE_OPEN_CNT
    unsigned long      alloc_cnt;
    unsigned long      free_cnt;
    sysqos_spin_lock_t lck;
#endif
} memory_cache_t;

int memory_cache_init(memory_cache_t *cache, unsigned long unit_size,
                      unsigned long cache_unit);

void memory_cache_exit(memory_cache_t *cache);

#ifdef __cplusplus
}
#endif
#endif //TEST_QOS_MEMORY_CACHE_H
