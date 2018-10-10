//
// Created by root on 18-9-11.
//


#include <sysqos_test_lib.h>
#include "dispatch_manager.h"

//
//typedef struct sysqos_disp_manager
//{
//    /*********************************************************************/
//    msg_event_ops_t msg_event;
//
//    int (*alloc_tokens)(struct sysqos_disp_manager *manager,
//                        resource_t *rs, long *fence_id);
//
//    void (*free_tokens)(struct sysqos_disp_manager *manager,
//                        resource_t *rs, long fence_id, bool is_fail);
//
//    void (*resource_increase)(struct sysqos_disp_manager *manager,
//                              unsigned long cost);
//
//    void (*resource_reduce)(struct sysqos_disp_manager *manager, long cost);
//
//    void (*set_dispatcher_event_ops)(struct sysqos_disp_manager *manager,
//                                     dispatcher_event_ops_t *ops);
//
//    void (*set_msg_ops)(struct sysqos_disp_manager *manager, msg_ops_t *ops);
//
//    /**私有属性或方法*******************************************************************************************/
//    token_global_t         tokens;
//    dispatcher_event_ops_t *disp_ops;
//    msg_ops_t              *msg_ops;
//
//    wait_increase_list_t lhead_wait_increase;
//
//    hash_table_t       *tab;
//    memory_cache_t     token_node_cache;
//    sysqos_rwlock_t   lck;
//    unsigned long      default_token_min;
//    unsigned long      fence_id;
//    count_controller_t cnt_controller;
//    fence_executor_t   executor;
//    struct token_update_ctx update_ctx;
//} sysqos_disp_manager_t;

//#define MAX_NODE_NUM 4096
#define UPDATE_INTERVAL_TMP 1
#define MAX_TOKEN_NUM   100000
static sysqos_disp_manager_t manager_static;
static sysqos_disp_manager_t *manager    = &manager_static;
static msg_ops_t             ops         =
                                     {
                                             .hash_id = test_hash,
                                             .compare = test_compare,
                                     };
static long                  fenceids[4] = {0};


static void check_d2a_version_quota(long version, unsigned long quota)
{
    dispatch2app_t d2a;
    int            err = 0;
    void           *id;
    
    id = (void *) 1;
    assert(manager->msg_event.snd_msg_len(&manager->msg_event, id)
           == sizeof(dispatch2app_t));
    err = manager->msg_event.snd_msg_buf(&manager->msg_event, id,
                                         sizeof(dispatch2app_t),
                                         (unsigned char *) &d2a);
    assert(err == QOS_ERROR_OK);
    assert(d2a.token_quota == quota);
    assert(d2a.version = version);
    
    
    id = (void *) (2 * NODE_TABLE_HASH_LEN + 1);
    assert(manager->msg_event.snd_msg_len(&manager->msg_event, id)
           == sizeof(dispatch2app_t));
    err = manager->msg_event.snd_msg_buf(&manager->msg_event, id,
                                         sizeof(dispatch2app_t),
                                         (unsigned char *) &d2a);
    assert(err == QOS_ERROR_OK);
    assert(d2a.token_quota == quota);
    assert(d2a.version = version);
    
    id = (void *) (NODE_TABLE_HASH_LEN + 1);
    assert(manager->msg_event.snd_msg_len(&manager->msg_event, id)
           == sizeof(dispatch2app_t));
    err = manager->msg_event.snd_msg_buf(&manager->msg_event, id,
                                         sizeof(dispatch2app_t),
                                         (unsigned char *) &d2a);
    assert(err == QOS_ERROR_OK);
    assert(d2a.token_quota == quota);
    assert(d2a.version = version);
    
    id = (void *) 2;
    assert(manager->msg_event.snd_msg_len(&manager->msg_event, id)
           == sizeof(dispatch2app_t));
    err = manager->msg_event.snd_msg_buf(&manager->msg_event, id,
                                         sizeof(dispatch2app_t),
                                         (unsigned char *) &d2a);
    assert(err == QOS_ERROR_OK);
    assert(d2a.token_quota == quota);
    assert(d2a.version = version);
}

static void alloc_min_success()
{
    resource_t rs;
    rs.id   = (void *) 1;
    rs.cost = MIN_RS_NUM;
    int err = 0;
    err = manager->alloc_tokens(manager, &rs, &fenceids[0]);
    assert(err == QOS_ERROR_OK);
    
    rs.id   = (void *) (2 * NODE_TABLE_HASH_LEN + 1);
    rs.cost = MIN_RS_NUM;
    
    err = manager->alloc_tokens(manager, &rs, &fenceids[1]);
    assert(err == QOS_ERROR_OK);
    
    rs.id   = (void *) (NODE_TABLE_HASH_LEN + 1);
    rs.cost = MIN_RS_NUM;
    
    err = manager->alloc_tokens(manager, &rs, &fenceids[2]);
    assert(err == QOS_ERROR_OK);
    
    rs.id   = (void *) 2;
    rs.cost = MIN_RS_NUM;
    err = manager->alloc_tokens(manager, &rs, &fenceids[3]);
    assert(err == QOS_ERROR_OK);
}

