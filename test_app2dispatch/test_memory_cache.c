// Created by root on 18-9-4.
//

#include "test_frame.h"
#include "test_memory_cache.h"


const unsigned long test_cache_unit       = 1000;
const unsigned long test_unit_size        = sizeof(struct list_head);
const int           test_alloc_thread_num = 100;
const int           test_level_time       = 1000;

//
//static void dumps(memory_cache_t *cache)
//{
//    printf(YELLOW"[cache]:\n"RESET);
//    printf(" .unit_size =%lu:\n", cache->unit_size);
//    printf(" .cache_unit=%lu:\n", cache->cache_unit);
//#ifdef CACHE_OPEN_CNT
//    printf(" .alloc_cnt =%lu:\n", cache->alloc_cnt);
//    printf(" .free_cnt  =%lu:\n", cache->free_cnt);
//#endif
//    printf(" .alloc     =%p:\n", cache->alloc);
//    printf(" .free      =%p:\n", cache->free);
//}

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
    return NULL;
}

static memory_cache_t cache;

static int test_init()
{
    int err;
    printf(YELLOW"\n--------------test_context_init--------------------:\n"RESET);
    err = memory_cache_init(&cache, test_unit_size, test_cache_unit);
    if ( err )
    { return CUE_NOMEMORY; }
    return CUE_SUCCESS;
}

static int test_clean()
{
    memory_cache_exit(&cache);
    printf(BLUE"\n----------------test_clean-----------------\n"RESET);
    return CUE_SUCCESS;
}


static void test_case_alloc_free()
{
    test_threads_t threads;
    test_threads_init(&threads);
    threads.add_type(&threads, test_alloc_thread_num, alloc_thread, &cache);
    threads.run(&threads);
    //    dumps(cache);
#ifdef CACHE_OPEN_CNT
    CU_ASSERT(cache.free_cnt == cache.alloc_cnt);
#endif
    test_threads_exit(&threads);
}

void memory_cache_suit_init(test_frame_t *frame)
{
    test_suit_t suit;
    
    test_suit_init(&suit,"memory_cache_suit",test_init,test_clean);
    suit.add_case(&suit,"test_case_alloc_free",test_case_alloc_free);
    
    frame->add_suit(frame,&suit);
}


