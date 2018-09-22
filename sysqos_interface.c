//
// Created by root on 18-9-19.
//

#include <string.h>
#include "sysqos_interface.h"
#include "sysqos_token_reqgrp_manager.h"
#include "sysqos_alloc.h"
#include "sysqos_dispatch_manager.h"
#include "sysqos_dump_interface.h"


/*
  * 功能描述：取得待发送消息的长度
  * 输入参数：
  *        void *id 发送消息的目的结点
  * 输出参数：
  *        无
  * 返回值：
  *         待发送消息的长度 为0时表示无待发送的消息
  * */
static long appsnd_msg_len(sysqos_applicant_ops_t *ops, void *id)
{
    if(!ops)
    {
        return QOS_ERROR_NULLPOINTER;
    }
    token_reqgrp_manager_t *manager = manager_by_applicant(ops);
    return manager->msg_event.snd_msg_len(&manager->msg_event,id);
}

/*
 * 功能描述：取得待发送消息的内容
 * 输入参数：
 *        void *id 发送消息的目的结点
 *        long len
 * 输出参数：
 *        void *buf 要发送的内容
 * 返回值：
 *         = 0 成功
 *         =-QOS_ERROR_FAILEDNODE 取消的消息对应的结点失效 但是msg_key的资源已经释放
 *         =-QOS_ERROR_TOOSMALLBUF 给入的buf大小不足
 *         =-QOS_ERROR_NULLPOINTER 非法的参数输入
 * */
int appsnd_msg_buf(sysqos_applicant_ops_t *ops, void *id, long len,
                   unsigned char *buf)
{
    if(!ops || !len || !buf)
    {
        return QOS_ERROR_NULLPOINTER;
    }
    token_reqgrp_manager_t *manager = manager_by_applicant(ops);
    return manager->msg_event.snd_msg_buf(&manager->msg_event,id,len,buf);
}

/*
    * 功能描述：接受到消息的通知
    *         对于某一个消息 或者通过这种方式接受或者通过
    * 输入参数：
    *        void *id 上线的结点
    *        long len 接受内容长度
    *        unsigned char *buf 接受内容缓冲区 在函数结束后资源有可能被回收 接受函数内部需要将内容进行拷贝
    * 输出参数：无
    * 返回值：  无
    * */
static void app_rcvd(struct sysqos_applicant_ops *ops, void *id,
                     long len, unsigned char *buf)
{
    token_reqgrp_manager_t *manager = manager_by_applicant(ops);
    manager->msg_event.rcvd(&manager->msg_event, id, len, buf);
}

#define GUESS_MAX_RESOURCE 50

/*
 * 功能描述：分配令牌资源 并将调用者的上下文置入其中
 * 输入参数：
 *        int rs_num  资源数
 *        resource_t rs[] 资源数组
 *        void *pri   调用者模块上下文指针 此函数不维护该指针内容所指向地址的生存周期 调用此函数成功时会被设置到token当中去
 *
 * 输出参数：
 *        token_reqgrp_t **token_grp 得到的token
 * 返回值：
 *       == QOS_ERROR_OK 获取token_grp
 *       == QOS_ERROR_MEMORY token资源不足 无法获取新的token（通常不可能发生）
 *       == QOS_ERROR_PENDING 挂起等待资源通知
 *       ==QOS_ERROR_NULLPOINTER 参数错误
 * */
static int get_token_grp(struct sysqos_applicant_ops *ops,
                           int rs_num, resource_t rs[],
                           void *pri,void **token_grp)
{
    int                    i        = 0;
    resource_list_t        *last_rs = NULL;
    resource_list_t        rs_list[GUESS_MAX_RESOURCE];
    token_reqgrp_manager_t *manager = manager_by_applicant(ops);
    
    if ( rs_num > GUESS_MAX_RESOURCE || rs_num <= 0 || !ops || !rs )
    {
        return QOS_ERROR_NULLPOINTER;
    }
    
    for ( i = 0; i < rs_num; ++i )
    {
        LISTHEAD_INIT(&rs_list[i].list);
        memcpy(&rs_list[i].rs, &rs[i], sizeof(resource_t));
        if ( last_rs )
        {
            list_add(&rs_list[i].list, &last_rs->list);
        }
        last_rs = &rs_list[i];
    }
    
    return manager->get_token_reqgrp(manager, last_rs, pri,
                                     (token_reqgrp_t **) token_grp);
}

