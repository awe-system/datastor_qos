//
// Created by root on 18-7-25.
//

#include <string.h>
#include <assert.h>
#include <pthread.h>
#include "sysqos_dispatch_base_node.h"
#include "sysqos_protocol.h"
#include "sysqos_interface.h"
#include "sysqos_test_lib.h"

static inline bool is_to_reset(dispatch_base_node_t *desc, long new_ver)
{
    bool is_reset = false;
    pthread_rwlock_rdlock(&desc->lck);
    if ( new_ver < desc->version )
    {
        is_reset = true;
    }
    pthread_rwlock_unlock(&desc->lck);
    return is_reset;
}

static inline bool is_version_same(dispatch_base_node_t *desc, long new_ver)
{
    bool is_same = false;
    pthread_rwlock_rdlock(&desc->lck);
    if ( new_ver - desc->version == 0 )
    {
        is_same = true;
    }
    pthread_rwlock_unlock(&desc->lck);
    return is_same;
}

static bool check_update_total_version(dispatch_base_node_t *base_node,
                                       unsigned long new_total,
                                       long new_ver, bool *is_reset)
{
    bool is_resource_increased = false;
    assert(is_reset);
    
    *is_reset = is_to_reset(base_node, new_ver);
    if ( *is_reset )
    {
        return false;
    }
    
    if ( is_version_same(base_node, new_ver) )
    {
        return false;
    }
    
    pthread_rwlock_wrlock(&base_node->lck);
    if ( new_ver - base_node->version > 0 )
    {
        if ( base_node->token_quota < new_total )
        {
            is_resource_increased = true;
            base_node->token_quota = base_node->token_quota_new = new_total;
        }
        else if ( base_node->token_quota > new_total )
        {
            base_node->token_quota_new = new_total;
            base_node->token_quota     = max(new_total,
                                             base_node->token_quota -
                                             base_node->respond_step);
        }
        base_node->version = new_ver;
    }
    pthread_rwlock_unlock(&base_node->lck);
    
    // check_update_version(base_node);
    return is_resource_increased;
}

static int check_alloc_from_desc(dispatch_base_node_t *desc, long cost)
{
    pthread_rwlock_rdlock(&desc->lck);
    if ( cost + desc->token_inuse > desc->token_quota )
    {
        pthread_rwlock_unlock(&desc->lck);
        return QOS_ERROR_PENDING;
    }
    pthread_rwlock_unlock(&desc->lck);
    return QOS_ERROR_OK;
}

static int try_alloc_from_desc(dispatch_base_node_t *desc, long cost)
{
    int err = check_alloc_from_desc(desc, cost);
    if ( err )
    {
        return err;
    }
    
    pthread_rwlock_wrlock(&desc->lck);
    if ( cost + desc->token_inuse <= desc->token_quota )
    {
        desc->token_inuse += cost;
        if ( desc->token_quota_new < desc->token_quota )
        {
            desc->token_quota = max(desc->token_quota_new,
                                    desc->token_quota - desc->respond_step);
        }
    }
    pthread_rwlock_unlock(&desc->lck);
    return QOS_ERROR_OK;
}

static void free_to_desc(dispatch_base_node_t *desc, long cost)
{
    pthread_rwlock_wrlock(&desc->lck);
    desc->token_inuse -= cost;
    pthread_rwlock_unlock(&desc->lck);
}

static long get_currentversion(dispatch_base_node_t *desc)
{
    long res = 0;
    pthread_rwlock_rdlock(&desc->lck);
    res = desc->version;
    pthread_rwlock_unlock(&desc->lck);
    return res;
}


static unsigned long get_alloced(dispatch_base_node_t *desc)
{
    unsigned long res = 0;
    pthread_rwlock_rdlock(&desc->lck);
    res = desc->token_inuse;
    pthread_rwlock_unlock(&desc->lck);
    return res;
}

static void reset(struct dispatch_base_node *desc)
{
    pthread_rwlock_wrlock(&desc->lck);
    desc->token_quota = MIN_RS_NUM;
    desc->token_quota_new = MIN_RS_NUM;
    desc->version     = 0;
    pthread_rwlock_unlock(&desc->lck);
}

