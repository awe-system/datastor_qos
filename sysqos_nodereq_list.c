//
// Created by root on 18-7-25.
//

#include <assert.h>
#include <pthread.h>
#include "sysqos_nodereq_list.h"

static void push_back(nodereq_list_t *tokens, resource_list_t *rs)
{
    assert(tokens && rs);
    LISTHEAD_INIT(&rs->list);
    sysqos_rwlock_wrlock(&tokens->lck);
    list_add_tail(&rs->list, &tokens->token_list);
    tokens->press += rs->rs.cost;
    sysqos_rwlock_wrunlock(&tokens->lck);
}

resource_list_t *front(nodereq_list_t *tokens)
{
    resource_list_t *rs = NULL;
    assert(tokens);
    sysqos_rwlock_rdlock(&tokens->lck);
    if ( !list_empty(&tokens->token_list) )
    {
        rs = container_of(tokens->token_list.next, resource_list_t, list);
    }
    sysqos_rwlock_rdunlock(&tokens->lck);
    return rs;
}

static int erase(nodereq_list_t *tokens, resource_list_t *rs)
{
    assert(tokens && rs);
    sysqos_rwlock_wrlock(&tokens->lck);
    tokens->press -= rs->rs.cost;
    list_del(&rs->list);
    sysqos_rwlock_wrunlock(&tokens->lck);
    return 0;
}

static void pop_all(nodereq_list_t *tokens, struct list_head *head)
{
    resource_list_t *rs = NULL;
    assert(tokens && head);
    sysqos_rwlock_wrlock(&tokens->lck);
    if ( !list_empty(&tokens->token_list) )
    {
        rs = container_of(tokens->token_list.next, resource_list_t, list);
        tokens->press = 0;
        list_del(&tokens->token_list);
        LISTHEAD_INIT(&tokens->token_list);
    }
    sysqos_rwlock_wrunlock(&tokens->lck);
    if ( rs )
    {
        list_add_tail(head, &rs->list);
    }
}

static void pop_front(nodereq_list_t *tokens)
{
    resource_list_t *rs = NULL;
    assert(tokens);
    sysqos_rwlock_wrlock(&tokens->lck);
    if ( !list_empty(&tokens->token_list) )
    {
        rs = container_of(tokens->token_list.next, resource_list_t, list);
        tokens->press -= rs->rs.cost;
        list_del(tokens->token_list.next);
    }
    sysqos_rwlock_wrunlock(&tokens->lck);
}

static press_t get_press(nodereq_list_t *tokens)
{
    press_t p;
    p.type = press_type_fifo;
    assert(tokens);
    sysqos_rwlock_rdlock(&tokens->lck);
    p.fifo.depth = tokens->press; //FIFO
    sysqos_rwlock_rdunlock(&tokens->lck);
    return p;
}

int nodereq_list_init(nodereq_list_t *tokens)
{
    tokens->press = 0;
    LISTHEAD_INIT(&tokens->token_list);
    tokens->get_press = get_press;
    tokens->front     = front;
    tokens->push_back = push_back;
    tokens->erase     = erase;
    tokens->pop_all   = pop_all;
    tokens->pop_front = pop_front;
    return sysqos_rwlock_init(&tokens->lck);
}

void nodereq_list_exit(nodereq_list_t *tokens)
{
    sysqos_rwlock_destroy(&tokens->lck);
}

