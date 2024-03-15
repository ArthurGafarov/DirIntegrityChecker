#ifndef PERIODIC_TASK_BASE_H
#define PERIODIC_TASK_BASE_H

#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>

class PeriodicTask
{
  public:
    PeriodicTask() = delete;
    virtual ~PeriodicTask();

    explicit PeriodicTask(int period, std::function<void()> action);

  private:
    std::mutex m_mutex;
    std::condition_variable m_cond;
    std::thread m_thread;
    int m_period;

    // to control thread was started
    std::promise<bool> m_promise;
    // not atomic because uses only by one thread
    bool m_isPromSet = false;

    void loop();
    std::function<void()> m_action;
};

#endif // PERIODIC_TASK_BASE_H
