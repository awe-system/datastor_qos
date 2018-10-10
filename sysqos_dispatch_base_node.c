//
// Created by root on 18-7-25.
//

#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include "sysqos_dispatch_base_node.h"

static inline bool is_to_reset(dispatch_base_node_t *desc, long new_ver)
{
    bool is_reset = false;
    sysqos_rwlock_rdlock(&desc->lck);
    if ( new_ver < desc->version )
    {
        is_reset = true;
    }
    sysqos_rwlock_rdunlock(&desc->lck);
    return is_reset;
}

static inline bool is_version_same(dispatch_base_node_t *desc, long new_ver)
{
    bool is_same = false;
    sysqos_rwlock_rdlock(&desc->lck);
    if ( new_ver - desc->version == 0 )
    {
        is_same = true;
    }
    sysqos_rwlock_rdunlock(&desc->lck);
    return is_same;
}


static inline void update_quota_target_force(dispatch_base_node_t *base_node)
{
    //decrease only
    if ( base_node->token_inuse - base_node->respond_step <
         base_node->token_quota_new )
    {
        base_node->token_quota_target = base_node->token_quota_new;
    }
    else
    {
        base_node->token_quota_target = MIN(base_node->token_quota_target,
                                            MAX(base_node->token_quota_new,
                                                base_node->token_quota_target
                                                - base_node->respond_step));
    }
}

static inline void update_quota_target(dispatch_base_node_t *base_node)
{
    //decrease only
    if ( base_node->token_inuse - base_node->respond_step <
         base_node->token_quota_new )
    {
        base_node->token_quota_target = base_node->token_quota_new;
    }
    else
    {
        base_node->token_quota_target = MIN(base_node->token_quota_target,
                                            MAX(base_node->token_quota_new,
                                                base_node->token_inuse +
                                                base_node->respond_step));
    }
}

static void update_quota(dispatch_base_node_t *base_node)
{
    //decrease only
    
    if ( base_node->token_quota_target == base_node->token_quota_new )
    {
        base_node->token_quota = MIN(base_node->token_quota,
                                     MAX(base_node->token_inuse,
                                         base_node->token_quota_new));
    }
    else
    {
        base_node->token_quota = MIN(base_node->token_quota,
                                     MAX(base_node->token_inuse,
                                         base_node->token_quota_target +
                                         base_node->respond_step));
    }
}

static bool check_update_total_version(dispatch_base_node_t *base_node,
                                       unsigned long new_total,
                                       long new_ver, bool *is_reset)
{
    bool is_resource_increased = false;
    assert(is_reset);
    
    *is_reset = is_to_reset(base_node, new_ver);
    if ( *is_reset )
    {
        return false;
    }
    
    if ( is_version_same(base_node, new_ver) )
    {
        return false;
    }
    
    sysqos_rwlock_wrlock(&base_node->lck);
    if ( new_ver - base_node->version > 0 )
    {
        if ( base_node->token_quota < new_total )
        {
            is_resource_increased = true;
            base_node->token_quota        = new_total;
            base_node->token_quota_new    = new_total;
            base_node->token_quota_target = new_total;
        }
        else if ( base_node->token_quota > new_total )
        {
            base_node->token_quota_new = new_total;
            update_quota_target_force(base_node);
            update_quota(base_node);
        }
        base_node->version = new_ver;
    }
    sysqos_rwlock_wrunlock(&base_node->lck);
    
    return is_resource_increased;
}

static int check_alloc_from_base(dispatch_base_node_t *desc, long cost)
{
    sysqos_rwlock_rdlock(&desc->lck);
    if ( cost + desc->token_inuse > desc->token_quota_target )
    {
        sysqos_rwlock_rdunlock(&desc->lck);
        return QOS_ERROR_PENDING;
    }
    sysqos_rwlock_rdunlock(&desc->lck);
    return QOS_ERROR_OK;
}


