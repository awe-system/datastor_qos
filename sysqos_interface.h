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
#define NODE_TABLE_HASH_LEN             127UL
#define MIN_RS_NUM                      10UL
#define MMAX_RESPOND_STEP               5UL
/***qos_error.h********************************************************************/
enum
{
    QOS_ERROR_OK,
    QOS_ERROR_PENDING,
    QOS_ERROR_FAILEDNODE,
    QOS_ERROR_TOOBIGCOST,
//    QOS_ERROR_TOOSMALLBUF,
//    QOS_ERROR_NULLPOINTER,
//    QOS_ERROR_TIMEOUT,
//    QOS_ERROR_NOSUCHSOURCE,
    QOS_ERROR_MEMORY,
    QOS_ERROR_TOO_MANY_NODES,
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

/*资源链表*/
typedef struct resource_list
{
    struct list_head list;
    resource_t       rs;//其中一个资源 这里只做值拷贝
} resource_list_t;
/*********************************************************************************/

/***qos_msg_interface.h***********************************************************/
/*消息机制提供的操作集*/
typedef struct msg_ops
{
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
    int (*compare)(void *id_a, void *id_b);
    
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
    int (*hash_id)(void *id, int talbe_len);
    
    
    /*
    * 功能描述：内部通知外部所有node指针已经不再进行使用
    * 输入参数：
    *        void *id 结点ID
    * 输出参数：无
    * 返回值：无
    * */
//    void (*node_offline_done)(void *id);
    
    /*
     * 功能描述：向id对应的结点发送消息
     *         注册时 若此函数为空 则指向内部缓存函数 外部通过调用msg_event_ops 中的snd_msg_len snd_msg_buf 来获取带传输数据
     *         此函数为空时内部回将其赋值为 内部函数较传输数据转化成内部数据知道用户调用 调用msg_event_ops中的附加包函数
     * 输入参数：
     *        void *id 目标结点的id
     *        long len 发送内容长度
     *        unsigned char *buf 发送内容缓冲区 在函数结束后资源有可能被回收 发送函数需要将内容进行拷贝
     * 输出参数：无
     * 返回值：  无
     * */
    void (*snd_msg)(void *id, long len, unsigned char *buf);
} msg_ops_t;

/*向消息机制注册的事件通知回调*/
typedef struct msg_event_ops
{
    /*
     * 功能描述：结点上线通知
     * 输入参数：
     *        void *id 上线的结点
     * 输出参数：无
     * 返回值：  无
     * */
    void (*node_reset)(struct msg_event_ops *ops, void *id);
    
    /*
     * 功能描述：结点上线通知
     * 输入参数：
     *        void *id 上线的结点
     * 输出参数：无
     * 返回值：  无
     * */
    void (*node_online)(struct msg_event_ops *ops, void *id);
    
    /*
     * 功能描述：结点下线通知
     * 输入参数：
     *        void *id 下线的结点
     * 输出参数：无
     * 返回值：  无
     * */
    void (*node_offline)(struct msg_event_ops *ops, void *id);
    
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
    void
    (*rcvd)(struct msg_event_ops *ops, void *id, long len, unsigned char *buf);
    
    /*
     * 功能描述：取得待发送消息的长度
     * 输入参数：
     *        void *id 发送消息的目的结点
     * 输出参数：
     *        无
     * 返回值：
     *         待发送消息的长度 为0时表示无待发送的消息
     * */
    long (*snd_msg_len)(struct msg_event_ops *ops, void *id);
    
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
    int (*snd_msg_buf)(struct msg_event_ops *ops, void *id, long len,
                       unsigned char *buf);
    
} msg_event_ops_t;
/*********************************************************************************/


/***qos_token.h*******************************************************************/

/*********************************************************************************/

/***qos_client.h******************************************************************/
/*资源申请端需要实现的事件通知接口*/
typedef struct applicant_event_ops
{
    /*
     * 功能描述：获得permission_got的事件通知
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
    void (*token_grp_got)(void *permission, void *pri, int err);
    
    /*
     * 功能描述：在某个结点的信用资源发生变化时每个token都会收到此回调通知某个node结点的资源得到了新增
     * 输入参数：
     *        void *token 由get_token_easy或者commit_token返回QOS_ERROR_TOOBIGCOST之后的token对象
     *        void *pri 资源申请端传入的上下文指针
     *        void *node 资源发生变化的对应结点
     * 输出参数：无
     * 返回值：
     *       =true  需要继续传递给下一个失败未put的token
     *       =false 不需要传递给其他的token
     * */
} applicant_event_ops_t;


/*向资源申请端提供的操作集*/
typedef struct applicant_ops
{
    /*
     * 功能描述：分配令牌资源 并将调用者的上下文置入其中
     * 输入参数：
     *        void *pri   调用者模块上下文指针 此函数不维护该指针内容所指向地址的生存周期 调用此函数成功时会被设置到token当中去
     * 输出参数：无
     * 返回值：
     *       = NULL token资源不足 无法获取新的token（通常不可能发生）
     *       !=NULL 返回token指针该 指针存周期由本模块维护 在put_token时释放
     * */
    void *(*get_token)(void *pri);
    
    /*
     * 功能描述：释放token资源 并将*token指针置成NULL
     * 输入参数：
     *        void **token 由get_token/get_token_easy获d取的token对象
     *        在commit_token或get_token_easy后返回大于0之后 token_got别调用之前token不能调用此函数
     * 输出参数：无
     * 返回值：
     *       = 0 释放成功
     *       =-QOS_ERROR_NULLPOINTER 传入参数指针非法
     * */
    int (*put_token)(void **token);
    
    /*
     * 功能描述：设置申请资源的事件回调 需要在构造后调用
     * 输入参数：applicant_event_ops_t *ops 申请资源的事件回调
     * 输出参数：无
     * 返回值：无
     * */
    void (*set_applicant_event_ops)(applicant_event_ops_t *ops);
} applicant_ops_t;


/*
 * 功能描述：qos的client端初始化 内部包含静态共享对象qos_client_obj的构造
 * 输入参数：无
 * 输出参数：无
 * 返回值：无
 * */
void qos_app_init(applicant_ops_t *);

/*
 * 功能描述：qos的client销毁 内部包含静态共享对象qos_client_obj的析构
 * 输入参数：无
 * 输出参数：无
 * 返回值：无
 * */
void qos_app_exit();

/*********************************************************************************/

/***qos_server.h******************************************************************/

/*调度资源操作集处理资源变化后的事件处理*/
typedef struct dispatcher_ops
{
    /*
     * 特别说明 ：本接口中假设所有资源的获取 都需要保存resource_t *token_need ，在网络异常等情形下调用者要必须释放掉所有已经申请的资源
     * */
    
    /*
     * 功能描述：调度资源池中资源得到永久性增加 首次上线时需要调用该函数置入资源初值
     * 输入参数：
     *        long cost 增加的资源数
     * 输出参数：无
     * 返回值：无
     * */
    void (*resource_increase)(long cost);
} dispatcher_ops_t;

/*
 * 功能描述：qos的server端初始化 内部包含静态共享对象qos_server_obj的构造
 * 输入参数：无
 * 输出参数：无
 * 返回值：无
 * */
void qos_dispatch_init(dispatcher_ops_t *);

/*
 * 功能描述：qos的server端销毁 内部包含静态共享对象qos_server_obj的析构
 * 输入参数：无
 * 输出参数：无
 * 返回值：无
 * */
void qos_server_exit(dispatcher_ops_t *);

/*********************************************************************************/

#ifdef __cplusplus
}
#endif
#endif //QOS_INTERFACE_H
