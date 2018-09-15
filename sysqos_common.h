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
#define RED "\033[31m"
#define GREEN "\033[36m"
#define RESET "\033[0m"

#ifndef MIN
#define MIN(a, b) ((a)<(b)?(a):(b))
#endif

#ifndef MAX
#define MAX(a, b) ((a)>(b)?(a):(b))
#endif


typedef int (*compare_id_func)(void *id_a, void *id_b);

typedef int (*hash_func)(void *id, int table_len);

//ctx isfor_each param item_in_tab is item in tab or list
typedef void (*for_each_dofunc_t)(void *ctx,void *id,void *item_in_tab);

#define end_func(err, err_no, lab) \
do\
{\
    (err) = (err_no);\
    goto lab;\
}while(0)

#ifdef __cplusplus
}
#endif

#endif //TEST_QOS_QOS_COLOR_H_H
