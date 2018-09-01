//
// Created by root on 18-8-14.
//

#include <stdbool.h>

#ifndef QOS_FENCE_EXECUTOR_H
#define QOS_FENCE_EXECUTOR_H

#ifdef __cplusplus
extern "C" {
#endif

struct fence_executor;
typedef void (*fence_executor_func_t)(struct fence_executor *executor, void *ctx);

//同一时刻只让一个执行
typedef struct fence_executor
{
    /**private:**/
    bool is_exe;
    fence_executor_func_t func;
    /**public:**/
    void (*execute)(struct fence_executor *executor, void *ctx);
}fence_executor_t;


int fence_executor_init(fence_executor_t *executor, fence_executor_func_t func);
void fence_executor_exit(fence_executor_t *executor);

void test_fence_executor(fence_executor_t *executor);
#ifdef __cplusplus
}
#endif
#endif //QOS_FENCE_EXECUTOR_H
