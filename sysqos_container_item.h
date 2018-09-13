//
// Created by root on 18-7-24.
//

#ifndef QOS_QOS_CONTAINER_ITEM_H
#define QOS_QOS_CONTAINER_ITEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "sysqos_type.h"

typedef struct sysqos_container_item
{
    void             *pri;
    void             *id;
    struct list_head list;
} qos_container_item_t;

static inline void
qos_container_item_init(qos_container_item_t *item, void *id, void *pri)
{
    assert(item);
    item->id  = id;
    item->pri = pri;
    LISTHEAD_INIT(&item->list);
}

#ifdef __cplusplus
}
#endif

#endif //TEST_QOS_QOS_CONTAINER_ITEM_H
