//
// Created by root on 18-8-14.
//

#include <assert.h>
#include "fence_executor.h"

static void execute(struct fence_executor *executor, void *ctx)
{
    assert(executor->func);
    if ( __sync_bool_compare_and_swap(&executor->is_exe, false, true) )
    {
        executor->func(executor, ctx);
    }
}

int fence_executor_init(fence_executor_t *executor, fence_executor_func_t func)
{
    executor->is_exe  = false;
    executor->func    = func;
    executor->execute = execute;
    return 0;
}

void fence_executor_exit(fence_executor_t *executor)
{
}

void test_fence_executor(fence_executor_t *executor)
{
    //FIXME
}
