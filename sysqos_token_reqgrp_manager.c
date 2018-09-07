//
// Created by GaoHualong on 2018/7/9.
//

#include <assert.h>
#include <stddef.h>
#include <pthread.h>
#include <string.h>
#include "sysqos_token_reqgrp_manager.h"
#include "sysqos_dispatch_node.h"

/*******************************************************************************************************************/
static inline void check_snd_msg(token_reqgrp_manager_t *manager,
                                 app2dispatch_t *atd)
{
    if ( manager->msg_ops->snd_msg )
    {
        manager->msg_ops->snd_msg(manager->msg_ops, sizeof(app2dispatch_t),
                                  (unsigned char *) &atd);
    }
}

static inline void end_token_grps(token_reqgrp_manager_t *manager,
                                  struct list_head *permission_list)
{
    struct list_head *pos = NULL;
    struct list_head *tmp = NULL;
    list_for_each_safe(pos, tmp, permission_list)
    {
        token_reqgrp_t *token_grp = container_of(pos, token_reqgrp_t,
                                                  lhead_reqgrep_got);
        manager->app_ops->token_grp_got(token_grp, token_grp->pri,
                                        token_grp->err);
    }
}

/*******************************************************************************************************************/

static int
try_to_alloc_from_manager_nolock(struct token_reqgrp_manager *manager,
                                 token_reqgrp_t *token_grp,
                                 token_req_t *rip)
{
    void *item = NULL;
    int  err   = QOS_ERROR_OK;
    assert(manager && rip);
    
    err = manager->app_node_table->find(manager->app_node_table, rip->token_need.rs.id, &item);
    if ( err )
    {
        return token_grp->to_failed(token_grp, rip);
    }
    
    err = ((dispatch_node_t *) item)->alloc_resource(item, &rip->token_need);
    return err;
}

static void try_to_free_to_manager_nolock(struct token_reqgrp_manager *manager,
                                          resource_list_t *rs,
                                          struct list_head *relative_token_grp_list)
{
    void *item = NULL;
    int  err   = QOS_ERROR_OK;
    assert(manager && rs);
    
    err = manager->app_node_table->find(manager->app_node_table, rs->rs.id, &item);
    if ( err )
    {
        return;
    }
    
    ((dispatch_node_t *) item)
            ->free_resource(item, rs, relative_token_grp_list);
}

/*******************************************************************************************************************/
static inline token_reqgrp_manager_t *
token_grp_manager_by_msg_event(struct msg_event_ops *ops)
{
    assert(ops);
    return container_of(ops, token_reqgrp_manager_t, msg_event);
}

static int try_to_get_tokens(struct token_reqgrp_manager *manager,
                             token_reqgrp_t *token_grp)
{
    int                      err   = QOS_ERROR_OK;
    struct list_head         *pos  = NULL;
    struct list_head         *tmp  = NULL;
    struct list_head         to_fail_wait_list;
    dispatch_node_t *item = NULL;
    LISTHEAD_INIT(&to_fail_wait_list);
    
    list_for_each_safe(pos, tmp, &token_grp->lhead_reqgrp)
    {
        token_req_t
                        *rip = app_token_by_grp_list(pos);
        resource_list_t *rs  = &rip->token_need;
        
        err = try_to_alloc_from_manager_nolock(manager, token_grp, rip);
    }
    return err;
}

static void try_to_put_tokens(struct token_reqgrp_manager *manager,
                              token_reqgrp_t *token_grp,
                              struct list_head *relative_token_grp_list)
{
    int              err  = QOS_ERROR_OK;
    struct list_head *pos = NULL;
    struct list_head *tmp = NULL;
    
    list_for_each_safe(pos, tmp, &token_grp->lhead_reqgrp)
    {
        token_req_t
                *rip = app_token_by_grp_list(pos);
        if ( rip->stat == app_token_stat_got )
        {
            resource_list_t *rs = &rip->token_need;
            try_to_free_to_manager_nolock(manager, rs,
                                          relative_token_grp_list);
        }
    }
}

