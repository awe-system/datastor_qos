//
// Created by root on 18-7-24.
//

#ifndef TEST_QOS_QOS_TEST_LIB_H
#define TEST_QOS_QOS_TEST_LIB_H
#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#ifndef INOUT
#define INOUT
#endif

#include <sys/types.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
#define MAX_TEST_THREAD_PER_TYPE  1024
#define MAX_TEST_TRHEAD_TYPE      10204

typedef void *(*test_thread_func)(void *);

typedef struct test_thread
{
    void             *pri;
    int              thread_num;
    pthread_t        *threads;
    test_thread_func func;
}            test_thread_t;

typedef struct test_threads
{
    int           thread_num;
    test_thread_t threads[MAX_TEST_TRHEAD_TYPE];
    
    void (*run)(struct test_threads *thread);
    
    void (*add_type)(struct test_threads *threads, int thread_num,
                     test_thread_func func, void *pri);
} test_threads_t;

void test_threads_init(test_threads_t *thread);

void test_threads_exit(test_threads_t *thread);

typedef struct test_key_pair
{
    unsigned long key;
    unsigned long val;
} test_key_pair_t;

//测试增删改查功能
typedef struct test_map
{
    void (*try_get_items_erase)(struct test_map *tm, INOUT
                                unsigned long *max_pair_num, OUT
                                test_key_pair_t pairs[]);
    
    void (*complete_erase)(struct test_map *tm, IN unsigned long pair_num, IN
                           test_key_pair_t pairs[]);
    
    void (*try_get_items_insert)(struct test_map *tm, INOUT
                                 unsigned long *max_pair_num, OUT
                                 test_key_pair_t pairs[]);
    
    void (*complete_insert)(struct test_map *tm, IN unsigned long pair_num, IN
                            test_key_pair_t pairs[]);
    
    void (*try_update_items_lock_erase)(struct test_map *tm, INOUT
                                        unsigned long *max_pair_num, OUT
                                        test_key_pair_t pairs[]);
    
    void (*try_find_items_lock_erase)(struct test_map *tm, INOUT
                                      unsigned long *max_pair_num, OUT
                                      test_key_pair_t pairs[]);
    
    void (*unlock_erase)(struct test_map *tm, IN unsigned long pair_num, IN
                         test_key_pair_t pairs[]);
    
    /*************************************************/
    void (*dump)(struct test_map *tm);
    
    /*************************************************/
    unsigned long    max_key_num;
    unsigned long    *vals;
    bool *in_tab;
    bool *out_tab;
    unsigned long    *lock_tab;
    pthread_rwlock_t lck;
} test_map_t;

void test_map_init(test_map_t *tm, unsigned long max_key_num);

void test_map_exit(test_map_t *tm);

void test_dump_pairs(unsigned long num, test_key_pair_t pairsp[]);


#define insert_pairs(cur_map, num, pairs, tm) \
do{\
    unsigned long i =0;\
    for(i= 0;i<(num);++i)\
    {\
        int err = (cur_map)->insert((cur_map),(void*)(pairs)[i].key,(void*)(pairs)[i].val);\
        if(err){(cur_map)->dump(cur_map);(tm)->dump(tm);test_dump_pairs(num,pairs);}\
        assert(err == 0);\
    }\
}while(0)

#define erase_pairs(cur_map, num, pairs) \
do{\
    unsigned long i =0;\
    for(i= 0;i<(num);++i)\
    {\
        void *tmp_val;\
        int err = (cur_map)->erase((cur_map),(void*)(pairs)[i].key,&tmp_val);\
        assert((pairs)[i].val == (unsigned long)tmp_val);\
        assert(err == 0);\
    }\
}while(0)

//
//#define checksum_pairs_diff(cur_map,num,pairs) \
//do{\
//    unsigned long i =0;\
//    for(i= 0;i<(num);++i)\
//    {\
//        void *tmp_val;\
//        int err = (cur_map)->find((cur_map),(void*)(pairs)[i].key,&tmp_val);\
//        assert((pairs)[i].val != (unsigned long)tmp_val);\
//        assert(err == 0);\
//    }\
//}while(0)
//
//
//#define update_pairs(cur_map,num,pairs) \
//do{\
//    unsigned long i =0;\
//    for(i= 0;i<(num);++i)\
//    {\
//        void **tmp_val;\
//        int err = (cur_map)->find((cur_map),(void*)(pairs)[i].key,tmp_val);\
//        assert((pairs)[i].val == (unsigned long)tmp_val);\
//        assert(err == 0);\
//    }\
//}while(0)

#define checksum_pairs(cur_map, num, pairs) \
do{\
    unsigned long i =0;\
    for(i= 0;i<(num);++i)\
    {\
        void *tmp_val;\
        int err = (cur_map)->find((cur_map),(void*)(pairs)[i].key,&tmp_val);\
        assert((pairs)[i].val == (unsigned long)tmp_val);\
        assert(err == 0);\
    }\
}while(0)

#define check_pairs_not_exist(cur_map, num, pairs) \
do{\
    unsigned long i =0;\
    for(i= 0;i<(num);++i)\
    {\
        void *tmp_val;\
        int err = (cur_map)->find((cur_map),(void*)(pairs)[i].key,&tmp_val);\
        assert(err);\
    }\
}while(0)


static inline int test_compare(void *id_a, void *id_b)
{
    if ( id_a == id_b )
    {
        return 0;
    }
    if ( id_a > id_b )
    {
        return 1;
    }
    return -1;
}

static inline int test_hash(void *id, int tab_len)
{
    unsigned long tmp = (unsigned long) id;
    
    int res = (int) (tmp & 0xefffffff) % tab_len;
    res = (res + tab_len) % tab_len;
    return res;
}


#ifdef __cplusplus
}
#endif
#endif //TEST_QOS_QOS_TEST_LIB_H
