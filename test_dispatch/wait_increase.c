//
// Created by root on 18-9-10.
//

#include "wait_increase.h"

wait_increase_list_t q_static;
wait_increase_list_t *q = &q_static;


#define  MAX_QUEUE_NUM 1024
typedef struct test_context
{
    void       *queue[MAX_QUEUE_NUM + 1];
    int        begin;
    int        end;
    app_node_t *nodes[MAX_QUEUE_NUM];
}                    test_context_t;

static void test_context_init(test_context_t *test)
{
    int i = 0;
    test->begin = test->end = 0;
    memset(test->queue, 0, (MAX_QUEUE_NUM + 1) * sizeof(void *));
    for ( i = 0; i < MAX_QUEUE_NUM; ++i )
    {
        test->nodes[i] = (app_node_t *) malloc(sizeof(app_node_t));
        app_node_init(test->nodes[i], 0, 0);
    }
}

static void test_context_exit(test_context_t *test)
{
    int i = 0;
    for ( i = 0; i < MAX_QUEUE_NUM; ++i )
    {
        app_node_exit(test->nodes[i]);
        free(test->nodes[i]);
    }
}

static void test_enqueue(test_context_t *test, void *val)
{
    test->queue[test->end] = val;
    if ( ++test->end > MAX_QUEUE_NUM )
    {
        test->end = 0;
    }
    assert(test->end != test->begin);
}

static void *test_dequeue(test_context_t *test)
{
    void *res = NULL;
    if ( test->end == test->begin )
    {
        return res;
    }
    res = test->queue[test->begin];
    if ( ++test->begin > MAX_QUEUE_NUM )
    {
        test->begin = 0;
    }
    return res;
}

static void test_case_init_exit(wait_increase_list_t *q)
{
    int err = wait_increase_list_init(q, MAX_QUEUE_NUM);
    assert(err == 0);
    
    wait_increase_list_exit(q);
    err = wait_increase_list_init(q, MAX_QUEUE_NUM);
    assert(err == 0);
    printf("[test_case_init_exit] %s[OK]%s\n", GREEN, RESET);
}

static bool test_check_dequeue(wait_increase_list_t *q, test_context_t *test)
{
    app_node_t *node;
    node = test_dequeue(test);
    assert(q && test);
    if ( node )
    {
        app_node_t *tmp_q = NULL;
        assert(q->is_first_exist(q, &tmp_q));
        assert(tmp_q == node);
        q->erase(q, node);
        return true;
    }
    else
    {
        return false;
    }
}

static void test_case_endequeue()
{
    int            i = 0;
    test_context_t test;
    app_node_t     *node;
    
    test_context_init(&test);
    
    for ( i = 0; i < MAX_QUEUE_NUM; i++ )
    {
        q->insert(q, test.nodes[i]);
        test_enqueue(&test, test.nodes[i]);
    }
    
    while ( test_check_dequeue(q, &test) );
    
    
    for ( i = 0; i < MAX_QUEUE_NUM; i++ )
    {
        q->insert(q, test.nodes[i]);
        test_enqueue(&test, test.nodes[i]);
        if ( rand() % 2 == 0 )
        {
            test_check_dequeue(q, &test);
        }
    }
    
    while ( test_check_dequeue(q, &test) );
    
    assert(!q->is_first_exist(q, &node));
    
    test_context_exit(&test);
    printf("[test_case_endequeue] %s[OK]%s\n", GREEN, RESET);
}


static void test_case_enerasequeue()
{
    int            i = 0;
    test_context_t test;
    test_context_t test_item;
    app_node_t *node;
    
    test_context_init(&test);
    test_context_init(&test_item);
    
    q->insert(q, test.nodes[1]);
    q->insert(q, test.nodes[2]);
    q->insert(q, test.nodes[3]);
    
    q->erase(q,test.nodes[1]);
    q->erase(q,test.nodes[3]);
    q->erase(q,test.nodes[2]);
    
    assert(!q->is_first_exist(q, &node));
    
    for ( i = 0; i < MAX_QUEUE_NUM; i++ )
    {
        q->insert(q, test.nodes[i]);
       
        test_enqueue(&test, test.nodes[i]);
        test_enqueue(&test_item, test.nodes[i]);
    }
    node     = test_dequeue(&test);
    while ( node )
    {
        app_node_t *tmp1 = NULL;
        void *tmp2 = test_dequeue(&test_item);

        assert(q->is_first_exist(q, &tmp1));
        assert(tmp1 == node);
        q->erase(q, tmp2);
        node = test_dequeue(&test);
    }

    assert(!q->is_first_exist(q, &node));
    test_context_exit(&test);
    test_context_exit(&test_item);
    printf("[test_case_enerasequeue] %s[OK]%s\n", GREEN, RESET);
}

static int test_init()
{
    printf(YELLOW"--------------test_init----------------------:\n"RESET);
    test_case_init_exit(q);
    return CUE_SUCCESS;
}

static int test_clean()
{
    wait_increase_list_exit(q);
    printf(BLUE"----------------test_clean-----------------\n"RESET);
    return CUE_SUCCESS;
}

void wait_increase_suit_init(test_frame_t *frame)
{
    test_suit_t suit;
    
    test_suit_init(&suit, "base_node_suit_init", test_init, test_clean);
    
    suit.add_case(&suit, "test_case_endequeue", test_case_endequeue);
    suit.add_case(&suit, "test_case_enerasequeue", test_case_enerasequeue);
    frame->add_suit(frame, &suit);
}
