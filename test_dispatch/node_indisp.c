//
// Created by root on 18-9-10.
//

#include <sysqos_app_node.h>
#include <assert.h>
#include "sysqos_waitincrease_list.h"
#include "node_indisp.h"

static app_node_t           node_static;
static app_node_t           *node    = &node_static;
static long                 fence_id = 0;
static wait_increase_list_t wait_list;
static wait_increase_list_t *q       = &wait_list;


#define MAX_NODE_NUM 1024

static int test_init()
{
    int err = app_node_init(node, MIN_RS_NUM, fence_id++);
    assert(err == QOS_ERROR_OK);
    
    err = wait_increase_list_init(q, MAX_NODE_NUM);
    assert(err == QOS_ERROR_OK);
    app_node_exit(node);
    
    wait_increase_list_exit(q);
    
    err = wait_increase_list_init(q, MAX_NODE_NUM);
    assert(err == QOS_ERROR_OK);
    
    err = app_node_init(node, MIN_RS_NUM, fence_id++);
    assert(err == QOS_ERROR_OK);
    return CUE_SUCCESS;
}

static int test_clean()
{
    app_node_exit(node);
    return CUE_SUCCESS;
}

static void test_case_updatenode()
{
    update_node_t *updatenode = &node->updatenode;
    assert(node == app_node_by_update(updatenode));
}


static unsigned long test_try_alloc_dircect(void *pri, unsigned long cost)
{
    return cost;//return as request
}


static void test_erease_direct(void *pri, struct app_node *node)
{
    q->erase(q, node);
}

static void test_insert_quota_direct(void *pri, struct app_node *node)
{
    q->insert(q, node);
}

static bool check_alloc_quota_direct()
{
    app_node_t *node;
    if ( q->is_first_exist(q, &node) )
    {
        node->update_token_quota(node, NULL, test_try_alloc_dircect,
                                 test_erease_direct);
        return true;
    }
    return false;
}

static void check_all_alloc_direct()
{
    while ( check_alloc_quota_direct() );
}

static void test_fake_online()
{
    unsigned long new_quota = node->update_token_quota_new(node, MIN_RS_NUM,
                                                           NULL,
                                                           test_insert_quota_direct);
    check_all_alloc_direct();
    assert(new_quota == MIN_RS_NUM);
    assert(node);
    assert(node->token_quota_new == node->token_quota);
    assert(node->token_quota == MIN_RS_NUM);
    assert(node->token_inuse == 0);
}

static void test_case_regular_alloc_free()
{
    int err = QOS_ERROR_OK;
    int i   = 0;
    test_fake_online();
    for ( i = 0; i < MIN_RS_NUM; ++i )
    {
        long       tmp_fence;
        resource_t rs;
        rs.cost = 1;
        rs.id   = NULL;
        err = node->alloc(node, &rs, &tmp_fence);
        assert(err == QOS_ERROR_OK);
        assert(tmp_fence == fence_id - 1);
    }
    assert(node->token_inuse == MIN_RS_NUM);
    
    for ( i = 0; i < MIN_RS_NUM; ++i )
    {
        resource_t rs;
        rs.cost = 1;
        rs.id   = NULL;
        node->free(node, &rs, fence_id - 1);
        assert(err == QOS_ERROR_OK);
    }
    
    assert(node->token_inuse == 0);
}

static void test_case_alloc_free_fence()
{
    int err = QOS_ERROR_OK;
    int i   = 0;
    test_fake_online();
    for ( i = 0; i < MIN_RS_NUM; ++i )
    {
        long       tmp_fence;
        resource_t rs;
        rs.cost = 1;
        rs.id   = NULL;
        err = node->alloc(node, &rs, &tmp_fence);
        assert(err == QOS_ERROR_OK);
        assert(tmp_fence == fence_id - 1);
    }
    
    assert(node->token_inuse == MIN_RS_NUM);
    test_clean();
    test_init();
    test_fake_online();
    assert(err == QOS_ERROR_OK);
    assert(node->token_inuse == 0);
    
    for ( i = 0; i < MIN_RS_NUM; ++i )
    {
        long       tmp_fence;
        resource_t rs;
        rs.cost = 1;
        rs.id   = NULL;
        err = node->alloc(node, &rs, &tmp_fence);
        assert(err == QOS_ERROR_OK);
        assert(tmp_fence == fence_id - 1);
    }
    
    for ( i = 0; i < MIN_RS_NUM; ++i )
    {
        resource_t rs;
        rs.cost = 1;
        rs.id   = NULL;
        node->free(node, &rs, fence_id - 2);
        assert(err == QOS_ERROR_OK);
    }
    
    for ( i = 0; i < MIN_RS_NUM / 2; ++i )
    {
        resource_t rs;
        rs.cost = 1;
        rs.id   = NULL;
        node->free(node, &rs, fence_id - 1);
        assert(err == QOS_ERROR_OK);
    }
    assert(node->token_inuse == MIN_RS_NUM - MIN_RS_NUM / 2);
    
    for ( i = 0; i < MIN_RS_NUM / 2; ++i )
    {
        resource_t rs;
        rs.cost = 1;
        rs.id   = NULL;
        node->free(node, &rs, fence_id - 1);
        assert(err == QOS_ERROR_OK);
    }
    assert(node->token_inuse == 0);
}


