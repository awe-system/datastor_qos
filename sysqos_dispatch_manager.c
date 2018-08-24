//
// Created by root on 18-8-11.
//

#include <pthread.h>
#include <assert.h>
#include <memory.h>
#include "sysqos_dispatch_manager.h"
#include "sysqos_app_node.h"
#include "sysqos_token_update.h"

static void try_fill_alloc_items(struct sysqos_disp_manager *manager)
{
    app_node_t *node  = NULL;
    while (manager->lhead_wait_increase.is_first_exist(&manager->lhead_wait_increase,&node))
    {
       // int err = manager->tokens.try_alloc(&manager->tokens,)
        manager->lhead_wait_increase.dequeue(&manager->lhead_wait_increase);
        
    }
//    if(manager->tokens.try_alloc)
    //FIXME
    //从free中将数据填充给带申请的资源队列
}

static void fill_resource(void *ctx,void *id,void *pri)
{
    limit_t limit;
    resource_t rs;
    app_node_t *node = pri;
    token_update_ctx_t *pctx = ctx;
    assert(ctx && id && pri);
    rs.id = id;
    rs.cost = node->get_zonetotal(node);
    node->get_limit(node,&limit);
    pctx->add_resource(pctx,&rs,node->get_press(node),&limit);
}

static void enqueue(void *pri, struct app_node *node)
{
    sysqos_disp_manager_t *manager  = pri;
    manager->lhead_wait_increase.enqueue(&manager->lhead_wait_increase,node);
}

static void set_tototal(void *pri,resource_t *rs)
{
    int err = QOS_ERROR_OK;
    void *node_val = NULL;
    unsigned long need_cost;
    app_node_t *node = NULL;
    sysqos_disp_manager_t *manager  = pri;
    assert(pri && rs);
    
    err = manager->tab->find(manager->tab, rs->id, &node_val);
    if ( err ) return;
    node = node_val;
    
    node->update_tototal(node,rs->cost,manager,enqueue);
}

static void update_nodes_tototal(fence_executor_t *executor, void *ctx)
{
    token_update_ctx_t *pctx = ctx;
    sysqos_disp_manager_t *manager =  NULL;
    assert(executor && ctx);
    
    manager = container_of(executor, sysqos_disp_manager_t,executor);
    
    pthread_rwlock_rdlock(&manager->lck);
    manager->tab->for_each_do(manager->tab,pctx,fill_resource);
    pthread_rwlock_unlock(&manager->lck);
    
    pctx->update(pctx);
    
    //遍历设置tototal
    pthread_rwlock_rdlock(&manager->lck);
    pctx->for_each_do(pctx,manager,set_tototal);
    pthread_rwlock_unlock(&manager->lck);
    
    pctx->reset(pctx);
}

static void update_policy(struct sysqos_disp_manager *manager, bool is_direct)
{
    if ( is_direct )
    {
        manager->cnt_controller.clear(&manager->cnt_controller);
    }
    else if ( !manager->cnt_controller.test_cnt(&manager->cnt_controller) )
    {
        return;
    }
    
    manager->executor.execute(&manager->executor,&manager->update_ctx);
    
    try_fill_alloc_items(manager);
}

static void check_online(struct sysqos_disp_manager *manager, void *id)
{
    void *node_val = NULL;
    int  err       = manager->tab->find(manager->tab, id, &node_val);
    if ( err )
    {
        manager->msg_event.node_online(&manager->msg_event, id);
    }
}

static int alloc_tokens(struct sysqos_disp_manager *manager,
                        resource_t *rs, long *fence_id)
{
    void              *node_val;
    app_node_t *node;
    int               err = QOS_ERROR_OK;
    assert(manager && rs && fence_id);
    
    check_online(manager, rs->id);
    
    pthread_rwlock_rdlock(&manager->lck);
    err = manager->tab->find(manager->tab, rs->id, &node_val);
    if ( err ) end_func(err, QOS_ERROR_FAILEDNODE, unlock_end);
    node = node_val;
    err  = node->alloc(node, rs, fence_id);
unlock_end:
    pthread_rwlock_unlock(&manager->lck);
    return err;
}

