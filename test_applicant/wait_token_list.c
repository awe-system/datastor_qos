//
// Created by root on 18-9-5.
//



#include <test_frame.h>
#include "sysqos_msg.h"
#include "wait_token_list.h"

#define TABLE_LEN      128
//#define MAX_NODE_NUM   4096
#define TMP_QUEUE_LEN  200

typedef struct token_manager_event
{
    void (*node_offline_done)(struct token_manager_event *event,
                              struct list_head *head);
} token_manager_event_t;

typedef struct token_manager
{
    struct msg_event_ops event;
    
    int (*push_back)(struct token_manager *manager, resource_list_t *rs);
    
    //FIXME将某一个资源从token_manager erase
    int (*erase)(struct token_manager *manager, resource_list_t *rs);
    
    resource_list_t *(*front)(struct token_manager *manager, void *id);
    
    void (*pop_front)(struct token_manager *manager, void *id);
    
    press_t (*get_press)(struct token_manager *manager, void *id);
    
    void (*set_compare)(struct token_manager *manager, compare_id_func);
    
    void (*set_hash)(struct token_manager *manager, hash_func);
    
    /****************************************************/
    token_manager_event_t *event_to_permission;
    memory_cache_t        cache;
    hash_table_t          *tokens;
    pthread_rwlock_t      lck;
} token_manager_t;


static token_manager_t manager_static;
static token_manager_t *manager = &manager_static;

static inline token_manager_t *token_manager_by_event(struct msg_event_ops *ops)
{
    return container_of(ops, token_manager_t, event);
}

static void node_online(struct msg_event_ops *ops, void *id)
{
    int             err;
    token_manager_t *manager = token_manager_by_event(ops);
    
    nodereq_list_t *tokens = manager->cache.alloc(&manager->cache);
    assert(tokens);
    nodereq_list_init(tokens);
    pthread_rwlock_wrlock(&manager->lck);
    err = manager->tokens->insert(manager->tokens, id, tokens);
    pthread_rwlock_unlock(&manager->lck);
    if ( err )
    {
        nodereq_list_exit(tokens);
        manager->cache.free(&manager->cache, tokens);
    }
}

static void node_offline(struct msg_event_ops *ops, void *id)
{
    token_manager_t *manager = token_manager_by_event(ops);
    void            *tokens  = NULL;
    int             err;
    assert(manager->event_to_permission);
    pthread_rwlock_wrlock(&manager->lck);
    err = manager->tokens->erase(manager->tokens, id, &tokens);
    pthread_rwlock_unlock(&manager->lck);
    if ( err == 0 && tokens )
    {
        struct list_head head;
        LISTHEAD_INIT(&head);
        ((nodereq_list_t *) tokens)->pop_all((nodereq_list_t *) tokens, &head);
        manager->event_to_permission
                ->node_offline_done(manager->event_to_permission, &head);
        nodereq_list_exit((nodereq_list_t *) tokens);
        manager->cache.free(&manager->cache, tokens);
    }
}

static int tpush_back(struct token_manager *manager, resource_list_t *rs)
{
    int  err;
    void *tokens = NULL;
    pthread_rwlock_rdlock(&manager->lck);
    do
    {
        err = manager->tokens->find(manager->tokens, rs->rs.id, &tokens);
        if ( err )
        { break; }
        ((nodereq_list_t *) tokens)->push_back((nodereq_list_t *) tokens, rs);
    } while ( 0 );
    pthread_rwlock_unlock(&manager->lck);
    return err;
}

static int terase(struct token_manager *manager, resource_list_t *rs)
{
    int  err;
    void *tokens = NULL;
    pthread_rwlock_rdlock(&manager->lck);
    do
    {
        err = manager->tokens->find(manager->tokens, rs->rs.id, &tokens);
        if ( err )
        { break; }
        err = ((nodereq_list_t *) tokens)->erase((nodereq_list_t *) tokens, rs);
    } while ( 0 );
    pthread_rwlock_unlock(&manager->lck);
    return err;
}

static resource_list_t *tfront(struct token_manager *manager, void *id)
{
    int             err;
    resource_list_t *rs     = NULL;
    void            *tokens = NULL;
    pthread_rwlock_rdlock(&manager->lck);
    do
    {
        err = manager->tokens->find(manager->tokens, id, &tokens);
        if ( err )
        { break; }
        rs = ((nodereq_list_t *) tokens)->front((nodereq_list_t *) tokens);
    } while ( 0 );
    pthread_rwlock_unlock(&manager->lck);
    return rs;
}

