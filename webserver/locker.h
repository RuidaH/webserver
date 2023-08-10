#ifndef LOCKER_H
#define LOCKER_H

#include <pthread.h>
#include <exception>
#include <semaphore.h>

// 线程同步机制封装类

// 互斥锁类
class Locker {
public:
 Locker();
 ~Locker();
 bool lock();
 bool unlock();
 pthread_mutex_t* get();

private:
 pthread_mutex_t m_mutex;
};

// 条件变量
class Cond {
public:
 Cond();
 ~Cond();
 bool wait(pthread_mutex_t *mutex);
 bool timedwait(pthread_mutex_t *mutex, struct timespec t);
 bool signal(); // 只是唤醒一个线程
 bool broadcast(); // 唤醒所有的线程

private:
 pthread_cond_t m_cond;
};

// 信号量类
class Sem {
public:
 Sem();
 Sem(int num);
 ~Sem();
 bool wait(); // 等待信号量
 bool post(); // 增加信号量

private:
 sem_t m_sem;
};

#endif