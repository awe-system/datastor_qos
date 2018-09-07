//
// Created by root on 18-7-25.
//

#ifndef _QOS_QOS_DISP_DESC_H
#define _QOS_QOS_DISP_DESC_H

#include "hash_table.h"
#include "sysqos_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dispatch_base_node
{
    unsigned long    token_inuse;
    unsigned long    token_quota;
    unsigned long    token_quota_target;//
    unsigned long    token_quota_new;
    unsigned long    respond_step;
    long             version;
    bool need_confirm;//
    pthread_rwlock_t lck;
    
    int (*check_alloc_from_base)(struct dispatch_base_node *desc, long cost);
    
    /*******************************************************************************/
    void (*reset)(struct dispatch_base_node *base_node);
    
    int (*try_alloc_from_base)(struct dispatch_base_node *base_node, long cost);
    
    void (*free_to_base)(struct dispatch_base_node *base_node, long cost);
    
    bool (*check_update_quota_version)(struct dispatch_base_node *base_node,
                                       unsigned long new_total, long new_ver,
                                       bool *is_reset);
    
    long (*get_version)(struct dispatch_base_node *base_node);
    
    unsigned long (*get_token_inuse)(struct dispatch_base_node *base_node);
} dispatch_base_node_t;

int dispatch_base_node_init(dispatch_base_node_t *base_node);

void dispatch_base_node_exit(dispatch_base_node_t *base_node);

#ifdef __cplusplus
}
#endif

#endif //TEST_QOS_QOS_DISP_DESC_H
