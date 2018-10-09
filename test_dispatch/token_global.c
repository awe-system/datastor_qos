//
// Created by root on 18-9-11.
//


#include "token_global.h"

//    unsigned long (*try_alloc)(struct token_global *tokens,
//                               unsigned long cost);
//
//    //cant del zero
//    void (*free)(struct token_global *tokens, unsigned long cost);
//
//    void (*increase)(struct token_global *tokens, unsigned long units);
//

static app_node_t     node_static;
static app_node_t     *node   = &node_static;
static token_global_t tokens_static;
static token_global_t *tokens = &tokens_static;
#define MAX_NODE_NUM_TMP 4096
#define MAX_TOKEN_NUM 100000

static int test_init()
{
    int err = 0;
    err = token_global_init(tokens, MAX_NODE_NUM_TMP, MIN_RS_NUM);
    assert(err == QOS_ERROR_OK);
    
    err = app_node_init(node, MIN_RS_NUM, 0);
    assert(err == QOS_ERROR_OK);
    
    app_node_exit(node);
    
    token_global_exit(tokens);
    err = token_global_init(tokens, MAX_NODE_NUM_TMP, MIN_RS_NUM);
    assert(err == QOS_ERROR_OK);
    
    err = app_node_init(node, MIN_RS_NUM, 0);
    assert(err == QOS_ERROR_OK);
    
    tokens->increase(tokens,MAX_TOKEN_NUM);
    
    return CUE_SUCCESS;
}

static int test_clean()
{
    token_global_exit(tokens);
    return CUE_SUCCESS;
}

static void test_case_regular_online_offline()
{
    int err;
    assert(node->token_quota_new == 0);
    assert(node->token_quota == 0);
//    assert(tokens->token_free == MAX_TOKEN_NUM - MAX_NODE_NUM_TMP *MIN_RS_NUM);
    assert(tokens->token_total == MAX_TOKEN_NUM);
//    assert(tokens->token_dynamic == MAX_TOKEN_NUM - MAX_NODE_NUM_TMP *MIN_RS_NUM);
    assert(tokens->token_used_static == 0);
    assert(tokens->app_max_token_rsvr == tokens->app_num_max*MIN_RS_NUM);
//    assert(tokens->app_token_rsvr == tokens->app_max_token_rsvr);
    assert(tokens->app_num_max == MAX_NODE_NUM_TMP);
    assert(tokens->app_num_cur == 0);
    
    err = tokens->online(tokens, node);
    assert(err == QOS_ERROR_OK);
    assert(node->token_quota_new == MIN_RS_NUM);
    assert(node->token_quota == MIN_RS_NUM);
    
//    assert(tokens->token_free == MAX_TOKEN_NUM- MAX_NODE_NUM_TMP *MIN_RS_NUM);
    assert(tokens->token_total == MAX_TOKEN_NUM);
//    assert(tokens->token_dynamic == MAX_TOKEN_NUM- MAX_NODE_NUM_TMP *MIN_RS_NUM);
    assert(tokens->token_used_static == MIN_RS_NUM);
    assert(tokens->app_max_token_rsvr == tokens->app_num_max*MIN_RS_NUM);
//    assert(tokens->app_token_rsvr == tokens->app_max_token_rsvr - MIN_RS_NUM);
    assert(tokens->app_num_max == MAX_NODE_NUM_TMP);
    assert(tokens->app_num_cur == 1);
    
    assert(node->token_quota_new == MIN_RS_NUM);
    tokens->offline(tokens, node);

//    assert(tokens->token_free == MAX_TOKEN_NUM - MAX_NODE_NUM_TMP *MIN_RS_NUM);
    assert(tokens->token_total == MAX_TOKEN_NUM);
//    assert(tokens->token_dynamic == MAX_TOKEN_NUM - MAX_NODE_NUM_TMP *MIN_RS_NUM);
    assert(tokens->token_used_static == 0);
    assert(tokens->app_max_token_rsvr == tokens->app_num_max*MIN_RS_NUM);
//    assert(tokens->app_token_rsvr == tokens->app_max_token_rsvr);
    assert(tokens->app_num_max == MAX_NODE_NUM_TMP);
    assert(tokens->app_num_cur == 0);
}


