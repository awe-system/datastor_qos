//
// Created by root on 18-8-11.
//

#include <assert.h>
#include <pthread.h>
#include "sysqos_token_global.h"
#include "sysqos_common.h"

//
//static void
//set_max_rsvr_nolock(token_global_t *tokens, unsigned long app_max_rsvr)
//{
//    tokens->app_max_token_rsvr = app_max_rsvr;
//    if ( tokens->app_token_rsvr > tokens->app_max_token_rsvr )
//    {
//        tokens->app_token_rsvr = tokens->app_max_token_rsvr;
//    }
//}
//
//static void set_max_rsvr(token_global_t *tokens, unsigned long app_max_rsvr)
//{
//    sysqos_spin_lock(&tokens->lck);
//    set_max_rsvr_nolock(tokens, app_max_rsvr);
//    sysqos_spin_unlock(&tokens->lck);
//}

static int check_up_app_num(struct token_global *tokens)
{
    if ( tokens->app_num_cur + 1 > tokens->app_num_max ||
         tokens->token_total <
         tokens->token_used_static + tokens->default_token_min )
    {
        return QOS_ERROR_TOO_MANY_NODES;
    }
    else
    {
        ++tokens->app_num_cur;
        return QOS_ERROR_OK;
    }
}

static void alloc_from_rsvr(struct token_global *tokens, app_node_t *node)
{
    unsigned long this_token;
    
    assert(node->token_quota <= tokens->default_token_min);
    
    this_token            = min(tokens->app_token_rsvr,
                                tokens->default_token_min - node->token_quota);
    
    tokens->app_token_rsvr -= this_token;
    node->token_quota += this_token;
    node->token_quota_new = node->token_quota;
    tokens->token_used_static += this_token;
}

static void alloc_from_dynamic(struct token_global *tokens, app_node_t *node)
{
    unsigned long this_token;
    
    assert(node->token_quota <= tokens->default_token_min);
    
    this_token            = tokens->default_token_min - node->token_quota;
    tokens->token_free -= this_token;
    tokens->token_dynamic_new -= this_token;
    tokens->token_dynamic -= this_token;
    tokens->token_used_static += this_token;
    node->token_quota += this_token;
    node->token_quota_new = node->token_quota;
}

static inline unsigned long free_to_rsvr(struct token_global *tokens,
                                         app_node_t *node,
                                  unsigned long token_left)
{
    unsigned long this_token = 0;
    this_token = min(tokens->app_max_token_rsvr - tokens->app_token_rsvr,
                     token_left);
    tokens->app_token_rsvr += this_token;
    node->token_quota -= this_token;
    return this_token;
}

static inline void free_to_rsvr_dynamic(struct token_global *tokens, app_node_t *node,
                                unsigned long *token_left)
{
    unsigned long this_token = free_to_rsvr(tokens,node,*token_left);
    *token_left -= this_token;
    tokens->token_dynamic -= this_token;
    tokens->token_dynamic_new -= this_token;
}

static void free_to_rsvr_static(struct token_global *tokens, app_node_t *node,
                                unsigned long *token_left)
{
    unsigned long this_token = free_to_rsvr(tokens,node,*token_left);
    *token_left -= this_token;
    tokens->token_used_static -= this_token;
}

static inline void free_to_dynamic(struct token_global *tokens,
                                         app_node_t *node,
                                         unsigned long token_left)
{
    tokens->token_free += token_left;
    node->token_quota -= token_left;
}

static inline void free_to_dynamic_dynamic(struct token_global *tokens,
                                   app_node_t *node,
                                   unsigned long token_left)
{
    free_to_dynamic(tokens,node,token_left);
}

static inline void free_to_dynamic_static(struct token_global *tokens,
                                   app_node_t *node,
                                   unsigned long token_left)
{
    free_to_dynamic(tokens,node,token_left);
    
    tokens->token_used_static -= token_left;
    tokens->token_dynamic_new += token_left;
    tokens->token_dynamic += token_left;
}

//为node分配额外的空间
static int online(struct token_global *tokens, app_node_t *node)
{
    int err = 0;
    sysqos_spin_lock(&tokens->lck);
    do
    {
        err = check_up_app_num(tokens);
        if ( err )
        { break; }
        
        alloc_from_rsvr(tokens, node);
        
        alloc_from_dynamic(tokens, node);
    } while ( 0 );
    sysqos_spin_unlock(&tokens->lck);
    return err;
}