static void tpop_front(struct token_manager *manager, void *id)
{
    int  err;
    void *tokens = NULL;
    pthread_rwlock_rdlock(&manager->lck);
    do
    {
        err = manager->tokens->find(manager->tokens, id, &tokens);
        if ( err )
        { break; }
        ((nodereq_list_t *) tokens)->pop_front((nodereq_list_t *) tokens);
    } while ( 0 );
    pthread_rwlock_unlock(&manager->lck);
}

static press_t tget_press(struct token_manager *manager, void *id)
{
    int     err;
    press_t press;
    press.val = 0;
    void *tokens = NULL;
    pthread_rwlock_rdlock(&manager->lck);
    do
    {
        err = manager->tokens->find(manager->tokens, id, &tokens);
        if ( err )
        { break; }
        press = ((nodereq_list_t *) tokens)
                ->get_press((nodereq_list_t *) tokens);
    } while ( 0 );
    pthread_rwlock_unlock(&manager->lck);
    return press;
}

static void tset_compare(struct token_manager *manager, compare_id_func compare)
{
    assert(compare && manager->tokens);
    manager->tokens->set_compare(manager->tokens, compare);
}

static void tset_hash(struct token_manager *manager, hash_func hash)
{
    assert(hash && manager->tokens);
    manager->tokens->set_hash(manager->tokens, hash);
}


static int token_manager_init(token_manager_t *manager,
                              token_manager_event_t *event, int tab_len,
                              int max_node_num)
{
    int err = memory_cache_init(&manager->cache, sizeof(nodereq_list_t),
                                (unsigned long) max_node_num);
    if ( err ) end_func(err, QOS_ERROR_MEMORY, memory_cache_init_failed);
    
    manager->tokens = alloc_hash_table(tab_len, (unsigned long) max_node_num);
    if ( !manager->tokens )
        end_func(err, QOS_ERROR_MEMORY, alloc_hash_table_failed);
    
    err = pthread_rwlock_init(&manager->lck, NULL);
    if ( err ) end_func(err, QOS_ERROR_MEMORY, pthread_rwlock_init_failed);
    
    manager->event_to_permission = event;
    manager->push_back           = tpush_back;
    manager->pop_front           = tpop_front;
    manager->front               = tfront;
    manager->event.node_online   = node_online;
    manager->event.node_offline  = node_offline;
    manager->get_press           = tget_press;
    manager->set_compare         = tset_compare;
    manager->set_hash            = tset_hash;
    manager->erase               = terase;
    
    return QOS_ERROR_OK;
//    pthread_rwlock_destroy(&manager->lck);
pthread_rwlock_init_failed:
    free_hash_table(manager->tokens);
alloc_hash_table_failed:
    memory_cache_exit(&manager->cache);
memory_cache_init_failed:
    return err;
}

static void token_manager_exit(token_manager_t *manager)
{
    assert(manager && manager->tokens);
    pthread_rwlock_destroy(&manager->lck);
    free_hash_table(manager->tokens);
    memory_cache_exit(&manager->cache);
}

static void test_node_offline_done(struct token_manager_event *event,
                                   struct list_head *head)
{
    int             i   = 0;
    resource_list_t *rs = NULL;
    if ( list_empty(head) )
    {
        return;
    }
    rs      = container_of(head->next, resource_list_t, list);
    for ( i = 0; i < TMP_QUEUE_LEN; ++i )
    {
        assert(rs);
        assert(rs->rs.id == (void *) 1);
        assert(rs->rs.cost == i + 1);
        rs = container_of(rs->list.next, resource_list_t, list);
    }
}

