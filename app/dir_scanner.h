#include <filesystem>
#include <shared_mutex>

#include "thread_pool.h"
#include "waitgroup.h"
#include "watcher.h"

class DirScanner : public Watcher {

    public:

        DirScanner(const std::string& dir, int threadsCount, int threadQeueSize);

        void Scan(bool save=false);

#ifdef COMPILE_UNUSED_FUNC
        void Check();
#endif

    private:

        // ATTENTION: m_waitGroup.Add() must be called before this function to synchronize output status
        void calculateCrc(const std::filesystem::path& filename, bool save);

        const std::filesystem::path m_directory;
        std::unique_ptr<ThreadPool> m_workerTreads;
        std::shared_mutex m_mutex;
        std::unordered_map<std::filesystem::path, uint32_t> m_fileCrcMap;
        // ATTENTION: possible deadlock or race condition, Done() MUST be called for each Add()
        WaitGroup m_waitGroup;
        std::atomic<bool> m_ok;
};