int dispatch_base_node_init(dispatch_base_node_t *base_node)
{
    base_node->token_inuse                = 0;
    base_node->version                    = 0;
    base_node->token_quota                = MIN_RS_NUM;
    base_node->token_quota_new            = MIN_RS_NUM;
    base_node->respond_step               = MMAX_RESPOND_STEP;
    base_node->get_version                = get_currentversion;
    base_node->get_token_inuse            = get_alloced;
    // base_node->check_alloc_from_desc = check_alloc_from_desc;
    base_node->free_to_base               = free_to_desc;
    base_node->try_alloc_from_base        = try_alloc_from_desc;
    base_node->check_update_quota_version = check_update_total_version;
    base_node->reset                      = reset;
    return pthread_rwlock_init(&base_node->lck, NULL);
}

void dispatch_base_node_exit(dispatch_base_node_t *base_node)
{
    pthread_rwlock_destroy(&base_node->lck);
}

/**for test*****************************************************************************************/

typedef struct disp_desc_manager_event
{
    void (*resource_increased)(struct disp_desc_manager_event *event, void *id);
} disp_desc_manager_event_t;

typedef struct disp_desc_manager
{
    hash_table_t              *tab;
    memory_cache_t            cache;
    disp_desc_manager_event_t *event_to_permission;
    pthread_rwlock_t          lck;
    /******************************************************************************/
    msg_event_ops_t           msg_event;//NOTE:此对象之实现online,offline,rcvd
    
    int
    (*check_alloc_resource)(struct disp_desc_manager *manager, resource_t *rs);
    
    int
    (*try_alloc_resource)(struct disp_desc_manager *manager, resource_t *rs);
    
    int (*free_resource)(struct disp_desc_manager *manager, resource_t *rs);
    
    unsigned long
    (*get_currentversion)(struct disp_desc_manager *manager, void *id);
    
    void (*set_hash)(struct disp_desc_manager *manager, hash_func hash);
    
    void
    (*set_compare)(struct disp_desc_manager *manager, compare_id_func compare);
    
    void (*set_event)(struct disp_desc_manager *manager,
                      disp_desc_manager_event_t *event);
} disp_desc_manager_t;

static int
check_alloc_resource(struct disp_desc_manager *manager, resource_t *rs)
{
    int  err   = QOS_ERROR_OK;
    void *desc = NULL;
    assert(manager && rs && rs->cost);
    pthread_rwlock_rdlock(&manager->lck);
    err = manager->tab->find(manager->tab, rs->id, &desc);
    if ( err )
        end_func(err, QOS_ERROR_FAILEDNODE, unlock_manager);
    assert(desc);
    
    err = check_alloc_from_desc(desc, rs->cost);
    if ( err )
        end_func(err, QOS_ERROR_PENDING, unlock_manager);
unlock_manager:
    pthread_rwlock_unlock(&manager->lck);
    return err;
}

static int try_alloc_resource(struct disp_desc_manager *manager, resource_t *rs)
{
    int  err   = QOS_ERROR_OK;
    void *desc = NULL;
    assert(manager && rs && rs->cost);
    pthread_rwlock_rdlock(&manager->lck);
    err = manager->tab->find(manager->tab, rs->id, &desc);
    if ( err )
        end_func(err, QOS_ERROR_FAILEDNODE, unlock_manager);
    assert(desc);
    
    err = try_alloc_from_desc(desc, rs->cost);
    if ( err )
        end_func(err, QOS_ERROR_PENDING, unlock_manager);
unlock_manager:
    pthread_rwlock_unlock(&manager->lck);
    return err;
}

int free_resource(struct disp_desc_manager *manager, resource_t *rs)
{
    int  err   = QOS_ERROR_OK;
    void *desc = NULL;
    assert(manager && rs && rs->cost);
    pthread_rwlock_rdlock(&manager->lck);
    err = manager->tab->find(manager->tab, rs->id, &desc);
    if ( err )
        end_func(err, QOS_ERROR_FAILEDNODE, unlock_manager);
    assert(desc);
    free_to_desc(desc, rs->cost);
unlock_manager:
    pthread_rwlock_unlock(&manager->lck);
    return err;
}

