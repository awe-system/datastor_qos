//
// Created by root on 18-7-24.
//

#include <stddef.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>


void *qos_alloc(unsigned long size)
{
    void *p = malloc(size);
    assert(p);
    memset(p, 0, size);
}

void qos_free(void *p)
{
    assert(p);
    free(p);
}


#include "sysqos_alloc.h"
