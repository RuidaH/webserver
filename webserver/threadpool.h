#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <list>
#include <exception>
#include <cstdio>

#include "locker.h"

// 模版类的定义和实现需要放在一个文件中

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

template <typename T>
Threadpool<T>::Threadpool(int thread_number, int max_request): 
    m_thread_number(thread_number), 
    m_max_requests(max_request), 
    m_stop(false), 
    m_threads(NULL) {
        if ((thread_number <= 0) || (max_request <= 0)) {
            throw std::exception();
        }

        // 创建线程池数组
        m_threads = new pthread_t[m_thread_number];
        if (!m_threads) {
            throw std::exception();
        }

        // 创建 thread_number 个线程, 并将他们设置为线程分离
        for (int i = 0; i < thread_number; ++i) {
            printf("create the %d-th thread\n", i);

            // 最后一个参数是 threadpool, 因为 worker 作为静态函数是不能访问对象的成员的
            // 所以可以把 this 作为 worker 的参数传进去
            if (pthread_create(m_threads + i, NULL, worker, this) != 0) {
              delete[] m_threads;
              throw std::exception();
            }

            if (pthread_detach(m_threads[i])) {
              delete[] m_threads;
              throw std::exception();
            }
        }
}

template <typename T>
Threadpool<T>::~Threadpool() {
    delete[] m_threads;
    m_stop = true;
}

template <typename T>
bool Threadpool<T>::append(T* request) {
    m_queue_locker.lock();
    if (m_workqueue.size() > m_max_requests) {
        m_queue_locker.unlock();
        return false;
    }

    m_workqueue.push_back(request);
    m_queue_locker.unlock();
    m_queue_stat.post(); // 信号通知有新的请求进去队列
    return true; 
}

template <typename T>
void* Threadpool<T>::worker(void* arg) {
    Threadpool *pool = (Threadpool *)arg;
    pool->run();
    return pool;
}

template <typename T>
void Threadpool<T>::run() {
    while (!m_stop) {
        m_queue_stat.wait(); // sem 表示消息队列中如果没有任务, 就会阻塞在这里, 防止循环空转
        m_queue_locker.lock();
        if (m_workqueue.empty()) { // 这里的判断优点多余, 但是可以用来 unlock()
            m_queue_locker.unlock();
            continue;
        }

        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queue_locker.unlock();
        if (!request) {
            continue;
        }

        request->process();
    }
}

#endif
