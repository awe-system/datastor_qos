//
// Created by root on 18-9-19.
//

#ifndef DATASTOR_QOS_SYSQOS_MSG_H
#define DATASTOR_QOS_SYSQOS_MSG_H

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

#endif //DATASTOR_QOS_SYSQOS_MSG_H