/*
* 功能描述：释放token资源 并将*token指针置成NULL
* 输入参数：
*        void *token_grp 由get_token_grp获取的token对象
*        在commit_token或get_token_easy后返回大于0之后 token_got别调用之前token不能调用此函数
*        int fail_rs_num  失败资源数
*        resource_t rs[] 失败资源数组
* 输出参数：无
* 返回值：
*       = 0 释放成功
*       =-QOS_ERROR_NULLPOINTER 传入参数指针非法
* */
static int put_token_grp(struct sysqos_applicant_ops *ops, void *token_grp,
                         int fail_rs_num, resource_t fail_rs[])
{
    
    int                    i        = 0;
    resource_list_t        *last_rs = NULL;
    resource_list_t        rs_list[GUESS_MAX_RESOURCE];
    token_reqgrp_manager_t *manager = manager_by_applicant(ops);
    
    if ( !ops || !token_grp )
    {
        return QOS_ERROR_NULLPOINTER;
    }
    
    for ( i = 0; i < fail_rs_num; ++i )
    {
        LISTHEAD_INIT(&rs_list[i].list);
        memcpy(&rs_list[i].rs, &fail_rs[i], sizeof(resource_t));
        if ( last_rs )
        {
            list_add(&rs_list[i].list, &last_rs->list);
        }
        last_rs = &rs_list[i];
    }
    
    manager->put_token_reqgrp(manager, token_grp, last_rs);
    return QOS_ERROR_OK;
}

int sysqos_app_init(sysqos_applicant_ops_t *ops,
                    sysqos_applicant_event_ops_t *event_ops,
                    compare_func compare, hash_id_func hash)
{
    msg_ops_t *msg_ops1;
    int       err = QOS_ERROR_OK;
    if ( !ops || !event_ops || !compare || !hash )
    {
        return QOS_ERROR_NULLPOINTER;
    }
    token_reqgrp_manager_t *manager = qos_alloc(sizeof(token_reqgrp_manager_t));
    if ( !manager )
    {
        end_func(err, QOS_ERROR_MEMORY, token_reqgrp_manager_allocfailed);
    }
    
    err = token_reqgrp_manager_init(manager, NODE_TABLE_HASH_LEN,
                                    MAX_TOKEN_GRP_NUM,
                                    MAX_RESOURCE_NUM, MAX_NODE_NUM);
    if ( err )
    {
        goto token_reqgrp_manager_init_failed;
    }
    
    msg_ops1 = qos_alloc(sizeof(msg_ops_t));
    if ( !msg_ops1 )
    {
        end_func(err, QOS_ERROR_MEMORY, msg_ops_alloc_failed);
    }
    
    msg_ops1->compare = compare;
    msg_ops1->hash_id = hash;
    msg_ops1->snd_msg = NULL;
    manager->set_msg_ops(manager, msg_ops1);
    manager->set_app_event_ops(manager, event_ops);
    
    ops->data          = manager;
    ops->rcvd          = app_rcvd;
    ops->snd_msg_len       = appsnd_msg_len;
    ops->snd_msg_buf       = appsnd_msg_buf;
    ops->get_token_grp = get_token_grp;
    ops->put_token_grp = put_token_grp;
    
    return err;
//    qos_free(msg_ops1);
msg_ops_alloc_failed:
    token_reqgrp_manager_exit(manager);
token_reqgrp_manager_init_failed:
    qos_free(manager);
token_reqgrp_manager_allocfailed:
    return err;
}

void sysqos_app_exit(sysqos_applicant_ops_t *ops)
{
    token_reqgrp_manager_t *manager  = manager_by_applicant(ops);
    msg_ops_t              *msg_ops1 = manager->msg_ops;
    assert(ops && manager && msg_ops1);
    qos_free(msg_ops1);
    token_reqgrp_manager_exit(manager);
    qos_free(manager);
}
/*******************************************************************************
 * ***************************************************************************
 * ***************************************************************************/
/*
  * 功能描述：取得待发送消息的长度
  * 输入参数：
  *        void *id 发送消息的目的结点
  * 输出参数：
  *        无
  * 返回值：
  *         待发送消息的长度 为0时表示无待发送的消息
  * */
static long dispsnd_msg_len(sysqos_dispatcher_ops_t *ops, void *id)
{
    if(!ops)
    {
        return QOS_ERROR_NULLPOINTER;
    }
    sysqos_disp_manager_t *manager = manager_by_dispatch(ops);
    return manager->msg_event.snd_msg_len(&manager->msg_event,id);
}

/*
 * 功能描述：取得待发送消息的内容
 * 输入参数：
 *        void *id 发送消息的目的结点
 *        long len
 * 输出参数：
 *        void *buf 要发送的内容
 * 返回值：
 *         = 0 成功
 *         =-QOS_ERROR_FAILEDNODE 取消的消息对应的结点失效 但是msg_key的资源已经释放
 *         =-QOS_ERROR_TOOSMALLBUF 给入的buf大小不足
 *         =-QOS_ERROR_NULLPOINTER 非法的参数输入
 * */
int dispsnd_msg_buf(sysqos_dispatcher_ops_t *ops, void *id, long len,
                    unsigned char *buf)
{
    if(!ops || !len || !buf)
    {
        return QOS_ERROR_NULLPOINTER;
    }
    sysqos_disp_manager_t *manager = manager_by_dispatch(ops);
    return manager->msg_event.snd_msg_buf(&manager->msg_event,id,len,buf);
}

