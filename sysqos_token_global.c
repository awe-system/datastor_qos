//
// Created by root on 18-8-11.
//

#include <assert.h>
#include <pthread.h>
#include "sysqos_token_global.h"
#include "sysqos_common.h"


static void
set_max_rsvr_nolock(token_global_t *tokens, unsigned long app_max_rsvr)
{
    tokens->app_max_token_rsvr = app_max_rsvr;
    if ( tokens->app_token_rsvr > tokens->app_max_token_rsvr )
    {
        tokens->app_token_rsvr = tokens->app_max_token_rsvr;
    }
}

static void set_max_rsvr(token_global_t *tokens, unsigned long app_max_rsvr)
{
    //FIXME:lock
    set_max_rsvr_nolock(tokens, app_max_rsvr);
}

static int check_up_app_num(struct token_global *tokens)
{
    if ( tokens->app_num_cur + 1 < tokens->app_num_max ||
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
 
    this_token = min(tokens->app_token_rsvr,
                     tokens->default_token_min - node->token_quota);
    
    tokens->app_token_rsvr -= this_token;
    node->token_quota += this_token;
    node->token_quota_new = node->token_quota;
    tokens->token_used_static += this_token;
}

static void alloc_from_free(struct token_global *tokens, app_node_t *node)
{
    unsigned long this_token;
    
    assert(node->token_quota <= tokens->default_token_min);
    
    this_token =  min(tokens->token_free,
                      tokens->default_token_min - node->token_quota);
    
    tokens->token_free -= this_token;
    node->token_quota += this_token;
    node->token_quota_new = node->token_quota;
    tokens->token_used_static += this_token;
}

static void alloc_from_dynamic(struct token_global *tokens, app_node_t *node)
{
    unsigned long this_token;
    
    assert(node->token_quota <= tokens->default_token_min);
    
    this_token = tokens->default_token_min - node->token_quota;
    
    tokens->token_dynamic_new -= this_token;
    node->token_quota += this_token;
    node->token_quota_new = node->token_quota;
}

static void free_to_rsvr(struct token_global *tokens, app_node_t *node)
{
    unsigned long this_token;
    
    this_token = min(tokens->app_max_token_rsvr - tokens->app_token_rsvr,
                     node->token_quota);

    tokens->app_token_rsvr += this_token;
    node->token_quota -= this_token;
}

static void free_to_dynamic(struct token_global *tokens, app_node_t *node)
{
    unsigned long token_static;
    unsigned long this_token;
    
    token_static = max(tokens->default_token_min, node->token_quota);
    tokens->token_used_static -= token_static;
    
    assert(tokens->token_dynamic_new <= tokens->token_dynamic);
    
    this_token = min(tokens->token_dynamic - tokens->token_dynamic_new,
                     node->token_quota - tokens->token_used_static);
    
    tokens->token_dynamic += this_token;
    node->token_quota -= this_token;
}

static void free_to_free(struct token_global *tokens, app_node_t *node)
{
    tokens->token_free += node->token_quota;
}

//为node分配额外的空间
static int online(struct token_global *tokens, app_node_t *node)
{
    int err = 0;
    //FIXME lock
    do
    {
        err = check_up_app_num(tokens);
        if ( err )
        { break; }
        
        alloc_from_rsvr(tokens, node);
        
        alloc_from_free(tokens,node);
        
        alloc_from_dynamic(tokens, node);
    } while ( 0 );
    return err;
}

static void offline(struct token_global *tokens, app_node_t *node)
{
   
    assert(tokens->app_num_cur > 0);
    //FIXME lock
    --tokens->app_num_cur;
    
    free_to_rsvr(tokens, node);
    
    free_to_dynamic(tokens, node);
    
    free_to_free(tokens,node);
}

static unsigned long try_alloc(struct token_global *tokens,
                               unsigned long cost)
{
    //FIXME lock
    unsigned long res = min(tokens->token_free, cost);
    tokens->token_free -= res;
    return res;
}

static void gfree(struct token_global *tokens, unsigned long cost)
{
    //FIXME lock
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
}

static void increase(struct token_global *tokens, unsigned long units)
{
    //FIXME lock
    tokens->token_free += units;
    tokens->token_total += units;
    tokens->token_dynamic_new += units;
    tokens->token_dynamic += units;
}

static bool could_decrease(struct token_global *tokens,
                           unsigned long units)
{
    if ( min(tokens->token_dynamic, tokens->token_dynamic_new) - units <
         tokens->app_max_token_rsvr - tokens->app_token_rsvr )
    {
        return false;
    }
    return true;
}

static bool try_decrease(struct token_global *tokens,
                         unsigned long units)
{
    //FIXME lock
    if ( could_decrease(tokens, units) )
    {
        tokens->token_dynamic_new -= units;
        tokens->token_dynamic -= units;
        tokens->token_free -= min(units, tokens->token_free);
        tokens->token_total -= units;
        return true;
    }
    else
    {
        return false;
    }
}

void token_global_init_func(token_global_t *tokens)
{
    tokens->online       = online;
    tokens->offline      = offline;
    tokens->try_alloc    = try_alloc;
    tokens->free         = gfree;
    tokens->increase     = increase;
    tokens->try_decrease = try_decrease;
    tokens->set_max_rsvr = set_max_rsvr;
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
    tokens->app_max_token_rsvr = 0;
}

int token_global_init(token_global_t *tokens, unsigned long max_node_nr,
                      unsigned long min_per_node)
{
    token_global_init_value(tokens, max_node_nr, min_per_node);
    
    token_global_init_func(tokens);
    
    return pthread_spin_init(&tokens->lck,PTHREAD_PROCESS_PRIVATE);
}

void token_global_exit(token_global_t *tokens)
{
    pthread_spin_destroy(&tokens->lck);
}

void test_token_global()
{
       //FIXME
}
