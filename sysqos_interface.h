//
// Created by GaoHualong on 2018/7/10.
//

#ifndef QOS_INTERFACE_H
#define QOS_INTERFACE_H
#ifdef __cplusplus
extern "C" {
#endif
#include "stdbool.h"
#include "list_head.h"

/**********************************************************************************/
#define NODE_TABLE_HASH_LEN             127
#define MIN_RS_NUM                      10
#define MMAX_RESPOND_STEP               5
/***qos_error.h********************************************************************/
enum
{
    QOS_ERROR_OK,
    QOS_ERROR_PENDING,
    QOS_ERROR_FAILEDNODE,
    QOS_ERROR_TOOBIGCOST,
    QOS_ERROR_TOOSMALLBUF,
    QOS_ERROR_NULLPOINTER,
    QOS_ERROR_TIMEOUT,
    QOS_ERROR_NOSUCHSOURCE,
    QOS_ERROR_MEMORY,
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
    long cost;
} resource_t;

/*��Դ����*/
typedef struct resource_list
{
    struct list_head list;
    resource_t       rs;//����һ����Դ ����ֻ��ֵ����
} resource_list_t;
/*********************************************************************************/

/***qos_msg_interface.h***********************************************************/
/*��Ϣ�����ṩ�Ĳ�����*/
typedef struct msg_ops
{
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
    int (*compare)(void *id_a, void *id_b);
    
    /*
     * ����������Ϊ����idʵ��hash������ID�����һ��С��talbe_len��hashֵ
     * ���������
     *        void *id ���ID
     *        int talbe_len hash���ĳ�����Ҫ����0
     * �����������
     * ����ֵ��
     *       <0  ����������Ϸ�
     *       >=0   hashֵ ��Χ����Ϊ0-table_len
     * */
    int (*hash_id)(void *id, int talbe_len);
    
    
    /*
    * �����������ڲ�֪ͨ�ⲿ����nodeָ���Ѿ����ٽ���ʹ��
    * ���������
    *        void *id ���ID
    * �����������
    * ����ֵ����
    * */
    void (*node_offline_done)(void *id);
    
    /*
     * ������������id��Ӧ�Ľ�㷢����Ϣ
     *         ע��ʱ ���˺���Ϊ�� ��ָ���ڲ����溯�� �ⲿͨ������msg_event_ops �е�snd_msg_len snd_msg_buf ����ȡ����������
     *         �˺���Ϊ��ʱ�ڲ��ؽ��丳ֵΪ �ڲ������ϴ�������ת�����ڲ�����֪���û����� ����msg_event_ops�еĸ��Ӱ�����
     * ���������
     *        void *id Ŀ�����id
     *        long len �������ݳ���
     *        unsigned char *buf �������ݻ����� �ں�����������Դ�п��ܱ����� ���ͺ�����Ҫ�����ݽ��п���
     * �����������
     * ����ֵ��  ��
     * */
    void (*snd_msg)(void *id, long len, unsigned char *buf);
} msg_ops_t;

/*����Ϣ����ע����¼�֪ͨ�ص�*/
typedef struct msg_event_ops
{
    /*
     * �����������������֪ͨ
     * ���������
     *        void *id ���ߵĽ��
     * �����������
     * ����ֵ��  ��
     * */
    void (*node_reset)(struct msg_event_ops *ops, void *id);
    
    /*
     * �����������������֪ͨ
     * ���������
     *        void *id ���ߵĽ��
     * �����������
     * ����ֵ��  ��
     * */
    void (*node_online)(struct msg_event_ops *ops, void *id);
    
    /*
     * �����������������֪ͨ
     * ���������
     *        void *id ���ߵĽ��
     * �����������
     * ����ֵ��  ��
     * */
    void (*node_offline)(struct msg_event_ops *ops, void *id);
    
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
    void
    (*rcvd)(struct msg_event_ops *ops, void *id, long len, unsigned char *buf);
    
    /*
     * ����������ȡ�ô�������Ϣ�ĳ���
     * ���������
     *        void *id ������Ϣ��Ŀ�Ľ��
     * ���������
     *        ��
     * ����ֵ��
     *         ��������Ϣ�ĳ��� Ϊ0ʱ��ʾ�޴����͵���Ϣ
     * */
    long (*snd_msg_len)(struct msg_event_ops *ops, void *id);
    
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
    int (*snd_msg_buf)(struct msg_event_ops *ops, void *id, long len,
                       unsigned char *buf);
    
} msg_event_ops_t;
/*********************************************************************************/


/***qos_token.h*******************************************************************/
/*
 * ������������ȡ�洢��token���е�����ģ��������
 * ���������
 *        void *token ����ָ��
 * �����������
 * ����ֵ��  �洢��token���еĵ�����ģ�������������
 * */
void *get_token_private(void *token);

/*
 * ������������token���õ�����ģ��������
 * ���������
 *        void *token ����ָ��
 *        void *pri   ������ģ��������ָ�� �˺�����ά����ָ��������ָ���ַ����������
 * �����������
 * ����ֵ��  ��
 * */
void set_token_private(void *token, void *pri);
/*********************************************************************************/

/***qos_client.h******************************************************************/
/*��Դ�������Ҫʵ�ֵ��¼�֪ͨ�ӿ�*/
typedef struct applicant_event_ops
{
    /*
     * �������������permission_got���¼�֪ͨ
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
    void (*token_grp_got)(void *permission, void *pri, int err);
    
    /*
     * ������������ĳ������������Դ�����仯ʱÿ��token�����յ��˻ص�֪ͨĳ��node������Դ�õ�������
     * ���������
     *        void *token ��get_token_easy����commit_token����QOS_ERROR_TOOBIGCOST֮���token����
     *        void *pri ��Դ����˴����������ָ��
     *        void *node ��Դ�����仯�Ķ�Ӧ���
     * �����������
     * ����ֵ��
     *       =true  ��Ҫ�������ݸ���һ��ʧ��δput��token
     *       =false ����Ҫ���ݸ�������token
     * */
    bool (*node_changed)(void *token, void *pri, void *node);
} applicant_event_ops_t;


/*����Դ������ṩ�Ĳ�����*/
typedef struct applicant_ops
{
    /*
     * ����������һ���Ի�ȡ��Դ�б���������Դ�ķ�������
     * ���������
     *        resource_list_t *rs_list Ҫ�������Դ�б�
     *        void *pri   ������ģ��������ָ�� �˺�����ά����ָ��������ָ���ַ���������� ���ô˺����ɹ�ʱ�ᱻ���õ�token����ȥ
     *        bool return_rightnow Ϊtrueʱ��ʾtoken���ᱻ���� ����������ʱ���ɷ���
     * ���������void **token ���ƵĶ���ָ�� ����*token�����������ɱ�ģ��ά�� ��put_tokenʱ�ͷ�
     * ����ֵ��
     *       = 0  ���ɻ��token�ɹ� ����ֱ�ӻ�ȡ��Դ
     *       > 0  �޷�ֱ�ӻ�ȡ��Դ��Ҫ�Ŷ�ʱ����ֵΪrs_list����Դ���� ��ȡ��Դʱ��ͨ�� applicant_event_ops�е� token_got����֪ͨ
     *       =-QOS_ERROR_PENDING  token��Դ���� �޷���ȡ�µ�token��ͨ�������ܷ�����
     *       =-QOS_ERROR_FAILEDNODE  ��Դ�б��д��ڲ��ɻ�ȡ����Դid
     *       =-QOS_ERROR_TOOBIGCOST  ��QOS_ERROR_FAILEDNODE���������������Դ�б��д��ڲ��Ϸ�����Դ����ֵ������/Ϊ0��
     *       =-QOS_ERROR_NULLPOINTER �������ָ��Ƿ�
     * */
    int (*get_token_easy)(void **token, resource_list_t *rs_list, void *pri,
                          bool return_rightnow);
    
    /*
     * ��������������������Դ ���������ߵ���������������
     * ���������
     *        void *pri   ������ģ��������ָ�� �˺�����ά����ָ��������ָ���ַ���������� ���ô˺����ɹ�ʱ�ᱻ���õ�token����ȥ
     * �����������
     * ����ֵ��
     *       = NULL token��Դ���� �޷���ȡ�µ�token��ͨ�������ܷ�����
     *       !=NULL ����tokenָ��� ָ��������ɱ�ģ��ά�� ��put_tokenʱ�ͷ�
     * */
    void *(*get_token)(void *pri);
    
    /*
     * �������������������ύ��Դ �൱�뽫get_token_easy���ɶ������ ÿ����Դsubmitһ�� ���commitʱ�Ż������ύ����
     * ���������
     *        void *token ��get_token��ȡ��token���� һ��token��commit�ɹ�������submit ����token_got��ʱʧ��
     *        get_token_easy �ɹ���õ���tokenҲ����submit ʧ�ܺ���Լ���submit/ignore���� commit
     *        resource_t * token_need Ҫ�������Դ���� ������ά��rs����������  �ڲ��������п���
     * �����������
     * ����ֵ��
     *       = 0 ����ɹ�
     *       =-QOS_ERROR_FAILEDNODE  ��Դ�б��д��ڲ��ɻ�ȡ����Դid
     *       =-QOS_ERROR_TOOBIGCOST  ��QOS_ERROR_FAILEDNODE���������������Դ�б��д��ڲ��Ϸ�����Դ����ֵ������/Ϊ0��
     *       =-QOS_ERROR_NULLPOINTER �������ָ��Ƿ�
     * */
    int (*sumbit_to_token)(void *token, resource_t *rs);
    
    /*
     * ������������token���Ѿ�
     * ���������
     *        void *token ��get_token��ȡ��token���� һ��token��commit�ɹ�������ignore ����token_got��ʱʧ��
     *        get_token_easy �ɹ���õ���tokenҲ����submit ʧ�ܺ���Լ���submit/ignore���� commit
     *        resource_t * token_need Ҫ�������Դ���� ������ά��rs����������  �ڲ��������п���
     * �����������
     * ����ֵ��
     *       = 0 ȡ��token�е�ĳ����Դ
     *       =-QOS_ERROR_FAILEDNODE  ��Դ�б��д��ڲ��ɻ�ȡ����Դid
     *       =-QOS_ERROR_TOOBIGCOST  ��QOS_ERROR_FAILEDNODE���������������Դ�б��д��ڲ��Ϸ�����Դ����ֵ������/Ϊ0��
     *       =-QOS_ERROR_NULLPOINTER �������ָ��Ƿ�
     *       =-QOS_ERROR_NOSUCHRESOURCE ���������resouce���ڴ�token��
     * */
    int (*ignore_from_token)(void *token, resource_t *rs);
    
    /*
     * ������������������������Դ �൱�뽫get_token_easy���ɶ������ ÿ����Դsubmitһ�� ���commitʱ�Ż������ύ����
     * ���������
     *        void *token ��get_token��ȡ��token���� һ��token��commit�ɹ�������submit��commit ����token_got��ʱʧ��
     *        get_token_easy �ɹ���õ���tokenҲ����submit ʧ�ܺ���Լ���submit/ignore���� commit
     *        bool return_rightnow Ϊtrueʱ��ʾtoken���ᱻ���� ����������ʱ���ɷ���
     * �����������
     * ����ֵ��
     *       = 0  ���ɻ��token�ɹ� ����ֱ�ӻ�ȡ��Դ
     *       > 0  �޷�ֱ�ӻ�ȡ��Դ��Ҫ�Ŷ�ʱ����ֵ����submit��������Դ���� ��ȡ��Դʱ��ͨ�� applicant_event_ops�е� token_got����֪ͨ
     *       =-QOS_ERROR_FAILEDNODE  ��Դ�б��д��ڲ��ɻ�ȡ����Դid
     *       =-QOS_ERROR_TOOBIGCOST  ��QOS_ERROR_FAILEDNODE���������������Դ�б��д��ڲ��Ϸ�����Դ����ֵ������/Ϊ0��
     *       =-QOS_ERROR_NULLPOINTER �������ָ��Ƿ�
     * */
    int (*commit_token)(void *token, bool return_rightnow);
    
    /*
     * �����������ͷ�token��Դ ����*tokenָ���ó�NULL
     * ���������
     *        void **token ��get_token/get_token_easy��dȡ��token����
     *        ��commit_token��get_token_easy�󷵻ش���0֮�� token_got�����֮ǰtoken���ܵ��ô˺���
     * �����������
     * ����ֵ��
     *       = 0 �ͷųɹ�
     *       =-QOS_ERROR_NULLPOINTER �������ָ��Ƿ�
     * */
    int (*put_token)(void **token);
    
    /*
     * ��������������������Դ���¼��ص� ��Ҫ�ڹ�������
     * ���������applicant_event_ops_t *ops ������Դ���¼��ص�
     * �����������
     * ����ֵ����
     * */
    void (*set_applicant_event_ops)(applicant_event_ops_t *ops);
    
    /*
     * ��������������server��Ϣ���ݲ����� ��Ҫ�ڹ�������
     * ���������msg_ops_t *ops ��Ϣ���ݲ�����
     * �����������
     * ����ֵ����
     * */
    void (*set_msg_ops)(msg_ops_t *ops);
} applicant_ops_t;


/*
 * ����������qos��client�˳�ʼ�� �ڲ�������̬��������qos_client_obj�Ĺ���
 * �����������
 * �����������
 * ����ֵ����
 * */
void qos_client_init();

/*
 * ����������qos��client���� �ڲ�������̬��������qos_client_obj������
 * �����������
 * �����������
 * ����ֵ����
 * */
void qos_client_exit();

/*********************************************************************************/

/***qos_server.h******************************************************************/
/*������Դ�¼��ص���*/
typedef struct dispatcher_event_ops
{
    /*
     * ����������������Դ������Դ�õ������Լ�����
     * ���������
     *        long new_size ���ٺ����Դ��
     * �����������
     * ����ֵ����
     * */
    void (*resource_reduced)(long new_size);
} dispatcher_event_ops_t;

/*������Դ������������Դ�仯����¼�����*/
typedef struct dispatcher_ops
{
    /*
     * �ر�˵�� �����ӿ��м���������Դ�Ļ�ȡ ����Ҫ����resource_t *token_need ���������쳣�������µ�����Ҫ�����ͷŵ������Ѿ��������Դ
     * */
    
    /*
     * ����������������Դ������Դ�õ����������� �״�����ʱ��Ҫ���øú���������Դ��ֵ
     * ���������
     *        long cost ���ӵ���Դ��
     * �����������
     * ����ֵ����
     * */
    void (*resource_increase)(long cost);
    
    /*
     * ����������������Դ������Դ�õ������Լ���
     * ���������
     *        long cost ���ٵ���Դ��
     * �����������
     * ����ֵ����
     * */
    void (*resource_reduce)(long cost);
    
    /*
     * ��������������server��Ϣ���ݲ����� ��Ҫ�ڹ�������
     * ���������dispatcher_event_ops_t* ops ������Դ�¼��ص���
     * �����������
     * ����ֵ����
     * */
    void (*set_dispatcher_event_ops)(dispatcher_event_ops_t *ops);
    
    /*
     * ��������������server��Ϣ���ݲ����� ��Ҫ�ڹ�������
     * ���������msg_ops_t *ops ��Ϣ���ݲ�����
     * �����������
     * ����ֵ����
     * */
    void (*set_msg_ops)(msg_ops_t *ops);
} dispatcher_ops_t;

/*
 * ����������qos��server�˳�ʼ�� �ڲ�������̬��������qos_server_obj�Ĺ���
 * �����������
 * �����������
 * ����ֵ����
 * */
void qos_server_init();

/*
 * ����������qos��server������ �ڲ�������̬��������qos_server_obj������
 * �����������
 * �����������
 * ����ֵ����
 * */
void qos_server_exit();

/*********************************************************************************/

#ifdef __cplusplus
}
#endif
#endif //QOS_INTERFACE_H