//
// Created by root on 18-7-29.
//

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>
#include "sysqos_token_reqgrp.h"
#include "list_head.h"
#include "memory_cache.h"
#include "sysqos_interface.h"
#include "sysqos_common.h"

static void free_token(token_req_t *resource,
                       memory_cache_t *resource_cache)
{
    assert(resource && resource_cache);
    resource_cache->free(resource_cache, resource);
}

static token_req_t *
alloc_token(token_reqgrp_t *permission,
            memory_cache_t *resource_cache,
            resource_t *rs)
{
    token_req_t *resource = NULL;
    assert(permission && resource_cache && rs);
    
    resource =
            (token_req_t *) resource_cache->alloc(resource_cache);
    if ( !resource )
    {
        return NULL;
    }
    LISTHEAD_INIT(&resource->list_reqgrp);
    resource->token_grp  = permission;
    resource->stat       = app_token_stat_init;
    resource->token_need.rs.cost = rs->cost;
    resource->token_need.rs.id   = rs->id;
    LISTHEAD_INIT(&resource->token_need.list);
    return resource;
}

bool _to_got(token_req_t *rip)
{
    return __sync_bool_compare_and_swap(&rip->stat,
                                        app_token_stat_init,
                                        app_token_stat_got);
    
}

bool _to_fail(token_req_t *rip)
{
    return __sync_bool_compare_and_swap(&rip->stat,
                                        app_token_stat_init,
                                        app_token_stat_failed);
}

/**********************************************************************************************************/

/**********************************************************************************************************/
static int to_got(token_reqgrp_t *token_grp, token_req_t *rip)
{
    int err = QOS_ERROR_PENDING;
    assert(token_grp && rip);
    if ( _to_got(rip) && 0 == __sync_sub_and_fetch(&token_grp->req_pending_nr, 1) )
    {
        if ( token_grp->failed_exist )
        {
            token_grp->err = err = QOS_ERROR_FAILEDNODE;
        }
        else
        {
            token_grp->err = err = QOS_ERROR_OK;
        }
    }
    return err;
}

static int to_failed(token_reqgrp_t *token_grp,
                     token_req_t *rip)
{
    int err = QOS_ERROR_PENDING;
    assert(token_grp && rip);
    token_grp->failed_exist = true;
    if ( _to_fail(rip) && 0 == __sync_sub_and_fetch(&token_grp->req_pending_nr, 1) )
    {
        token_grp->err = err = QOS_ERROR_FAILEDNODE;
    }
    return err;
}

/********************************************************************************************************/

static void clear_token_grp_resource_from_list(struct list_head *head,
                                               memory_cache_t *resource_cache)
{
    struct list_head *pos = NULL;
    struct list_head *tmp = NULL;
    list_for_each_safe(pos, tmp, head)
    {
        token_req_t
                *resource = container_of(pos, token_req_t, list_reqgrp);
        list_del(pos);
        assert(resource);
        free_token(resource, resource_cache);
    }
}

static int init_rip_list_by_rs_list(token_reqgrp_t *token_grp,
                                    memory_cache_t *resource_cache,
                                    struct list_head *rs_list)
{
    int              err  = QOS_ERROR_OK;
    struct list_head *pos = NULL;
    struct list_head *tmp = NULL;
    list_for_each_safe(pos, tmp, rs_list)
    {
        resource_list_t *rs = container_of(pos, resource_list_t, list);
        assert(rs);
        token_req_t *resource = alloc_token(
                token_grp, resource_cache, &rs->rs);
        if ( !resource )
            end_func(err, QOS_ERROR_MEMORY, alloc_resource_failed);
        
        list_add(&resource->list_reqgrp, &token_grp->lhead_reqgrp);
        ++token_grp->req_total_nr;
        ++token_grp->req_pending_nr;
    }
alloc_resource_failed:
    return err;
}

static inline void fill_token_grp_func(token_reqgrp_t *token_grp)
{
    token_grp->to_got    = to_got;
    token_grp->to_failed = to_failed;
}

static inline void fill_normal_val(token_reqgrp_t *token_grp)
{
    LISTHEAD_INIT(&token_grp->lhead_reqgrp);
    LISTHEAD_INIT(&token_grp->lhead_reqgrep_got);
    
    token_grp->err          = QOS_ERROR_PENDING;
    token_grp->failed_exist = false;
    token_grp->req_total_nr       = 0;
    token_grp->req_pending_nr     = 0;
    
}

/************************************************************************************/

static void clear_token_grp_resource(token_reqgrp_t *token_grp,
                                     memory_cache_t *resource_cache)
{
    clear_token_grp_resource_from_list(&token_grp->lhead_reqgrp,
                                       resource_cache);
}

int token_reqgrp_init(token_reqgrp_t *token_grp, memory_cache_t *resource_cache,
                      struct list_head *rs_list, void *pri)
{
    int err = QOS_ERROR_OK;
    assert(token_grp && resource_cache && rs_list);
    token_grp->pri = pri;
    fill_normal_val(token_grp);
    fill_token_grp_func(token_grp);
    
    err = init_rip_list_by_rs_list(token_grp, resource_cache, rs_list);
    if ( err )
    {
        goto alloc_resource_failed;
    }
    return QOS_ERROR_OK;
alloc_resource_failed:
    clear_token_grp_resource(token_grp, resource_cache);
    return err;
}

void token_reqgrp_exit(token_reqgrp_t *token_grp,
                       memory_cache_t *resource_cache)
{
    assert(token_grp && resource_cache);
    clear_token_grp_resource(token_grp, resource_cache);
}