static void test_case_offline_done()
{
    int             err;
    int             i = 0;
    resource_list_t rs[TMP_QUEUE_LEN];
    for ( i = 0; i < TMP_QUEUE_LEN; ++i )
    {
        rs[i].rs.id   = (void *) 1;
        rs[i].rs.cost = (unsigned long) (i + 1);
    }
    manager->event.node_online(&manager->event, (void *) 1);
    
    for ( i = 0; i < TMP_QUEUE_LEN; ++i )
    {//NOTE：可能node online多次 但不影响测试
        err = manager->push_back(manager, &rs[i]);
        assert(err == 0);
    }
    manager->event.node_offline(&manager->event, (void *) 1);

#ifdef CACHE_OPEN_CNT
    assert(manager->cache.alloc_cnt == manager->cache.free_cnt);
    assert(manager->tokens->cache.alloc_cnt == manager->tokens->cache.free_cnt);
#endif
    printf("[test_case_offline_done] %s[OK]%s\n", GREEN, RESET);
}

static token_manager_event_t test_token_manger = {
        .node_offline_done = test_node_offline_done,
};

static void test_case_init_exit(token_manager_t *manager)
{
    int err = token_manager_init(manager, &test_token_manger, TABLE_LEN,
                                 MAX_NODE_NUM);
    assert(err == 0);
    manager->set_compare(manager, test_compare);
    manager->set_hash(manager, test_hash);
    token_manager_exit(manager);
    
    err = token_manager_init(manager, &test_token_manger, TABLE_LEN,
                             MAX_NODE_NUM);
    assert(err == 0);
    manager->set_compare(manager, test_compare);
    manager->set_hash(manager, test_hash);
    
    printf("[test_case_init_exit] %s[OK]%s\n", GREEN, RESET);
}


static void test_case_regular_online_offline()
{
    int             err;
    press_t         p;
    resource_list_t rs;
    resource_list_t *prs;
    
    rs.rs.cost = 1;
    rs.rs.id   = (void *) 1;
    err = manager->push_back(manager, &rs);
    assert(err);
    prs = manager->front(manager, (void *) 1);
    assert(NULL == prs);
    p = manager->get_press(manager, (void *) 1);
    assert(p.val == 0);
    manager->pop_front(manager, (void *) 1);
    manager->event.node_online(&manager->event, (void *) 1);
    manager->event.node_online(&manager->event, (void *) 1);
    manager->event.node_online(&manager->event, (void *) 2);
    manager->event.node_offline(&manager->event, (void *) 2);
    manager->event.node_offline(&manager->event, (void *) 1);
    manager->event.node_offline(&manager->event, (void *) 2);

#ifdef CACHE_OPEN_CNT
    assert(manager->cache.alloc_cnt == manager->cache.free_cnt);
    assert(manager->tokens->cache.alloc_cnt == manager->tokens->cache.free_cnt);
#endif
    printf("[test_case_regular_online_offline] %s[OK]%s\n", GREEN, RESET);
}

static void test_case_regular_push_pop()
{
    int             err;
    int             i = 0;
    resource_list_t rs[TMP_QUEUE_LEN];
    for ( i = 0; i < TMP_QUEUE_LEN; ++i )
    {
        rs[i].rs.id   = (void *) 1;
        rs[i].rs.cost = (unsigned long) (i + 1);
    }
    resource_list_t *prs;
    manager->event.node_online(&manager->event, (void *) 1);
    
    for ( i = 0; i < TMP_QUEUE_LEN; ++i )
    {
        err = manager->push_back(manager, &rs[i]);
        assert(err == 0);
    }
    
    for ( i = 0; i < TMP_QUEUE_LEN; ++i )
    {
        prs = manager->front(manager, (void *) 1);
        assert(prs == &rs[i]);
        manager->pop_front(manager, (void *) 1);
    }
    assert(NULL == manager->front(manager, (void *) 1));
    manager->event.node_offline(&manager->event, (void *) 1);

#ifdef CACHE_OPEN_CNT
    assert(manager->cache.alloc_cnt == manager->cache.free_cnt);
    assert(manager->tokens->cache.alloc_cnt == manager->tokens->cache.free_cnt);
#endif
    printf("[test_case_regular_push_pop] %s[OK]%s\n", GREEN, RESET);
}