static void test_case_regular_increase()
{
    unsigned long  new_quota = 0;
    dispatch2app_t d2a;
    test_clean();
    test_init();
    test_fake_online();
    node->get_protocol(node, &d2a);
    assert(d2a.token_quota == MIN_RS_NUM);
    assert(d2a.version == 1);
    new_quota = node->update_token_quota_new(node, 3 * MIN_RS_NUM,
                                             NULL, test_insert_quota_direct);
    check_all_alloc_direct();
    assert(new_quota == 3 * MIN_RS_NUM);
    assert(node->token_quota == 3 * MIN_RS_NUM);
    assert(node->token_inuse == 0);
    assert(node->token_quota_new == 3 * MIN_RS_NUM);
    node->get_protocol(node, &d2a);
    assert(d2a.token_quota == 3*MIN_RS_NUM);
    assert(d2a.version == 2);
}


static void test_case_regular_reduce()
{
    unsigned long free_size = 0;
    unsigned long  new_quota;
    app2dispatch_t a2d;
    dispatch2app_t d2a;
    
    test_case_regular_increase();
    
    new_quota = node->update_token_quota_new(node, 2 * MIN_RS_NUM,
                                             NULL, test_insert_quota_direct);
    assert(new_quota == 2*MIN_RS_NUM);
    assert(node->token_quota_new == 2*MIN_RS_NUM);
    
    node->get_protocol(node, &d2a);
    assert(d2a.token_quota == 2*MIN_RS_NUM);
    assert(d2a.version == 3);
    assert(3*MIN_RS_NUM== node->token_quota);
    
    a2d.version = 3;
    a2d.token_in_use = 3*MIN_RS_NUM;
    a2d.press.type = press_type_fifo;
    a2d.press.fifo.depth = 100;
    
    assert(!node->rcvd(node,&a2d,&free_size));
    assert(free_size == 0);
    assert(node->token_quota == 3*MIN_RS_NUM);
    assert(node->token_quota_new == 2*MIN_RS_NUM);
    
    a2d.version = 3;
    a2d.token_in_use = 2*MIN_RS_NUM+1;
    a2d.press.type = press_type_fifo;
    a2d.press.fifo.depth = 100;
    
    assert(!node->rcvd(node,&a2d,&free_size));
    assert(free_size == MIN_RS_NUM - 1);
    assert(node->token_quota == 2*MIN_RS_NUM+1);
    assert(node->token_quota_new == 2*MIN_RS_NUM);
    
    a2d.version = 3;
    a2d.token_in_use = 2*MIN_RS_NUM;
    a2d.press.type = press_type_fifo;
    a2d.press.fifo.depth = 100;
    
    assert(!node->rcvd(node,&a2d,&free_size));
    assert(free_size == 1);
    assert(node->token_quota == 2*MIN_RS_NUM);
    assert(node->token_quota_new == 2*MIN_RS_NUM);
}

void node_indisp_suit_init(test_frame_t *frame)
{
    test_suit_t suit;
    
    test_suit_init(&suit, "node_indisp_suit_init", test_init, test_clean);
    
    suit.add_case(&suit, "test_case_updatenode", test_case_updatenode);
    
    suit.add_case(&suit, "test_case_regular_alloc_free",
                  test_case_regular_alloc_free);
    
    suit.add_case(&suit, "test_case_alloc_free_fence",
                  test_case_alloc_free_fence);
    
    suit.add_case(&suit, "test_case_regular_increase",
                  test_case_regular_increase);
    
    suit.add_case(&suit, "test_case_regular_reduce",
                  test_case_regular_reduce);
    
    
    frame->add_suit(frame, &suit);
}
