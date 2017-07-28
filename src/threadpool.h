#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"

template< typename T >
class threadpool
{
private:
    int m_thread_number;
    int m_max_requests;
    pthread_t* m_threads;  //线程标识符指针
    std::list< T* > m_workqueue;  //工作队列
    locker m_queuelocker;  //互斥锁
    sem m_queuestat;  //信号量
    bool m_stop;

public:
    threadpool( int thread_number = 8, int max_requests = 10000 );
    ~threadpool();
    bool append( T* request );  //往请求队列中追加任务

private:
    static void* worker( void* arg );  //工作线程执行函数，从工作队列中取出任务并执行
    void run();
};

template< typename T >
threadpool< T >::threadpool( int thread_number, int max_requests ) :
        m_thread_number( thread_number ), m_max_requests( max_requests ), m_stop( false ), m_threads( NULL )
{
    if( ( thread_number <= 0 ) || ( max_requests <= 0 ) )
    {
        throw std::exception();
    }

    m_threads = new pthread_t[ m_thread_number ];
    if( ! m_threads )
    {
        throw std::exception();
    }

    for ( int i = 0; i < thread_number; ++i )
    {
        printf( "create the %dth thread\n", i );
        if( pthread_create( m_threads + i, NULL, worker, this ) != 0 )  //线程执行函数(静态函数)中要访问线程池动态成员(包括函数和动态变量)，可将类的对象作为参数传递给该静态函数，然后在静态函数中引用这个对象
        {
            delete [] m_threads;
            throw std::exception();
        }
        if( pthread_detach( m_threads[i] ) )  //以分离方式启动线程，线程结束后会自动释放资源
        {
            delete [] m_threads;
            throw std::exception();
        }
    }
}

template< typename T >
threadpool< T >::~threadpool()
{
    delete [] m_threads;
    m_stop = true;
}

template< typename T >
bool threadpool< T >::append( T* request )  //往请求队列中添加任务
{
    m_queuelocker.lock();
    if ( m_workqueue.size() > m_max_requests )
    {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back( request );
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template< typename T >
void* threadpool< T >::worker( void* arg )
{
    threadpool* pool = ( threadpool* )arg;  //静态函数中引用类对象
    pool->run();
    return pool;
}

template< typename T >
void threadpool< T >::run()
{
    while ( ! m_stop )
    {
        m_queuestat.wait();  //信号量
        m_queuelocker.lock();  //互斥锁
        if ( m_workqueue.empty() )
        {
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();  //获取工作队列中的任务
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if ( ! request )
        {
            continue;
        }
        request->process();  //线程池处理业务逻辑的入口
    }
}

#endif
