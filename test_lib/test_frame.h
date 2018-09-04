//
// Created by root on 18-9-4.
//

#ifndef TEST_QOS_TEST_FRAME_H
#define TEST_QOS_TEST_FRAME_H
#define TEST_CONCURRENCY
#include "CUnit/CUnit.h"

typedef int (*init_suit_func_t)(void);

typedef int (*clean_suit_func_t)(void);

typedef void (*case_func_t)(void);


typedef struct test_case
{
    void (*case_func)();
    
    const char *case_name;
}            test_case_t;
#define MAX_CASE_NUM 100
typedef struct test_suit
{
    /**private************************************/
    const char *name;
    int         case_num;
    test_case_t cases[MAX_CASE_NUM];
    CU_pSuite   pSuite;
    init_suit_func_t init;
    clean_suit_func_t clean;
    /**public*************************************/
    void (*add_case)(struct test_suit *,const char *, case_func_t);
} test_suit_t;

void test_suit_init(test_suit_t *suit,const char *suit_name,
                    init_suit_func_t init,clean_suit_func_t clean);

typedef struct test_frame
{
    void (*add_suit)(struct test_frame *frame, test_suit_t *suit);
    
    void (*run)(struct test_frame *frame);
} test_frame_t;

void test_frame_init(test_frame_t *frame);

void test_frame_exit(test_frame_t *frame);

#endif //TEST_QOS_TEST_FRAME_H