static void node_online(struct msg_event_ops *ops, void *id)
{
    token_reqgrp_manager_t      *manager = token_grp_manager_by_msg_event(ops);
    dispatch_node_t *item    = NULL;
    long                     atd_len  = sizeof(app2dispatch_t);
    app2dispatch_t            atd;
    init_app2dispatch(&atd);
    int err = QOS_ERROR_OK;
    
    item = (dispatch_node_t *) manager->manager_item_cache
            .alloc(&manager->manager_item_cache);
    if ( !item )
    {
        goto manager_item_alloc_failed;
    }
    err = dispatch_node_init(item,
                             __sync_fetch_and_add(&manager->version,
                                                  1));
    if ( err )
    {
        goto token_grp_manager_item_init_failed;
    }
    
    atd.press.val = 0;
    atd.version       = 0;
    
    pthread_rwlock_wrlock(&manager->lck);
    do
    {
        err = manager->app_node_table->insert(manager->app_node_table, id, item);
        if ( err )
        {
            break;
        }
        //        item->get_protocol(item,&atd);
        //        assert(atd_len == sizeof(atd));
    } while ( 0 );
    pthread_rwlock_unlock(&manager->lck);
    if ( err )
    {
        goto insert_failed;
    }
    //    check_snd_msg(manager,&atd);
    return;
insert_failed:
    dispatch_node_exit(item);
token_grp_manager_item_init_failed:
    manager->manager_item_cache.free(&manager->manager_item_cache, item);
manager_item_alloc_failed:
    return;
}


static void node_offline(struct msg_event_ops *ops, void *id)
{
    token_reqgrp_manager_t *manager = token_grp_manager_by_msg_event(ops);
    int                 err      = QOS_ERROR_OK;
    void                *item    = NULL;
    
    struct list_head relative_token_grps;
    LISTHEAD_INIT(&relative_token_grps);
    
    pthread_rwlock_wrlock(&manager->lck);
    do
    {
        err = manager->app_node_table->find(manager->app_node_table, id, &item);
        if ( err )
        {
            break;
        }
        ((dispatch_node_t *) item)
                ->pop_all(item, &relative_token_grps);
        
        err = manager->app_node_table->erase(manager->app_node_table, id, &item);
        assert(err == 0);
    } while ( 0 );
    pthread_rwlock_unlock(&manager->lck);
    if ( err == 0 )
    {
        dispatch_node_exit(item);
        manager->manager_item_cache.free(&manager->manager_item_cache, item);
    }
    end_token_grps(manager, &relative_token_grps);
}

static void node_reset(struct msg_event_ops *ops, void *id)
{
    token_reqgrp_manager_t *manager = token_grp_manager_by_msg_event(ops);
    void                *item    = NULL;
    int                 err      = QOS_ERROR_OK;
    pthread_rwlock_wrlock(&manager->lck);
    do
    {
        err = manager->app_node_table->find(manager->app_node_table, id, &item);
        if ( err )
        {
            break;
        }
        ((dispatch_node_t *) item)
                ->reset((dispatch_node_t *) item);
    } while ( 0 );
    pthread_rwlock_unlock(&manager->lck);
}

#define RCVD_FAIL_TIMES 2

static void rcvd(struct msg_event_ops *ops,
                 void *id, long len, unsigned char *buf)
{
    int                 err_cnt  = RCVD_FAIL_TIMES;
    int                 err      = QOS_ERROR_OK;
    token_reqgrp_manager_t *manager = token_grp_manager_by_msg_event(ops);
    void                *item    = NULL;
    bool                is_reset = false;
    dispatch2app_t       dta;
    struct list_head    relative_token_grps;
    LISTHEAD_INIT(&relative_token_grps);
    
    assert(ops && sizeof(dispatch2app_t) == len);
    memcpy(&dta, buf, len);
    do
    {
        pthread_rwlock_rdlock(&manager->lck);
        do
        {
            err = manager->app_node_table->find(manager->app_node_table, id, &item);
            if ( err )
            {
                break;
            }
            is_reset = ((dispatch_node_t *) item)
                    ->resource_changed(item, &dta, &relative_token_grps);
        } while ( 0 );
        pthread_rwlock_unlock(&manager->lck);
        
        if ( err && --err_cnt > 0 )
        {
            ops->node_online(ops, id);
            continue;
        }
    } while ( 0 );
    
    
    if ( is_reset )
    {
        ops->node_reset(ops, id);
    }
    end_token_grps(manager, &relative_token_grps);
}

