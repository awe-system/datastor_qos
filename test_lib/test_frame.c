//
// Created by root on 18-9-4.
//

#define BLUE "\033[34m"
#define YELLOW "\033[32m"
#define RED "\033[31m"
#define GREEN "\033[36m"
#define RESET "\033[0m"

#include <assert.h>
#include <stdlib.h>
#include "test_frame.h"
#include "CUnit/Basic.h"

static void add_case(test_suit_t *suit,
                     const char *case_name, case_func_t case_func)
{
    assert(suit->case_num < MAX_CASE_NUM);
    suit->cases[suit->case_num].case_name = case_name;
    suit->cases[suit->case_num].case_func = case_func;
    ++suit->case_num;
}

void test_suit_init(test_suit_t *suit, const char *suit_name,
                    init_suit_func_t init, clean_suit_func_t clean)
{
    suit->init     = init;
    suit->clean    = clean;
    suit->case_num = 0;
    suit->name     = suit_name;
    suit->pSuite   = NULL;
    suit->add_case = add_case;
    memset(suit->cases, 0, sizeof(test_case_t) * MAX_CASE_NUM);
}


static void add_suit(struct test_frame *frame, test_suit_t *suit)
{
    int i = 0;
    suit->pSuite = CU_add_suite(suit->name, suit->init, suit->clean);
    assert ( NULL != suit->pSuite );
    
    for ( i = 0; i < suit->case_num; ++i )
    {
        assert ( NULL !=
             CU_add_test(suit->pSuite,
                         suit->cases[i].case_name, suit->cases[i].case_func) );
    }
}

static void run(struct test_frame *frame)
{
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
}

void test_frame_init(test_frame_t *frame)
{
    printf(YELLOW"\n--------------%s start--------------------:\n"RESET,
           __DATE__);
    if ( CUE_SUCCESS != CU_initialize_registry() )
        abort();
    frame->run      = run;
    frame->add_suit = add_suit;
}

void test_frame_exit(test_frame_t *frame)
{
    CU_cleanup_registry();
    assert(CUE_SUCCESS == CU_get_error());
    printf(BLUE"\n----------------%s end-----------------\n"RESET, __DATE__);
}