static void offline(struct token_global *tokens, app_node_t *node)
{
    unsigned long token_static  = 0;
    unsigned long token_dynamic = 0;
    assert(tokens->app_num_cur > 0);
    sysqos_spin_lock(&tokens->lck);
    --tokens->app_num_cur;
    token_static  = min(node->token_quota, tokens->default_token_min);
    token_dynamic = node->token_quota - token_static;
    
    free_to_rsvr_static(tokens, node, &token_static);
    if ( token_static )
    {
        free_to_dynamic_static(tokens, node, token_static);
    }
    else
    {
        free_to_rsvr_dynamic(tokens, node, &token_dynamic);
    }
    free_to_dynamic_dynamic(tokens, node, token_dynamic);
    
    sysqos_spin_unlock(&tokens->lck);
}

static unsigned long try_alloc(struct token_global *tokens,
                               unsigned long cost)
{
    sysqos_spin_lock(&tokens->lck);
    unsigned long res = min(tokens->token_free, cost);
    tokens->token_free -= res;
    sysqos_spin_unlock(&tokens->lck);
    return res;
}

static void gfree(struct token_global *tokens, unsigned long cost)
{
    sysqos_spin_lock(&tokens->lck);
    if ( tokens->token_dynamic_new < tokens->token_dynamic )
    {
        unsigned long token_need = tokens->token_dynamic -
                                   tokens->token_dynamic_new;
        
        unsigned long token_avaliable = min(token_need, cost);
        
        cost -= token_avaliable;
        tokens->token_dynamic -= token_avaliable;
        tokens->token_used_static += token_avaliable;
    }
    tokens->token_free += cost;
    sysqos_spin_unlock(&tokens->lck);
}

static void increase(struct token_global *tokens, unsigned long units)
{
    unsigned long token_rsvr_increase;
    sysqos_spin_lock(&tokens->lck);
    tokens->token_total += units;
    token_rsvr_increase = min(tokens->app_max_token_rsvr,
                              tokens->app_token_rsvr + units)
                          - tokens->app_token_rsvr;
    tokens->app_token_rsvr += token_rsvr_increase;
    tokens->token_dynamic_new += units - token_rsvr_increase;
    tokens->token_dynamic += units - token_rsvr_increase;
    tokens->token_free += units - token_rsvr_increase;
    sysqos_spin_unlock(&tokens->lck);
}
//
//static bool could_decrease(struct token_global *tokens,
//                           unsigned long units)
//{
//    if ( min(tokens->token_dynamic, tokens->token_dynamic_new) - units <
//         tokens->app_max_token_rsvr - tokens->app_token_rsvr )
//    {
//        return false;
//    }
//    return true;
//}
//
//static bool try_decrease(struct token_global *tokens,
//                         unsigned long units)
//{
//    sysqos_spin_lock(&tokens->lck);
//    if ( could_decrease(tokens, units) )
//    {
//        tokens->token_dynamic_new -= units;
//        tokens->token_dynamic -= units;
//        tokens->token_free -= min(units, tokens->token_free);
//        tokens->token_total -= units;
//        sysqos_spin_unlock(&tokens->lck);
//        return true;
//    }
//    else
//    {
//        sysqos_spin_unlock(&tokens->lck);
//        return false;
//    }
//}

void token_global_init_func(token_global_t *tokens)
{
    tokens->online    = online;
    tokens->offline   = offline;
    tokens->try_alloc = try_alloc;
    tokens->free      = gfree;
    tokens->increase  = increase;
//    tokens->try_decrease = try_decrease;
//    tokens->set_max_rsvr = set_max_rsvr;
}

static void token_global_init_value(token_global_t *tokens,
                                    unsigned long max_node_nr,
                                    unsigned long min_per_node)
{
    tokens->default_token_min  = min_per_node;
    tokens->token_total        = 0;
    tokens->token_free         = 0;
    tokens->token_used_static  = 0;
    tokens->token_dynamic      = 0;
    tokens->token_dynamic_new  = 0;
    tokens->app_token_rsvr     = 0;
    tokens->app_num_max        = max_node_nr;
    tokens->app_num_cur        = 0;
    tokens->app_max_token_rsvr = max_node_nr * min_per_node;
}

int token_global_init(token_global_t *tokens, unsigned long max_node_nr,
                      unsigned long min_per_node)
{
    token_global_init_value(tokens, max_node_nr, min_per_node);
    
    token_global_init_func(tokens);
    
    return sysqos_spin_init(&tokens->lck);
}

void token_global_exit(token_global_t *tokens)
{
    sysqos_spin_destroy(&tokens->lck);
}
