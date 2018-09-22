//
// Created by root on 18-9-20.
//

#include "sysqos_interface.h"
#include "sysqos_dispatch_manager.h"
#include "sysqos_token_reqgrp_manager.h"

#ifndef DATASTOR_QOS_DUMP_INTERFACE_H
#define DATASTOR_QOS_DUMP_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

static inline token_reqgrp_manager_t *
manager_by_applicant(sysqos_applicant_ops_t *ops)
{
    return (token_reqgrp_manager_t *) (ops->data);
}

static inline sysqos_disp_manager_t *
manager_by_dispatch(sysqos_dispatcher_ops_t *ops)
{
    return (sysqos_disp_manager_t *) (ops->data);
}

/***测试调试****************************************************************/
typedef struct sysqos_app_dump_info
{
    unsigned long token_inuse;//从got到put的token数
    unsigned long token_quota;//生效配额
    unsigned long token_quota_target;//短期目标
    unsigned long token_quota_new;//最终目标
    unsigned long respond_step;//下调步长
    long          version;//版本
    unsigned long press;
} sysqos_app_dump_info_t;

void sysqos_dump_app(sysqos_applicant_ops_t *ops, void *disp_id,
                     sysqos_app_dump_info_t *info);

typedef struct sysqos_disp_dump_info
{
    unsigned long token_quota;
    unsigned long token_quota_new;
    unsigned long press;
    unsigned long min;
    long          version;//ser版本
    unsigned long token_used_static;//整个server相同的已经指定的预留区域
    unsigned long token_dynamic;//
} sysqos_disp_dump_info_t;

void sysqos_dump_disp(sysqos_dispatcher_ops_t *ops, void *app_id,
                      sysqos_disp_dump_info_t *info);

#ifdef __cplusplus
}
#endif
#endif //DATASTOR_QOS_DUMP_INTERFACE_H