static void free_tokens(struct sysqos_disp_manager *manager,
                        resource_t *rs, long fence_id,
                        bool is_fail)
{
    void              *node_val;
    app_node_t *node;
    int               err = QOS_ERROR_OK;
    assert(manager && rs);
    
    if ( is_fail )
    {
        manager->msg_event.node_offline(&manager->msg_event, rs->id);
        return;
    }
    
    pthread_rwlock_rdlock(&manager->lck);
    err = manager->tab->find(manager->tab, rs->id, &node_val);
    if ( err ) end_func(err, QOS_ERROR_FAILEDNODE, unlock_end);
    node = node_val;
    node->free(node, rs, fence_id);
unlock_end:
    pthread_rwlock_unlock(&manager->lck);
}

static void resource_increase(struct sysqos_disp_manager *manager,
                              unsigned long cost)
{
    pthread_rwlock_rdlock(&manager->lck);
    manager->tokens.increase(&manager->tokens, cost);
    pthread_rwlock_unlock(&manager->lck);
    update_policy(manager, true);
}

static void resource_reduce(struct sysqos_disp_manager *manager, long cost)
{
    int err = QOS_ERROR_OK;
    pthread_rwlock_rdlock(&manager->lck);
    err = manager->tokens.try_decrease(&manager->tokens, cost);
    assert(err == QOS_ERROR_OK);
    pthread_rwlock_unlock(&manager->lck);
    update_policy(manager, true);
}

static void set_dispatcher_event_ops(struct sysqos_disp_manager *manager,
                                     dispatcher_event_ops_t *ops)
{
    manager->disp_ops = ops;
}

static void set_msg_ops(struct sysqos_disp_manager *manager, msg_ops_t *ops)
{
    manager->msg_ops = ops;
}

static inline sysqos_disp_manager_t *manager_by_msg(msg_event_ops_t *ops)
{
    return container_of(ops, sysqos_disp_manager_t, msg_event);
}

//NOTE:重置version暂时以达到让对方重置从而驱动自己重置的结果
static void node_reset(struct msg_event_ops *ops, void *id)
{
    void              *node_val;
    app_node_t *node;
    int               err = QOS_ERROR_OK;
    assert(ops);
    
    sysqos_disp_manager_t *manager = manager_by_msg(ops);
    pthread_rwlock_rdlock(&manager->lck);
    err = manager->tab->find(manager->tab, id, &node_val);
    if ( err ) end_func(err, QOS_ERROR_FAILEDNODE, unlock_end);
    node = node_val;
    node->reset(node);
unlock_end:
    pthread_rwlock_unlock(&manager->lck);
    
    update_policy(manager, true);
}

static void node_online(struct msg_event_ops *ops, void *id)
{
    void                  *node_val;
    int                   err      = QOS_ERROR_OK;
    sysqos_disp_manager_t *manager = manager_by_msg(ops);
    app_node_t     *node    = NULL;
    assert(ops);
    
    node = manager->token_node_cache.alloc(&manager->token_node_cache);
    if ( !node )
    { goto token_node_cache_alloc_failed; }
    
    err = app_node_init(node, manager->default_token_min,
                        __sync_fetch_and_add(&manager->fence_id, 1));
    if ( err )
    {
        goto disp_token_node_init_failed;
    }
    
    pthread_rwlock_wrlock(&manager->lck);
    
    err = manager->tab->find(manager->tab, id, &node_val);
    if ( err == QOS_ERROR_OK )
    {
        goto find_node_failed;
    }
    
    err = manager->tokens.try_online(&manager->tokens);
    if ( err )
    {
        goto left_tokens_try_online_failed;
    }
    
    err = manager->tab->insert(manager->tab, id, node);
    if ( err )
    {
        goto insert_node_failed;
    }
    
    pthread_rwlock_unlock(&manager->lck);
    
    update_policy(manager, true);
    
    return;
    manager->tab->erase(manager->tab, id, (void **) &node);
insert_node_failed:
    manager->tokens.offline(&manager->tokens);
left_tokens_try_online_failed:
find_node_failed:
    pthread_rwlock_unlock(&manager->lck);
    
    app_node_exit(node);
disp_token_node_init_failed:
    manager->token_node_cache.free(&manager->token_node_cache, node);
token_node_cache_alloc_failed:
    return;
}

