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
//��demo�н�max_node����Ϊ10
#define MAX_NODE_NUM                    4096
//resource����ĸ��� MAX_TOKEN_GRP_NUM * �������������
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
/*����ͨ����Դ*/
typedef struct resource
{
    /*��Դid resource�в�ά�����ڴ治����������
     * ��client�� idΪ��Դ��Ӧserver�˵ı�ʶ
     * ��server�� idΪ��Դ��Ӧclient�˵ı�ʶ*/
    void *id;
    
    /*��Դ����ֵ ֻ֧�ֶԵ�������Դ ֻ���Ĵ�С ����������*/
    unsigned long cost;
} resource_t;

/*********************************************************************************/
/***����id����ʵ��*****************************************************************************/

/*
 * �����������Ƚ�������Դid������ ��Ҫ�����Ƚ���ͬ�ͷ��ò����㷨
 * ���������
 *        void *id_a ���Ƚϵ���Դid
 *        void *id_b ���Ƚϵ���Դid
 * �����������
 * ����ֵ��
 *       = 0  ����Դid_a����Դid_bָ������ͬ��Դʱ
 *       = 1  ����Դid_a��λ�ý�id_bλ�ÿ�ǰ
 *       =-1 ����Դid_a��λ�ý�id_bλ�ÿ���
 * */
typedef int (*compare_func)(void *id_a, void *id_b);

/*
 * ����������Ϊ����idʵ��hash����ID�����һ��С��talbe_len��hashֵ
 * ���������
 *        void *id ���ID
 *        int talbe_len hash��ĳ�����Ҫ����0
 * �����������
 * ����ֵ��
 *       <0  ����������Ϸ�
 *       >=0   hashֵ ��Χ����Ϊ0-table_len
 * */
typedef int (*hash_id_func)(void *id, int talbe_len);

/*********************************************************************************/

/***qos_client.h******************************************************************/
/*��Դ�������Ҫʵ�ֵ��¼�֪ͨ�ӿ�*/
typedef struct sysqos_applicant_event_ops
{
    /*
     * �������������token_grp_got���¼�֪ͨ
     * ���������
     *        void *token_grp ��get_token_easy��ȡ��token���� ����commit_token ֮���token����
     *        void *pri ��Դ����˴����������ָ��
     *        int err ������
     *              = 0 δ����������token
     *              =-QOS_ERROR_TIMEOUT token��ʱ ��ʱ����ѡ������commit_token��ֱ��put_token
     *              =-QOS_ERROR_FAILEDNODE  ��Դ�б��д��ڲ��ɻ�ȡ����Դid
     *              =-QOS_ERROR_TOOBIGCOST  ��QOS_ERROR_FAILEDNODE���������������Դ�б��д��ڲ��Ϸ�����Դ����ֵ������/Ϊ0��
     * �����������
     * ����ֵ����
     * */
    void (*token_grp_got)(void *token_grp, void *pri, int err);
} sysqos_applicant_event_ops_t;


