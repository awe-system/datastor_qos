//
// Created by root on 18-8-11.
//

#ifndef _QOS_SYSQOS_DISP_UPDATE_GRP_H
#define _QOS_SYSQOS_DISP_UPDATE_GRP_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <sys/types.h>
#include "list_head.h"
#include "sysqos_policy_param.h"

typedef struct update_node
{
    unsigned long    weight;
    unsigned long    tmp_token_quota;
    unsigned long    token_base;
    struct list_head list_update;
} update_node_t;

static update_node_t * update_node_by_list(struct list_head *list)
{
    return container_of(list,update_node_t,list_update);
}

void update_node_init(update_node_t *node,
                      unsigned long token_quota, limit_t *plimit,
                      unsigned long press);

#ifdef __cplusplus
}
#endif
#endif //TEST_QOS_SYSQOS_DISP_UPDATE_GRP_H
