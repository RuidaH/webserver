#include "threadpool.h"

template <typename T>
Threadpool<T>::Threadpool(int thread_number, int max_request): 
    m_thread_number(thread_number), 
    m_max_request(max_request), 
    m_stop(false), 
    m_threads(NULL) {
        if ((thread_number <= 0) || (max_request <= 0)) {
            throw std::exception();
        }

        // 创建线程池数组
        m_threads = new pthread_t[m_thread_number];
        if (m_threads) {
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

            if (pthread_detach(m_thread[i])) {
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
    threadpool *pool = (threadpoll *)arg;
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