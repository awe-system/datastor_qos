//
// Created by root on 18-9-4.
//

//#include <bits/time.h>
#include <time.h>
#include <unistd.h>
#include "test_rw_list.h"
#include "sysqos_test_lib.h"

#define TEST_MAX_ITEM_NUM 4096
#define first_insert_num 20
#define first_erase_num 10
#define item_piece      10UL
#define second_insert_num   200000
#define insert_thread_num   10
#define second_erase_num    200010
#define erase_thread_num    10
#define second_find_num     400000
#define find_thread_num     10


static memory_cache_t cache;
static safe_rw_list_t list;
static test_map_t     test;

static void test_case_init_exit(safe_rw_list_t *list, memory_cache_t *cache)
{
    assert(0 == safe_rw_list_init(list, cache));
    list->set_compare(list, test_compare);
    safe_rw_list_exit(list);
    assert(0 == safe_rw_list_init(list, cache));
    list->set_compare(list, test_compare);
    printf("[test_case_init_exit] %s[OK]%s\n", GREEN, RESET);
}

static void test_case_regular_insert_find()
{
    unsigned long   max_num  = first_insert_num;
    unsigned long   left_num = first_insert_num;
    test_key_pair_t pairs[first_insert_num];
    while ( left_num > 0 )
    {
        unsigned long tmp_len = left_num;
        test.try_get_items_insert(&test, &tmp_len,
                                  pairs + (max_num - left_num));
        insert_pairs(&list, tmp_len, &pairs[max_num - left_num], &test);
        left_num -= tmp_len;
    }
    test.complete_insert(&test, max_num, pairs);
    checksum_pairs(&list, max_num, pairs);
#ifdef CACHE_OPEN_CNT
    assert(list.cache->alloc_cnt == first_insert_num &&
           list.cache->free_cnt == 0);
#endif
    printf("[test_case_regular_insert_find] %s[OK]%s\n", GREEN, RESET);
}

static void test_case_regular_erase()
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
    checksum_pairs(&list, max_num, pairs);
    erase_pairs(&list, max_num, pairs);
    check_pairs_not_exist(&list, max_num, pairs);
    test.complete_erase(&test, max_num, pairs);
#ifdef CACHE_OPEN_CNT
    assert(list.cache->alloc_cnt == first_insert_num &&
           list.cache->free_cnt == first_erase_num);
#endif
    printf("[test_case_regular_erase] %s[OK]%s\n", GREEN, RESET);
}

static void test_case_regular_find()
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
    checksum_pairs(&list, max_num, pairs);
    test.unlock_erase(&test, max_num, pairs);
#ifdef CACHE_OPEN_CNT
    assert(list.cache->alloc_cnt == first_insert_num &&
           list.cache->free_cnt == first_erase_num);
#endif
    printf("[test_case_regular_find] %s[OK]%s\n", GREEN, RESET);
}

#ifdef TEST_CONCURRENCY

static void *insert_thread_func(void *arg)
{
    safe_rw_list_t  *list      = arg;
    unsigned long   insert_num = second_insert_num / insert_thread_num;
    unsigned long   left_num   = insert_num;
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
    return NULL;
}

static void *erase_thread_func(void *arg)
{
    safe_rw_list_t  *list     = arg;
    unsigned long   erase_num = second_erase_num / erase_thread_num;
    unsigned long   left_num  = erase_num;
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
    return NULL;
}

static void *find_thread_func(void *arg)
{
    safe_rw_list_t  *list    = arg;
    int             find_num = second_find_num / find_thread_num;
    unsigned long   left_num = find_num;
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
    return NULL;
}

static void test_case_concurrency()
{
    clock_t        begin_time, end_time;
    test_threads_t threads;
    test_threads_init(&threads);
    threads.add_type(&threads, insert_thread_num, insert_thread_func, &list);
    threads.add_type(&threads, erase_thread_num, erase_thread_func, &list);
    threads.add_type(&threads, find_thread_num, find_thread_func, &list);
    begin_time = clock();
    threads.run(&threads);
    end_time = clock();
#ifdef CACHE_OPEN_CNT
    assert(list.cache->free_cnt == list.cache->alloc_cnt);
#endif
    test_threads_exit(&threads);
    printf("[test_case_concurrency] %s[OK]%s takes %fs\n", GREEN, RESET,
           (double) (end_time - begin_time) / CLOCKS_PER_SEC);
};
#endif

static int test_init()
{
    printf(YELLOW"--------------test_context_init----------------------:\n"RESET);
    test_map_init(&test, TEST_MAX_ITEM_NUM);
    memory_cache_init(&cache, sizeof(qos_container_item_t), TEST_MAX_ITEM_NUM);
    test_case_init_exit(&list, &cache);
    return CUE_SUCCESS;
}

static int test_clean()
{
    test_map_exit(&test);
    safe_rw_list_exit(&list);
    printf(BLUE"----------------test_clean----------------:\n"RESET);
    return CUE_SUCCESS;
}

void safe_rw_suit_init(test_frame_t *frame)
{
    test_suit_t suit;
    
    test_suit_init(&suit, "safe_rw_suit_init", test_init, test_clean);
    suit.add_case(&suit, "test_case_regular_insert_find",
                  test_case_regular_insert_find);
    
    suit.add_case(&suit, "test_case_regular_erase", test_case_regular_erase);
    
    suit.add_case(&suit, "test_case_regular_find", test_case_regular_find);

#ifdef TEST_CONCURRENCY
    suit.add_case(&suit, "test_case_concurrency", test_case_concurrency);
#endif
    
    frame->add_suit(frame, &suit);
}