static void check_node_in_when_get(struct token_reqgrp_manager *manager,
                                   struct list_head *phead)
{
    struct list_head *pos      = NULL;
    struct list_head *tmp_list = NULL;
    assert(manager && phead);
    void *tmp = NULL;
    
    list_for_each_safe(pos, tmp_list, phead)
    {
        resource_list_t *rs = container_of(pos, resource_list_t, list);
        int             err = manager->app_node_table->find(manager->app_node_table, rs->rs.id, &tmp);
        if ( err )
        {
            manager->msg_event.node_online(&manager->msg_event, rs->rs.id);
        }
    }
}

static int get_token_grp(struct token_reqgrp_manager *manager,
                         resource_list_t *rs,
                         void *pri,
                         token_reqgrp_t **pp_token_grp)
{
    int              err = QOS_ERROR_OK;
    struct list_head head;
    struct list_head tmp_got_list;
    struct list_head relative_token_grp;
    assert(manager && rs && pp_token_grp);
    
    LISTHEAD_INIT(&head);
    LISTHEAD_INIT(&tmp_got_list);
    LISTHEAD_INIT(&relative_token_grp);
    
    list_add(&head, &rs->list);
    *pp_token_grp = (token_reqgrp_t *) manager->token_grp_cache
            .alloc(&manager->token_grp_cache);
    if ( !*pp_token_grp )
        end_func(err, QOS_ERROR_MEMORY, token_grp_cache_alloc_failed);
    
    err = token_reqgrp_init(*pp_token_grp, &manager->resource_cache, &head, pri);
    
    if ( err )
        end_func(err, QOS_ERROR_MEMORY, permission_init_failed);
    
    check_node_in_when_get(manager, &head);
    
    pthread_rwlock_rdlock(&manager->lck);
    err = try_to_get_tokens(manager, *pp_token_grp);
    pthread_rwlock_unlock(&manager->lck);
    
    
    if ( err && err != QOS_ERROR_PENDING )
    {
        manager->put_token_reqgrp(manager, *pp_token_grp, NULL);
    }
    
    list_del(&head);
    return err;
    //try_alloc_permision_failed:
    //    token_reqgrp_exit(*pp_token_grp,&manager->resource_cache);
permission_init_failed:
    manager->token_grp_cache.free(&manager->token_grp_cache, *pp_token_grp);
token_grp_cache_alloc_failed:
    list_del(&head);
    return err;
}


static void check_node_out_when_put(struct token_reqgrp_manager *manager,
                                    struct list_head *phead)
{
    struct list_head *pos      = NULL;
    struct list_head *tmp_list = NULL;
    assert(manager && phead);
    void *tmp = NULL;
    
    list_for_each_safe(pos, tmp_list, phead)
    {
        resource_list_t *rs = container_of(pos, resource_list_t, list);
        int             err = manager->app_node_table->find(manager->app_node_table, rs->rs.id, &tmp);
        if ( !err )
        {
            manager->msg_event.node_offline(&manager->msg_event, rs->rs.id);
        }
    }
}


static void put_token_grp(struct token_reqgrp_manager *manager,
                          token_reqgrp_t *p_token_grp,
                          resource_list_t *fail_rs)
{
    struct list_head fail_head;
    struct list_head relative_token_grp_list;
    
    LISTHEAD_INIT(&relative_token_grp_list);
    LISTHEAD_INIT(&fail_head);
    if ( fail_rs )
    {
        list_add(&fail_head, &fail_rs->list);
    }
    check_node_out_when_put(manager, &fail_head);
    
    pthread_rwlock_rdlock(&manager->lck);
    try_to_put_tokens(manager, p_token_grp, &relative_token_grp_list);
    pthread_rwlock_unlock(&manager->lck);
    end_token_grps(manager, &relative_token_grp_list);
    
    token_reqgrp_exit(p_token_grp, &manager->resource_cache);
    manager->token_grp_cache.free(&manager->token_grp_cache, p_token_grp);
}