unsigned long get_currentversion_m(struct disp_desc_manager *manager, void *id)
{
    int           err = QOS_ERROR_OK;
    unsigned long ver = 0;
    void          *desc;
    assert(manager);
    pthread_rwlock_rdlock(&manager->lck);
    err = manager->tab->find(manager->tab, id, &desc);
    if ( !err )
    {
        assert(desc);
        ver = ((dispatch_base_node_t *) desc)->get_version(desc);
    }
    pthread_rwlock_unlock(&manager->lck);
    return ver;
}

void set_hash(struct disp_desc_manager *manager, hash_func hash)
{
    assert(manager && hash);
    manager->tab->set_hash(manager->tab, hash);
}

void set_compare(struct disp_desc_manager *manager, compare_id_func compare)
{
    assert(manager && compare);
    manager->tab->set_compare(manager->tab, compare);
}

void
set_event(struct disp_desc_manager *manager, disp_desc_manager_event_t *event)
{
    assert(manager && event);
    manager->event_to_permission = event;
}

static inline disp_desc_manager_t *
disp_desc_manager_by_msg_event(struct msg_event_ops *ops)
{
    assert(ops);
    return container_of(ops, disp_desc_manager_t, msg_event);
}

static void node_online(struct msg_event_ops *ops, void *id)
{
    disp_desc_manager_t *manager = disp_desc_manager_by_msg_event(ops);
    int                 err      = QOS_ERROR_OK;
    dispatch_base_node_t     *desc    = manager->cache.alloc(&manager->cache);
    assert(desc);
    err = dispatch_base_node_init(desc);
    assert(err == 0);
    pthread_rwlock_wrlock(&manager->lck);
    err = manager->tab->insert(manager->tab, id, desc);
    pthread_rwlock_unlock(&manager->lck);
    if ( err )
    {
        dispatch_base_node_exit(desc);
        manager->cache.free(&manager->cache, desc);
    }
}

static void node_reset(struct msg_event_ops *ops, void *id)
{
    disp_desc_manager_t *manager = disp_desc_manager_by_msg_event(ops);
    void                *desc    = NULL;
    int                 err      = QOS_ERROR_OK;
    pthread_rwlock_wrlock(&manager->lck);
    do
    {
        err = manager->tab->find(manager->tab, id, &desc);
        if ( err )
        {
            break;
        }
        ((dispatch_base_node_t *) desc)->reset((dispatch_base_node_t *) desc);
    } while ( 0 );
    pthread_rwlock_unlock(&manager->lck);
}

static void node_offline(struct msg_event_ops *ops, void *id)
{
    disp_desc_manager_t *manager = disp_desc_manager_by_msg_event(ops);
    void                *desc    = NULL;
    int                 err      = QOS_ERROR_OK;
    pthread_rwlock_wrlock(&manager->lck);
    err = manager->tab->erase(manager->tab, id, &desc);
    pthread_rwlock_unlock(&manager->lck);
    if ( err == 0 && desc )
    {
        dispatch_base_node_exit((dispatch_base_node_t *) desc);
        manager->cache.free(&manager->cache, desc);
    }
}

static void
rcvd(struct msg_event_ops *ops, void *id, long len, unsigned char *buf)
{
    disp_desc_manager_t *manager              =
                                disp_desc_manager_by_msg_event(ops);
    bool                is_resource_increased = false;
    bool                is_reset              = false;
    dispatch2app_t      app;
    assert(len == sizeof(dispatch2app_t) && manager->event_to_permission &&
           manager->event_to_permission->resource_increased);
    memcpy(&app, buf, len);
    
    void *desc;
    pthread_rwlock_rdlock(&manager->lck);
    do
    {
        int err = manager->tab->find(manager->tab, id, &desc);
        if ( err )
        {
            break;
        }
        
        is_resource_increased =
                check_update_total_version((dispatch_base_node_t *) desc, app.total,
                                           app.ver, &is_reset);
    } while ( 0 );
    pthread_rwlock_unlock(&manager->lck);
    if ( is_reset )
    {
        ops->node_reset(ops, id);
    }
    else if ( is_resource_increased )
    {
        manager->event_to_permission
                ->resource_increased(manager->event_to_permission, id);
    }
}

