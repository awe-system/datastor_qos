//#define MEMORY_CACHE_TEST


#include <iostream>
#include "memory_cache.h"
#include "hash_table.h"
#include "safe_rw_list.h"
#include "sysqos_policy_param.h"
#include "sysqos_interface.h"
#include "sysqos_dispatch_base_node.h"
#include "sysqos_nodereq_list.h"
#include "sysqos_token_reqgrp.h"
#include "sysqos_dispatch_node.h"
#include "sysqos_token_reqgrp_manager.h"
//#include "wait_increase_list.h"

#define IN
#define OUT
#define INOUT

enum
{
    qos_applicant_model_fifo,
    qos_applicant_model_failreturn
};

typedef struct qos_applicant_cb
{
    void (*permission_got)(IN void **permission, IN void *pri, IN int err);
    
    bool
    (*permission_notify)(IN void **permission, IN void *pri, IN int err);//可选
} qos_applicant_cb;

typedef struct qos_applicant
{
    /************************************************************************************/
    int (*get_permission)(IN resource_list_t *rs_list, IN void *pri, OUT
                          void **permission);
    
    int (*put_permission)(IN void *permission);
    
    int (*set_cb)(IN qos_applicant_cb *cb);
    
    /**下面内用使用者可以不需知道*************************************************************/
    void *internal_data;//内部
    
    int (*set_msg_ops)(IN msg_ops_t *ops);//可选
    
    int (*ignore_from_permission)(IN void *permission, IN resource_t *rs);//可选
    
    int (*set_model)(IN int model);//可选
} qos_applicant_t;

int qos_applicant_init(qos_applicant_t *qos);

int qos_applicant_exit(qos_applicant_t *qos);


#define MAX_HASH_NUM  128
#define MAX_RETURN_NUM 10 //系统最大返还
#define PRESS_TOLERANCE 10 //压力容忍度 见如为10时 1-11认为是一个级别的


typedef struct press_in_table
{
    void             *node_id;
    press_t          press;
    struct list_head hash_list;
} press_in_table_t;

typedef struct press_table
{
    struct list_head hash[MAX_HASH_NUM];
} press_table_t;

typedef struct limit_in_talbe
{
    limit_t          limit;
    struct list_head list;
} limit_in_table;

typedef struct limit_table
{
    struct list_head hash[MAX_HASH_NUM];
} limit_table_t;




typedef struct app_desc
{

} app_desc;

typedef struct res_desc
{
    unsigned long total;
    unsigned long free;
    unsigned long tofree;
} res_desc_t;

typedef struct to_alloc_item
{
    app_desc         *desc;
    struct list_head list;
} to_alloc_item_t;

typedef struct to_alloc_list
{
    struct list_head list;
} to_alloc_list_t;


int main()
{
    test_memory_cache();
    test_safe_rw_list();
    test_hash_table();
    test_dispatch_base_node();
    test_wait_token_list();
    test_token_reqgrp();
    test_dispatch_node();
    test_token_reqgrp_manager();
//    test_wait_increase_list();
    return 0;
}