static long snd_msg_len(struct msg_event_ops *ops, void *id)
{
    return sizeof(app2dispatch_t);
}

static int snd_msg_buf(struct msg_event_ops *ops, void *id,
                       long len, unsigned char *buf)
{
    int                 err      = QOS_ERROR_OK;
    app2dispatch_t       atd;
    void                *item    = NULL;
    token_reqgrp_manager_t *manager = token_grp_manager_by_msg_event(ops);
    
    assert(ops && len == sizeof(app2dispatch_t) && buf);
    
    pthread_rwlock_rdlock(&manager->lck);
    err = manager->app_node_table->find(manager->app_node_table, id, &item);
    if ( err )
        end_func(err, QOS_ERROR_FAILEDNODE, find_failed);
    ((dispatch_node_t *) item)
            ->get_protocol((dispatch_node_t *) item, &atd);
    memcpy(buf, &atd, len);

find_failed:
    pthread_rwlock_unlock(&manager->lck);
    return err;
}

void set_app_event_ops(struct token_reqgrp_manager *manager,
                       applicant_event_ops_t *ops)
{
    assert(manager && ops);
    manager->app_ops = ops;
}

void set_msg_ops(struct token_reqgrp_manager *manager, msg_ops_t *ops)
{
    assert(manager && ops);
    manager->msg_ops = ops;//用作snd_msg
    manager->app_node_table->set_hash(manager->app_node_table, ops->hash_id);
    manager->app_node_table->set_compare(manager->app_node_table, ops->compare);
}

int token_reqgrp_manager_init(token_reqgrp_manager_t *manager, int tab_len,
                              int max_token_grp_num, int max_resource_num,
                              int max_node_num)
{
    int err = QOS_ERROR_OK;
    manager->app_node_table = alloc_hash_table(tab_len, max_node_num);
    if ( !manager->app_node_table )
        end_func(err, QOS_ERROR_MEMORY, alloc_hash_table_failed);
    err = memory_cache_init(&manager->manager_item_cache,
                            sizeof(dispatch_node_t), max_node_num);
    if ( err )
        end_func(err, QOS_ERROR_MEMORY, manager_item_cache_init_failed);
    err = memory_cache_init(&manager->token_grp_cache, sizeof(token_reqgrp_t),
                            max_token_grp_num);
    if ( err )
        end_func(err, QOS_ERROR_MEMORY, token_grp_cache_init_failed);
    err = memory_cache_init(&manager->resource_cache,
                            sizeof(token_req_t), max_resource_num);
    if ( err )
        end_func(err, QOS_ERROR_MEMORY, resource_in_permission_init_failed);
    
    err = pthread_rwlock_init(&manager->lck, NULL);
    if ( err )
        end_func(err, QOS_ERROR_MEMORY, rwlock_init_failed);
    manager->get_token_reqgrp     = get_token_grp;
    manager->put_token_reqgrp     = put_token_grp;
    manager->set_app_event_ops = set_app_event_ops;
    manager->set_msg_ops       = set_msg_ops;
    
    manager->msg_event.rcvd         = rcvd;
    manager->msg_event.node_offline = node_offline;
    manager->msg_event.node_online  = node_online;
    manager->msg_event.node_reset   = node_reset;
    manager->msg_event.snd_msg_buf  = snd_msg_buf;
    manager->msg_event.snd_msg_len  = snd_msg_len;
    
    manager->version = 0;
    
    return err;
    pthread_rwlock_destroy(&manager->lck);
rwlock_init_failed:
    memory_cache_exit(&manager->resource_cache);
resource_in_permission_init_failed:
    memory_cache_exit(&manager->token_grp_cache);
token_grp_cache_init_failed:
    memory_cache_exit(&manager->manager_item_cache);
manager_item_cache_init_failed:
    free_hash_table(manager->app_node_table);
alloc_hash_table_failed:
    return err;
}

void token_reqgrp_manager_exit(token_reqgrp_manager_t *manager)
{
    pthread_rwlock_destroy(&manager->lck);
    memory_cache_exit(&manager->resource_cache);
    memory_cache_exit(&manager->token_grp_cache);
    memory_cache_exit(&manager->manager_item_cache);
    free_hash_table(manager->app_node_table);
}
