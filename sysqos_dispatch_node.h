//
// Created by root on 18-7-29.
//

#ifndef QOS_QOS_PERMISSION_ITEM_H
#define QOS_QOS_PERMISSION_ITEM_H


#include "sysqos_nodereq_list.h"
#include "sysqos_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dispatch_node
{
    /********************************************/
    /*
     * return is_rest
     * */
    bool (*resource_changed)(struct dispatch_node *item,
                             dispatch2app_t *dta,
                             struct list_head *got_permission_list);
    
    //返回的是获得
    void
    (*free_resource)(struct dispatch_node *item, resource_list_t *rs,
                     struct list_head *got_permission_list);
    
    //失败则放入队列
    int (*alloc_resource)(struct dispatch_node *item,
                          resource_list_t *rs);
    
    void
    (*get_protocol)(struct dispatch_node *item, app2dispatch_t *atd);
    
    void (*pop_all)(struct dispatch_node *item,
                    struct list_head *fail_permission_list);
    
    void (*reset)(struct dispatch_node *item);
    
    /********************************************/
    dispatch_base_node_t    base_node;
    nodereq_list_t     lhead_nodereq;
    sysqos_spin_lock_t lck;
    int                fence_id;
} dispatch_node_t;

int dispatch_node_init(dispatch_node_t *item, int version);

void dispatch_node_exit(dispatch_node_t *item);

#ifdef __cplusplus
}
#endif

#endif //TEST_QOS_QOS_PERMISSION_ITEM_H