static void alloc_min_failed()
{
    long       fence_id = 0;
    resource_t rs;
    rs.id   = (void *) 1;
    rs.cost = MIN_RS_NUM;
    int err = 0;
    err = manager->alloc_tokens(manager, &rs, &fence_id);
    assert(err != QOS_ERROR_OK);
    
    rs.id   = (void *) (2 * NODE_TABLE_HASH_LEN + 1);
    rs.cost = MIN_RS_NUM;
    
    err = manager->alloc_tokens(manager, &rs, &fence_id);
    assert(err != QOS_ERROR_OK);
    
    rs.id   = (void *) (NODE_TABLE_HASH_LEN + 1);
    rs.cost = MIN_RS_NUM;
    
    err = manager->alloc_tokens(manager, &rs, &fence_id);
    assert(err != QOS_ERROR_OK);
    
    rs.id   = (void *) 2;
    rs.cost = MIN_RS_NUM;
    err = manager->alloc_tokens(manager, &rs, &fence_id);
    assert(err != QOS_ERROR_OK);
}

static void free_min_success()
{
    resource_t rs;
    rs.id   = (void *) 1;
    rs.cost = MIN_RS_NUM;
    manager->free_tokens(manager, &rs, fenceids[0], false);
    
    rs.id   = (void *) (2 * NODE_TABLE_HASH_LEN + 1);
    rs.cost = MIN_RS_NUM;
    
    manager->free_tokens(manager, &rs, fenceids[1], false);
    
    rs.id   = (void *) (NODE_TABLE_HASH_LEN + 1);
    rs.cost = MIN_RS_NUM;
    
    manager->free_tokens(manager, &rs, fenceids[2], false);
    
    rs.id   = (void *) 2;
    rs.cost = MIN_RS_NUM;
    manager->free_tokens(manager, &rs, fenceids[3], false);
}

static void test_rcvd(app2dispatch_t *a2d)
{
    void *id = NULL;
    id = (void *) 1;
    manager->msg_event
            .rcvd(&manager->msg_event, id, sizeof(app2dispatch_t),
                  (unsigned char *) a2d);
    
    
    id = (void *) (2 * NODE_TABLE_HASH_LEN + 1);
    manager->msg_event
            .rcvd(&manager->msg_event, id, sizeof(app2dispatch_t),
                  (unsigned char *) a2d);
    
    id = (void *) (NODE_TABLE_HASH_LEN + 1);
    manager->msg_event
            .rcvd(&manager->msg_event, id, sizeof(app2dispatch_t),
                  (unsigned char *) a2d);
    
    id = (void *) 2;
    manager->msg_event
            .rcvd(&manager->msg_event, id, sizeof(app2dispatch_t),
                  (unsigned char *) a2d);
}


static void free_min_failed()
{
    resource_t rs;
    rs.id   = (void *) 1;
    rs.cost = MIN_RS_NUM;
    manager->free_tokens(manager, &rs, fenceids[0] + 1, false);
    
    rs.id   = (void *) (2 * NODE_TABLE_HASH_LEN + 1);
    rs.cost = MIN_RS_NUM;
    
    manager->free_tokens(manager, &rs, fenceids[1] + 1, false);
    
    rs.id   = (void *) (NODE_TABLE_HASH_LEN + 1);
    rs.cost = MIN_RS_NUM;
    
    manager->free_tokens(manager, &rs, fenceids[2] + 1, false);
    
    rs.id   = (void *) 2;
    rs.cost = MIN_RS_NUM;
    manager->free_tokens(manager, &rs, fenceids[3] + 1, false);
    
}


static void check_quota()
{
    void                  *id       = NULL;
    app_node_t            *node;
    unsigned long         quota_new = 0;
    unsigned long         quota     = 0;
    sysqos_disp_manager_t *pm       = manager;
    id = (void *) 1;
    manager->tab->find(manager->tab, id, (void **) &node);
    if ( QOS_ERROR_OK == manager->tab->find(manager->tab, id, (void **) &node) )
    {
        quota_new += node->token_quota_new;
        quota += node->token_quota;
    }
    
    id = (void *) (2 * NODE_TABLE_HASH_LEN + 1);
    if ( QOS_ERROR_OK == manager->tab->find(manager->tab, id, (void **) &node) )
    {
        quota_new += node->token_quota_new;
        quota += node->token_quota;
    }
    
    id = (void *) (NODE_TABLE_HASH_LEN + 1);
    if ( QOS_ERROR_OK == manager->tab->find(manager->tab, id, (void **) &node) )
    {
        quota_new += node->token_quota_new;
        quota += node->token_quota;
    }
    
    id = (void *) 2;
    if ( QOS_ERROR_OK == manager->tab->find(manager->tab, id, (void **) &node) )
    {
        quota_new += node->token_quota_new;
        quota += node->token_quota;
    }
    
    assert(quota == quota_new
           && quota == pm->tokens.token_dynamic + pm->tokens.token_used_static
           && quota + pm->tokens.app_token_rsvr == pm->tokens.token_total);
}