/*����Դ������ṩ�Ĳ�����*/
typedef struct sysqos_applicant_ops
{
    void *data;
    
    /*
    * ����������ȡ�ô�������Ϣ�ĳ���
    * ���������
    *        void *id ������Ϣ��Ŀ�Ľ��
    * ���������
    *        ��
    * ����ֵ��
    *         ��������Ϣ�ĳ��� Ϊ0ʱ��ʾ�޴����͵���Ϣ
    * */
    long (*snd_msg_len)(struct sysqos_applicant_ops *ops, void *id);
    
    /*
     * ����������ȡ�ô�������Ϣ������
     * ���������
     *        void *id ������Ϣ��Ŀ�Ľ��
     *        long len
     * ���������
     *        void *buf Ҫ���͵�����
     * ����ֵ��
     *         = 0 �ɹ�
     *         =-QOS_ERROR_FAILEDNODE ȡ������Ϣ��Ӧ�Ľ��ʧЧ ����msg_key����Դ�Ѿ��ͷ�
     *         =-QOS_ERROR_TOOSMALLBUF �����buf��С����
     *         =-QOS_ERROR_NULLPOINTER �Ƿ��Ĳ�������
     * */
    int (*snd_msg_buf)(struct sysqos_applicant_ops *ops, void *id, long len,
                       unsigned char *buf);
    
    /*
    * �������������ܵ���Ϣ��֪ͨ
    *         ����ĳһ����Ϣ ����ͨ�����ַ�ʽ���ܻ���ͨ��
    * ���������
    *        void *id ���ߵĽ��
    *        long len �������ݳ���
    *        unsigned char *buf �������ݻ����� �ں�����������Դ�п��ܱ����� ���ܺ����ڲ���Ҫ�����ݽ��п���
    * �����������
    * ����ֵ��  ��
    * */
    void (*rcvd)(struct sysqos_applicant_ops *ops, void *id,
                 long len, unsigned char *buf);
    
    /*
     * ��������������������Դ ���������ߵ���������������
     * ���������
     *        int rs_num  ��Դ��
     *        resource_t rs[] ��Դ����
     *        void *pri   ������ģ��������ָ�� �˺�����ά����ָ��������ָ���ַ���������� ���ô˺����ɹ�ʱ�ᱻ���õ�token����ȥ
     *
     * ���������
     *        token_reqgrp_t **token_grp �õ���token
     * ����ֵ��
     *       == QOS_ERROR_OK ��ȡtoken_grp
     *       == QOS_ERROR_MEMORY token��Դ���� �޷���ȡ�µ�token��ͨ�������ܷ�����
     *       == QOS_ERROR_PENDING ����ȴ���Դ֪ͨ
     *       ==QOS_ERROR_NULLPOINTER ��������
    * */
    int (*get_token_grp)(struct sysqos_applicant_ops *ops,
                         int rs_num, resource_t rs[],
                         void *pri, void **token_grp);
    
    /*
     * �����������ͷ�token��Դ ����*tokenָ���ó�NULL
     * ���������
     *        void *token_grp ��get_token_grp��ȡ��token����
     *        ��commit_token��get_token_easy�󷵻ش���0֮�� token_got�����֮ǰtoken���ܵ��ô˺���
     *        int fail_rs_num  ʧ����Դ��
     *        resource_t rs[] ʧ����Դ����
     * �����������
     * ����ֵ��
     *       = 0 �ͷųɹ�
     *       =-QOS_ERROR_NULLPOINTER �������ָ��Ƿ�
     * */
    int (*put_token_grp)(struct sysqos_applicant_ops *ops, void *token_grp,
                         int fail_rs_num, resource_t fail_rs[]);
} sysqos_applicant_ops_t;


/*
 * ����������qos��client�˳�ʼ�� �ڲ�������̬�������qos_client_obj�Ĺ���
 * ���������
 *        sysqos_applicant_event_ops_t *ops ������Դ���¼��ص�
 *        compare_func compare ��֯�ڲ����hash����õıȽϺ���
 *        hash_id_func hash ��֯�ڲ����hash����õ�hash����
 * �����������
 * ����ֵ��������
 *      == QOS_ERROR_MEMORY �ڴ����
 *      == QOS_ERROR_OK �ɹ�
 * */
int sysqos_app_init(sysqos_applicant_ops_t *, sysqos_applicant_event_ops_t *ops,
                    compare_func compare, hash_id_func hash);

/*
 * ����������qos��client���� �ڲ�������̬�������qos_client_obj������
 * �����������
 * �����������
 * ����ֵ����
 * */
void sysqos_app_exit(sysqos_applicant_ops_t *);

/*********************************************************************************/

/***qos_server.h******************************************************************/

