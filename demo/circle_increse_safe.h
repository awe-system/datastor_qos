//
// Created by root on 18-9-14.
//

#ifndef DATASTOR_QOS_CIRCLE_INCRESE_SAFE_H
#define DATASTOR_QOS_CIRCLE_INCRESE_SAFE_H


#include <mutex>

class circle_increse_safe
{
    std::mutex m;
    int cnt;
    int max_size;
public:
    circle_increse_safe(int _max_size);
    int get_next();
};


#endif //DATASTOR_QOS_CIRCLE_INCRESE_SAFE_H