static int disp_desc_manager_init(disp_desc_manager_t *manager, int tab_len,
                                  int max_node_num)
{
    int err = QOS_ERROR_OK;
    memset(manager, 0, sizeof(disp_desc_manager_t));
    manager->tab = alloc_hash_table(tab_len, max_node_num);
    if ( !manager->tab )
        end_func(err, QOS_ERROR_MEMORY, alloc_hash_table_failed);
    
    err = memory_cache_init(&manager->cache, sizeof(dispatch_base_node_t),
                            max_node_num);
    if ( err )
        end_func(err, QOS_ERROR_MEMORY, memory_cache_init_failed);
    
    err = pthread_rwlock_init(&manager->lck, NULL);
    if ( err )
        end_func(err, QOS_ERROR_MEMORY, rwlock_init_failed);
    
    manager->msg_event.node_offline = node_offline;
    manager->msg_event.node_online  = node_online;
    manager->msg_event.node_reset   = node_reset;
    manager->msg_event.rcvd         = rcvd;
    
    manager->set_compare = set_compare;
    manager->set_hash    = set_hash;
    manager->set_event   = set_event;
    
    manager->check_alloc_resource = check_alloc_resource;
    manager->try_alloc_resource   = try_alloc_resource;
    manager->free_resource        = free_resource;
    manager->get_currentversion   = get_currentversion_m;
    
    return QOS_ERROR_OK;
    pthread_rwlock_destroy(&manager->lck);
rwlock_init_failed:
    memory_cache_exit(&manager->cache);
memory_cache_init_failed:
    free_hash_table(manager->tab);
alloc_hash_table_failed:
    return err;
}

static void disp_desc_manager_exit(disp_desc_manager_t *manager)
{
    assert(manager && manager->tab);
    pthread_rwlock_destroy(&manager->lck);
    memory_cache_exit(&manager->cache);
    free_hash_table(manager->tab);
}

/**for test*****************************************************************************************/
#ifdef DISP_DESC_MANAGER_TEST

#include <stdio.h>

#define TABLE_LEN      128
#define MAX_NODE_NUM   4096
#define FIRST_TOTAL    2000
#define RESOURCE_COST_LIST_NUM 10
const unsigned long rs_cost_list[RESOURCE_COST_LIST_NUM] =
                            {5, 23, 17, 16, 9, 7, 31, 37, 53, 91};
#define FIRST_NODE_NUM 5
static bool is_increased = false;

static void resource_increased(struct disp_desc_manager_event *event, void *id)
{
    is_increased = true;
}

disp_desc_manager_event_t test_event =
                                  {
                                          .resource_increased = resource_increased
                                  };

static void test_case_init_exit(disp_desc_manager_t *manager)
{
    int err = disp_desc_manager_init(manager, TABLE_LEN, MAX_NODE_NUM);
    assert(err == 0);
    manager->set_compare(manager, test_compare);
    manager->set_hash(manager, test_hash);
    manager->set_event(manager, &test_event);
    
    disp_desc_manager_exit(manager);
    
    err = disp_desc_manager_init(manager, TABLE_LEN, MAX_NODE_NUM);
    assert(err == 0);
    manager->set_compare(manager, test_compare);
    manager->set_hash(manager, test_hash);
    manager->set_event(manager, &test_event);
    
    printf("[test_case_init_exit] %s[OK]%s\n", GREEN, RESET);
}

static void test_case_regular_online_offline(disp_desc_manager_t *manager)
{
    int        err = QOS_ERROR_OK;
    resource_t rs;
    rs.cost = 1;
    rs.id   = (void *) 1;
    err = manager->try_alloc_resource(manager, &rs);
    assert(err);
    err = manager->free_resource(manager, &rs);
    assert(err);
    manager->msg_event.node_online(&manager->msg_event, (void *) 1);
    manager->msg_event.node_online(&manager->msg_event, (void *) 1);
    manager->msg_event.node_online(&manager->msg_event, (void *) 2);
    manager->msg_event.node_offline(&manager->msg_event, (void *) 2);
    manager->msg_event.node_offline(&manager->msg_event, (void *) 1);
    manager->msg_event.node_offline(&manager->msg_event, (void *) 2);

#ifdef CACHE_OPEN_CNT
    assert(manager->cache.alloc_cnt == manager->cache.free_cnt);
    assert(manager->tab->cache.alloc_cnt == manager->tab->cache.free_cnt);
#endif
    printf("[test_case_regular_online_offline] %s[OK]%s\n", GREEN, RESET);
}