/*
   * 功能描述：dispatch端资源预判断
   * 输入参数：
   *        void *rs 需要申请的资源描述
   * 输出参数：
   *        long *fence_id 对应的fence_id 在free_tokens时判断是否时当次的配额
   * 返回值：
   *        == QOS_ERROR_OK 资源允许申请
   *        == QOS_ERROR_TOOBIGCOST 资源不允许申请
   * */
static int alloc_tokens(struct sysqos_dispatcher_ops *ops,
                        resource_t *rs, long *fence_id)
{
    sysqos_disp_manager_t *manager = manager_by_dispatch(ops);
    manager->alloc_tokens(manager, rs, fence_id);
}

/*
* 功能描述：dispatch端资源标记返回
* 输入参数：
*        void *rs 需要返回的资源描述
* 输出参数：
*        long fence_id 对应的fence_id 在free_tokens时判断是否时当次的配额
 *       bool is_fail 当前资源对应结点是否已经判定为离线状态
* 返回值：
*        == QOS_ERROR_OK 资源允许申请
*        == QOS_ERROR_TOOBIGCOST 资源不允许申请
* */
static void free_tokens(struct sysqos_dispatcher_ops *ops,
                        resource_t *rs, long fence_id, bool is_fail)
{
    sysqos_disp_manager_t *manager = manager_by_dispatch(ops);
    manager->free_tokens(manager, rs, fence_id, is_fail);
}

/*
* 功能描述：接受到消息的通知
*         对于某一个消息 或者通过这种方式接受或者通过
* 输入参数：
*        void *id 上线的结点
*        long len 接受内容长度
*        unsigned char *buf 接受内容缓冲区 在函数结束后资源有可能被回收 接受函数内部需要将内容进行拷贝
* 输出参数：无
* 返回值：  无
* */
static void disprcvd(struct sysqos_dispatcher_ops *ops, void *id,
                     long len, unsigned char *buf)
{
    sysqos_disp_manager_t *manager = manager_by_dispatch(ops);
    if ( !ops || !buf )
    {
        return;
    }
    manager->msg_event.rcvd(&manager->msg_event, id, len, buf);
}

/*
 * 功能描述：调度资源池中资源得到永久性增加 首次上线时需要调用该函数置入资源初值
 * 输入参数：
 *        long cost 增加的资源数
 * 输出参数：无
 * 返回值：无
 * */
static void resource_increase(struct sysqos_dispatcher_ops *ops, long cost)
{
    sysqos_disp_manager_t *manager = manager_by_dispatch(ops);
    if ( !ops || cost <= 0 )
    {
        return;
    }
    manager->resource_increase(manager, cost);
}


int sysqos_dispatch_init(sysqos_dispatcher_ops_t *ops, compare_func compare,
                         hash_id_func hash)
{
    msg_ops_t *msg_ops1;
    int       err = QOS_ERROR_OK;
    if ( !ops || !compare || !hash )
    {
        return QOS_ERROR_NULLPOINTER;
    }
    sysqos_disp_manager_t *manager = qos_alloc(sizeof(sysqos_disp_manager_t));
    if ( !manager )
    {
        end_func(err, QOS_ERROR_MEMORY, sysqos_disp_manager_allocfailed);
    }
    
    err = sysqos_disp_manager_init(manager, NODE_TABLE_HASH_LEN,
                                   MAX_NODE_NUM,
                                   UPDATE_INTERVAL, MIN_RS_NUM);
    if ( err )
    {
        goto sysqos_disp_manager_init_failed;
    }
    
    msg_ops1 = qos_alloc(sizeof(msg_ops_t));
    if ( !msg_ops1 )
    {
        end_func(err, QOS_ERROR_MEMORY, msg_ops_alloc_failed);
    }
    
    msg_ops1->compare = compare;
    msg_ops1->hash_id = hash;
    msg_ops1->snd_msg = NULL;
    manager->set_msg_ops(manager, msg_ops1);
    
    ops->data              = manager;
    ops->rcvd              = disprcvd;
    ops->snd_msg_len       = dispsnd_msg_len;
    ops->snd_msg_buf       = dispsnd_msg_buf;
    ops->alloc_tokens      = alloc_tokens;
    ops->free_tokens       = free_tokens;
    ops->resource_increase = resource_increase;
    
    return err;
//    qos_free(msg_ops1);
msg_ops_alloc_failed:
    sysqos_disp_manager_exit(manager);
sysqos_disp_manager_init_failed:
    qos_free(manager);
sysqos_disp_manager_allocfailed:
    return err;
}

void sysqos_dispatch_exit(sysqos_dispatcher_ops_t *ops)
{
    sysqos_disp_manager_t *manager  = manager_by_dispatch(ops);
    msg_ops_t             *msg_ops1 = manager->msg_ops;
    assert(ops && manager && msg_ops1);
    qos_free(msg_ops1);
    sysqos_disp_manager_exit(manager);
    qos_free(manager);
}
