//
// Created by root on 18-7-25.
//

#ifndef QOS_QOS_PRESS_H
#define QOS_QOS_PRESS_H

#ifdef __cplusplus
extern "C" {
#endif

#define PRESS_TYPE_BIT 2
#define PRESS_VAL_BIT  (sizeof(unsigned long)<<3 - PRESS_TYPE_BIT)
enum
{
    press_type_fifo,
    press_type_fail,
    press_type_min,
    press_type_max,
};

typedef struct fifo_press
{
    unsigned long type:PRESS_TYPE_BIT;
    unsigned long depth:PRESS_VAL_BIT;
} fifo_press_t;

typedef struct fail_press
{
    unsigned long type:PRESS_TYPE_BIT;
    unsigned long fail_num:PRESS_VAL_BIT;
} fail_press_t;

typedef struct min_press
{
    unsigned long type:PRESS_TYPE_BIT;
    unsigned long min_num:PRESS_VAL_BIT;
} min_press_t;

typedef struct max_press
{
    unsigned long type:PRESS_TYPE_BIT;
    unsigned long max_num:PRESS_VAL_BIT;
} max_press_t;

typedef union press
{
    unsigned long type:PRESS_TYPE_BIT;
    fifo_press_t  fifo;
    fail_press_t  fail;
    min_press_t   min;
    max_press_t   max;
    unsigned long val;
} press_t;

typedef struct limit
{
    unsigned long min;
    unsigned long max;
}limit_t;

#ifdef __cplusplus
}
#endif

#endif //TEST_QOS_QOS_PRESS_H
