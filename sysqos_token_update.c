//
// Created by root on 18-8-11.
//
#include <pthread.h>
#include <assert.h>
#include "list_head.h"
#include "sysqos_token_update.h"
#include "sysqos_dispatch_manager.h"

static inline void
insert_by_weight(struct token_update_ctx *ctx, update_node_t *updatenode)
{
    //TODO:需要对weight去灵敏度时 才会选择排序方式否则直接插入即可
    list_add(&updatenode->list_update, &ctx->lhead_update_node);
    ctx->total_weight += updatenode->weight;
}

static void add_resource(struct token_update_ctx *ctx, app_node_t *node)
{
    update_node_t *updatenode = NULL;
    pthread_rwlock_wrlock(&ctx->lck);
    updatenode = node->init_update_node(node);
    assert(updatenode);
    insert_by_weight(ctx, updatenode);
    ctx->token_total += updatenode->tmp_token_quota;
    ctx->token_static += updatenode->token_base;
    updatenode->tmp_token_quota -=
            pthread_rwlock_unlock(&ctx->lck);
}

static inline unsigned long update_token_left_base(struct token_update_ctx *ctx)
{
    //TODO:暂时采用的是全部 返还
    return ctx->token_total - ctx->token_static;
}

static inline unsigned long update_weight(struct token_update_ctx *ctx)
{
    //TODO:暂时采用press直接作为weight
    return ctx->total_weight;
}

//返回消耗值
static unsigned long update_node_quota(struct list_head *pos,
                                       unsigned long token_total,
                                       unsigned long total_weight,
                                       sysqos_disp_manager_t *manager,
                                       set_quota_new_func set_quota_new)
{
    unsigned long new_real_quota = 0;
    update_node_t *updatenode    = update_node_by_list(pos);
    app_node_t    *node          = app_node_by_update(updatenode);
    assert(total_weight > 0);
    
    updatenode->tmp_token_quota =
            token_total * updatenode->weight / total_weight
            + updatenode->token_base;
    
    new_real_quota = set_quota_new(manager, updatenode->tmp_token_quota, node);
    return new_real_quota - updatenode->token_base;
}

static unsigned long update_node_quota_left(struct list_head *pos,
                                            unsigned long token_left,
                                            sysqos_disp_manager_t *manager,
                                            set_quota_new_func set_quota_new)
{
    unsigned long new_real_quota = 0;
    update_node_t *updatenode = update_node_by_list(pos);
    app_node_t    *node       = app_node_by_update(updatenode);
    unsigned long old_quota   = updatenode->tmp_token_quota;
    
    updatenode->tmp_token_quota += token_left;
    
    new_real_quota = set_quota_new(manager, updatenode->tmp_token_quota, node);
    return new_real_quota - old_quota;
}

static void clear(struct token_update_ctx *ctx)
{
    pthread_rwlock_wrlock(&ctx->lck);
    ctx->total_weight = 0;
    ctx->token_total  = ctx->token_static = 0;
    LISTHEAD_INIT(&ctx->lhead_update_node);
    pthread_rwlock_unlock(&ctx->lck);
}

static void update(struct token_update_ctx *ctx,
                   struct sysqos_disp_manager *manager,
                   set_quota_new_func func)
{
    struct list_head *pos         = NULL;
    unsigned long    token_left   = 0;
    unsigned long    token_total  = 0;
    unsigned long    total_weight = 0;
    
    
    token_total = token_left = update_token_left_base(ctx);
    
    total_weight = update_weight(ctx);
    
    list_for_each(pos, &ctx->lhead_update_node)
    {
        token_left -= update_node_quota(pos, token_total, total_weight, manager,
                                        func);
        if ( token_left <= 0 )
        { break; }
    }
    
    list_for_each(pos, &ctx->lhead_update_node)
    {
        token_left -= update_node_quota_left(pos, token_left, manager, func);
        if ( token_left <= 0 )
        { break; }
    }
    
    clear(ctx);
}

static void token_update_init_val(token_update_ctx_t *ctx/*,
                                  unsigned long max_token_once,
                                  int rebalance_ratio*/)
{
    ctx->total_weight = 0;
    ctx->token_total  = ctx->token_static = 0;
//    ctx->max_token_once  = max_token_once;
//    ctx->rebalance_ratio = rebalance_ratio;
    LISTHEAD_INIT(&ctx->lhead_update_node);
}

static void token_update_init_func(token_update_ctx_t *ctx)
{
    ctx->add_resource = add_resource;
    ctx->update       = update;
}


int token_update_ctx_init(token_update_ctx_t *ctx/*,
                          unsigned long max_token_once,
                          int rebalance_ratio*/)
{
    token_update_init_val(ctx/*, max_token_once, rebalance_ratio*/);
    
    token_update_init_func(ctx);
    
    return pthread_rwlock_init(&ctx->lck, NULL);
    
}

void token_update_ctx_exit(token_update_ctx_t *ctx)
{
    pthread_rwlock_destroy(&ctx->lck);
}
