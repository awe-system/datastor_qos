//
// Created by root on 18-8-14.
//

#ifndef _QOS_COUNT_CONTROLLER_H
#define _QOS_COUNT_CONTROLLER_H

#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct count_controller
{
    /**private:**/
    int cnt;
    int max;
    /**public:**/
    bool (*test_cnt)(struct count_controller *);
    void (*clear)(struct count_controller *);
}count_controller_t;

int count_controller_init(count_controller_t *controller,int interval_cnt);

void count_controller_exit(count_controller_t *controller);

void test_count_controller();

#ifdef __cplusplus
}
#endif

#endif //TEST_QOS_COUNT_CONTROLLER_H
