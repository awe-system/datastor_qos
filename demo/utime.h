//
// Created by root on 18-9-15.
//

#ifndef DATASTOR_QOS_UTIME_H
#define DATASTOR_QOS_UTIME_H

class utime
{
public:
    std::chrono::system_clock::time_point tp;
    
    utime(const utime & other);
    
    utime();
    
    void record();
    
    long usec_elapsed_since(utime &last_time);
};


#endif //DATASTOR_QOS_UTIME_H
