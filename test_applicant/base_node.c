//
// Created by root on 18-9-4.
//

#include <CUnit/CUnit.h>
#include <test_frame.h>
#include "sysqos_msg.h"
#include "sysqos_test_lib.h"
#include "base_node.h"


#define TABLE_LEN      128
//#define MAX_NODE_NUM   4096
#define FIRST_TOTAL    2000
#define RESOURCE_COST_LIST_NUM 10

//#define FIRST_NODE_NUM 5
static bool is_increased            = false;


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
//
//    int
//    (*check_alloc_resource)(struct disp_desc_manager *manager, resource_t *rs);
//
    int
    (*try_alloc_resource)(struct disp_desc_manager *manager, resource_t *rs);
    
    int (*free_resource)(struct disp_desc_manager *manager, resource_t *rs,
                         bool *could_alloc);
    
    long
    (*get_currentversion)(struct disp_desc_manager *manager, void *id);
    
    void (*set_hash)(struct disp_desc_manager *manager, hash_func hash);
    
    void
    (*set_compare)(struct disp_desc_manager *manager, compare_id_func compare);
    
    void (*set_event)(struct disp_desc_manager *manager,
                      disp_desc_manager_event_t *event);
} disp_desc_manager_t;

static disp_desc_manager_t manager_static;
static disp_desc_manager_t *manager = &manager_static;

static int try_alloc_resource(struct disp_desc_manager *manager, resource_t *rs)
{
    int  err;
    void *desc = NULL;
    assert(manager && rs && rs->cost);
    pthread_rwlock_rdlock(&manager->lck);
    err = manager->tab->find(manager->tab, rs->id, &desc);
    if ( err )
        end_func(err, QOS_ERROR_FAILEDNODE, unlock_manager);
    assert(desc != NULL);
    
    err = ((dispatch_base_node_t *) desc)->try_alloc_from_base(desc, rs->cost);
    if ( err )
        end_func(err, QOS_ERROR_PENDING, unlock_manager);
unlock_manager:
    pthread_rwlock_unlock(&manager->lck);
    return err;
}

static int free_resource(struct disp_desc_manager *manager,
                         resource_t *rs,
                         bool *could_alloc)
{
    int  err;
    void *desc = NULL;
    assert(manager && rs && rs->cost);
    pthread_rwlock_rdlock(&manager->lck);
    err = manager->tab->find(manager->tab, rs->id, &desc);
    if ( err )
        end_func(err, QOS_ERROR_FAILEDNODE, unlock_manager);
    assert(desc != NULL);
    *could_alloc = ((dispatch_base_node_t *) desc)->free_to_base(desc,
                                                                 rs->cost);
unlock_manager:
    pthread_rwlock_unlock(&manager->lck);
    return err;
}