static void check_erase_from_alloc_item(sysqos_disp_manager_t *manager,
                                        app_node_t *node)
{
    manager->lhead_wait_increase.erase(&manager->lhead_wait_increase, node);
    
}

static void node_offline(struct msg_event_ops *ops, void *id)
{
    void *node_val = NULL;
    
    int                   err      = QOS_ERROR_OK;
    app_node_t     *node    = NULL;
    sysqos_disp_manager_t *manager = manager_by_msg(ops);
    assert(ops);
    
    pthread_rwlock_wrlock(&manager->lck);
    
    err = manager->tab->erase(manager->tab, id, node_val);
    if ( err )
    {
        goto erase_node_failed;
    }
    node = node_val;
    check_erase_from_alloc_item(manager, node);
    
    manager->tokens.offline(&manager->tokens);
    
    pthread_rwlock_unlock(&manager->lck);
    
    app_node_exit(node);
    manager->token_node_cache.free(&manager->token_node_cache, node);
    
    update_policy(manager, true);
    
    return;
erase_node_failed:
    pthread_rwlock_unlock(&manager->lck);
    return;
}

static void rcvd(struct msg_event_ops *ops, void *id,
                 long len, unsigned char *buf)
{
    bool                  is_reset      = false;
    void                  *node_val;
    int                   err           = QOS_ERROR_OK;
    sysqos_disp_manager_t *manager      = manager_by_msg(ops);
    app_node_t     *node         = NULL;
    app2dispatch_t        a2d;
    unsigned long         new_free_size = 0;
    assert(ops && len == sizeof(app2dispatch_t) && buf);
    
    check_online(manager, id);
    
    pthread_rwlock_rdlock(&manager->lck);
    do
    {
        err = manager->tab->find(manager->tab, id, &node_val);
        if ( err )
        {
            break;
        }
        
        node     = node_val;
        is_reset = node->rcvd(node, &a2d, &new_free_size);
        if ( new_free_size )
        {
            manager->tokens.free(&manager->tokens, new_free_size);
        }
    } while ( 0 );
    pthread_rwlock_unlock(&manager->lck);
    
    if ( is_reset )
    {
        manager->msg_event.node_reset(&manager->msg_event, id);
    }
    
    update_policy(manager, false);
}

static long snd_msg_len(struct msg_event_ops *ops, void *id)
{
    return sizeof(dispatch2app_t);
}

static int snd_msg_buf(struct msg_event_ops *ops, void *id, long len,
                       unsigned char *buf)
{
    void                  *node_val;
    int                   err      = QOS_ERROR_OK;
    sysqos_disp_manager_t *manager = manager_by_msg(ops);
    app_node_t     *node    = NULL;
    dispatch2app_t        d2a;
    assert(ops && len == sizeof(dispatch2app_t) && buf);
    
    pthread_rwlock_wrlock(&manager->lck);
    do
    {
        err = manager->tab->find(manager->tab, id, &node_val);
        if ( err )
        {
            break;
        }
        
        node = node_val;
        node->get_protocol(node, &d2a);
        memcpy(buf, &d2a, len);
    } while ( 0 );
    pthread_rwlock_unlock(&manager->lck);
    
    if ( err )
    {
        return QOS_ERROR_FAILEDNODE;
    }
    
    return err;
}

