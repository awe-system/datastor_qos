//
// Created by root on 18-8-11.
//

#ifndef QOS_SYSQOS_DISP_LEFT_TOKENS_H
#define QOS_SYSQOS_DISP_LEFT_TOKENS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct token_global
{
    unsigned long max_node_nr;
    unsigned long min_per_node;
    
    int           online_nr;
    unsigned long Total;
    unsigned long Free;
    unsigned long ToFree;
    
    int (*try_online)(struct token_global *left_tokens);
    
    void (*offline)(struct token_global *left_tokens);
    
    unsigned long (*try_alloc)(struct token_global *left_tokens,
                     unsigned long cost);
    
    void (*free)(struct token_global *left_tokens, unsigned long cost);
    
    void (*increase)(struct token_global *left_tokens, unsigned long units);
    
    int (*try_decrease)(struct token_global *left_tokens,
                        unsigned long units);
    
    unsigned long (*real_total)(struct token_global *left_tokens);
    
} token_global_t;

int token_global_init(token_global_t *left_tokens,
                      unsigned long max_node_nr,
                      unsigned long min_per_node);

void token_global_exit(token_global_t *left_tokens);

#ifdef __cplusplus
}
#endif

#endif //TEST_QOS_SYSQOS_DISP_LEFT_TOKENS_H