static long
get_currentversion_m(struct disp_desc_manager *manager, void *id)
{
    int  err;
    long ver = 0;
    void *desc;
    assert(manager != NULL);
    pthread_rwlock_rdlock(&manager->lck);
    err = manager->tab->find(manager->tab, id, &desc);
    if ( !err )
    {
        assert(desc != NULL);
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
    assert(ops != NULL);
    return container_of(ops, disp_desc_manager_t, msg_event);
}

static void node_online(struct msg_event_ops *ops, void *id)
{
    disp_desc_manager_t  *manager = disp_desc_manager_by_msg_event(ops);
    int                  err;
    dispatch_base_node_t *desc    = manager->cache.alloc(&manager->cache);
    assert(desc != NULL);
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
    int                 err;
    pthread_rwlock_wrlock(&manager->lck);
    err = manager->tab->find(manager->tab, id, &desc);
    if ( !err )
    {
        ((dispatch_base_node_t *) desc)->reset((dispatch_base_node_t *) desc);
    }
    pthread_rwlock_unlock(&manager->lck);
}

static void node_offline(struct msg_event_ops *ops, void *id)
{
    disp_desc_manager_t *manager = disp_desc_manager_by_msg_event(ops);
    void                *desc    = NULL;
    int                 err;
    pthread_rwlock_wrlock(&manager->lck);
    err = manager->tab->erase(manager->tab, id, &desc);
    pthread_rwlock_unlock(&manager->lck);
    if ( err == 0 && desc )
    {
        dispatch_base_node_exit((dispatch_base_node_t *) desc);
        manager->cache.free(&manager->cache, desc);
    }
}

static void rcvd(struct msg_event_ops *ops, void *id,
                 long len, unsigned char *buf)
{
    disp_desc_manager_t *manager              =
                                disp_desc_manager_by_msg_event(ops);
    bool                is_resource_increased = false;
    bool                is_reset              = false;
    dispatch2app_t      app;
    assert(len == sizeof(dispatch2app_t) && manager->event_to_permission &&
           manager->event_to_permission->resource_increased);
    memcpy(&app, buf, (size_t)len);
    
    void *desc;
    pthread_rwlock_rdlock(&manager->lck);
    int err = manager->tab->find(manager->tab, id, &desc);
    if ( !err )
    {
        is_resource_increased =
                ((dispatch_base_node_t *) desc)->check_update_quota_version(
                        (dispatch_base_node_t *) desc,
                        (unsigned long) app.token_quota,
                        app.version, &is_reset);
    }
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
                                  unsigned long max_node_num)
{
    int err;
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

//    manager->check_alloc_resource = check_alloc_resource;
    manager->try_alloc_resource = try_alloc_resource;
    manager->free_resource      = free_resource;
    manager->get_currentversion = get_currentversion_m;
    
    return QOS_ERROR_OK;
//    pthread_rwlock_destroy(&manager->lck);
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

const unsigned long rs_cost_list[RESOURCE_COST_LIST_NUM] =
                            {5, 23, 17, 16, 9, 7, 31, 37, 53, 91};

static void resource_increased(disp_desc_manager_event_t *event, void *id)
{
    printf("evnet %p, id %p\n",event,id);
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

static void test_case_regular_online_offline()
{
    bool       could_alloc = false;
    int        err;
    resource_t rs;
    rs.cost = 1;
    rs.id   = (void *) 1;
    err = manager->try_alloc_resource(manager, &rs);
    assert(err);
    err = manager->free_resource(manager, &rs, &could_alloc);
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

static void test_case_regular_rcvd()
{
    dispatch2app_t dta;
    dta.version     = 1;
    dta.token_quota = FIRST_TOTAL;
    manager->msg_event
            .rcvd(&manager->msg_event, (void *) 1, sizeof(dispatch2app_t),
                  (unsigned char *) &dta);
    manager->msg_event.node_online(&manager->msg_event, (void *) 1);
    assert(manager->get_currentversion(manager, (void *) 1) == 0);
    manager->msg_event
            .rcvd(&manager->msg_event, (void *) 1, sizeof(dispatch2app_t),
                  (unsigned char *) &dta);
    assert(manager->get_currentversion(manager, (void *) 1) == dta.version);
    manager->msg_event.node_offline(&manager->msg_event, (void *) 1);
#ifdef CACHE_OPEN_CNT
    assert(manager->cache.alloc_cnt == manager->cache.free_cnt);
    assert(manager->tab->cache.alloc_cnt == manager->tab->cache.free_cnt);
#endif
    printf("[test_case_regular_rcvd] %s[OK]%s\n", GREEN, RESET);
}

static void test_case_regular_rcvd_reduce()
{
    bool could_alloc  = false;
    unsigned long  left_len = FIRST_TOTAL;
    int            i        = 0;
    int            err      = 0;
    void           *desc;
    dispatch2app_t dta;
    dta.version     = 1;
    dta.token_quota = FIRST_TOTAL;
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
        rs.cost = MIN(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = try_alloc_resource(manager, &rs);
        assert(err == 0);
        left_len -= rs.cost;
    }
    dta.version     = 2;
    dta.token_quota = FIRST_TOTAL / 2;
    is_increased = false;
    manager->msg_event
            .rcvd(&manager->msg_event, (void *) 1, sizeof(dispatch2app_t),
                  (unsigned char *) &dta);
    assert(is_increased == false);
    
    rs.cost = 1;
    rs.id   = (void *) 1;
    
    assert(manager->get_currentversion(manager, (void *) 1) == 2);
    assert(0 == manager->tab->find(manager->tab, (void *) 1, &desc));
    assert(((dispatch_base_node_t *) desc)->token_inuse == FIRST_TOTAL);
    assert(((dispatch_base_node_t *) desc)->token_quota == FIRST_TOTAL);
    
    assert(((dispatch_base_node_t *) desc)->token_quota_target
           == FIRST_TOTAL - MMAX_RESPOND_STEP);
    
    
    left_len = FIRST_TOTAL / 2;
    for ( i  = 0; i < FIRST_TOTAL / 2; ++i )
    {
        if ( left_len == 0 )
        {
            break;
        }
        rs.cost = MIN(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = free_resource(manager, &rs,&could_alloc);
        assert(err == 0);
        left_len -= rs.cost;
    }
    
    assert(manager->get_currentversion(manager, (void *) 1) == 2);
    assert(0 == manager->tab->find(manager->tab, (void *) 1, &desc));
    assert(((dispatch_base_node_t *) desc)->token_inuse == FIRST_TOTAL / 2);
    assert(((dispatch_base_node_t *) desc)->token_quota
           == FIRST_TOTAL / 2 );
    assert(((dispatch_base_node_t *) desc)->token_quota_target
           == FIRST_TOTAL / 2);
    assert(is_increased == false);
    
    left_len = FIRST_TOTAL / 2;
    for ( i  = 0; i < FIRST_TOTAL / 2; ++i )
    {
        if ( left_len == 0 )
        {
            break;
        }
        rs.cost = MIN(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = free_resource(manager, &rs, &could_alloc);
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


static void test_case_rcvd_reduce_alloc()
{
    bool could_alloc = false;
    unsigned long  left_len = FIRST_TOTAL;
    int            i        = 0;
    int            err      = 0;
    void           *desc;
    dispatch2app_t dta;
    dta.version     = 1;
    dta.token_quota = FIRST_TOTAL;
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
        rs.cost = MIN(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = try_alloc_resource(manager, &rs);
        assert(err == 0);
        left_len -= rs.cost;
    }
    dta.version     = 2;
    dta.token_quota = FIRST_TOTAL / 2;
    is_increased = false;
    manager->msg_event
            .rcvd(&manager->msg_event, (void *) 1, sizeof(dispatch2app_t),
                  (unsigned char *) &dta);
    assert(is_increased == false);
    
    rs.cost = 1;
    rs.id   = (void *) 1;
    
    assert(manager->get_currentversion(manager, (void *) 1) == 2);
    assert(0 == manager->tab->find(manager->tab, (void *) 1, &desc));
    assert(((dispatch_base_node_t *) desc)->token_inuse == FIRST_TOTAL);
    assert(((dispatch_base_node_t *) desc)->token_quota == FIRST_TOTAL);
    assert(((dispatch_base_node_t *) desc)->token_quota_target
           == FIRST_TOTAL - MMAX_RESPOND_STEP);
    
    
    left_len = FIRST_TOTAL / 2;
    for ( i  = 0; i < MMAX_RESPOND_STEP; ++i )
    {
        if ( left_len == 0 )
        {
            break;
        }
        rs.cost = MIN(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = free_resource(manager, &rs,&could_alloc);
        assert(err == 0);
        left_len -= rs.cost;
    }
    assert(could_alloc);
    rs.cost = 1;
    rs.id   = (void *) 1;
    err = try_alloc_resource(manager, &rs);
    assert(err == 0);
    
    err = try_alloc_resource(manager, &rs);
    assert(err == QOS_ERROR_PENDING);
    
    err = free_resource(manager, &rs,&could_alloc);
    assert(!could_alloc);
    assert(err == 0);
    
    for ( ; i < FIRST_TOTAL / 2; ++i )
    {
        if ( left_len == 0 )
        {
            break;
        }
        rs.cost = MIN(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = free_resource(manager, &rs,&could_alloc);
        assert(err == 0);
        left_len -= rs.cost;
    }
    
    assert(manager->get_currentversion(manager, (void *) 1) == 2);
    assert(0 == manager->tab->find(manager->tab, (void *) 1, &desc));
//    printf("token_quota %ld\n",((dispatch_base_node_t *) desc)->token_inuse);
    
    assert(((dispatch_base_node_t *) desc)->token_inuse == FIRST_TOTAL / 2);
    assert(((dispatch_base_node_t *) desc)->token_quota
           == FIRST_TOTAL / 2);
    assert(is_increased == false);
    
    left_len = FIRST_TOTAL / 2;
    for ( i  = 0; i < FIRST_TOTAL / 2; ++i )
    {
        if ( left_len == 0 )
        {
            break;
        }
        rs.cost = MIN(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = free_resource(manager, &rs,&could_alloc);
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


static void test_case_regular_reset()
{
    bool could_alloc;
    unsigned long  left_len = FIRST_TOTAL;
    int            i        = 0;
    int            err      = 0;
    void           *desc;
    dispatch2app_t dta;
    dta.version     = 1;
    dta.token_quota = FIRST_TOTAL;
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
        rs.cost = MIN(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = try_alloc_resource(manager, &rs);
        assert(err == 0);
        left_len -= rs.cost;
    }
    dta.version     = 0;
    dta.token_quota = FIRST_TOTAL / 2;
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
        rs.cost = MIN(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = free_resource(manager, &rs,&could_alloc);
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
        rs.cost = MIN(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = free_resource(manager, &rs,&could_alloc);
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

static void test_case_regular_rcvd_increase()
{
    unsigned long  left_len = FIRST_TOTAL / 2;
    int            i        = 0;
    int            err      = 0;
    void           *desc;
    dispatch2app_t dta;
    dta.version     = 1;
    dta.token_quota = FIRST_TOTAL / 2;
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
        rs.cost = MIN(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = try_alloc_resource(manager, &rs);
        assert(err == 0);
        left_len -= rs.cost;
    }
    
    err = try_alloc_resource(manager, &rs);
    assert(err != 0);
    
    dta.version     = 2;
    dta.token_quota = FIRST_TOTAL;
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
        rs.cost = MIN(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
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

static void test_case_regular_alloc_free()
{
    bool could_alloc = false;
    unsigned long  left_len = FIRST_TOTAL;
    int            i        = 0;
    int            err      = 0;
    void           *desc;
    dispatch2app_t dta;
    dta.version     = 1;
    dta.token_quota = FIRST_TOTAL;
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
        rs.cost = MIN(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
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
        rs.cost = MIN(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = free_resource(manager, &rs, &could_alloc);
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


static int
check_alloc_resource(struct disp_desc_manager *manager, resource_t *rs)
{
    int  err;
    void *desc = NULL;
    assert(manager && rs && rs->cost);
    pthread_rwlock_rdlock(&manager->lck);
    err = manager->tab->find(manager->tab, rs->id, &desc);
    if ( err )
        end_func(err, QOS_ERROR_FAILEDNODE, unlock_manager);
    assert(desc != NULL);
    
    err = ((dispatch_base_node_t *) desc)
            ->check_alloc_from_base(desc, rs->cost);
    if ( err )
        end_func(err, QOS_ERROR_PENDING, unlock_manager);
unlock_manager:
    pthread_rwlock_unlock(&manager->lck);
    return err;
}


static void test_case_regular_check_free()
{
    bool           could_alloc = false;
    unsigned long  left_len    = FIRST_TOTAL;
    int            i           = 0;
    int            err         = 0;
    void           *desc;
    dispatch2app_t dta;
    dta.version     = 1;
    dta.token_quota = FIRST_TOTAL;
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
        rs.cost = MIN(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
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
        rs.cost = MIN(rs_cost_list[i % RESOURCE_COST_LIST_NUM], left_len);
        rs.id   = (void *) 1;
        err = free_resource(manager, &rs, &could_alloc);
        assert(err == 0);
        assert(could_alloc);
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

static int test_init()
{
    printf(YELLOW"--------------test_context_init----------------:\n"RESET);
    test_case_init_exit(manager);
    
    return CUE_SUCCESS;
}

static int test_clean()
{
    disp_desc_manager_exit(manager);
    printf(BLUE"--------------test_clean end--------------:\n"RESET);
    return CUE_SUCCESS;
}

void base_node_suit_init(test_frame_t *frame)
{
    test_suit_t suit;
    
    test_suit_init(&suit, "base_node_suit_init", test_init, test_clean);
    
    suit.add_case(&suit, "test_case_regular_online_offline",
                  test_case_regular_online_offline);
    
    suit.add_case(&suit, "test_case_regular_rcvd",
                  test_case_regular_rcvd);
    
    suit.add_case(&suit, "test_case_regular_alloc_free",
                  test_case_regular_alloc_free);
    
    suit.add_case(&suit, "test_case_regular_check_free",
                  test_case_regular_check_free);
    
    suit.add_case(&suit, "test_case_regular_rcvd_reduce",
                  test_case_regular_rcvd_reduce);
    
    suit.add_case(&suit, "test_case_rcvd_reduce_alloc",
                  test_case_rcvd_reduce_alloc);
    
    suit.add_case(&suit, "test_case_regular_rcvd_increase",
                  test_case_regular_rcvd_increase);
    
    suit.add_case(&suit, "test_case_regular_reset",
                  test_case_regular_reset);
    
    frame->add_suit(frame, &suit);
}
