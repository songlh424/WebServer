#ifndef THREADPOOL_H_
#define THREADPOOL_H_

/*
    线程池类，定义成模板以便代码复用
*/

#include <pthread.h>
#include <list>
#include <cstdio>
#include <exception>

#include "locker.h"
#include "mysql/sql_connection_pool.h"

template<typename T>
class threadpool {
public:
    threadpool(int actor_model, connection_pool *connPool, int thread_number = 8, int max_requests = 10000);
    ~threadpool();
    bool append(T* request, int state);
    bool append_p(T *request);

private:
    //工作线程运行的函数，它不断从工作队列中取出任务并执行之
    static void* worker(void* arg);
    void run();

    int m_thread_number; // 线程的数量
    int m_max_requests; // 请求队列中最多允许的等待处理的请求数量
    pthread_t* m_threads; // 线程池数组，大小为m_thread_number
    std::list<T *> m_workqueue; // 请求队列
    locker m_queuelocker; // 互斥锁
    sem m_queuenstat; // 信号量用来判断是否有任务需要处理
    connection_pool *m_connPool; //数据库
    int m_actor_model;  //模式切换
};

template<typename T>
threadpool<T>::threadpool(int actor_model, connection_pool *connPool, int thread_number, int max_requests) :
    m_actor_model(actor_model), m_thread_number(thread_number), m_max_requests(max_requests),
    m_threads(NULL), m_connPool(connPool)
{
    if(thread_number <= 0 || max_requests <= 0) {
        throw std::exception();
    }
    //线程号数组
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads) {
        throw std::exception();
    }

    // 创建thread_number个线程，并将它们设置为线程脱离
    for(int i = 0; i < thread_number; ++i) {
        printf("create the %dth thread\n", i);

        if(pthread_create(m_threads + i, NULL, worker, this) != 0) {
            delete [] m_threads;
            throw std::exception();
        } 

        if(thread_detach(m_threads[i])) {
            delete [] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool() {
    delete[] m_threads;
}

template<typename T>
bool threadpool<T>::append(T* request, int state) {
    m_queuelocker.lock();
    if(m_workqueue.size() > m_max_requests) {
        m_queuelocker.unlock();
        return false;
    }

    request->m_state = state;
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuenstat.post();
    return true;
}

template<typename T>
bool threadpool<T>::append_p(T* request) {
    m_queuelocker.lock();
    if(m_workqueue.size() > m_max_requests) {
        m_queuelocker.unlock();
        return false;
    }

    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuenstat.post();
    return true;
}

template<typename T>
void* threadpool<T>::worker(void* arg) {
    threadpool* pool = (threadpool*)arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run() {
    while(true) {
        m_queuenstat.wait();
        m_queuelocker.lock();
        if(m_workqueue.empty()) {
            m_queuelocker.unlock();
            continue;
        }

        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();

        if(!request) {
            continue;
        }
        // Reactor模式
        if(m_actor_model == 1)
        {
            if(request->m_state)//m_state为1，则是读事件
            {
                if(request->read_once())
                {
                    request->improv = 1;
                    connectionRAII mysqlcon(&request->mysql, m_connPool);
                    request->process();
                }
                else 
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
            else    //m_state为0，则是写事件
            {
                if(request->write())
                {
                    request->improv = 1;
                }
                else 
                {
                    request->improv = 1;
                    request->timer_flag = 1;
                }
            }
        }
        else 
        {
            connectionRAII mysqlcon(&request->mysql, m_connPool);
            request->process();
        }
    }
}

#endif //THREADPOOL_H_