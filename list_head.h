#ifndef _LIST_HEAD
#define _LIST_HEAD

/**
ptr是指向list_head类型链表的指针
type为一个包含list_head结构的结构类型
member为结构中类型为list_head的域

  如果data现在在0地址上，那么&（type *)0->member就是从0地址到list_head的偏移量（相对长度），
  说白了就是数据域data在 此结构中的占多长的空间。
  这样如果我们有一个绝对的地址ptr（list_head类型）那么： 
  
	   绝对地址 - 相对地址 = 包含list_head结构的绝对地址
*/
#define offset_of(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define container_of(ptr, type, member) ({  \
const typeof( ((type *)0)->member ) *__mptr = (ptr); \
(type *)( (char *)__mptr - offset_of(type,member) );})

struct list_head
{
    struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) {&(name), &(name)}

#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)

#define list_entry(ptr, type, member)   ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

#define list_for_each(pos, head) for(pos = (head)->next; pos != (head); pos = pos->next)

static int list_empty(struct list_head *head)
{
    return head->next == head;
}

static void __list_add(struct list_head *newNode, struct list_head *prev,
                       struct list_head *next)
{
    next->prev    = newNode;
    newNode->next = next;
    newNode->prev = prev;
    prev->next    = newNode;
}

static void list_add(struct list_head *newNode, struct list_head *head)
{
    __list_add(newNode, head, head->next);
}

static void list_add_tail(struct list_head *newNode, struct list_head *head)
{
    __list_add(newNode, head->prev, head);
}

static void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

static void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
}

static void LISTHEAD_INIT(struct list_head *head)
{
    head->next = head;
    head->prev = head;
}

#endif
