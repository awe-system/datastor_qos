//
// Created by root on 18-7-24.
//

#ifndef QOS_QOS_COMMON_H_H
#define QOS_QOS_COMMON_H_H

#ifdef __cplusplus
extern "C" {
#endif


#define BLUE "\033[34m"
#define YELLOW "\033[32m"
//#define RED "\033[31m"
#define GREEN "\033[36m"
#define RESET "\033[0m"

#ifndef min
#define min(a, b) ((a)<(b)?(a):(b))
#endif

#ifndef max
#define max(a, b) ((a)>(b)?(a):(b))
#endif


typedef int (*compare_id_func)(void *id_a, void *id_b);

typedef int (*hash_func)(void *id, int table_len);

typedef void (*for_each_dofunc_t)(void *ctx,void *id,void *pri);

#define end_func(err, err_no, lab) \
do\
{\
    (err) = (err_no);\
    goto lab;\
}while(0)

#ifndef list_for_each_safe
#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
        pos = n, n = pos->next)
#endif

#ifdef __cplusplus
}
#endif

#endif //TEST_QOS_QOS_COLOR_H_H