static void test_case_regular_rcvd(disp_desc_manager_t *manager)
{
    dispatch2app_t dta;
    dta.ver   = 1;
    dta.total = FIRST_TOTAL;
    manager->msg_event
            .rcvd(&manager->msg_event, (void *) 1, sizeof(dispatch2app_t),
                  (unsigned char *) &dta);
    manager->msg_event.node_online(&manager->msg_event, (void *) 1);
    assert(manager->get_currentversion(manager, (void *) 1) == 0);
    manager->msg_event
            .rcvd(&manager->msg_event, (void *) 1, sizeof(dispatch2app_t),
                  (unsigned char *) &dta);
    assert(manager->get_currentversion(manager, (void *) 1) == dta.ver);
    manager->msg_event.node_offline(&manager->msg_event, (void *) 1);
#ifdef CACHE_OPEN_CNT
    assert(manager->cache.alloc_cnt == manager->cache.free_cnt);
    assert(manager->tab->cache.alloc_cnt == manager->tab->cache.free_cnt);
#endif
    printf("[test_case_regular_rcvd] %s[OK]%s\n", GREEN, RESET);
}

//FIXME 改成更具step分步骤 减少的 用 例
static void test_case_regular_rcvd_reduce(disp_desc_manager_t *manager)
{
    unsigned long  left_len = FIRST_TOTAL;
    int            i        = 0;
    int            err      = 0;
    void           *desc;
    dispatch2app_t dta;
    dta.ver   = 1;
    dta.total = FIRST_TOTAL;
    is_increased = false;
    manager->msg_event.node_online(&manager->msg_event, (void *) 1);
    manager->msg_event
            .rcvd(&manager->msg_event, (void *) 1, sizeof(dispatch2app_t),
                  (unsigned char *) &dta);
    assert(is_increased == true);
    resource_t rs;
    for ( i = 0; i < FIRST_TOTAL; ++i )
    {
        if ( left_len == 0 )
        {
            break;
        }
        rs.cost = min(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = try_alloc_resource(manager, &rs);
        assert(err == 0);
        left_len -= rs.cost;
    }
    dta.ver   = 2;
    dta.total = FIRST_TOTAL / 2;
    is_increased = false;
    manager->msg_event
            .rcvd(&manager->msg_event, (void *) 1, sizeof(dispatch2app_t),
                  (unsigned char *) &dta);
    assert(is_increased == false);
    
    assert(manager->get_currentversion(manager, (void *) 1) == 2);
    assert(0 == manager->tab->find(manager->tab, (void *) 1, &desc));
    assert(((dispatch_base_node_t *) desc)->token_inuse == FIRST_TOTAL);
    assert(((dispatch_base_node_t *) desc)->token_quota == FIRST_TOTAL / 2);
    
    left_len = FIRST_TOTAL / 2;
    for ( i  = 0; i < FIRST_TOTAL / 2; ++i )
    {
        if ( left_len == 0 )
        {
            break;
        }
        rs.cost = min(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = free_resource(manager, &rs);
        assert(err == 0);
        left_len -= rs.cost;
    }
    assert(manager->get_currentversion(manager, (void *) 1) == 2);
    assert(0 == manager->tab->find(manager->tab, (void *) 1, &desc));
    assert(((dispatch_base_node_t *) desc)->token_inuse == FIRST_TOTAL / 2);
    assert(((dispatch_base_node_t *) desc)->token_quota == FIRST_TOTAL / 2);
    assert(is_increased == false);
    
    left_len = FIRST_TOTAL / 2;
    for ( i  = 0; i < FIRST_TOTAL / 2; ++i )
    {
        if ( left_len == 0 )
        {
            break;
        }
        rs.cost = min(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = free_resource(manager, &rs);
        assert(err == 0);
        left_len -= rs.cost;
    }
    assert(manager->get_currentversion(manager, (void *) 1) == 2);
    assert(0 == manager->tab->find(manager->tab, (void *) 1, &desc));
    assert(((dispatch_base_node_t *) desc)->token_inuse == 0);
    assert(((dispatch_base_node_t *) desc)->token_quota == FIRST_TOTAL / 2);
    
    manager->msg_event.node_offline(&manager->msg_event, (void *) 1);
#ifdef CACHE_OPEN_CNT
    assert(manager->cache.alloc_cnt == manager->cache.free_cnt);
    assert(manager->tab->cache.alloc_cnt == manager->tab->cache.free_cnt);
#endif
    printf("[test_case_regular_rcvd_reduce] %s[OK]%s\n", GREEN, RESET);
}


static void test_case_regular_reset(disp_desc_manager_t *manager)
{
    unsigned long  left_len = FIRST_TOTAL;
    int            i        = 0;
    int            err      = 0;
    void           *desc;
    dispatch2app_t dta;
    dta.ver   = 1;
    dta.total = FIRST_TOTAL;
    is_increased = false;
    manager->msg_event.node_online(&manager->msg_event, (void *) 1);
    manager->msg_event
            .rcvd(&manager->msg_event, (void *) 1, sizeof(dispatch2app_t),
                  (unsigned char *) &dta);
    assert(is_increased == true);
    resource_t rs;
    for ( i = 0; i < FIRST_TOTAL; ++i )
    {
        if ( left_len == 0 )
        {
            break;
        }
        rs.cost = min(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = try_alloc_resource(manager, &rs);
        assert(err == 0);
        left_len -= rs.cost;
    }
    dta.ver   = 0;
    dta.total = FIRST_TOTAL / 2;
    is_increased = false;
    manager->msg_event
            .rcvd(&manager->msg_event, (void *) 1, sizeof(dispatch2app_t),
                  (unsigned char *) &dta);
    assert(is_increased == false);
    
    assert(manager->get_currentversion(manager, (void *) 1) == 0);
    assert(0 == manager->tab->find(manager->tab, (void *) 1, &desc));
    assert(((dispatch_base_node_t *) desc)->token_inuse == FIRST_TOTAL);
    assert(((dispatch_base_node_t *) desc)->token_quota == MIN_RS_NUM);
    
    left_len = FIRST_TOTAL / 2;
    for ( i  = 0; i < FIRST_TOTAL / 2; ++i )
    {
        if ( left_len == 0 )
        {
            break;
        }
        rs.cost = min(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = free_resource(manager, &rs);
        assert(err == 0);
        left_len -= rs.cost;
    }
    assert(manager->get_currentversion(manager, (void *) 1) == 0);
    assert(0 == manager->tab->find(manager->tab, (void *) 1, &desc));
    assert(((dispatch_base_node_t *) desc)->token_inuse == FIRST_TOTAL / 2);
    assert(((dispatch_base_node_t *) desc)->token_quota == MIN_RS_NUM);
    assert(is_increased == false);
    
    left_len = FIRST_TOTAL / 2;
    for ( i  = 0; i < FIRST_TOTAL / 2; ++i )
    {
        if ( left_len == 0 )
        {
            break;
        }
        rs.cost = min(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = free_resource(manager, &rs);
        assert(err == 0);
        left_len -= rs.cost;
    }
    assert(manager->get_currentversion(manager, (void *) 1) == 0);
    assert(0 == manager->tab->find(manager->tab, (void *) 1, &desc));
    assert(((dispatch_base_node_t *) desc)->token_inuse == 0);
    assert(((dispatch_base_node_t *) desc)->token_quota == MIN_RS_NUM);
    
    manager->msg_event.node_offline(&manager->msg_event, (void *) 1);
#ifdef CACHE_OPEN_CNT
    assert(manager->cache.alloc_cnt == manager->cache.free_cnt);
    assert(manager->tab->cache.alloc_cnt == manager->tab->cache.free_cnt);
#endif
    printf("[test_case_regular_reset] %s[OK]%s\n", GREEN, RESET);
}

static void test_case_regular_rcvd_increase(disp_desc_manager_t *manager)
{
    unsigned long  left_len = FIRST_TOTAL / 2;
    int            i        = 0;
    int            err      = 0;
    void           *desc;
    dispatch2app_t dta;
    dta.ver   = 1;
    dta.total = FIRST_TOTAL / 2;
    is_increased = false;
    manager->msg_event.node_online(&manager->msg_event, (void *) 1);
    manager->msg_event
            .rcvd(&manager->msg_event, (void *) 1, sizeof(dispatch2app_t),
                  (unsigned char *) &dta);
    assert(is_increased == true);
    resource_t rs;
    for ( i = 0; i < FIRST_TOTAL / 2; ++i )
    {
        if ( left_len == 0 )
        {
            break;
        }
        rs.cost = min(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = try_alloc_resource(manager, &rs);
        assert(err == 0);
        left_len -= rs.cost;
    }
    
    err = try_alloc_resource(manager, &rs);
    assert(err != 0);
    
    dta.ver   = 2;
    dta.total = FIRST_TOTAL;
    is_increased = false;
    manager->msg_event
            .rcvd(&manager->msg_event, (void *) 1, sizeof(dispatch2app_t),
                  (unsigned char *) &dta);
    assert(is_increased == true);
    
    assert(manager->get_currentversion(manager, (void *) 1) == 2);
    assert(0 == manager->tab->find(manager->tab, (void *) 1, &desc));
    assert(((dispatch_base_node_t *) desc)->token_inuse == FIRST_TOTAL / 2);
    assert(((dispatch_base_node_t *) desc)->token_quota == FIRST_TOTAL);
    
    left_len = FIRST_TOTAL / 2;
    for ( i  = 0; i < FIRST_TOTAL / 2; ++i )
    {
        if ( left_len == 0 )
        {
            break;
        }
        rs.cost = min(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = try_alloc_resource(manager, &rs);
        assert(err == 0);
        left_len -= rs.cost;
    }
    
    assert(manager->get_currentversion(manager, (void *) 1) == 2);
    assert(0 == manager->tab->find(manager->tab, (void *) 1, &desc));
    assert(((dispatch_base_node_t *) desc)->token_inuse == FIRST_TOTAL);
    assert(((dispatch_base_node_t *) desc)->token_quota == FIRST_TOTAL);
    
    manager->msg_event.node_offline(&manager->msg_event, (void *) 1);
#ifdef CACHE_OPEN_CNT
    assert(manager->cache.alloc_cnt == manager->cache.free_cnt);
    assert(manager->tab->cache.alloc_cnt == manager->tab->cache.free_cnt);
#endif
    printf("[test_case_regular_rcvd_increase] %s[OK]%s\n", GREEN, RESET);
}

static void test_case_regular_alloc_free(disp_desc_manager_t *manager)
{
    unsigned long  left_len = FIRST_TOTAL;
    int            i        = 0;
    int            err      = 0;
    void           *desc;
    dispatch2app_t dta;
    dta.ver   = 1;
    dta.total = FIRST_TOTAL;
    manager->msg_event.node_online(&manager->msg_event, (void *) 1);
    manager->msg_event
            .rcvd(&manager->msg_event, (void *) 1, sizeof(dispatch2app_t),
                  (unsigned char *) &dta);
    resource_t rs;
    for ( i = 0; i < FIRST_TOTAL; ++i )
    {
        if ( left_len == 0 )
        {
            break;
        }
        rs.cost = min(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = try_alloc_resource(manager, &rs);
        assert(err == 0);
        left_len -= rs.cost;
    }
    
    err = try_alloc_resource(manager, &rs);
    assert(err != 0);
    
    assert(0 == manager->tab->find(manager->tab, (void *) 1, &desc));
    assert(((dispatch_base_node_t *) desc)->token_inuse == FIRST_TOTAL);
    assert(((dispatch_base_node_t *) desc)->token_quota == FIRST_TOTAL);
    
    
    left_len = FIRST_TOTAL;
    for ( i  = 0; i < FIRST_TOTAL; ++i )
    {
        if ( left_len == 0 )
        {
            break;
        }
        rs.cost = min(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = free_resource(manager, &rs);
        assert(err == 0);
        left_len -= rs.cost;
    }
    
    assert(0 == manager->tab->find(manager->tab, (void *) 1, &desc));
    assert(((dispatch_base_node_t *) desc)->token_inuse == 0);
    assert(((dispatch_base_node_t *) desc)->token_quota == FIRST_TOTAL);
    
    manager->msg_event.node_offline(&manager->msg_event, (void *) 1);
#ifdef CACHE_OPEN_CNT
    assert(manager->cache.alloc_cnt == manager->cache.free_cnt);
    assert(manager->tab->cache.alloc_cnt == manager->tab->cache.free_cnt);
#endif
    printf("[test_case_regular_alloc_free] %s[OK]%s\n", GREEN, RESET);
}


static void test_case_regular_check_free(disp_desc_manager_t *manager)
{
    unsigned long  left_len = FIRST_TOTAL;
    int            i        = 0;
    int            err      = 0;
    void           *desc;
    dispatch2app_t dta;
    dta.ver   = 1;
    dta.total = FIRST_TOTAL;
    manager->msg_event.node_online(&manager->msg_event, (void *) 1);
    manager->msg_event
            .rcvd(&manager->msg_event, (void *) 1, sizeof(dispatch2app_t),
                  (unsigned char *) &dta);
    resource_t rs;
    for ( i = 0; i < FIRST_TOTAL; ++i )
    {
        if ( left_len == 0 )
        {
            break;
        }
        rs.cost = min(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = check_alloc_resource(manager, &rs);
        assert(err == 0);
        err = try_alloc_resource(manager, &rs);
        assert(err == 0);
        left_len -= rs.cost;
    }
    
    
    err = check_alloc_resource(manager, &rs);
    assert(err != 0);
    err = try_alloc_resource(manager, &rs);
    assert(err != 0);
    
    assert(0 == manager->tab->find(manager->tab, (void *) 1, &desc));
    assert(((dispatch_base_node_t *) desc)->token_inuse == FIRST_TOTAL);
    assert(((dispatch_base_node_t *) desc)->token_quota == FIRST_TOTAL);
    
    
    left_len = FIRST_TOTAL;
    for ( i  = 0; i < FIRST_TOTAL; ++i )
    {
        if ( left_len == 0 )
        {
            break;
        }
        rs.cost = min(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = free_resource(manager, &rs);
        assert(err == 0);
        left_len -= rs.cost;
    }
    
    assert(0 == manager->tab->find(manager->tab, (void *) 1, &desc));
    assert(((dispatch_base_node_t *) desc)->token_inuse == 0);
    assert(((dispatch_base_node_t *) desc)->token_quota == FIRST_TOTAL);
    
    manager->msg_event.node_offline(&manager->msg_event, (void *) 1);
#ifdef CACHE_OPEN_CNT
    assert(manager->cache.alloc_cnt == manager->cache.free_cnt);
    assert(manager->tab->cache.alloc_cnt == manager->tab->cache.free_cnt);
#endif
    printf("[test_case_regular_alloc_free] %s[OK]%s\n", GREEN, RESET);
}

#endif

void test_dispatch_base_node()
{
#ifdef DISP_DESC_MANAGER_TEST
    int err = 0;
    printf(YELLOW"--------------test_dispatch_base_node----------------:\n"RESET);
    disp_desc_manager_t manager;
    test_case_init_exit(&manager);
    
    test_case_regular_online_offline(&manager);
    test_case_regular_rcvd(&manager);
    
    test_case_regular_alloc_free(&manager);
    
    test_case_regular_check_free(&manager);
    
//    test_case_regular_rcvd_reduce(&manager);
    
    test_case_regular_rcvd_increase(&manager);
    
    test_case_regular_reset(&manager);
    
    //FIXME：设计一个梯度用例 token_quota_new step
    
    disp_desc_manager_exit(&manager);
    printf("[disp_desc_manager_exit] %s[OK]%s\n", GREEN, RESET);
    printf(BLUE"--------------test_dispatch_base_node end--------------:\n"RESET);

#endif
}