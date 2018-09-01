//
// Created by root on 18-7-26.
//

#ifndef _QOS_QOS_PROTOCOL_H
#define _QOS_QOS_PROTOCOL_H

#include <unwind.h>
#include "sysqos_policy_param.h"

typedef struct app2dispatch
{
    press_t       press;
    long          version;
    unsigned long token_in_use;
} app2dispatch_t;

typedef struct dispatch2app
{
    unsigned long token_quota;
    long          version;
} dispatch2app_t;

static inline void init_app2dispatch(app2dispatch_t *atd)
{
    atd->press.val = 0;
    atd->version   = 0;
}

#endif //TEST_QOS_QOS_PROTOCOL_H