static void test_case_regular_push_erase()
{
    int             err;
    int             i = 0;
    resource_list_t rs[TMP_QUEUE_LEN];
    for ( i = 0; i < TMP_QUEUE_LEN; ++i )
    {
        rs[i].rs.id   = (void *) 1;
        rs[i].rs.cost = (unsigned long) (i + 1);
    }
    resource_list_t *prs;
    manager->event.node_online(&manager->event, (void *) 1);
    
    for ( i = 0; i < TMP_QUEUE_LEN; ++i )
    {
        err = manager->push_back(manager, &rs[i]);
        assert(err == 0);
    }
    
    for ( i = 0; i < TMP_QUEUE_LEN; ++i )
    {
        prs = manager->front(manager, (void *) 1);
        assert(prs == &rs[i]);
        manager->erase(manager, rs + i);
    }
    assert(NULL == manager->front(manager, (void *) 1));
    manager->event.node_offline(&manager->event, (void *) 1);

#ifdef CACHE_OPEN_CNT
    assert(manager->cache.alloc_cnt == manager->cache.free_cnt);
    assert(manager->tokens->cache.alloc_cnt == manager->tokens->cache.free_cnt);
#endif
    printf("[test_case_regular_push_erase] %s[OK]%s\n", GREEN, RESET);
}


static void test_case_regular_press()
{
    int             err;
    press_t         presses[TMP_QUEUE_LEN / 3];
    int             i = 0;
    resource_list_t rs[TMP_QUEUE_LEN];
    for ( i = 0; i < TMP_QUEUE_LEN; ++i )
    {
        unsigned long j = (unsigned long) (i % (TMP_QUEUE_LEN / 3));
        rs[i].rs.id           = (void *) j;
        rs[i].rs.cost         = (unsigned long) (i + 1);
        presses[j].type       = press_type_fifo;
        presses[j].fifo.depth = 0;
    }
    resource_list_t *prs;
    
    for ( i = 0; i < TMP_QUEUE_LEN; ++i )
    {//NOTE：可能node online多次 但不影响测试
        unsigned long j = (unsigned long) (i % (TMP_QUEUE_LEN / 3));
        manager->event.node_online(&manager->event, rs[i].rs.id);
        err = manager->push_back(manager, rs + i);
        assert(err == 0);
        presses[j].fifo.depth += rs[i].rs.cost;
        if ( presses[j].fifo.depth !=
             manager->get_press(manager, rs[i].rs.id).fifo.depth )
        {
            assert(presses[j].fifo.depth ==
                   manager->get_press(manager, rs[i].rs.id).fifo.depth);
        }
        assert(presses[j].fifo.depth ==
               manager->get_press(manager, rs[i].rs.id).fifo.depth);
    }
    
    for ( i = 0; i < TMP_QUEUE_LEN; ++i )
    {
        prs = manager->front(manager, rs[i].rs.id);
        assert(prs == &rs[i]);
        manager->pop_front(manager, rs[i].rs.id);
    }
    
    for ( i = 0; i < TMP_QUEUE_LEN / 3; ++i )
    {
        unsigned long j = (unsigned long) i;
        assert(0 == manager->get_press(manager, (void *) j).fifo.depth);
        manager->event.node_offline(&manager->event, rs[i].rs.id);
    }

#ifdef CACHE_OPEN_CNT
    assert(manager->cache.alloc_cnt == manager->cache.free_cnt);
    assert(manager->tokens->cache.alloc_cnt == manager->tokens->cache.free_cnt);
#endif
    printf("[test_case_regular_press] %s[OK]%s\n", GREEN, RESET);
}

int test_init()
{
    srand((unsigned int) time(0));
    printf(YELLOW"--------------test_context_init----------------------:\n"RESET);
    test_case_init_exit(manager);
    return CUE_SUCCESS;
}

static int test_clean()
{
    token_manager_exit(manager);
    printf(BLUE"--------------test_clean----------------------:\n"RESET);
    return CUE_SUCCESS;
}

void wait_list_suit_init(test_frame_t *frame)
{
    test_suit_t suit;
    
    test_suit_init(&suit, "wait_list_suit_init", test_init, test_clean);
    
    suit.add_case(&suit, "test_case_regular_online_offline",
                  test_case_regular_online_offline);
    
    suit.add_case(&suit, "test_case_regular_push_pop",
                  test_case_regular_push_pop);
    
    suit.add_case(&suit, "test_case_regular_push_erase",
                  test_case_regular_push_erase);
    
    suit.add_case(&suit, "test_case_regular_press",
                  test_case_regular_press);
    
    suit.add_case(&suit, "test_case_offline_done",
                  test_case_offline_done);
    
    frame->add_suit(frame, &suit);
}