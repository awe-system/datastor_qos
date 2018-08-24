//
// Created by root on 18-7-23.
//

#ifndef TEST_QOS_MEMORY_CACHE_H
#define TEST_QOS_MEMORY_CACHE_H

#include <sys/param.h>

#ifdef __cplusplus
extern "C" {
#endif
#define MEMORY_CACHE_TEST

#define CACHE_OPEN_CNT
#ifdef MEMORY_CACHE_TEST
#ifndef CACHE_OPEN_CNT
#define CACHE_OPEN_CNT
#endif
#endif
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
    pthread_spinlock_t lck;
#endif
} memory_cache_t;

int memory_cache_init(memory_cache_t *cache, unsigned long unit_size,
                      unsigned long cache_unit);

void memory_cache_exit(memory_cache_t *cache);

#ifdef MEMORY_CACHE_TEST

//测试接口
void test_memory_cache();

#endif
#ifdef __cplusplus
}
#endif
#endif //TEST_QOS_MEMORY_CACHE_H
