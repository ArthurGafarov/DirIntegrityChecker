#ifndef THREADPOOLQUEUE_H
#define	THREADPOOLQUEUE_H

#include <queue>
#include <functional>
#include <mutex>

class ThreadPoolQueue
{
public:
    
    typedef std::function< void() > ThreadFunc;
    
    ThreadPoolQueue(int nSize = 100000):m_maxSize(nSize){}
    
    bool push(const ThreadFunc &func);
    ThreadFunc pop();
    size_t size() const;
    bool isEmpty() const;
    
private:
    
    std::queue< ThreadFunc > m_queue;
    int m_maxSize;
    mutable std::mutex m_mut;
};

#endif	/* THREADPOOLQUEUE_H */