/*������Դ������������Դ�仯����¼�����*/
typedef struct sysqos_dispatcher_ops
{
    void *data;
    
    /*
     * ����������ȡ�ô�������Ϣ�ĳ���
     * ���������
     *        void *id ������Ϣ��Ŀ�Ľ��
     * ���������
     *        ��
     * ����ֵ��
     *         ��������Ϣ�ĳ��� Ϊ0ʱ��ʾ�޴����͵���Ϣ
     * */
    long (*snd_msg_len)(struct sysqos_dispatcher_ops *ops, void *id);
    
    /*
     * ����������ȡ�ô�������Ϣ������
     * ���������
     *        void *id ������Ϣ��Ŀ�Ľ��
     *        long len
     * ���������
     *        void *buf Ҫ���͵�����
     * ����ֵ��
     *         = 0 �ɹ�
     *         =-QOS_ERROR_FAILEDNODE ȡ������Ϣ��Ӧ�Ľ��ʧЧ ����msg_key����Դ�Ѿ��ͷ�
     *         =-QOS_ERROR_TOOSMALLBUF �����buf��С����
     *         =-QOS_ERROR_NULLPOINTER �Ƿ��Ĳ�������
     * */
    int (*snd_msg_buf)(struct sysqos_dispatcher_ops *ops, void *id, long len,
                       unsigned char *buf);
    
    /*
    * �������������ܵ���Ϣ��֪ͨ
    *         ����ĳһ����Ϣ ����ͨ�����ַ�ʽ���ܻ���ͨ��
    * ���������
    *        void *id ���ߵĽ��
    *        long len �������ݳ���
    *        unsigned char *buf �������ݻ����� �ں�����������Դ�п��ܱ����� ���ܺ����ڲ���Ҫ�����ݽ��п���
    * �����������
    * ����ֵ��  ��
    * */
    void (*rcvd)(struct sysqos_dispatcher_ops *ops, void *id,
                 long len, unsigned char *buf);
    
    
    /*
    * ����������dispatch����ԴԤ�ж�
    * ���������
    *        void *rs ��Ҫ�������Դ����
    * ���������
    *        long *fence_id ��Ӧ��fence_id ��free_tokensʱ�ж��Ƿ�ʱ���ε����
    * ����ֵ��
    *        == QOS_ERROR_OK ��Դ��������
    *        == QOS_ERROR_TOOBIGCOST ��Դ����������
    * */
    int (*alloc_tokens)(struct sysqos_dispatcher_ops *ops,
                        resource_t *rs, long *fence_id);
    
    /*
    * ����������dispatch����Դ��Ƿ���
    * ���������
    *        void *rs ��Ҫ���ص���Դ����
    * ���������
    *        long fence_id ��Ӧ��fence_id ��free_tokensʱ�ж��Ƿ�ʱ���ε����
     *       bool is_fail ��ǰ��Դ��Ӧ����Ƿ��Ѿ��ж�Ϊ����״̬
    * ����ֵ��
    *        == QOS_ERROR_OK ��Դ��������
    *        == QOS_ERROR_TOOBIGCOST ��Դ����������
    * */
    void (*free_tokens)(struct sysqos_dispatcher_ops *ops,
                        resource_t *rs, long fence_id, bool is_fail);
    
    /*
     * ����������������Դ������Դ�õ����������� �״�����ʱ��Ҫ���øú���������Դ��ֵ
     * ���������
     *        long cost ���ӵ���Դ��
     * �����������
     * ����ֵ����
     * */
    void (*resource_increase)(struct sysqos_dispatcher_ops *ops, long cost);
} sysqos_dispatcher_ops_t;

/*
 * ����������qos��server�˳�ʼ�� �ڲ�������̬�������qos_server_obj�Ĺ���
 * ���������
 *        compare_func compare ��֯�ڲ����hash����õıȽϺ���
 *        hash_id_func hash ��֯�ڲ����hash����õ�hash����
 * �����������
 * ����ֵ����
 * */
int sysqos_dispatch_init(sysqos_dispatcher_ops_t *,
                         compare_func compare, hash_id_func hash);

/*
 * ����������qos��server������ �ڲ�������̬�������qos_server_obj������
 * �����������
 * �����������
 * ����ֵ����
 * */
void sysqos_dispatch_exit(sysqos_dispatcher_ops_t *);

/*********************************************************************************/

#ifdef __cplusplus
}
#endif
#endif //QOS_INTERFACE_H
