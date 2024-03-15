#include "periodic_task.h"

#include <functional>
#include <utility>

PeriodicTask::PeriodicTask(int period, std::function<void()> action) //
    : m_period(period), m_action(action)

{
    m_thread = std::thread(&PeriodicTask::loop, this);
}

PeriodicTask::~PeriodicTask()
{
    std::future<bool> future = m_promise.get_future();

    // must be get without mutex protect, waiting before thread has started
    future.get();

    {
        std::lock_guard guard(m_mutex);
        m_cond.notify_all();
    }

    if (m_thread.joinable())
        m_thread.join();
}

void PeriodicTask::loop()
{
    while (true)
    {

        std::unique_lock lock(m_mutex);

        // must be set under mutex protect to warrant that the std::conditional_variable will go into waiting state
        // before that a notify will be sent
        if (!m_isPromSet)
        {
            m_promise.set_value(true); // any value to control that thread was started
            m_isPromSet = true;
        }

        if (m_cond.wait_for(lock, std::chrono::seconds(m_period)) == std::cv_status::no_timeout)
            break;

        m_action();
    }
}
