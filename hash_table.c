//
// Created by root on 18-7-23.
//

#include <assert.h>
#include <pthread.h>
#include "hash_table.h"
#include "sysqos_alloc.h"
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
