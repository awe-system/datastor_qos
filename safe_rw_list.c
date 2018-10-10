//
// Created by root on 18-7-24.
//


#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include "sysqos_common.h"
#include "safe_rw_list.h"

static int rwfind_nolock(struct safe_rw_list *list, void *id, void **pri)
{
    int              err   = safe_rw_list_error_notfound;
    struct list_head *head = NULL;
    struct list_head *pos  = NULL;
    assert(list && pri && list->compare);
    
    head = &list->list;
    list_for_each(pos, head)
    {
        qos_container_item_t
                *item = container_of(pos, qos_container_item_t, list);
        if ( 0 == list->compare(item->id, id) )
        {
            *pri = item->pri;
            err = safe_rw_list_error_ok;
            break;
        }
    }
    return err;
}

static int rwinsert(struct safe_rw_list *list, void *id, void *pri)
{
    int                  err   = safe_rw_list_error_exist;
    qos_container_item_t *item = NULL;
    struct list_head     *head = NULL;
    void                 *tmp_pri;
    assert(list);
    
    ////当插入经常失败时需要打开此部分内容
//    if(0 == rwfind(list_reqgrp,id,&tmp_pri))
//        return err;
    
    item = list->cache->alloc(list->cache);
    assert(item);
    qos_container_item_init(item, id, pri);
    
    head = &list->list;
    
    sysqos_rwlock_wrlock(&list->rwlock);
    if ( 0 != rwfind_nolock(list, id, &tmp_pri) )
    {
        list_add(&item->list, head);
        err = safe_rw_list_error_ok;
    }
    sysqos_rwlock_wrunlock(&list->rwlock);
    
    if ( err )
    {
        list->cache->free(list->cache, item);
    }
    return err;
}

static int rwerase(struct safe_rw_list *list, void *id, void **pri)
{
    int              err   = safe_rw_list_error_notfound;
    struct list_head *head = NULL;
    struct list_head *pos  = NULL;
    struct list_head *tmp  = NULL;
    assert(list && list->compare);
    
    head = &list->list;
    sysqos_rwlock_wrlock(&list->rwlock);
    list_for_each_safe(pos,tmp, head)
    {
        qos_container_item_t
                *item = container_of(pos, qos_container_item_t, list);
        if ( 0 == list->compare(item->id, id) )
        {
            list_del(&item->list);
            sysqos_rwlock_wrunlock(&list->rwlock);
            if ( pri )
            {
                *pri = item->pri;
            }
            list->cache->free(list->cache, item);
            return safe_rw_list_error_ok;
        }
    }
    sysqos_rwlock_wrunlock(&list->rwlock);
    return err;
}

static int rwfind(struct safe_rw_list *list, void *id, void **pri)
{
    int err;
    sysqos_rwlock_rdlock(&list->rwlock);
    err = rwfind_nolock(list, id, pri);
    sysqos_rwlock_rdunlock(&list->rwlock);
    return err;
}


static void rwset_compare(struct safe_rw_list *list, compare_id_func compare)
{
    list->compare = compare;
}

static void for_each_do(struct safe_rw_list *list, void *ctx,
               for_each_dofunc_t dofunc)
{
    struct list_head *pos = NULL;
    sysqos_rwlock_rdlock(&list->rwlock);
    list_for_each(pos, &list->list)
    {
        qos_container_item_t
                *item = container_of(pos, qos_container_item_t, list);
        dofunc(ctx,item->id,item->pri);
    }
    sysqos_rwlock_rdunlock(&list->rwlock);
}

static void rwclear(struct safe_rw_list *list)
{
    struct list_head *head = &list->list;
    struct list_head *pos  = NULL;
    assert(list);
    sysqos_rwlock_wrlock(&list->rwlock);
    list_for_each(pos, head)
    {
        qos_container_item_t
                *item = container_of(pos, qos_container_item_t, list);
        list->cache->free(list->cache, item);
    }
    LISTHEAD_INIT(head);
    sysqos_rwlock_wrunlock(&list->rwlock);
}

void safe_rw_list_exit(safe_rw_list_t *list)
{
    assert(list);
    rwclear(list);
    sysqos_rwlock_destroy(&list->rwlock);
}


int safe_rw_list_init(safe_rw_list_t *list, memory_cache_t *item_cache)
{
    int err;
    assert(list && item_cache);
    memset(list, 0, sizeof(safe_rw_list_t));
    list->cache = item_cache;
    LISTHEAD_INIT(&list->list);
    list->for_each_do = for_each_do;
    list->set_compare = rwset_compare;
    list->insert      = rwinsert;
    list->erase       = rwerase;
    list->find        = rwfind;

    err = sysqos_rwlock_init(&list->rwlock);
    if ( err )
    { err = safe_rw_list_error_initlock; }
    return err;
}