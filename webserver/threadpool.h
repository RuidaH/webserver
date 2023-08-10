#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <list>
#include <exception>
#include <cstdio>

#include "locker.h"

// 线程池类, 定位成模版类是为了代码的复用, 模版参数就是任务类
template <typename T>
class Threadpool {
public:
 Threadpool(int thread_number = 8, int max_request = 10000);
 ~Threadpool();
 bool append(T* request);
 void run();

private:
 // 需要设置为静态函数, 因为函数传入thread只能有一个参数, 如果是成员函数的话就会有两个参数 (this, arg)
 static void *worker(void *arg); 

private:
 int m_thread_number;           // 线程池的数量
 pthread_t* m_threads;          // 线程池数组的大小
 int m_max_requests;            // 请求队列中最多被允许的等待处理的请求数量
 std::list<T*> m_workqueue;     // 供所有线程共享的请求队列
 Locker m_queue_locker;         // 请求队列的互斥锁
 Sem m_queue_stat;              // 信号量用来判断是否有任务需要处理
 bool m_stop;                   // 是否结束线程
};

#endif