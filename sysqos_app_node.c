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
    int err = QOS_ERROR_OK;
    assert(node && rs && pfence_id);
    pthread_spin_lock(&node->lck);
    if ( node->token_inuse + rs->cost > node->token_quota )
    {
        end_func(err, QOS_ERROR_TOOBIGCOST, unlock_end);
    }
    node->token_inuse += rs->cost;
    *pfence_id = node->fence_id;
unlock_end:
    pthread_spin_unlock(&node->lck);
    return err;
}

static void dtn_free(struct app_node *node, resource_t *rs,
                     long fence_id)
{
    assert(node && rs);
    pthread_spin_lock(&node->lck);
    if ( node->fence_id != fence_id )
    { goto unlock_end; }
    assert(node->token_inuse < rs->cost);
    node->token_inuse -= rs->cost;
unlock_end:
    pthread_spin_unlock(&node->lck);
}

static void get_protocol(app_node_t *node,dispatch2app_t *d2a)
{
    pthread_spin_lock(&node->lck);
    d2a->ver = node->Version;
    d2a->total = min(node->token_quota_new,node->token_quota);
    pthread_spin_unlock(&node->lck);
}

int app_node_init(app_node_t *node, unsigned long min_units,
                  long fence_id)
{
    node->alloc = dtn_alloc;
    node->free  = dtn_free;
    node->get_protocol = get_protocol;
    
    node->token_inuse   = 0;
    node->token_quota = min_units;
    node->press     = 0;
    node->limit.min = min_units;
    node->limit.max = 0;//表示无穷大
    node->Version   = 0;
    node->fence_id  = fence_id;
    
    return pthread_spin_init(&node->lck, PTHREAD_PROCESS_PRIVATE);
}

void app_node_exit(app_node_t *node)
{
    pthread_spin_destroy(&node->lck);
}

/**for test********************************************************************/

void test_app_node()
{
}