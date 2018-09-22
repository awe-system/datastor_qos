//
// Created by root on 18-7-29.
//

#ifndef _QOS_QOS_PERMISSION_H
#define _QOS_QOS_PERMISSION_H

#include <sys/types.h>
#include "sysqos_interface.h"
#include "memory_cache.h"
#include "sysqos_resource.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef enum app_token_stat
{
    app_token_stat_init,
    app_token_stat_got,
    app_token_stat_failed,
} app_token_stat_t;

struct token_reqgrp;
typedef struct token_req
{
    int                  stat;
    int                  Nodeid;
    struct token_reqgrp  *token_grp;
    struct resource_list token_need;//包含list_nodereq
    struct list_head     list_reqgrp;
} token_req_t;

static inline resource_list_t* resource_list_bylist(struct list_head *plist)
{
    return container_of(plist, resource_list_t, list);
}

static inline token_req_t *app_token_by_list(struct list_head *list)
{
    return container_of(resource_list_bylist(list),
                        token_req_t, token_need);
}

static inline token_req_t *app_token_by_grp_list(struct list_head *list)
{
    return container_of(list, token_req_t, list_reqgrp);
}

typedef struct token_reqgrp
{
    //进入终态
    int (*to_got)(struct token_reqgrp *permission,
                  token_req_t *resource);
    
    int (*to_failed)(struct token_reqgrp *permission,
                     token_req_t *resource);
    
    /**************************************************/
//FIXME :list_total_reqgrp for debug
    struct list_head lhead_reqgrp;
    struct list_head lhead_reqgrep_got;
    int              req_pending_nr;//total_nr - ok_nr
    int              req_total_nr;
    void             *pri;
    int              err;
    bool failed_exist;
} token_reqgrp_t;


static inline token_reqgrp_t *permission_by_token_list(struct list_head *list)
{
    return app_token_by_list(list)->token_grp;
}

int token_reqgrp_init(token_reqgrp_t *token_grp, memory_cache_t *resource_cache,
                      struct list_head *rs_list, void *pri);

void token_reqgrp_exit(token_reqgrp_t *token_grp,
                       memory_cache_t *resource_cache);

#ifdef __cplusplus
}
#endif

#endif //TEST_QOS_QOS_PERMISSION_H
