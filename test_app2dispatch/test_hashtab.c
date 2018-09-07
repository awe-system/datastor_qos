//
// Created by root on 18-9-4.
//

#include <test_frame.h>
#include "test_hashtab.h"

#define TEST_TAB_LEN 127
#define TEST_MAX_ITEM_NUM 4096
#define first_insert_num 20
#define first_erase_num 10
#define item_piece      10
#define second_insert_num   200000
#define insert_thread_num   10
#define second_erase_num    200010
#define erase_thread_num    10
#define second_find_num     400000
#define find_thread_num     10

static test_map_t test;
static hash_table_t *tab = NULL;

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
        insert_pairs(tab, tmp_len, &pairs[max_num - left_num], &test);
        left_num -= tmp_len;
    }
    test.complete_insert(&test, max_num, pairs);
    checksum_pairs(tab, max_num, pairs);
#ifdef CACHE_OPEN_CNT
    assert(tab->cache.alloc_cnt == first_insert_num &&
           tab->cache.free_cnt == 0);
#endif
    printf("[test_case_regular_insert_find] %s[OK]%s\n", GREEN, RESET);
}

void test_case_regular_erase()
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
    checksum_pairs(tab, max_num, pairs);
    erase_pairs(tab, max_num, pairs);
    check_pairs_not_exist(tab, max_num, pairs);
    test.complete_erase(&test, max_num, pairs);
#ifdef CACHE_OPEN_CNT
    assert(tab->cache.alloc_cnt == first_insert_num &&
           tab->cache.free_cnt == first_erase_num);
#endif
    printf("[test_case_regular_erase] %s[OK]%s\n", GREEN, RESET);
}

void test_case_regular_find()
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
    checksum_pairs(tab, max_num, pairs);
    test.unlock_erase(&test, max_num, pairs);
#ifdef CACHE_OPEN_CNT
    assert(tab->cache.alloc_cnt == first_insert_num &&
           tab->cache.free_cnt == first_erase_num);
#endif
    printf("[test_case_regular_find] %s[OK]%s\n", GREEN, RESET);
}

#define min(a, b) ((a)<(b)?(a):(b))

void *insert_thread_func(void *arg)
{
    hash_table_t    *tab       = arg;
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
        insert_pairs(tab, tmp_len, pairs, (&test));
        test.complete_insert(&test, tmp_len, pairs);
        left_num -= tmp_len;
        usleep(1000);
    }
}

void *erase_thread_func(void *arg)
{
    hash_table_t    *tab      = arg;
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
        erase_pairs(tab, tmp_len, pairs);
        test.complete_erase(&test, tmp_len, pairs);
        left_num -= tmp_len;
    }
}

void *find_thread_func(void *arg)
{
    hash_table_t    *tab     = arg;
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
        checksum_pairs(tab, tmp_len, pairs);
        test.unlock_erase(&test, tmp_len, pairs);
        left_num -= tmp_len;
    }
}

void test_case_concurrency()
{
    clock_t        begin_time, end_time;
    test_threads_t threads;
    test_threads_init(&threads);
    threads.add_type(&threads, insert_thread_num, insert_thread_func, tab);
    threads.add_type(&threads, erase_thread_num, erase_thread_func, tab);
    threads.add_type(&threads, find_thread_num, find_thread_func, tab);
    begin_time = clock();
    threads.run(&threads);
    end_time = clock();
#ifdef CACHE_OPEN_CNT
    assert(tab->cache.free_cnt == tab->cache.alloc_cnt);
#endif
    test_threads_exit(&threads);
    printf("[test_case_concurrency] %s[OK]%s takes %fs\n", GREEN, RESET,
           (double) (end_time - begin_time) / CLOCKS_PER_SEC);
}

static int test_init()
{
    printf(YELLOW"\n-------------test_init----------------------:\n"RESET);
    test_map_init(&test, TEST_MAX_ITEM_NUM);
    
    tab = alloc_hash_table(TEST_TAB_LEN, TEST_MAX_ITEM_NUM);
    printf("[alloc_hash_table] %s[OK]%s\n", GREEN, RESET);
    
    tab->set_hash(tab, test_hash);
    tab->set_compare(tab, test_compare);
    assert(tab);
    return CUE_SUCCESS;
}

static int test_clean()
{
    test_map_exit(&test);
    free_hash_table(tab);
    printf("\n[free_hash_table] %s[OK]%s\n", GREEN, RESET);
    printf(BLUE"\n----------------test_hash_table end---------------:\n"RESET);
    return CUE_SUCCESS;
}

void hashtab_suit_init(test_frame_t *frame)
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

