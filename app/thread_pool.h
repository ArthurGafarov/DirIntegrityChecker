#ifndef THREADPOOL_H
#define	THREADPOOL_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>
#include "thread_pool_queue.h"

class ThreadPool
{
public:

    ThreadPool(int threads, int queue_size):m_done(false), m_tasks(queue_size)
    {
        try
        {
            for( int i = 0; i < threads; ++i ) {
                std::thread th(&ThreadPool::worker, this);
                m_threads.push_back(std::move(th));
            }
        }
        catch(...)
        {
            terminate();

            throw;
        }
    }

    ~ThreadPool()
    {
        terminate();
    }

    template < typename FuncType >
    bool addTask(FuncType f)
    {
        {
            std::lock_guard< std::mutex > lg(m_mut);

            if ( !m_tasks.push(ThreadPoolQueue::ThreadFunc(f)) )
                return false;
        }
        m_cond.notify_one();

        return true;
    }

private:

    std::atomic< bool > m_done;
    std::mutex m_mut;
    std::condition_variable m_cond;

    ThreadPoolQueue m_tasks;
    std::vector< std::thread > m_threads;

    void worker()
    {
        ThreadPoolQueue::ThreadFunc f;

        while( 1 )
        {
            {
                std::unique_lock< std::mutex > ul(m_mut);
                m_cond.wait(ul, [&]{return !m_tasks.isEmpty();});

                if ( m_done )
                    return;

                f = m_tasks.pop();

                //
                // if tasks stay in queue
                //
                if ( !m_tasks.isEmpty() )
                    m_cond.notify_one();
            }

            f();
        }
    }

    void terminate()
    {
        m_done = true;

        //
        // add empty task for unlock condition
        //
        addTask(ThreadPoolQueue::ThreadFunc());
        m_cond.notify_all();

        for( size_t i = 0; i < m_threads.size(); ++i )
        {
            if ( m_threads[ i ].joinable() )
                m_threads[ i ].join();
        }
    }
};

#endif	/* THREADPOOL_H */

