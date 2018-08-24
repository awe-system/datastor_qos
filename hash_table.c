//
// Created by root on 18-7-23.
//

#include <assert.h>
#include <pthread.h>
#include "hash_table.h"
#include "sysqos_alloc.h"
#include "sysqos_common.h"
#include "sysqos_container_item.h"

static int hinsert(hash_table_t *tab, void *id, void *pri)
{
    int index = 0;
    assert(tab && tab->find && tab->hash);
    
    index = tab->hash(id, tab->tab_len);
    assert(index < tab->tab_len);
    safe_rw_list_t *list = &tab->hash_table[index];
    return list->insert(list, id, pri);
}

static int herase(hash_table_t *tab, void *id, void **pri)
{
    int index = 0;
    assert(tab && tab->hash);
    index = tab->hash(id, tab->tab_len);
    assert(index < tab->tab_len);
    safe_rw_list_t *list = &tab->hash_table[index];
    return list->erase(list, id, pri);
}

static int hfind(hash_table_t *tab, void *id, void **pri)
{
    int index = 0;
    assert(tab && pri && tab->hash);
    index = tab->hash(id, tab->tab_len);
    assert(index < tab->tab_len);
    safe_rw_list_t *list = &tab->hash_table[index];
    return list->find(list, id, pri);
}

static void hset_compare(hash_table_t *tab, compare_id_func compare)
{
    int i = 0;
    for ( i = 0; i < tab->tab_len; ++i )
    {
        tab->hash_table[i].set_compare(tab->hash_table + i, compare);
    }
}

static void
for_each_do(struct hash_table *tab, void *ctx, for_each_dofunc_t dofunc)
{
    int       i = 0;
    for ( i = 0; i < tab->tab_len; ++i )
    {
        tab->hash_table[i].for_each_do(&tab->hash_table[i], ctx, dofunc);
    }
}

static void hset_hash(hash_table_t *tab, hash_func hash)
{
    tab->hash = hash;
}

#ifdef HASH_TABLE_TEST

static void hdump(struct hash_table *tab);

#endif

hash_table_t *alloc_hash_table(int table_len, unsigned long item_num)
{
    int i = 0, j = 0;
    assert(table_len > 0);
    hash_table_t *tab = (hash_table_t *) qos_alloc(
            sizeof(hash_table_t) + sizeof(safe_rw_list_t) * table_len);
    if ( !tab )
    { goto malloc_hash_tab_failed; }
    
    int err = memory_cache_init(&tab->cache, sizeof(qos_container_item_t),
                                item_num);
    if ( err )
    { goto memory_cache_init_failed; }
    
    tab->for_each_do = for_each_do;
    tab->tab_len     = table_len;
    tab->set_hash    = hset_hash;
    tab->set_compare = hset_compare;
    tab->find        = hfind;
    tab->erase       = herase;
    tab->insert      = hinsert;
#ifdef HASH_TABLE_TEST
    tab->dump = hdump;
#endif
    for ( i = 0; i < table_len; ++i )
    {
        err = safe_rw_list_init(&tab->hash_table[i], &tab->cache);
        if ( err )
        { goto safe_rw_list_init_faild; }
    }
    
    return tab;
safe_rw_list_init_faild:
    for ( j = 0; j < i; ++j )
    {
        safe_rw_list_exit(&tab->hash_table[j]);
    }
    memory_cache_exit(&tab->cache);
memory_cache_init_failed:
    qos_free(tab);
malloc_hash_tab_failed:
    return NULL;
}

void free_hash_table(hash_table_t *tab)
{
    int i = 0;
    for ( i = 0; i < tab->tab_len; ++i )
    {
        safe_rw_list_exit(&tab->hash_table[i]);
    }
    
    memory_cache_exit(&tab->cache);
    qos_free(tab);
}

/*********************************************************************************************************/

/**for test****************************************************************************************************/
#ifdef HASH_TABLE_TEST

#include <stdio.h>
#include <unistd.h>
#include "sysqos_test_lib.h"

#define TEST_TAB_LEN 127
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


static void hdump(struct hash_table *tab)
{
    int              i     = 0;
    struct list_head *head = NULL;
    struct list_head *pos  = NULL;
    assert(tab);
    printf(GREEN"-----hdump-------------------------------------------------------\n"RESET);
    for ( i = 0; i < tab->tab_len; ++i )
    {
        tab->hash_table[i].dump(tab->hash_table);
    }
}

static void test_case_regular_insert_find(hash_table_t *tab)
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

void test_case_regular_erase(hash_table_t *tab)
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

void test_case_regular_find(hash_table_t *tab)
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

void test_case_concurrency(hash_table_t *tab)
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
};
#endif

void test_hash_table()
{
#ifdef SAFE_RW_LIST_TEST
    int          err  = 0;
    hash_table_t *tab = NULL;
    printf(YELLOW"--------------test_hash_table----------------------:\n"RESET);
    test_map_init(&test, TEST_MAX_ITEM_NUM);
    
    tab = alloc_hash_table(TEST_TAB_LEN, TEST_MAX_ITEM_NUM);
    tab->set_hash(tab, test_hash);
    tab->set_compare(tab, test_compare);
    assert(tab);
    printf("[alloc_hash_table] %s[OK]%s\n", GREEN, RESET);
    
    test_case_regular_insert_find(tab);
    
    test_case_regular_erase(tab);
    
    test_case_regular_find(tab);
    
    test_case_concurrency(tab);
    
    //FIXME:fo_r_each _do
    
    test_map_exit(&test);
    
    free_hash_table(tab);
    printf("[free_hash_table] %s[OK]%s\n", GREEN, RESET);
    printf(BLUE"----------------test_hash_table end---------------:\n"RESET);
#endif
}
