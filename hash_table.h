//
// Created by root on 18-7-23.
//

#ifndef QOS_HASH_TABLE_H
#define QOS_HASH_TABLE_H

#include "sysqos_type.h"
#include "memory_cache.h"
#include "sysqos_common.h"
#include "safe_rw_list.h"
#include "sysqos_common.h"

#define HASH_TABLE_TEST

#ifdef __cplusplus
extern "C" {
#endif

//enum
//{
//    hash_table_error_ok,
//    hash_table_error_exist,
//    hash_table_error_notfound,
//};

typedef struct hash_table
{
    int (*insert)(struct hash_table *tab, void *id, void *pri);
    
    int (*erase)(struct hash_table *tab, void *id, void **pri);
    
    int (*find)(struct hash_table *tab, void *id, void **pri);
    
    void (*set_hash)(struct hash_table *tab, hash_func hash);
    
    void (*set_compare)(struct hash_table *tab, compare_id_func compare);
    
    void (*for_each_do)(struct hash_table *tab,void *ctx,for_each_dofunc_t dofunc);
    /*************************************/
#ifdef HASH_TABLE_TEST

#endif
    memory_cache_t cache;
    hash_func      hash;
    int            tab_len;
    safe_rw_list_t hash_table[0];
} hash_table_t;

hash_table_t *alloc_hash_table(int table_len, unsigned long item_num);

void free_hash_table(hash_table_t *tab);

#ifdef __cplusplus
}
#endif
#endif //TEST_QOS_HASH_TABLE_H
