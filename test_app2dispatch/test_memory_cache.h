//
// Created by root on 18-9-4.
//

#ifndef TEST_QOS_MEMORY_CACHE_C_H
#define TEST_QOS_MEMORY_CACHE_C_H
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sysqos_waitincrease_list.h>
#include "list_head.h"
#include "sysqos_alloc.h"
#include "sysqos_common.h"
#include "sysqos_test_lib.h"
#include <CUnit/Util.h>

void memory_cache_suit_init(test_frame_t *frame);

#endif //TEST_QOS_MEMORY_CACHE_C_H