static void test_case_regular_alloc_free()
{
    int err;
    unsigned long cost;
    test_clean();
    test_init();
//    assert(tokens->token_free == MAX_TOKEN_NUM - MAX_NODE_NUM_TMP *MIN_RS_NUM);
    assert(tokens->token_total == MAX_TOKEN_NUM);
//    assert(tokens->token_dynamic == MAX_TOKEN_NUM - MAX_NODE_NUM_TMP *MIN_RS_NUM);
    assert(tokens->token_used_static == 0);
    assert(tokens->app_max_token_rsvr == tokens->app_num_max*MIN_RS_NUM);
//    assert(tokens->app_token_rsvr == tokens->app_max_token_rsvr);
    assert(tokens->app_num_max == MAX_NODE_NUM_TMP);
    assert(tokens->app_num_cur == 0);
    
    err = tokens->online(tokens, node);
    assert(err == QOS_ERROR_OK);
    assert(node->token_quota_new == MIN_RS_NUM);
    assert(node->token_quota == MIN_RS_NUM);
    
//    assert(tokens->token_free == MAX_TOKEN_NUM- MAX_NODE_NUM_TMP *MIN_RS_NUM);
    assert(tokens->token_total == MAX_TOKEN_NUM);
//    assert(tokens->token_dynamic == MAX_TOKEN_NUM- MAX_NODE_NUM_TMP *MIN_RS_NUM);
    assert(tokens->token_used_static == MIN_RS_NUM);
    assert(tokens->app_max_token_rsvr == tokens->app_num_max*MIN_RS_NUM);
//    assert(tokens->app_token_rsvr == tokens->app_max_token_rsvr - MIN_RS_NUM);
    assert(tokens->app_num_max == MAX_NODE_NUM_TMP);
    assert(tokens->app_num_cur == 1);
    assert(node->token_quota_new == MIN_RS_NUM);
    
    cost = tokens->try_alloc(tokens,tokens->token_free+5);
//    assert(cost == MAX_TOKEN_NUM- MAX_NODE_NUM_TMP *MIN_RS_NUM);
    assert(err == QOS_ERROR_OK);
    assert(node->token_quota_new == MIN_RS_NUM);
    assert(node->token_quota == MIN_RS_NUM);
    
    assert(tokens->token_free == 0);
    assert(tokens->token_total == MAX_TOKEN_NUM);
//    assert(tokens->token_dynamic == MAX_TOKEN_NUM- MAX_NODE_NUM_TMP *MIN_RS_NUM);
    assert(tokens->token_used_static == MIN_RS_NUM);
    assert(tokens->app_max_token_rsvr == tokens->app_num_max*MIN_RS_NUM);
//    assert(tokens->app_token_rsvr == tokens->app_max_token_rsvr - MIN_RS_NUM);
    assert(tokens->app_num_max == MAX_NODE_NUM_TMP);
    assert(tokens->app_num_cur == 1);
    assert(node->token_quota_new == MIN_RS_NUM);
    
    tokens->free(tokens,cost);
    
//    assert(tokens->token_free == MAX_TOKEN_NUM- MAX_NODE_NUM_TMP *MIN_RS_NUM);
    assert(tokens->token_total == MAX_TOKEN_NUM);
//    assert(tokens->token_dynamic == MAX_TOKEN_NUM- MAX_NODE_NUM_TMP *MIN_RS_NUM);
    assert(tokens->token_used_static == MIN_RS_NUM);
    assert(tokens->app_max_token_rsvr == tokens->app_num_max*MIN_RS_NUM);
//    assert(tokens->app_token_rsvr == tokens->app_max_token_rsvr - MIN_RS_NUM);
    assert(tokens->app_num_max == MAX_NODE_NUM_TMP);
    assert(tokens->app_num_cur == 1);
    assert(node->token_quota_new == MIN_RS_NUM);
    
    tokens->offline(tokens, node);
    
//    assert(tokens->token_free == MAX_TOKEN_NUM - MAX_NODE_NUM_TMP *MIN_RS_NUM);
    assert(tokens->token_total == MAX_TOKEN_NUM);
//    assert(tokens->token_dynamic == MAX_TOKEN_NUM - MAX_NODE_NUM_TMP *MIN_RS_NUM);
    assert(tokens->token_used_static == 0);
    assert(tokens->app_max_token_rsvr == tokens->app_num_max*MIN_RS_NUM);
//    assert(tokens->app_token_rsvr == tokens->app_max_token_rsvr);
    assert(tokens->app_num_max == MAX_NODE_NUM_TMP);
    assert(tokens->app_num_cur == 0);
}

void token_global_suit_init(test_frame_t *frame)
{
    test_suit_t suit;
    
    test_suit_init(&suit, "token_global_suit_init", test_init, test_clean);
    
    suit.add_case(&suit, "test_case_regular_online_offline",
                  test_case_regular_online_offline);
    
    
    suit.add_case(&suit, "test_case_regular_alloc_free",
                  test_case_regular_alloc_free);
    
    frame->add_suit(frame, &suit);
}
