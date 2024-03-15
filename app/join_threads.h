#ifndef JOINTHREADS_H
#define	JOINTHREADS_H

#include <thread>
#include <vector>

class JoinThreads
{
public:
    
    explicit JoinThreads(std::vector< std::thread > &_threads):m_threads(_threads){}
    
    ~JoinThreads()
    {
        for( size_t i = 0; i < m_threads.size(); ++i )
            if ( m_threads[ i ].joinable() )
                m_threads[ i ].join();
    }
    
private:
    
    std::vector< std::thread >& m_threads;
};

#endif	/* JOINTHREADS_H */

