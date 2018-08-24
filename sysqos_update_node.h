//
// Created by root on 18-8-11.
//

#ifndef _QOS_SYSQOS_DISP_UPDATE_GRP_H
#define _QOS_SYSQOS_DISP_UPDATE_GRP_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "list_head.h"

typedef struct update_node
{
    int              weight;
    unsigned long    tmp_token_quota;
    unsigned long    token_base;
    struct list_head list_update;
    
    
} update_node_t;

#ifdef __cplusplus
}
#endif
#endif //TEST_QOS_SYSQOS_DISP_UPDATE_GRP_H