static inline void fill_disp_manager_val(sysqos_disp_manager_t *manager,
                                         unsigned long min_rs_num)
{
    manager->default_token_min = min_rs_num;
    manager->fence_id   = 0;
    
    manager->msg_event.node_online  = node_online;
    manager->msg_event.node_offline = node_offline;
    manager->msg_event.rcvd         = rcvd;
    manager->msg_event.node_reset   = node_reset;
    manager->msg_event.snd_msg_len  = snd_msg_len;
    manager->msg_event.snd_msg_buf  = snd_msg_buf;
    
    manager->set_msg_ops              = set_msg_ops;
    manager->set_dispatcher_event_ops = set_dispatcher_event_ops;
    
    manager->alloc_tokens      = alloc_tokens;
    manager->free_tokens       = free_tokens;
    manager->resource_increase = resource_increase;
    manager->resource_reduce   = resource_reduce;
}


int sysqos_disp_manager_init(sysqos_disp_manager_t *manager, int table_len,
                             unsigned long max_node_num, int update_interval,
                             unsigned long min_rs_num, unsigned long max_grp)
{
    int err = QOS_ERROR_OK;
    
    memset(manager, 0, sizeof(sysqos_disp_manager_t));
    fill_disp_manager_val(manager, min_rs_num);
    
    err = wait_increase_list_init(&manager->lhead_wait_increase, max_node_num);
    if ( err ) end_func(err, QOS_ERROR_MEMORY, list_head_queue_init_failed);
    
    manager->tab = alloc_hash_table(table_len, max_node_num);
    if ( !manager->tab )
        end_func(err, QOS_ERROR_MEMORY, alloc_hash_table_failed);
    
    err = pthread_rwlock_init(&manager->lck, NULL);
    if ( err ) end_func(err, QOS_ERROR_MEMORY, pthread_rwlock_init_failed);
    
    err = token_global_init(&manager->tokens, max_node_num,
                            min_rs_num);
    if ( err )
    {
        goto disp_left_tokens_init_failed;
    }
    
    err = memory_cache_init(&manager->token_node_cache,
                            sizeof(app_node_t), max_node_num);
    if ( err ) end_func(err, QOS_ERROR_MEMORY, memory_cache_init_failed);
    
    err = count_controller_init(&manager->cnt_controller, update_interval);
    if ( err ) end_func(err, QOS_ERROR_MEMORY, count_controller_init_failed);
    
    err = fence_executor_init(&manager->executor,update_nodes_tototal);
    if ( err ) end_func(err, QOS_ERROR_MEMORY, fence_executor_init_failed);
    
    err = token_update_ctx_init(&manager->update_ctx, max_node_num, max_grp);
    if ( err ) end_func(err, QOS_ERROR_MEMORY, policy_update_ctx_init_failed);
    
    return err;
    token_update_ctx_exit(&manager->update_ctx);
policy_update_ctx_init_failed:
    fence_executor_exit(&manager->executor);
fence_executor_init_failed:
    count_controller_exit(&manager->cnt_controller);
count_controller_init_failed:
    memory_cache_exit(&manager->token_node_cache);
memory_cache_init_failed:
    token_global_exit(&manager->tokens);
disp_left_tokens_init_failed:
    pthread_rwlock_destroy(&manager->lck);
pthread_rwlock_init_failed:
    free_hash_table(manager->tab);
alloc_hash_table_failed:
    wait_increase_list_exit(&manager->lhead_wait_increase);
list_head_queue_init_failed:
    return err;
}

void sysqos_disp_manager_exit(sysqos_disp_manager_t *manager)
{
    token_update_ctx_exit(&manager->update_ctx);
    count_controller_exit(&manager->cnt_controller);
    memory_cache_exit(&manager->token_node_cache);
    token_global_exit(&manager->tokens);
    pthread_rwlock_destroy(&manager->lck);
    free_hash_table(manager->tab);
    wait_increase_list_exit(&manager->lhead_wait_increase);
}

void test_sysqos_disp_manager()
{
    
    //FIXME
}
