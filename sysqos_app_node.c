//
// Created by root on 18-8-11.
//

#include <stddef.h>
#include <assert.h>
#include <pthread.h>
#include "sysqos_app_node.h"
#include "sysqos_common.h"

static int dtn_alloc(struct app_node *node, resource_t *rs,
                     long *pfence_id)
{
    //TODO:降低锁冲突
    int err = QOS_ERROR_OK;
    assert(node && rs && pfence_id);
    sysqos_spin_lock(&node->lck);
    if ( node->token_inuse + rs->cost > node->token_quota )
    {
        end_func(err, QOS_ERROR_TOOBIGCOST, unlock_end);
    }
    node->token_inuse += rs->cost;
    *pfence_id = node->fence_id;
unlock_end:
    sysqos_spin_unlock(&node->lck);
    return err;
}

static void dtn_free(struct app_node *node, resource_t *rs,
                     long fence_id)
{
    //TODO:降低锁冲突
    assert(node && rs);
    sysqos_spin_lock(&node->lck);
    if ( node->fence_id != fence_id )
    { goto unlock_end; }
    assert(node->token_inuse < rs->cost);
    node->token_inuse -= rs->cost;
unlock_end:
    sysqos_spin_unlock(&node->lck);
}

static void get_protocol(app_node_t *node, dispatch2app_t *d2a)
{
    sysqos_spin_lock(&node->lck);
    d2a->version     = node->version;
    d2a->token_quota = min(node->token_quota_new, node->token_quota);
    sysqos_spin_unlock(&node->lck);
}


static update_node_t *init_update_node(struct app_node *node)
{
    update_node_init(&node->updatenode, node->token_quota, &node->limit,
                     node->press);
    return &node->updatenode;
}

//只要try_alloc_func不是败则返回true
static bool update_token_quota(struct app_node *node, void *pri,
                               try_alloc_func_t try_alloc_func,
                               erease_func_t erase_func)
{
    bool          is_global_enough = true;
    unsigned long new_quota        = 0;
    assert(node && pri && try_alloc_func && erase_func);
    sysqos_spin_lock(&node->lck);
    if ( node->token_quota < node->token_quota_new )
    {//变小时在rcvd触发 这里只处理变大的情况
        new_quota = node->token_quota_new - node->token_quota;
        new_quota = try_alloc_func(pri, new_quota);
        if ( new_quota == node->token_quota_new - node->token_quota )
        {
            erase_func(pri, node);
        }
        else
        {
            is_global_enough = false;
        }
    }
    sysqos_spin_unlock(&node->lck);
    return is_global_enough;
}

static inline unsigned long new_quota_under_limit(struct app_node *node,
                                                  unsigned long token_quota_new)
{
    if ( node->limit.max > 0 )//过滤超过最大值的设置
    {
        token_quota_new = min(node->limit.max, token_quota_new);
    }
    return token_quota_new;
}

static inline void update_token_quota_new_base(struct app_node *node,
                                               unsigned long token_quota_new,
                                               void *pri,
                                               insert_func_t insert_func)
{
    node->token_quota_new = token_quota_new;
    ++node->version;
}

static inline void update_token_quota_new_up(struct app_node *node,
                                             unsigned long token_quota_new,
                                             void *pri,
                                             insert_func_t insert_func)
{
    update_token_quota_new_base(node, token_quota_new, pri, insert_func);
    insert_func(pri, node);//插入到待分配队列中
}

static inline void update_token_quota_new_down(struct app_node *node,
                                               unsigned long token_quota_new,
                                               void *pri,
                                               insert_func_t insert_func)
{
    update_token_quota_new_base(node, token_quota_new, pri, insert_func);
}

//返回新设置的值
static unsigned long update_token_quota_new(struct app_node *node,
                                            unsigned long token_quota_new,
                                            void *pri,
                                            insert_func_t insert_func)
{
    assert(token_quota_new >= node->limit.min);
    sysqos_spin_lock(&node->lck);
    token_quota_new = new_quota_under_limit(node, token_quota_new);
    
    if ( node->token_quota_new > token_quota_new )//比上次调整变小
    {
        update_token_quota_new_down(node, token_quota_new, pri, insert_func);
    }
    else if ( node->token_quota_new < token_quota_new )//比上次调整变大
    {
        update_token_quota_new_up(node, token_quota_new, pri, insert_func);
    }
    node->token_quota_new = token_quota_new;
    sysqos_spin_unlock(&node->lck);
    assert(token_quota_new >= node->limit.min);
    return token_quota_new;
    
}

static unsigned long update_quota_after_rcvd(app_node_t *node,
                                             unsigned long confirm_token)
{
    unsigned long new_free_size = 0;
    unsigned long new_quota = min(confirm_token, node->token_quota_new);
    //只有变小才有意义
    if ( node->token_quota < new_quota )
    {
        new_free_size = new_quota - node->token_quota;
        node->token_quota = new_quota;
    }
    return new_free_size;
}

//更新press以及调整根据回馈调整 token_quota 返回是否需要reset 暂时不需要返回true
static bool rcvd(struct app_node *node, app2dispatch_t *a2d,
                 unsigned long *free_size)
{
    sysqos_spin_lock(&node->lck);
    if ( a2d->version == node->version )
    {
        node->press = a2d->press.fifo.depth;
        *free_size = update_quota_after_rcvd(node, a2d->token_in_use);
    }
    sysqos_spin_unlock(&node->lck);
    return false;
}


static void appnode_init_func(app_node_t *node)
{
    node->alloc                  = dtn_alloc;
    node->free                   = dtn_free;
    node->get_protocol           = get_protocol;
    node->init_update_node       = init_update_node;
    node->update_token_quota     = update_token_quota;
    node->rcvd                   = rcvd;
    node->update_token_quota_new = update_token_quota_new;
}

static void appnode_init_val(app_node_t *node,
                             unsigned long min_units,
                             long fence_id)
{
    LISTHEAD_INIT(&node->list_wait_increase);
    node->token_inuse     = 0;
    node->token_quota     = 0;
    node->token_quota_new = 0;
    node->press           = 0;
    node->limit.min       = min_units;
    node->limit.max       = 0;
    node->version         = 0;
    node->fence_id        = fence_id;
    update_node_init(&node->updatenode, node->token_quota, &node->limit,
                     node->press);
}

int app_node_init(app_node_t *node, unsigned long min_units,
                  long fence_id)
{
    appnode_init_val(node, min_units, fence_id);
    
    appnode_init_func(node);
    
    return sysqos_spin_init(&node->lck);
}

void app_node_exit(app_node_t *node)
{
    sysqos_spin_destroy(&node->lck);
}

/**for test********************************************************************/

void test_app_node()
{

}