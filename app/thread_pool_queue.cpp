#include "thread_pool_queue.h"

bool ThreadPoolQueue::push(const ThreadFunc& func)
{
    std::lock_guard< std::mutex > lg(m_mut);
    
    if ( (int)m_queue.size() >= m_maxSize )
        return false;
    
    m_queue.push(func);
    
    return true;
}

ThreadPoolQueue::ThreadFunc ThreadPoolQueue::pop()
{
    std::lock_guard< std::mutex > lg(m_mut);
    
    if ( m_queue.empty() )
        return ThreadFunc();
    
    ThreadFunc f = m_queue.front();
    m_queue.pop();
    
    return f;
}

size_t ThreadPoolQueue::size() const
{
    std::lock_guard< std::mutex > lg(m_mut);
    
    return m_queue.size();
}

bool ThreadPoolQueue::isEmpty() const
{
    std::lock_guard< std::mutex > lg(m_mut);
    
    return m_queue.empty();
}
