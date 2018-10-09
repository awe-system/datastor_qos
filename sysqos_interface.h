//
// Created by GaoHualong on 2018/7/10.
//

#ifndef QOS_INTERFACE_H
#define QOS_INTERFACE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "sysqos_type.h"

/**********************************************************************************/
//#define GRP_MEMORY_CACHE
#define MAX_TOKEN_GRP_NUM               100000
//在demo中将max_node设置为10
#define MAX_NODE_NUM                    4096
//resource对象的个数 MAX_TOKEN_GRP_NUM * 条带中条块个数
#define MAX_RESOURCE_NUM                1000000
#define NODE_TABLE_HASH_LEN             127UL
#define MIN_RS_NUM                      10UL
#define MMAX_RESPOND_STEP               5UL
#define UPDATE_INTERVAL                 1000
/***qos_error.h********************************************************************/
enum
{
    QOS_ERROR_OK,
    QOS_ERROR_PENDING,
    QOS_ERROR_FAILEDNODE,
    QOS_ERROR_TOOBIGCOST,
    QOS_ERROR_MEMORY,
    QOS_ERROR_TOO_MANY_NODES,
    QOS_ERROR_NULLPOINTER,
    QOS_ERROR_UNKNOWN = 99,
};

/*********************************************************************************/
/***qos_resource.h****************************************************************/
/*定义通用资源*/
typedef struct resource
{
    /*资源id resource中不维护其内存不关心其内容
     * 在client端 id为资源对应server端的标识
     * 在server端 id为资源对应client端的标识*/
    void *id;
    
    /*资源消耗值 只支持对等类型资源 只关心大小 不关心内容*/
    unsigned long cost;
} resource_t;

/*********************************************************************************/
/***更具id特性实现*****************************************************************************/

/*
 * 功能描述：比较两个资源id的内容 主要用作比较相同和放置查找算法
 * 输入参数：
 *        void *id_a 被比较当资源id
 *        void *id_b 被比较当资源id
 * 输出参数：无
 * 返回值：
 *       = 0  当资源id_a和资源id_b指的是相同资源时
 *       = 1  当资源id_a的位置较id_b位置靠前
 *       =-1 当资源id_a的位置较id_b位置靠后
 * */
typedef int (*compare_func)(void *id_a, void *id_b);

/*
 * 功能描述：为了用id实现hash表，用ID计算成一个小于talbe_len的hash值
 * 输入参数：
 *        void *id 结点ID
 *        int talbe_len hash表的长度需要大于0
 * 输出参数：无
 * 返回值：
 *       <0  输入参数不合法
 *       >=0   hash值 范围必须为0-table_len
 * */
typedef int (*hash_id_func)(void *id, int talbe_len);

/*********************************************************************************/

/***qos_client.h******************************************************************/
/*资源申请端需要实现的事件通知接口*/
typedef struct sysqos_applicant_event_ops
{
    /*
     * 功能描述：获得token_grp_got的事件通知
     * 输入参数：
     *        void *token_grp 由get_token_easy获取的token对象 或者commit_token 之后的token对象
     *        void *pri 资源申请端传入的上下文指针
     *        int err 错误码
     *              = 0 未发生错误获得token
     *              =-QOS_ERROR_TIMEOUT token超时 此时可以选择重新commit_token或直接put_token
     *              =-QOS_ERROR_FAILEDNODE  资源列表中存在不可获取的资源id
     *              =-QOS_ERROR_TOOBIGCOST  在QOS_ERROR_FAILEDNODE不发生的情况下资源列表中存在不合法的资源消耗值（过大/为0）
     * 输出参数：无
     * 返回值：无
     * */
    void (*token_grp_got)(void *token_grp, void *pri, int err);
} sysqos_applicant_event_ops_t;


