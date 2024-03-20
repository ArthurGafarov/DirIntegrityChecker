#include <mutex>
#include <atomic>
#include <condition_variable>

class WaitGroup
{
public:
    void Add(int incr = 1) { counter += incr; }
    void Done() { if (--counter <= 0) cond.notify_all(); }
    void Wait() {
        std::unique_lock<std::mutex> lock(mutex);
        cond.wait(lock, [&] { return counter <= 0; });
    }

private:
    std::mutex mutex;
    std::atomic<int> counter = 0;
    std::condition_variable cond;
};
