//
// Created by root on 18-7-24.
//

#ifndef QOS_SAFE_RW_LIST_H
#define QOS_SAFE_RW_LIST_H

#include "sysqos_type.h"
#include "memory_cache.h"
#include "sysqos_common.h"
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SAFE_RW_LIST_TEST

enum
{
    safe_rw_list_error_ok,
    safe_rw_list_error_notfound,
    safe_rw_list_error_initlock,
    safe_rw_list_error_exist,
};



typedef struct safe_rw_list
{
    int (*insert)(struct safe_rw_list *list, void *id, void *pri);
    
    int (*erase)(struct safe_rw_list *list, void *id, void **pri);
    
    int (*find)(struct safe_rw_list *list, void *id, void **pri);
    
    void (*set_compare)(struct safe_rw_list *list, compare_id_func compare);
    
    void (*for_each_do)(struct safe_rw_list *list,void *ctx, for_each_dofunc_t dofunc);
    /***********************************************************/
#ifdef SAFE_RW_LIST_TEST
#endif
    /******************************************************/
    compare_id_func compare;
    memory_cache_t  *cache;
    
    struct list_head list;
    sysqos_rwlock_t rwlock;
} safe_rw_list_t;

int safe_rw_list_init(safe_rw_list_t *list, memory_cache_t *item_cache);

void safe_rw_list_exit(safe_rw_list_t *list);


#ifdef __cplusplus
}
#endif

#endif //TEST_QOS_SAFE_RW_LIST_H
