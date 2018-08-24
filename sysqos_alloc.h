//
// Created by root on 18-7-24.
//

#ifndef QOS_ZERO_ALLOC_H
#define QOS_ZERO_ALLOC_H

#ifdef __cplusplus
extern "C" {
#endif

void *qos_alloc(unsigned long size);

void qos_free(void *p);

#ifdef __cplusplus
}
#endif
#endif //TEST_QOS_ZERO_ALLOC_H
