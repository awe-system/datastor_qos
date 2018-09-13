//
// Created by root on 18-8-11.
//

#include "sysqos_update_node.h"
#include "sysqos_common.h"

void update_node_init(update_node_t *node,unsigned long token_quota,
                      limit_t *plimit, unsigned long press)
{
    LISTHEAD_INIT(&node->list_update);
    node->token_base      = min(plimit->min, token_quota);
    node->tmp_token_quota = token_quota;
    node->weight          = press;
    if(press >0)
    {
        node->weight          = press;
    }
}
