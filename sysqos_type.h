//
// Created by GaoHualong on 2018/9/8.
//

#ifndef TEST_QOS_SYSQOS_TYPE_H
#define TEST_QOS_SYSQOS_TYPE_H

//#define RUNNING_OFS
#define RUNNING_MAC
#include <sys/types.h>

#ifdef RUNNING_OFS

#elif defined(__APPLE__)

#include <sys/types.h>
#include <libkern/OSAtomic.h>
#define sysqos_spin_lock_t OSSpinLock
#define sysqos_spin_lock OSSpinLockLock
#define sysqos_spin_unlock OSSpinLockUnlock
#define sysqos_spin_init(lck) ({*(lck) = 0;})
#define sysqos_spin_destroy(lck)
#elif defined(__linux)
#include <sys/types.h>

#define sysqos_spin_lock_t pthread_spinlock_t
#define sysqos_spin_lock pthread_spin_lock
#define sysqos_spin_unlock pthread_spin_unlock
#define sysqos_spin_init(lck) pthread_spin_init(lck, PTHREAD_PROCESS_PRIVATE)
#define sysqos_spin_destroy pthread_spin_destroy
#define sysqos_spinlock_t pthread_spinlock_t
#endif

#endif //TEST_QOS_SYSQOS_TYPE_H