static void test_case_press_update()
{
    app2dispatch_t a2d;
    a2d.version          = 0;
    a2d.press.type       = press_type_fifo;
    a2d.press.fifo.depth = 1000;
    a2d.token_in_use     = MIN_RS_NUM;
    
    test_rcvd(&a2d);
    check_quota();
    a2d.version = 1;
    test_rcvd(&a2d);
    check_quota();
    a2d.version = 2;
    test_rcvd(&a2d);
    check_quota();
    a2d.version = 3;
    test_rcvd(&a2d);
    check_quota();
    alloc_min_success();
    check_quota();
    alloc_min_success();
    free_min_success();
    free_min_success();
}

static void test_case_alloc_free()
{
    alloc_min_success();
    check_d2a_version_quota(1, MIN_RS_NUM);
    alloc_min_failed();
    check_d2a_version_quota(1, MIN_RS_NUM);
    free_min_failed();
    check_d2a_version_quota(1, MIN_RS_NUM);
    free_min_success();
    check_d2a_version_quota(1, MIN_RS_NUM);
    alloc_min_success();
    check_d2a_version_quota(1, MIN_RS_NUM);
    free_min_success();
    check_d2a_version_quota(1, MIN_RS_NUM);
}

static void part_failed_free()
{
    
    resource_t rs;
    rs.id = (void *) 1;
    
    rs.cost = MIN_RS_NUM;
    manager->free_tokens(manager, &rs, fenceids[0], true);
    
    rs.id   = (void *) (2 * NODE_TABLE_HASH_LEN + 1);
    rs.cost = MIN_RS_NUM;
    
    manager->free_tokens(manager, &rs, fenceids[1], false);
    
    rs.id   = (void *) (NODE_TABLE_HASH_LEN + 1);
    rs.cost = MIN_RS_NUM;
    
    manager->free_tokens(manager, &rs, fenceids[2], true);
    
    rs.id   = (void *) 2;
    rs.cost = MIN_RS_NUM;
    manager->free_tokens(manager, &rs, fenceids[3], false);
}

static void test_case_offline()
{
    app2dispatch_t a2d;
    a2d.version          = 0;
    a2d.press.type       = press_type_fifo;
    a2d.press.fifo.depth = 1000;
    a2d.token_in_use     = MIN_RS_NUM;
    
    test_rcvd(&a2d);
    check_quota();
    a2d.version = 1;
    test_rcvd(&a2d);
    check_quota();
    a2d.version = 2;
    test_rcvd(&a2d);
    check_quota();
    alloc_min_success();
    check_quota();
    alloc_min_success();
    part_failed_free();
    free_min_success();
    check_quota();
}


static int test_init()
{
    int err;
    err = sysqos_disp_manager_init(manager, NODE_TABLE_HASH_LEN, MAX_NODE_NUM,
                                   UPDATE_INTERVAL_TMP, MIN_RS_NUM);
    assert(err == QOS_ERROR_OK);
    manager->set_msg_ops(manager, &ops);
    
    sysqos_disp_manager_exit(manager);
    
    err = sysqos_disp_manager_init(manager, NODE_TABLE_HASH_LEN, MAX_NODE_NUM,
                                   UPDATE_INTERVAL_TMP, MIN_RS_NUM);
    assert(err == QOS_ERROR_OK);
    manager->set_msg_ops(manager, &ops);
    manager->resource_increase(manager, MAX_TOKEN_NUM);
    return CUE_SUCCESS;
}

static int test_clean()
{
    sysqos_disp_manager_exit(manager);
    return CUE_SUCCESS;
}

static void test_case_alloc_most()
{
    int err = QOS_ERROR_OK;
    long fence_id ;
    int i = 0;
    app2dispatch_t a2d;
    a2d.version          = 0;
    a2d.press.type       = press_type_fifo;
    a2d.press.fifo.depth = 1000;
    a2d.token_in_use     = MIN_RS_NUM;
    void *id = (void *)MAX_NODE_NUM + 10;
    
    for ( i = 0; i < MAX_NODE_NUM + 10; ++i )
    {
        resource_t rs;
        rs.id   = (void *) 1;
        rs.cost = MIN_RS_NUM;
        err = manager->alloc_tokens(manager, &rs,&fence_id);
    }
    
    manager->msg_event
            .rcvd(&manager->msg_event, id, sizeof(app2dispatch_t),
                  (unsigned char *) &a2d);
    
    assert(err != QOS_ERROR_OK);
}

void dispatch_manager_suit_init(test_frame_t *frame)
{
    test_suit_t suit;
    
    test_suit_init(&suit, "dispatch_manager_suit_init", test_init, test_clean);
    
    suit.add_case(&suit, "test_case_alloc_free",
                  test_case_alloc_free);
    
    suit.add_case(&suit, "test_case_press_update",
                  test_case_press_update);
    
    suit.add_case(&suit, "test_case_offline",
                  test_case_offline);
    
    suit.add_case(&suit, "test_case_alloc_most",
                  test_case_alloc_most);
    
    frame->add_suit(frame, &suit);
}