static int try_alloc_from_desc(dispatch_base_node_t *desc, long cost)
{
    int err = check_alloc_from_base(desc, cost);
    if ( err )
    {
        return err;
    }
    
    sysqos_rwlock_wrlock(&desc->lck);
    if ( cost + desc->token_inuse <= desc->token_quota_target )
    {
        update_quota_target_force(desc);
        desc->token_inuse += cost;
//        printf(BLUE"try_alloc_from_desc cost = %li token_inuse = %lu\n", cost,
//               desc->token_inuse);
    }
    sysqos_rwlock_wrunlock(&desc->lck);
    return QOS_ERROR_OK;
}

//返回剧当前是否能得到申请配额
static bool free_to_base(dispatch_base_node_t *desc, long cost)
{
    bool could_alloc = false;
    sysqos_rwlock_wrlock(&desc->lck);
    assert(desc->token_inuse >= cost);
    desc->token_inuse -= cost;
//    printf(RED"free_to_base cost = %li token_inuse = %lu\n", cost,
//           desc->token_inuse);
    
    update_quota_target(desc);
    if ( desc->token_quota_target + desc->respond_step < desc->token_quota
         || desc->token_quota_target == desc->token_quota_new )
    {
        update_quota(desc);
    }
    
    if ( desc->token_inuse < desc->token_quota_target )
    {
        could_alloc = true;
    }
    sysqos_rwlock_wrunlock(&desc->lck);
    return could_alloc;
}

static long get_currentversion(dispatch_base_node_t *desc)
{
    long res = 0;
    sysqos_rwlock_rdlock(&desc->lck);
    res = desc->version;
    sysqos_rwlock_rdunlock(&desc->lck);
    return res;
}


static unsigned long get_alloced(dispatch_base_node_t *desc)
{
    unsigned long res = 0;
    sysqos_rwlock_rdlock(&desc->lck);
    res = desc->token_inuse;
    sysqos_rwlock_rdunlock(&desc->lck);
    return res;
}

static void reset(struct dispatch_base_node *desc)
{
    sysqos_rwlock_wrlock(&desc->lck);
    desc->token_quota        = MIN_RS_NUM;
    desc->token_quota_target = MIN_RS_NUM;
    desc->token_quota_new    = MIN_RS_NUM;
    desc->version            = 0;
    sysqos_rwlock_wrunlock(&desc->lck);
}

int dispatch_base_node_init(dispatch_base_node_t *base_node)
{
    base_node->token_inuse                = 0;
    base_node->version                    = 0;
    base_node->token_quota                = MIN_RS_NUM;
    base_node->token_quota_target         = MIN_RS_NUM;
    base_node->token_quota_new            = MIN_RS_NUM;
    base_node->respond_step               = MMAX_RESPOND_STEP;
    base_node->get_version                = get_currentversion;
    base_node->get_token_inuse            = get_alloced;
    base_node->check_alloc_from_base      = check_alloc_from_base;
    base_node->free_to_base               = free_to_base;
    base_node->try_alloc_from_base        = try_alloc_from_desc;
    base_node->check_update_quota_version = check_update_total_version;
    base_node->reset                      = reset;
    return sysqos_rwlock_init(&base_node->lck);
}

void dispatch_base_node_exit(dispatch_base_node_t *base_node)
{
    sysqos_rwlock_destroy(&base_node->lck);
}

//
//static int
//check_alloc_resource(struct disp_desc_manager *manager, resource_t *rs)
//{
//    int  err   = QOS_ERROR_OK;
//    void *desc = NULL;
//    assert(manager && rs && rs->cost);
//    sysqos_rwlock_rdlock(&manager->lck);
//    err = manager->tab->find(manager->tab, rs->id, &desc);
//    if ( err )
//        end_func(err, QOS_ERROR_FAILEDNODE, unlock_manager);
//    assert(desc);
//
//    err = check_alloc_from_base(desc, rs->cost);
//    if ( err )
//        end_func(err, QOS_ERROR_PENDING, unlock_manager);
//unlock_manager:
//    sysqos_rwlock_rdunlock(&manager->lck);
//    return err;
//}
