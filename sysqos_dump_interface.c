//
// Created by root on 18-9-20.
//

#include <pthread.h>
#include <string.h>
#include "sysqos_dump_interface.h"
#include "sysqos_dispatch_node.h"

void sysqos_dump_app(sysqos_applicant_ops_t *ops, void *disp_id,
                     sysqos_app_dump_info_t *info)
{
    int                    err      = QOS_ERROR_OK;
    dispatch_node_t        *node;
    token_reqgrp_manager_t *manager = manager_by_applicant(ops);
    memset(info, 0, sizeof(sysqos_app_dump_info_t));
    sysqos_rwlock_rdlock(&manager->lck);
    err = manager->app_node_table->find(manager->app_node_table, disp_id,
                                        (void **) &node);
    if ( err )
        end_func(err, QOS_ERROR_FAILEDNODE, find_failed);
    
    sysqos_spin_lock(&node->lck);
    sysqos_rwlock_rdlock(&node->base_node.lck);
    info->token_quota        = node->base_node.token_quota;
    info->token_quota_new    = node->base_node.token_quota_new;
    info->version            = node->base_node.version;
    info->token_inuse        = node->base_node.token_inuse;
    info->token_quota_target = node->base_node.token_quota_target;
    info->respond_step       = node->base_node.respond_step;
    info->press              = node->lhead_nodereq.press;
    sysqos_rwlock_rdunlock(&node->base_node.lck);
    sysqos_spin_unlock(&node->lck);

find_failed:
    sysqos_rwlock_rdunlock(&manager->lck);
}

void sysqos_dump_disp(sysqos_dispatcher_ops_t *ops, void *app_id,
                      sysqos_disp_dump_info_t *info)
{
    int        err = QOS_ERROR_OK;
    app_node_t *node;
    memset(info, 0, sizeof(sysqos_disp_dump_info_t));
    sysqos_disp_manager_t *manager = manager_by_dispatch(ops);
    sysqos_rwlock_rdlock(&manager->lck);
    sysqos_spin_lock(&manager->tokens.lck);
    info->token_used_static = manager->tokens.token_used_static;
    info->token_dynamic     = manager->tokens.token_dynamic;
    sysqos_spin_unlock(&manager->tokens.lck);
    err = manager->tab->find(manager->tab, app_id, (void **) &node);
    if ( err )
        end_func(err, QOS_ERROR_FAILEDNODE, find_failed);
    sysqos_spin_lock(&node->lck);
    info->token_quota_new = node->token_quota_new;
    info->min             = node->limit.min;
    info->token_quota     = node->token_quota;
    info->version         = node->version;
    info->press           = node->press;
    sysqos_spin_unlock(&node->lck);
find_failed:
    sysqos_rwlock_rdunlock(&manager->lck);
}
