//
// Created by root on 18-9-19.
//

#ifndef DATASTOR_QOS_SYSQOS_RESOURCE_H_H
#define DATASTOR_QOS_SYSQOS_RESOURCE_H_H

#include "sysqos_interface.h"

/*资源链表*/
typedef struct resource_list
{
    struct list_head list;
    resource_t       rs;//其中一个资源 这里只做值拷贝
} resource_list_t;

#endif //DATASTOR_QOS_SYSQOS_RESOURCE_H_H
