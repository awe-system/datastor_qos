//
// Created by root on 18-7-29.
//

#include <assert.h>
#include <pthread.h>
#include "sysqos_dispatch_node.h"
#include "sysqos_token_reqgrp.h"

static struct list_head *
permission_list_if_final_by_rs_list(struct list_head *list)
{
    token_reqgrp_t *permission = permission_by_token_list(list);
    token_req_t    *rip        =
                           app_token_by_list(list);
    int            err         = permission->to_got(permission, rip);
    if ( err == QOS_ERROR_FAILEDNODE || err == QOS_ERROR_OK )
    {
        return &permission->lhead_reqgrep_got;
    }
    return NULL;
}

static void resource_increased(dispatch_node_t *item,
                               struct list_head *got_permission_list)
{
    int                  err;
    struct list_head     *permission_list = NULL;
    nodereq_list_t       *tokens          = &item->lhead_nodereq;
    dispatch_base_node_t *desc            = &item->base_node;
    resource_list_t      *rs              = NULL;
    assert(item && got_permission_list);
    
    rs = tokens->front(tokens);
    if ( !rs )
    {
        return;
    }
    
    while ( NULL != (rs = tokens->front(tokens)) )
    {
        err = desc->try_alloc_from_base(desc, rs->rs.cost);
        if ( err )
        {
            break;
        }
        tokens->pop_front(tokens);
        permission_list = permission_list_if_final_by_rs_list(&rs->list);
        if ( permission_list )
        {
            list_add(permission_list, got_permission_list);
        }
    }
}

static bool resource_changed(dispatch_node_t *item,
                             dispatch2app_t *dta,
                             struct list_head *got_permission_list)
{
    bool is_reset = false;
    bool is_increased;
    assert(item && dta && got_permission_list);
    is_increased = item->base_node
            .check_update_quota_version(&item->base_node, dta->token_quota,
                                        dta->version,
                                        &is_reset);
    if ( is_increased )
    {
        resource_increased(item, got_permission_list);
    }
    sysqos_spin_unlock(&item->lck);
    return is_reset;
}

static void reset(dispatch_node_t *item)
{
    item->base_node.reset(&item->base_node);
}

//返回的是获得
static void
free_resource(struct dispatch_node *item, resource_list_t *rs,
              struct list_head *got_permission_list)
{
    token_req_t
            *rip = app_token_by_list(&rs->list);
    assert(item && rs && got_permission_list);
    //NOTE:删除之前的资源数和当前版本的资源数无关
    sysqos_spin_lock(&item->lck);
    if ( rip->Nodeid == item->fence_id )
    {
        
        bool is_increase = item->base_node
                .free_to_base(&item->base_node, rs->rs.cost);
        if ( is_increase )
        {
            resource_increased(item, got_permission_list);
        }
        
    }
    sysqos_spin_unlock(&item->lck);
    
}

//失败则放入队列
static int
alloc_resource(struct dispatch_node *item, resource_list_t *rs)
{
    assert(item && rs);
    int         err;
    token_req_t *rip = app_token_by_list(
            &rs->list);
    rip->Nodeid = item->fence_id;
    err = item->base_node.try_alloc_from_base(&item->base_node, rs->rs.cost);
    if ( err )
    {
        sysqos_spin_lock(&item->lck);
        err = item->base_node
                .try_alloc_from_base(&item->base_node, rs->rs.cost);
        if ( err )
        {
            item->lhead_nodereq.push_back(&item->lhead_nodereq, rs);
        }
        else
        {
            token_reqgrp_t *permission = rip->token_grp;
            err                        = permission->to_got(permission, rip);
        }
        sysqos_spin_unlock(&item->lck);
    }
    else
    {
        token_reqgrp_t *permission = rip->token_grp;
        err = permission->to_got(permission, rip);
    }
    
    
    return err;
}

static void get_protocol(struct dispatch_node *item,
                         app2dispatch_t *atd)
{
    atd->press        = item->lhead_nodereq.get_press(&item->lhead_nodereq);
    atd->version      = item->base_node.get_version(&item->base_node);
    atd->token_in_use = item->base_node.get_token_inuse(&item->base_node);
}

static void pop_all(struct dispatch_node *item,
                    struct list_head *fail_permission_list)
{
    struct list_head *pos = NULL;
    struct list_head *tmp = NULL;
    struct list_head fail_token_list;
    LISTHEAD_INIT(&fail_token_list);
    
    item->lhead_nodereq.pop_all(&item->lhead_nodereq, &fail_token_list);
    list_for_each_safe(pos, tmp, &fail_token_list)
    {
        token_reqgrp_t *permission = permission_by_token_list(pos);
        token_req_t    *rip        =
                               app_token_by_list(
                                       pos);
        
        int err = permission->to_failed(permission, rip);
        if ( err == QOS_ERROR_FAILEDNODE )
        {
            list_add(&permission->lhead_reqgrep_got, fail_permission_list);
        }
    }
}

void dispatch_node_exit(dispatch_node_t *item)
{
    assert(item);
    sysqos_spin_destroy(&item->lck);
    nodereq_list_exit(&item->lhead_nodereq);
    dispatch_base_node_exit(&item->base_node);
}

int dispatch_node_init(dispatch_node_t *item, int version)
{
    int err;
    assert(item);
    
    err = dispatch_base_node_init(&item->base_node);
    if ( err )
    {
        goto disp_desc_init_failed;
    }
    
    err = nodereq_list_init(&item->lhead_nodereq);
    if ( err )
    {
        goto token_list_init_failed;
    }
    
    err = sysqos_spin_init(&item->lck);
    if ( err )
        end_func(err, QOS_ERROR_MEMORY, spin_init_failed);
    
    item->fence_id = version;
    
    item->resource_changed = resource_changed;
    item->free_resource    = free_resource;
    item->alloc_resource   = alloc_resource;
    item->get_protocol     = get_protocol;
    item->pop_all          = pop_all;
    item->reset            = reset;
    
    return err;
//    sysqos_spin_destroy(&item->lck);
spin_init_failed:
    nodereq_list_exit(&item->lhead_nodereq);
token_list_init_failed:
    dispatch_base_node_exit(&item->base_node);
disp_desc_init_failed:
    return err;
}