/*向资源申请端提供的操作集*/
typedef struct sysqos_applicant_ops
{
    void *data;
    
    /*
    * 功能描述：取得待发送消息的长度
    * 输入参数：
    *        void *id 发送消息的目的结点
    * 输出参数：
    *        无
    * 返回值：
    *         待发送消息的长度 为0时表示无待发送的消息
    * */
    long (*snd_msg_len)(struct sysqos_applicant_ops *ops, void *id);
    
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
    int (*snd_msg_buf)(struct sysqos_applicant_ops *ops, void *id, long len,
                       unsigned char *buf);
    
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
    void (*rcvd)(struct sysqos_applicant_ops *ops, void *id,
                 long len, unsigned char *buf);
    
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
    int (*get_token_grp)(struct sysqos_applicant_ops *ops,
                         int rs_num, resource_t rs[],
                         void *pri, void **token_grp);
    
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
    int (*put_token_grp)(struct sysqos_applicant_ops *ops, void *token_grp,
                         int fail_rs_num, resource_t fail_rs[]);
} sysqos_applicant_ops_t;


/*
 * 功能描述：qos的client端初始化 内部包含静态共享对象qos_client_obj的构造
 * 输入参数：
 *        sysqos_applicant_event_ops_t *ops 申请资源的事件回调
 *        compare_func compare 组织内部结点hash表的用的比较函数
 *        hash_id_func hash 组织内部结点hash表的用的hash函数
 * 输出参数：无
 * 返回值：错误码
 *      == QOS_ERROR_MEMORY 内存错误
 *      == QOS_ERROR_OK 成功
 * */
int sysqos_app_init(sysqos_applicant_ops_t *, sysqos_applicant_event_ops_t *ops,
                    compare_func compare, hash_id_func hash);

/*
 * 功能描述：qos的client销毁 内部包含静态共享对象qos_client_obj的析构
 * 输入参数：无
 * 输出参数：无
 * 返回值：无
 * */
void sysqos_app_exit(sysqos_applicant_ops_t *);

/*********************************************************************************/

/***qos_server.h******************************************************************/

/*调度资源操作集处理资源变化后的事件处理*/
typedef struct sysqos_dispatcher_ops
{
    void *data;
    
    /*
     * 功能描述：取得待发送消息的长度
     * 输入参数：
     *        void *id 发送消息的目的结点
     * 输出参数：
     *        无
     * 返回值：
     *         待发送消息的长度 为0时表示无待发送的消息
     * */
    long (*snd_msg_len)(struct sysqos_dispatcher_ops *ops, void *id);
    
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
    int (*snd_msg_buf)(struct sysqos_dispatcher_ops *ops, void *id, long len,
                       unsigned char *buf);
    
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
    void (*rcvd)(struct sysqos_dispatcher_ops *ops, void *id,
                 long len, unsigned char *buf);
    
    
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
    int (*alloc_tokens)(struct sysqos_dispatcher_ops *ops,
                        resource_t *rs, long *fence_id);
    
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
    void (*free_tokens)(struct sysqos_dispatcher_ops *ops,
                        resource_t *rs, long fence_id, bool is_fail);
    
    /*
     * 功能描述：调度资源池中资源得到永久性增加 首次上线时需要调用该函数置入资源初值
     * 输入参数：
     *        long cost 增加的资源数
     * 输出参数：无
     * 返回值：无
     * */
    void (*resource_increase)(struct sysqos_dispatcher_ops *ops, long cost);
} sysqos_dispatcher_ops_t;

/*
 * 功能描述：qos的server端初始化 内部包含静态共享对象qos_server_obj的构造
 * 输入参数：
 *        compare_func compare 组织内部结点hash表的用的比较函数
 *        hash_id_func hash 组织内部结点hash表的用的hash函数
 * 输出参数：无
 * 返回值：无
 * */
int sysqos_dispatch_init(sysqos_dispatcher_ops_t *,
                         compare_func compare, hash_id_func hash);

/*
 * 功能描述：qos的server端销毁 内部包含静态共享对象qos_server_obj的析构
 * 输入参数：无
 * 输出参数：无
 * 返回值：无
 * */
void sysqos_dispatch_exit(sysqos_dispatcher_ops_t *);

/*********************************************************************************/

#ifdef __cplusplus
}
#endif
#endif //QOS_INTERFACE_H
