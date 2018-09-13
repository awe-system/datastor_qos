//
// Created by root on 18-8-14.
//

#include "count_controller.h"

static inline int up_cnt(int cnt, int max, bool *is_max)
{
    int res = 0;
    *is_max = true;
    if ( cnt + 1 < max )
    {
        *is_max = false;
        res = cnt + 1;
    }
    return res;
}

//nolock
static bool test_cnt(count_controller_t *controller)
{
    bool res = false;
    int  old = controller->cnt;
    while ( !__sync_bool_compare_and_swap(&controller->cnt,
                                          old,
                                          up_cnt(controller->cnt,
                                                 controller->max, &res)) )
    {
        old = controller->cnt;
    }
    return res;
}

static void clear(count_controller_t *controller)
{
    controller->cnt = 0;
}

int count_controller_init(count_controller_t *controller, int interval_cnt)
{
    controller->cnt = 0;
    controller->max = interval_cnt;
    controller->clear = clear;
    controller->test_cnt = test_cnt;
    return 0;
}

void count_controller_exit(count_controller_t *controller)
{
}
